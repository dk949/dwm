#ifndef DWM_COLORS_HPP
#define DWM_COLORS_HPP


#include <X11/Xft/Xft.h>

#define DRW_COLOR_SCHEME_FIELDS_DO(X) X
#define DRW_COLOR_SCHEME_FIELDS_FOREACH()     \
    DRW_COLOR_SCHEME_FIELDS_DO(norm)          \
    DRW_COLOR_SCHEME_FIELDS_DO(sel)           \
    DRW_COLOR_SCHEME_FIELDS_DO(status)        \
    DRW_COLOR_SCHEME_FIELDS_DO(tags_sel)      \
    DRW_COLOR_SCHEME_FIELDS_DO(tags_norm)     \
    DRW_COLOR_SCHEME_FIELDS_DO(info_sel)      \
    DRW_COLOR_SCHEME_FIELDS_DO(info_norm)     \
    DRW_COLOR_SCHEME_FIELDS_DO(info_progress) \
    DRW_COLOR_SCHEME_FIELDS_DO(off_progress)  \
    DRW_COLOR_SCHEME_FIELDS_DO(bright_progress)
#define DRW_COLOR_FIELDS_DO(X) X
#define DRW_COLOR_FIELDS_FOREACH() \
    DRW_COLOR_FIELDS_DO(fg)        \
    DRW_COLOR_FIELDS_DO(bg)        \
    DRW_COLOR_FIELDS_DO(border)

struct Color {
#undef DRW_COLOR_FIELDS_DO
#define DRW_COLOR_FIELDS_DO(X) XftColor X;
    DRW_COLOR_FIELDS_FOREACH()
private:
    struct Inverted {
#undef DRW_COLOR_FIELDS_DO
#define DRW_COLOR_FIELDS_DO(X) XftColor const &X;
        DRW_COLOR_FIELDS_FOREACH()
    };
public:
    Inverted invert(bool do_invert = true) const {
        if (do_invert)
            return {
                .fg = bg,
                .bg = fg,
                .border = border,
            };
        else
            return {
                .fg = fg,
                .bg = bg,
                .border = border,
            };
    }
};

struct ColorName {
#undef DRW_COLOR_FIELDS_DO
#define DRW_COLOR_FIELDS_DO(X) char const *const X;
    DRW_COLOR_FIELDS_FOREACH()
};

struct ColorScheme {
#undef DRW_COLOR_SCHEME_FIELDS_DO
#define DRW_COLOR_SCHEME_FIELDS_DO(f) Color f;
    DRW_COLOR_SCHEME_FIELDS_FOREACH()
};

struct ColorSchemeName {
#undef DRW_COLOR_SCHEME_FIELDS_DO
#define DRW_COLOR_SCHEME_FIELDS_DO(f) ColorName f;
    DRW_COLOR_SCHEME_FIELDS_FOREACH()
};

#endif  // DWM_COLORS_HPP
