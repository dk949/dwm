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
    friend struct EventLoop;
private:
    pid_t m_pid = -1;
    int m_stdin = -1;
    int m_stdout = -1;
    int m_stderr = -1;
    static FDPtr dev_null;
    static FDPtr sfd;

    Proc() = default;
    Proc(pid_t pid, int inpipe, int outpipe, int errpipe);
public:
    static constexpr int pipe = INT_MIN;
    enum struct ReachedEOF : bool { Yes = true, No = false };

    struct PipeFds {
        int read = -1;
        int write = -1;
    };

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

    static bool addFDFlag(int fd, unsigned flag);

    static void setupSignals();
    static void setupDebugging();

    static std::optional<std::string_view> writeFD(std::string &&sv, int fd) = delete;
    [[nodiscard]]
    static std::optional<std::string_view> writeFD(std::string_view sv, int fd);
    [[nodiscard]]
    static std::optional<std::string_view> writeFD(std::string const &sv, int fd);
    [[nodiscard]]
    static std::optional<std::string_view> writeFD(char const *sv, int fd);

    [[nodiscard]]
    static std::optional<std::pair<std::string, Proc::ReachedEOF>> readFD(int fd);

    void closeStdin() noexcept;
    void closeStdout() noexcept;
    void closeStderr() noexcept;
    void closeAll() noexcept;
    static bool isPipe(int fd);

private:
    struct SpawnConfig {
        std::optional<int> in = std::nullopt;
        std::optional<int> out = std::nullopt;
        std::optional<int> err = std::nullopt;
        bool detach = false;
    };

    [[nodiscard]]
    static std::optional<Proc> spawn(Display *dpy, char *const *argv, SpawnConfig);
    // SHOULD ONLY EVER BE CALLED FROM CHILD PROCESS!!!
    static void tryRedirect(Redirection r, int bad_exit);
    // SHOULD ONLY EVER BE CALLED FROM CHILD PROCESS!!!
    static void trySetsid(int bad_exit);
    static void closePipe(int pipe);

    [[nodiscard]]
    static std::array<PipeFds, 3> arrangePipes(SpawnConfig const &conf);
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
    static constexpr auto parse(std::format_parse_context &ctx) {
        return ctx.begin();
    }

    static auto format(Proc::Redirection const &r, std::format_context &ctx) {
        std::format_context::iterator out = ctx.out();
        out = formatFD(r.from, out);
        std::format_to(out, " -> ");
        out = formatFD(r.to, out);
        return out;
    }
};

#endif  // DWM_PROC_HPP
