#ifndef DWM_X_UTILS_HPP
#define DWM_X_UTILS_HPP

#include <X11/Xlib.h>

enum {
    PropGetTypeError = LastExtensionError + 1,
    PropGetFormatError,
    PropGetNoItemError,
    PropGetItemError,
    PropGetDoesNotExistError,
};

char const *xstrerror(Display *dpy, int code);
char const *atomTypeName(Atom a);


#endif  // DWM_X_UTILS_HPP
