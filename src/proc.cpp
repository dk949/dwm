#include "proc.hpp"

#include "file.hpp"
#include "log.hpp"
#include "strerror.hpp"

#include <fcntl.h>
#include <libgen.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
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
    static constexpr auto bad_exit = 127;
    if (auto count = cleanUpZombies(); count != -1ull) lg::debug("Cleaned {} zombies on startup", count);


    switch (auto child = fork()) {
        case 0: {
            if (dpy) close(ConnectionNumber(dpy));
            if (setsid() < 0) {
                lg::error("Child process setsid error: {}", strError(errno));
                _exit(bad_exit);
            }
            if (auto err = pthread_sigmask(SIG_SETMASK, &original_sigset, nullptr)) {
                lg::error("Child process failed to reset signal mask: {}", strError(err));
                _exit(bad_exit);
            }

            if (!Proc::redirect({.from = Proc::stdOut(), .to = Proc::devNull()})) {
                lg::warn("Could not redirect child stdout to /dev/null: {}", strError(errno));
                _exit(bad_exit);
            }
            if (!Proc::redirect({.from = Proc::stdErr(), .to = Proc::devNull()})) {
                lg::warn("Could not redirect child stderr to /dev/null: {}", strError(errno));
                _exit(bad_exit);
            }
            execvp(argv[0], argv);
            lg::error("Child process {} failed to execvp", argv[0]);
            _exit(bad_exit);
        } break;
        default: return child;
        case -1: lg::error("fork failed: {}", strError(errno)); return -1;
    }
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

int Proc::devNull() {
    // Since child process' stdout and stderr are redirected to this fd, it is not opened with O_CLOEXEC
    if (!dev_null) dev_null.acquire(open("/dev/null", O_WRONLY));
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
