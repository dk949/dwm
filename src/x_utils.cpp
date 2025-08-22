#include <X11/Xlib.h>
#include <x_utils.hpp>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
char const *xstrerror(Display *dpy, int code) {
    static constexpr auto bufsz = 2048;
    static char buf[bufsz];
    XGetErrorText(dpy, code, buf, bufsz);
    return buf;
}

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
