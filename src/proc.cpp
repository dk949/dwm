#include "proc.hpp"

#include "file.hpp"
#include "log.hpp"
#include "strerror.hpp"

#include <fcntl.h>
#include <libgen.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ut/sv_to_num/sv_to_num.hpp>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <ranges>

namespace rng = std::ranges;
namespace vws = std::views;
static sigset_t original_sigset {};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

pid_t Proc::spawnDetached(Display *dpy, std::vector<std::string> args) {
    auto argv = args | vws::transform([](auto &str) noexcept { return str.data(); }) | rng::to<std::vector>();
    argv.push_back(nullptr);
    return spawnDetached(dpy, argv.data());
}

pid_t Proc::spawnDetached(Display *dpy, char *const *argv) {
    return spawn(dpy, argv, {.out = Proc::devNull(), .err = Proc::devNull(), .detach = true})
        .transform([](Proc p) noexcept { return p.m_pid; })
        .value_or(-1);
}

std::size_t Proc::cleanUpZombies() {
    std::size_t count = 0;
    while (true) {
        switch (waitpid(-1, nullptr, WNOHANG)) {
            case 0: return count;
            case -1:
                if (errno != EINTR) {
                    lg::error("waitpid failed when cleaning up zombies: {}", strError(errno));
                    return -1ull;
                }
                break;
            default: count++;
        }
    }
}

FDPtr Proc::dev_null {};

static int tryOpenDevNull() {
    // When dwm is restarted, /dev/fd is re-opened, try to pick up that fd instead of opening a new one
    std::error_code ec;
    for (auto const &dirent : std::filesystem::directory_iterator {"/dev/fd"}) {
        if (!dirent.is_symlink(ec) || ec) continue;
        auto const &symlink_path = dirent.path();
        auto real_path = std::filesystem::read_symlink(symlink_path, ec);
        if (ec) continue;
        if (real_path != "/dev/null") continue;
        if (auto fd = ut::svToNum<int>(symlink_path.stem().native())) return *fd;
    }

    return open("/dev/null", O_WRONLY);
}

int Proc::devNull() {
    // Since child process' stdout and stderr are redirected to this fd, it is not opened with O_CLOEXEC
    if (!dev_null) dev_null.acquire(tryOpenDevNull());
    return dev_null.get();
}

int Proc::stdIn() {
    return STDIN_FILENO;
}

int Proc::stdOut() {
    return STDOUT_FILENO;
}

int Proc::stdErr() {
    return STDERR_FILENO;
}

bool Proc::redirect(Redirection r) {
    return dup2(r.to, r.from) >= 0;
}

FDPtr Proc::sfd {};

void Proc::setupSignals() {
    sigset_t set {};
    if (sigemptyset(&set)) {
        lg::error("Failed to create empty signal set: {}", strError(errno));
        return;
    };
    if (sigaddset(&set, SIGCHLD)) {
        lg::error("Failed to add SIGCHLD to the signal set: {}", strError(errno));
        return;
    }
    if (auto err = pthread_sigmask(SIG_BLOCK, &set, &original_sigset)) {
        lg::error("Failed to set signal mast to block SIGCHLD: {}", strError(err));
        return;
    }

    sfd.acquire(signalfd(-1, &set, SFD_CLOEXEC | SFD_NONBLOCK));

    if (sfd.get() == -1) {
        lg::error("Failed to open signalfd: {}", strError(errno));
        (void)sfd.takeOwnership();
        return;
    }
}

std::optional<Proc> Proc::spawn(Display *dpy, char *const *argv, SpawnConfig conf) {
    static constexpr auto bad_exit = 127;

    auto [stdinpipe, stdoutpipe, stderrpipe] = arrangePipes(conf);
    switch (auto child = fork()) {
        case 0: {
            closePipe(stdinpipe.write);
            closePipe(stdoutpipe.read);
            closePipe(stderrpipe.read);
            if (dpy) close(ConnectionNumber(dpy));
            if (auto err = pthread_sigmask(SIG_SETMASK, &original_sigset, nullptr)) {
                lg::error("Child process failed to reset signal mask: {}", strError(err));
                _exit(bad_exit);
            }
            if (conf.detach) trySetsid(bad_exit);
            if (conf.in) tryRedirect({.from = STDIN_FILENO, .to = stdinpipe.read}, bad_exit);
            if (conf.out) tryRedirect({.from = STDOUT_FILENO, .to = stdoutpipe.write}, bad_exit);
            if (conf.err) tryRedirect({.from = STDERR_FILENO, .to = stderrpipe.write}, bad_exit);

            execvp(argv[0], argv);
            lg::error("Child process {} failed to execvp", argv[0]);
            _exit(bad_exit);
        } break;
        default:
            closePipe(stdinpipe.read);
            closePipe(stdoutpipe.write);
            closePipe(stderrpipe.write);
            return Proc {child, stdinpipe.write, stdoutpipe.read, stderrpipe.read};
        case -1: lg::error("fork failed: {}", strError(errno)); return std::nullopt;
    }
}

void Proc::tryRedirect(Redirection r, int bad_exit) {
    if (!Proc::redirect(r)) {
        lg::warn("Could not redirect {}: {}", r, strError(errno));
        _exit(bad_exit);
    }
}

void Proc::trySetsid(int bad_exit) {
    if (setsid() < 0) {
        lg::error("Child process setsid error: {}", strError(errno));
        _exit(bad_exit);
    }
}

// TODO(dk949): Make individual types for stdin, stdout and stderr
Proc::Proc(pid_t pid, int inpipe, int outpipe, int errpipe)
        : m_pid(pid)
        , m_stdin(inpipe)
        , m_stdout(outpipe)
        , m_stderr(errpipe) { }

// TODO(dk949): Maybe make a type for file descriptors
bool Proc::addFDFlag(int fd, unsigned flag) {
    auto flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        lg::error("Failed to get flags for FD {}: {}", fd, strError(errno));
        return false;
    }
    auto uflags = static_cast<unsigned>(flags);
    uflags |= flag;
    if (fcntl(fd, F_SETFD, uflags) == -1) {
        lg::error("Failed to set flags for FD {} to {:x}: {}", fd, uflags, strError(errno));
        return false;
    }
    return true;
}

std::optional<std::string_view> Proc::writeFD(std::string_view sv, int fd) {
    if (fd < 0) {
        lg::error("writeFD: invalid fd {}", fd);
        return std::nullopt;
    }

    char const *ptr = sv.data();
    size_t remaining = sv.size();

    // Limit each write() to at most SSIZE_MAX for portability.
    constexpr auto MAX_CHUNK = static_cast<size_t>(SSIZE_MAX);

    while (remaining > 0) {
        size_t to_write = std::min(remaining, MAX_CHUNK);
        ssize_t n = ::write(fd, ptr, to_write);

        if (n > 0) {
            ptr += static_cast<size_t>(n);
            remaining -= static_cast<size_t>(n);
            continue;
        }

        if (n == 0) {
            int saved_errno = errno;
            lg::error("writeFD: failed to write to fd {}: no progress (errno={})", fd, strError(saved_errno));
            return std::nullopt;
        }

        // n < 0: error
        if (errno == EINTR) continue;


        if (DWM_IS_EAGAIN(errno)) {
            size_t written = sv.size() - remaining;
            return sv.substr(written);
        }

        int saved_errno = errno;
        lg::error("writeFD: write failed for fd {}: {}", fd, strError(saved_errno));
        return std::nullopt;
    }

    return std::string_view {};
}

std::optional<std::string_view> Proc::writeFD(std::string const &sv, int fd) {
    return writeFD(std::string_view {sv}, fd);
}

std::optional<std::string_view> Proc::writeFD(char const *sv, int fd) {
    return writeFD(std::string_view {sv}, fd);
}

std::optional<std::pair<std::string, Proc::ReachedEOF>> Proc::readFD(int fd) {
    if (fd < 0) {
        lg::error("readFD: invalid fd {}", fd);
        return std::nullopt;
    }

    std::string out;
    constexpr size_t BUF_SZ = 8192;
    std::array<char, BUF_SZ> buf;

    while (true) {
        ssize_t n = ::read(fd, buf.data(), buf.size());

        if (n > 0) {
            out.append(buf.data(), static_cast<size_t>(n));
            continue;
        }

        // EOF
        if (n == 0) return std::make_optional(std::make_pair(std::move(out), ReachedEOF::Yes));


        // n < 0: error
        if (errno == EINTR) continue;

        if (DWM_IS_EAGAIN(errno)) return std::pair {std::move(out), ReachedEOF::No};


        int saved_errno = errno;
        lg::error("readFD: read failed for fd {}: {}", fd, strError(saved_errno));
        return std::nullopt;
    }
}

void Proc::closePipe(int p) {
    if (isPipe(p)) close(p);
}

void Proc::closeStdin() noexcept {
    closePipe(m_stdin);
    m_stdin = -1;
}

void Proc::closeStdout() noexcept {
    closePipe(m_stdout);
    m_stdout = -1;
}

void Proc::closeStderr() noexcept {
    closePipe(m_stderr);
    m_stderr = -1;
}

void Proc::closeAll() noexcept {
    closeStdin();
    closeStdout();
    closeStderr();
}

std::array<Proc::PipeFds, 3> Proc::arrangePipes(SpawnConfig const &conf) {
    std::array<Proc::PipeFds, 3> out {};

    if (conf.in) out[0] = {.read = STDIN_FILENO, .write = *conf.in};
    if (conf.out) out[1] = {.read = *conf.out, .write = STDOUT_FILENO};
    if (conf.err) out[2] = {.read = *conf.err, .write = STDERR_FILENO};


    for (auto &p : out) {
        if (p.read != pipe && p.write != pipe) continue;
        std::array<int, 2> pipes {};
        if (::pipe2(pipes.data(), 0) < 0) {
            lg::error("Failed to open a pipe for child process: {}", strError(errno));
            p.read = devNull();
            p.write = devNull();
            continue;
        }

        p.read = pipes[0];
        p.write = pipes[1];
    }

    return out;
}

bool Proc::isPipe(int fd) {
    switch (fd) {
        case -1:
        case STDIN_FILENO:
        case STDOUT_FILENO:
        case STDERR_FILENO: return false;
        default:
            if (fd == Proc::devNull()) return false;
    }
    return true;
}

void Proc::setupDebugging() {
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) == -1) lg::error("Failed to allow ptrace: {}", strError(errno));
}
