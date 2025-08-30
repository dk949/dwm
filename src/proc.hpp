#ifndef DWM_PROC_HPP
#define DWM_PROC_HPP



#include "file.hpp"
#include "type_utils.hpp"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include <string>
#include <vector>

struct Proc {
    friend class EventLoop;
private:
    // pid_t m_pid;
    static FDPtr dev_null;
    static FDPtr sfd;
public:

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

    static void setupSignals();
};


#endif  // DWM_PROC_HPP
