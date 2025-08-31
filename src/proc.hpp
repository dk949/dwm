#ifndef DWM_PROC_HPP
#define DWM_PROC_HPP



#include "file.hpp"
#include "type_utils.hpp"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include <format>
#include <optional>
#include <string>
#include <vector>

struct Proc {
    friend class EventLoop;
private:
    pid_t m_pid;
    FDPtr m_stdin;
    FDPtr m_stdout;
    FDPtr m_stderr;
    static FDPtr dev_null;
    static FDPtr sfd;

    Proc(pid_t pid, int inpipe, int outpipe, int errpipe);
public:
    enum struct ReachedEOF : bool { Yes = true, No = false };

    struct Redirection {
        int from;
        int to;
    };

    static int devNull();
    static int stdIn();
    static int stdOut();
    static int stdErr();

    template<StringLike Str, StringLike... Strs>
    static pid_t spawnDetached(Display *dpy, Str &&prog, Strs &&...strs) {
        std::vector<std::string> args {};
        args.reserve(sizeof...(strs) + 1);
        args.emplace_back(std::forward<Str>(prog));
        (args.emplace_back(std::forward<Strs>(strs)), ...);
        return spawnDetached(dpy, std::move(args));
    }

    static pid_t spawnDetached(Display *dpy, std::vector<std::string> args);

    static pid_t spawnDetached(Display *dpy, char *const *argv);
    static std::size_t cleanUpZombies();

    // Can be used to redirect any file descriptor to any other file descriptor
    static bool redirect(Redirection r);

    static bool addFDFlag(int fd, int flag);

    static void setupSignals();

    static std::optional<std::string_view> writeFD(std::string &&sv, int fd) = delete;
    [[nodiscard]]
    static std::optional<std::string_view> writeFD(std::string_view sv, int fd);
    [[nodiscard]]
    static std::optional<std::string_view> writeFD(std::string const &sv, int fd);
    [[nodiscard]]
    static std::optional<std::string_view> writeFD(char const *sv, int fd);

    [[nodiscard]]
    static std::optional<std::pair<std::string, Proc::ReachedEOF>> readFD(int fd);

private:
    struct SpawnConfig {
        std::optional<int> in = std::nullopt;
        std::optional<int> out = std::nullopt;
        std::optional<int> err = std::nullopt;
        bool detach = false;
    };

    static pid_t spawnImpl(Display *dpy, char *const *argv, SpawnConfig);
    static std::optional<Proc> spawn(Display *dpy, char *const *argv);
    // SHOULD ONLY EVER BE CALLED FROM CHILD PROCESS!!!
    static void tryRedirect(Redirection r, int bad_exit);
    // SHOULD ONLY EVER BE CALLED FROM CHILD PROCESS!!!
    static void trySetsid(int bad_exit);
};

template<>
struct std::formatter<Proc::Redirection> {
private:
    static std::format_context::iterator formatFD(int fd, std::format_context::iterator it) {
        switch (fd) {
            case STDIN_FILENO: return std::format_to(it, "stdin");
            case STDOUT_FILENO: return std::format_to(it, "stdout");
            case STDERR_FILENO: return std::format_to(it, "stderr");
            default:
                if (fd == Proc::devNull())
                    return std::format_to(it, "/dev/null");
                else
                    return std::format_to(it, "{}", fd);
        }
    }
public:
    constexpr auto parse(std::format_parse_context &ctx) {
        return ctx.begin();
    }

    auto format(Proc::Redirection const &r, std::format_context &ctx) const {
        std::format_context::iterator out = ctx.out();
        out = formatFD(r.from, out);
        std::format_to(out, " -> ");
        out = formatFD(r.to, out);
        return out;
    }
};

#endif  // DWM_PROC_HPP
