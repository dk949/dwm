#include "proc.hpp"

#include "file.hpp"
#include "log.hpp"
#include "strerror.hpp"

#include <libgen.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ranges>

namespace rng = std::ranges;
namespace vws = std::views;

void Proc::spawnDetached(Display *dpy, std::vector<std::string> args) {
    auto argv = args | vws::transform([](auto &str) noexcept { return str.data(); }) | rng::to<std::vector>();
    argv.push_back(nullptr);
    return spawnDetached(dpy, argv.data());
}

void Proc::spawnDetached(Display *dpy, char *const *argv) {
    static constexpr auto bad_execvp = 127;

    enum ChildExit : std::uint8_t { OK = 0, SETSID, FORK, REDIRECT_STDOUT, REDIRECT_STDERR };

    switch (auto child = fork()) {
        case 0: {
            if (dpy) close(ConnectionNumber(dpy));
            if (setsid() < 0) _exit(SETSID);
            struct sigaction sa {};
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGCHLD, &sa, nullptr);

            if (!Proc::redirect({.from = Proc::stdOut(), .to = Proc::devNull()})) _exit(REDIRECT_STDOUT);
            if (!Proc::redirect({.from = Proc::stdErr(), .to = Proc::devNull()})) _exit(REDIRECT_STDERR);

            /* Could use either vfork or posix_spawnp instead of fork here.
             *
             * vfork should be safe, *BUT* if somehow call to `execvp` (or I guess call to `_exit`...)
             * hangs, this will hang the whole WM 🫤
             *
             * Why not posix_spawnp? Because I don't know how it interacts with signal handlers 🤷
             * (or how to use it in general...)
             */
            switch (fork()) {
                case 0:
                    execvp(argv[0], argv);
                    _exit(bad_execvp);  // No real way to catch this one
                    break;
                default: _exit(OK);
                case -1: _exit(FORK);
            }
        } break;
        default: {
            int status = 0;
            if (waitpid(child, &status, 0) < 0) {
                lg::error("waitpid failed: {}", strError(errno));
                return;
            }
            if (!WIFEXITED(status)) {
                lg::error("child process closed by signal {}", WTERMSIG(status));
                return;
            }
            switch (auto const code = ChildExit(WEXITSTATUS(status))) {
                case OK: break;
                case SETSID: lg::error("Child process setsid error"); break;
                case FORK: lg::error("Child process failed to double-fork"); break;
                case REDIRECT_STDOUT: lg::warn("Could not redirect child stdout to /dev/null"); break;
                case REDIRECT_STDERR: lg::warn("Could not redirect child stderr to /dev/null"); break;
                default: lg::error("Child process exited with unexpected code {}", std::to_underlying(code));
            }
        } break;
        case -1: lg::error("fork failed: {}", strError(errno)); break;
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
