#ifndef DWM_X_UTILS_HPP
#define DWM_X_UTILS_HPP

#include <X11/Xlib.h>

#include <string_view>

enum {
    PROP_GET_TYPE_ERROR = LastExtensionError + 1,
    PROP_GET_FORMAT_ERROR,
    PROP_GET_NO_ITEM_ERROR,
    PROP_GET_ITEM_ERROR,
    PROP_GET_DOES_NOT_EXIST_ERROR,
};

std::string_view xstrerror(Display *dpy, int code);
char const *atomTypeName(Atom atom);


#endif  // DWM_X_UTILS_HPP
