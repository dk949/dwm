/* See LICENSE file for copyright and license details. */
#include "drw.hpp"

#include "colors.hpp"
#include "log.hpp"
#include "util.hpp"

#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>

static constexpr auto UTF_INVALID = 0xFFFD;
static constexpr auto UTF_SIZ = 4uz;

namespace {
enum UtfInvalidRange { begin = 0xD800, end = 0xDFFF };
}  // namespace

static constexpr std::array<unsigned char, UTF_SIZ + 1> utfbyte {0x80, 0, 0xC0, 0xE0, 0xF0};
static constexpr std::array<unsigned char, UTF_SIZ + 1> utfmask {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static constexpr std::array<long, UTF_SIZ + 1> utfmin {0, 0, 0x80, 0x800, 0x10000};
static constexpr std::array<long, UTF_SIZ + 1> utfmax {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static long utf8DecodeByte(char byte, size_t *idx) {
    for (*idx = 0; *idx < (UTF_SIZ + 1); ++(*idx))
        if ((static_cast<unsigned char>(byte) & utfmask[*idx]) == utfbyte[*idx])
            return static_cast<unsigned char>(byte) & static_cast<unsigned char>(~utfmask[*idx]);

    return 0;
}

static size_t utf8Validate(long *codepoint, size_t idx) {
    if (!between(*codepoint, utfmin[idx], utfmax[idx])
        || between(*codepoint, UtfInvalidRange::begin, UtfInvalidRange::end)) {
        *codepoint = UTF_INVALID;
    }
    for (idx = 1; *codepoint > utfmax[idx]; ++idx) {
        ;
    }
    return idx;
}

static size_t utf8Decode(char const *str, long *codepoint, size_t clen) {
    size_t len = 0;
    size_t type = 0;

    *codepoint = UTF_INVALID;
    if (!clen) {
        return 0;
    }
    long udecoded = utf8DecodeByte(str[0], &len);
    if (!between(len, 1uz, UTF_SIZ)) {
        return 1;
    }
    size_t idx = 1;
    size_t out = 1;
    for (; idx < clen && out < len; ++idx, ++out) {
        udecoded = (udecoded << 6u) | utf8DecodeByte(str[idx], &type);
        if (type) {
            return out;
        }
    }
    if (out < len) {
        return 0;
    }
    *codepoint = udecoded;
    utf8Validate(codepoint, len);

    return len;
}

Drw::Drw(Display *dpy, int screen, Window win, int width, int height)
        : m_screen_width(width)
        , m_screen_height(height)
        , m_dpy(dpy)
        , m_screen(screen)
        , m_root(win)
        , m_drawable(XCreatePixmap(dpy,
              win,
              static_cast<unsigned>(width),
              static_cast<unsigned>(height),
              static_cast<unsigned>(DefaultDepth(dpy, screen))))
        , m_gc(XCreateGC(dpy, win, 0, nullptr))
        , m_cursors(dpy) {
    XSetLineAttributes(m_dpy, m_gc, 1, LineSolid, CapButt, JoinMiter);
}

Drw::~Drw() {
    XFreePixmap(m_dpy, m_drawable);
    XFreeGC(m_dpy, m_gc);
    drwFontsetFree(m_fonts);
}

void Drw::resize(int w, int h) {
    m_screen_width = w;
    m_screen_height = h;
    if (m_drawable) XFreePixmap(m_dpy, m_drawable);
    m_drawable = XCreatePixmap(m_dpy,
        m_root,
        static_cast<unsigned>(w),
        static_cast<unsigned>(h),
        static_cast<unsigned>(DefaultDepth(m_dpy, m_screen)));
}

/* This function is an implementation detail. Library users should use
 * drw_fontset_create instead.
 */
std::optional<Fnt> Drw::xfontCreate(char const *fontname) {
    // TODO(dk949): consider making this the constructor for Fnt
    Fnt font;

    /* Using the pattern found at font.xfont->pattern does not yield the
     * same substitution results as using the pattern returned by
     * FcNameParse; using the latter results in the desired fallback
     * behaviour whereas the former just results in missing-character
     * rectangles being drawn, at least with some fonts. */
    if (auto xfont = XftFontOpenName(m_dpy, m_screen, fontname)) {
        font.xfont = xfont;
    } else {
        lg::warn("cannot load font from name: '{}'", fontname);
        return std::nullopt;
    }
    if (auto *pattern = FcNameParse(reinterpret_cast<FcChar8 const *>(fontname))) {
        font.pattern = pattern;
    } else {
        lg::warn("cannot parse font name to pattern: '{}'", fontname);
        XftFontClose(m_dpy, font.xfont);
        return std::nullopt;
    }
    font.h = font.xfont->ascent + font.xfont->descent;
    font.dpy = m_dpy;

    return font;
}

std::optional<Fnt> Drw::xfontCreate(FcPattern *fontpattern) {
    // TODO(dk949): consider making this the constructor for Fnt
    Fnt font;

    // NOTE: this *does not* set the pattern field, AFACT for no better reason than drawText using this to
    //       determine if a font was loaded from a pattern.

    if (auto xfont = XftFontOpenPattern(m_dpy, fontpattern)) {
        font.xfont = xfont;
    } else {
        lg::warn("error, cannot load font from pattern.");
        return std::nullopt;
    }

    font.h = font.xfont->ascent + font.xfont->descent;
    font.dpy = m_dpy;

    return font;
}

static void xfontFree(Fnt const &font) {
    // TODO(dk949): consider making this the destructor for Fnt
    if (font.pattern) FcPatternDestroy(font.pattern);

    XftFontClose(font.dpy, font.xfont);
}

bool Drw::fontsetCreate(std::span<char const *const> fonts) {

    bool success = false;
    for (auto const *font_name : fonts)
        if (auto xfont = xfontCreate(font_name)) {
            m_fonts.push_back(*xfont);
            success = true;
        }
    return success;
}

void drwFontsetFree(std::vector<Fnt> &fonts) {
    for (auto const &font : fonts)
        xfontFree(font);
}

Clr Drw::clrCreate(char const *clrname) const {
    Clr out;

    if (!XftColorAllocName(m_dpy, DefaultVisual(m_dpy, m_screen), DefaultColormap(m_dpy, m_screen), clrname, &out))
        lg::fatal("error, cannot allocate color '{}'", clrname);

    return out;
}

Color Drw::nameToColor(ColorName const &name) const {
    Color out;
#undef DRW_COLOR_FIELDS_DO
#define DRW_COLOR_FIELDS_DO(f) out.f = clrCreate(name.f);
    DRW_COLOR_FIELDS_FOREACH()
    return out;
};

void Drw::setColorScheme(ColorSchemeName clrnames) {
#undef DRW_COLOR_SCHEME_FIELDS_DO
#define DRW_COLOR_SCHEME_FIELDS_DO(f) m_scheme.f = nameToColor(clrnames.f);
    DRW_COLOR_SCHEME_FIELDS_FOREACH()
}

// TODO(dk949): make the bools strongly typed
void Drw::drawRect(int x, int y, int w, int h, bool filled, bool invert) {
    if (!m_current_color) return;

    XSetForeground(m_dpy, m_gc, invert ? currentColor().bg.pixel : currentColor().fg.pixel);
    if (filled)
        XFillRectangle(m_dpy, m_drawable, m_gc, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h));
    else
        XDrawRectangle(m_dpy, m_drawable, m_gc, x, y, static_cast<unsigned>(w - 1), static_cast<unsigned>(h - 1));
}

// TODO(dk949): make the bools strongly typed
int Drw::drawText(int x, int y, int w, int h, int left_pad, char const *text, bool invert) {
    int ellipsis_x = 0;
    int tmpw = 0;
    int ellipsis_w = 0;
    XftDraw *draw = nullptr;
    int render = x || y || w || h;
    long utf8codepoint = 0;
    XftResult result;
    int charexists = 0, overflow = 0;

    // TODO(dk949): use an actual UTF-8 library

    /* keep track of a couple codepoints for which we have no match. */

    static constexpr auto nomatches_len = 64;

    static struct {
        std::array<long, nomatches_len> codepoint;
        unsigned int idx;
    } nomatches;

    static int ellipsis_width = 0;

    if ((render && (!m_current_color || !w)) || !text) return 0;

    if (!render) {
        w = invert ? 1u : std::numeric_limits<int>::max();
    } else {
        XSetForeground(m_dpy, m_gc, currentColor().invert(invert).bg.pixel);
        XFillRectangle(m_dpy, m_drawable, m_gc, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h));
        draw = XftDrawCreate(m_dpy, m_drawable, DefaultVisual(m_dpy, m_screen), DefaultColormap(m_dpy, m_screen));
        x += left_pad;
        w -= left_pad;
    }

    Fnt usedfont = m_fonts.front();
    if (!ellipsis_width && render) ellipsis_width = fontsetGetwidth("...");
    while (true) {
        int extent_w = 0;
        std::size_t ellipsis_len = 0;
        std::size_t utf8strlen = 0;
        char const *utf8str = text;
        std::optional<Fnt> nextfont = std::nullopt;
        while (*text) {
            auto utf8charlen = utf8Decode(text, &utf8codepoint, UTF_SIZ);
            for (auto &curfont : m_fonts) {
                charexists = charexists || XftCharExists(m_dpy, curfont.xfont, static_cast<FcChar32>(utf8codepoint));
                if (charexists) {
                    drwFontGetexts(&curfont, text, utf8charlen, &tmpw, nullptr);
                    if (extent_w + ellipsis_width <= w) {
                        /* keep track where the ellipsis still fits */
                        ellipsis_x = x + extent_w;
                        ellipsis_w = w - extent_w;
                        ellipsis_len = utf8strlen;
                    }

                    if (extent_w + tmpw > w) {
                        overflow = 1;
                        /* called from drw_fontset_getwidth_clamp():
                         * it wants the width AFTER the overflow
                         */
                        if (!render)
                            x += tmpw;
                        else
                            utf8strlen = ellipsis_len;
                    } else if (curfont == usedfont) {
                        utf8strlen += utf8charlen;
                        text += utf8charlen;
                        extent_w += tmpw;
                    } else {
                        nextfont = curfont;
                    }
                    break;
                }
            }

            if (overflow || !charexists || nextfont) {
                break;
            }
            charexists = 0;
        }

        if (utf8strlen) {
            if (render) {
                auto text_y = y + ((h - usedfont.h) / 2) + usedfont.xfont->ascent;
                XftDrawStringUtf8(draw,
                    &currentColor().invert(invert).fg,
                    usedfont.xfont,
                    x,
                    text_y,
                    reinterpret_cast<XftChar8 const *>(utf8str),
                    static_cast<int>(utf8strlen));
            }
            x += extent_w;
            w -= extent_w;
        }
        if (render && overflow) drawText(ellipsis_x, y, ellipsis_w, h, 0, "...", invert);

        if (!*text || overflow) {
            break;
        } else if (nextfont) {
            charexists = 0;
            usedfont = *nextfont;
        } else {
            /* Regardless of whether or not a fallback font is found, the
             * character must be drawn. */
            charexists = 1;

            bool no_match = false;
            for (long codepoint : nomatches.codepoint) {
                /* avoid calling XftFontMatch if we know we won't find a match */
                if (utf8codepoint == codepoint) {
                    no_match = true;
                    usedfont = m_fonts.front();
                    break;
                }
            }


            if (!no_match) {

                FcCharSet *fccharset = FcCharSetCreate();
                FcCharSetAddChar(fccharset, static_cast<FcChar32>(utf8codepoint));

                if (!m_fonts.front().pattern) {
                    /* Refer to the comment in xfontCreate for more information. */
                    lg::fatal("the first font in the cache must be loaded from a font string.");
                }

                FcPattern *fcpattern = FcPatternDuplicate(m_fonts.front().pattern);
                FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
                FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);

                FcConfigSubstitute(nullptr, fcpattern, FcMatchPattern);
                FcDefaultSubstitute(fcpattern);
                FcPattern *match = XftFontMatch(m_dpy, m_screen, fcpattern, &result);

                FcCharSetDestroy(fccharset);
                FcPatternDestroy(fcpattern);

                // TODO(dk949): the `match` is never deleted???
                if (match) {
                    auto new_font = xfontCreate(match);
                    if (new_font && XftCharExists(m_dpy, new_font->xfont, static_cast<FcChar32>(utf8codepoint))) {
                        usedfont = *new_font;
                        m_fonts.push_back(usedfont);
                    } else {
                        if (new_font) xfontFree(*new_font);
                        nomatches.codepoint[++nomatches.idx % nomatches_len] = utf8codepoint;
                        usedfont = m_fonts.front();
                    }
                }
            }
        }
    }
    if (draw) {
        XftDrawDestroy(draw);
    }

    return x + (render ? w : 0);
}

void Drw::map(Window win, Rect<int> dims) {
    XCopyArea(m_dpy,
        m_drawable,
        win,
        m_gc,
        dims.x,
        dims.y,
        static_cast<unsigned>(dims.w),
        static_cast<unsigned>(dims.h),
        dims.x,
        dims.y);
    XSync(m_dpy, False);
}

int Drw::fontsetGetwidth(char const *text) {
    if (!text) return 0;

    return drawText(0, 0, 0, 0, 0, text, false);
}

void drwFontGetexts(Fnt *font, char const *text, std::size_t len, int *w, int *h) {
    // TODO(dk949): Use std::stroing_view?
    XGlyphInfo ext;

    if (!font || !text) {
        return;
    }

    XftTextExtentsUtf8(font->dpy, font->xfont, reinterpret_cast<XftChar8 const *>(text), static_cast<int>(len), &ext);
    if (w) {
        *w = ext.xOff;
    }
    if (h) {
        *h = font->h;
    }
}
