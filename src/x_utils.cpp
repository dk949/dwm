#include "x_utils.hpp"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <array>
#include <cstring>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
char const *xstrerror(Display *dpy, int code) {
    static constexpr auto bufsz = 2048;
    static char buf[bufsz];
    if (code < LastExtensionError)
        XGetErrorText(dpy, code, buf, bufsz);
    else
        switch (code) {
            case PropGetTypeError: return std::strncpy(buf, "Retrieved incorrect type when querying property", bufsz);
            case PropGetFormatError:
                return std::strncpy(buf, "Retrieved incorrect format when querying property", bufsz);
            case PropGetNoItemError: return std::strncpy(buf, "Retrieved no items when querying property", bufsz);
            case PropGetItemError:
                return std::strncpy(buf, "Retrieved incorrect number of items when querying property", bufsz);
            case PropGetDoesNotExistError:
                return std::strncpy(buf, "Property does not exist on the specified client", bufsz);
            default: return std::strncpy(buf, "Unknown error", bufsz);
        }
    return buf;
}

static constexpr auto atom_names = [] {
    // Leaving this uninitialised makes sure it's a compile error not to fill the array.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    std::array<char const *, XA_LAST_PREDEFINED + 1> out;
    out[None] = "None";
    out[XA_PRIMARY] = "XA_PRIMARY";
    out[XA_SECONDARY] = "XA_SECONDARY";
    out[XA_ARC] = "XA_ARC";
    out[XA_ATOM] = "XA_ATOM";
    out[XA_BITMAP] = "XA_BITMAP";
    out[XA_CARDINAL] = "XA_CARDINAL";
    out[XA_COLORMAP] = "XA_COLORMAP";
    out[XA_CURSOR] = "XA_CURSOR";
    out[XA_CUT_BUFFER0] = "XA_CUT_BUFFER0";
    out[XA_CUT_BUFFER1] = "XA_CUT_BUFFER1";
    out[XA_CUT_BUFFER2] = "XA_CUT_BUFFER2";
    out[XA_CUT_BUFFER3] = "XA_CUT_BUFFER3";
    out[XA_CUT_BUFFER4] = "XA_CUT_BUFFER4";
    out[XA_CUT_BUFFER5] = "XA_CUT_BUFFER5";
    out[XA_CUT_BUFFER6] = "XA_CUT_BUFFER6";
    out[XA_CUT_BUFFER7] = "XA_CUT_BUFFER7";
    out[XA_DRAWABLE] = "XA_DRAWABLE";
    out[XA_FONT] = "XA_FONT";
    out[XA_INTEGER] = "XA_INTEGER";
    out[XA_PIXMAP] = "XA_PIXMAP";
    out[XA_POINT] = "XA_POINT";
    out[XA_RECTANGLE] = "XA_RECTANGLE";
    out[XA_RESOURCE_MANAGER] = "XA_RESOURCE_MANAGER";
    out[XA_RGB_COLOR_MAP] = "XA_RGB_COLOR_MAP";
    out[XA_RGB_BEST_MAP] = "XA_RGB_BEST_MAP";
    out[XA_RGB_BLUE_MAP] = "XA_RGB_BLUE_MAP";
    out[XA_RGB_DEFAULT_MAP] = "XA_RGB_DEFAULT_MAP";
    out[XA_RGB_GRAY_MAP] = "XA_RGB_GRAY_MAP";
    out[XA_RGB_GREEN_MAP] = "XA_RGB_GREEN_MAP";
    out[XA_RGB_RED_MAP] = "XA_RGB_RED_MAP";
    out[XA_STRING] = "XA_STRING";
    out[XA_VISUALID] = "XA_VISUALID";
    out[XA_WINDOW] = "XA_WINDOW";
    out[XA_WM_COMMAND] = "XA_WM_COMMAND";
    out[XA_WM_HINTS] = "XA_WM_HINTS";
    out[XA_WM_CLIENT_MACHINE] = "XA_WM_CLIENT_MACHINE";
    out[XA_WM_ICON_NAME] = "XA_WM_ICON_NAME";
    out[XA_WM_ICON_SIZE] = "XA_WM_ICON_SIZE";
    out[XA_WM_NAME] = "XA_WM_NAME";
    out[XA_WM_NORMAL_HINTS] = "XA_WM_NORMAL_HINTS";
    out[XA_WM_SIZE_HINTS] = "XA_WM_SIZE_HINTS";
    out[XA_WM_ZOOM_HINTS] = "XA_WM_ZOOM_HINTS";
    out[XA_MIN_SPACE] = "XA_MIN_SPACE";
    out[XA_NORM_SPACE] = "XA_NORM_SPACE";
    out[XA_MAX_SPACE] = "XA_MAX_SPACE";
    out[XA_END_SPACE] = "XA_END_SPACE";
    out[XA_SUPERSCRIPT_X] = "XA_SUPERSCRIPT_X";
    out[XA_SUPERSCRIPT_Y] = "XA_SUPERSCRIPT_Y";
    out[XA_SUBSCRIPT_X] = "XA_SUBSCRIPT_X";
    out[XA_SUBSCRIPT_Y] = "XA_SUBSCRIPT_Y";
    out[XA_UNDERLINE_POSITION] = "XA_UNDERLINE_POSITION";
    out[XA_UNDERLINE_THICKNESS] = "XA_UNDERLINE_THICKNESS";
    out[XA_STRIKEOUT_ASCENT] = "XA_STRIKEOUT_ASCENT";
    out[XA_STRIKEOUT_DESCENT] = "XA_STRIKEOUT_DESCENT";
    out[XA_ITALIC_ANGLE] = "XA_ITALIC_ANGLE";
    out[XA_X_HEIGHT] = "XA_X_HEIGHT";
    out[XA_QUAD_WIDTH] = "XA_QUAD_WIDTH";
    out[XA_WEIGHT] = "XA_WEIGHT";
    out[XA_POINT_SIZE] = "XA_POINT_SIZE";
    out[XA_RESOLUTION] = "XA_RESOLUTION";
    out[XA_COPYRIGHT] = "XA_COPYRIGHT";
    out[XA_NOTICE] = "XA_NOTICE";
    out[XA_FONT_NAME] = "XA_FONT_NAME";
    out[XA_FAMILY_NAME] = "XA_FAMILY_NAME";
    out[XA_FULL_NAME] = "XA_FULL_NAME";
    out[XA_CAP_HEIGHT] = "XA_CAP_HEIGHT";
    out[XA_WM_CLASS] = "XA_WM_CLASS";
    out[XA_WM_TRANSIENT_FOR] = "XA_WM_TRANSIENT_FOR";

    return out;
}();

char const *atomTypeName(Atom a) {
    if (a < atom_names.size()) return atom_names.at(a);
    return "UNKNOWN_ATOM";
}

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
