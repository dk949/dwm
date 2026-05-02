#ifndef DWM_X_UTILS_HPP
#define DWM_X_UTILS_HPP

#include <X11/Xlib.h>

#include <string_view>

enum {
    PropGetTypeError = LastExtensionError + 1,
    PropGetFormatError,
    PropGetNoItemError,
    PropGetItemError,
    PropGetDoesNotExistError,
};

std::string_view xstrerror(Display *dpy, int code);
char const *atomTypeName(Atom atom);


#endif  // DWM_X_UTILS_HPP
