/* See LICENSE file for copyright and license details. */
#include "drw.hpp"

#include "colors.hpp"
#include "log.hpp"
#include "util.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <span>

#define UTF_INVALID 0xFFFD
#define UTF_SIZ     4uz

static unsigned char const utfbyte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static unsigned char const utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static long const utfmin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
static long const utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static long utf8decodebyte(char const c, size_t *i) {
    for (*i = 0; *i < (UTF_SIZ + 1); ++(*i)) {
        if (((unsigned char)c & utfmask[*i]) == utfbyte[*i]) {
            return (unsigned char)c & ~utfmask[*i];
        }
    }
    return 0;
}

static size_t utf8validate(long *u, size_t i) {
    if (!between(*u, utfmin[i], utfmax[i]) || between(*u, 0xD800, 0xDFFF)) {
        *u = UTF_INVALID;
    }
    for (i = 1; *u > utfmax[i]; ++i) {
        ;
    }
    return i;
}

static size_t utf8decode(char const *c, long *u, size_t clen) {
    size_t i;
    size_t j;
    size_t len;
    size_t type;
    long udecoded;

    *u = UTF_INVALID;
    if (!clen) {
        return 0;
    }
    udecoded = utf8decodebyte(c[0], &len);
    if (!between(len, 1uz, UTF_SIZ)) {
        return 1;
    }
    for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
        udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
        if (type) {
            return j;
        }
    }
    if (j < len) {
        return 0;
    }
    *u = udecoded;
    utf8validate(u, len);

    return len;
}

Drw::Drw(Display *dpy, int screen, Window root, unsigned int width, unsigned int height)
        : m_screen_width(width)
        , m_screen_height(height)
        , m_dpy(dpy)
        , m_screen(screen)
        , m_root(root)
        , m_drawable(XCreatePixmap(dpy, root, width, height, (unsigned)DefaultDepth(dpy, screen)))
        , m_gc(XCreateGC(dpy, root, 0, nullptr))
        , m_cursors(dpy) {
    XSetLineAttributes(m_dpy, m_gc, 1, LineSolid, CapButt, JoinMiter);
}

Drw::~Drw() {
    XFreePixmap(m_dpy, m_drawable);
    XFreeGC(m_dpy, m_gc);
    drw_fontset_free(m_fonts);
}

void Drw::resize(unsigned int w, unsigned int h) {
    m_screen_width = w;
    m_screen_height = h;
    if (m_drawable) XFreePixmap(m_dpy, m_drawable);
    m_drawable = XCreatePixmap(m_dpy, m_root, w, h, (unsigned)DefaultDepth(m_dpy, m_screen));
}

/* This function is an implementation detail. Library users should use
 * drw_fontset_create instead.
 */
std::optional<Fnt> Drw::xfont_create(char const *fontname) {
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
    if (auto pattern = FcNameParse((FcChar8 *)fontname)) {
        font.pattern = pattern;
    } else {
        lg::warn("cannot parse font name to pattern: '{}'", fontname);
        XftFontClose(m_dpy, font.xfont);
        return std::nullopt;
    }
    font.h = (unsigned)(font.xfont->ascent + font.xfont->descent);
    font.dpy = m_dpy;

    return font;
}

std::optional<Fnt> Drw::xfont_create(FcPattern *fontpattern) {
    // TODO(dk949): consider making this the constructor for Fnt
    Fnt font;

    // NOTE: this *does not* set the pattern field, AFACT for no better reason than draw_text using this to
    //       determine if a font was loaded from a pattern.

    if (auto xfont = XftFontOpenPattern(m_dpy, fontpattern)) {
        font.xfont = xfont;
    } else {
        lg::warn("error, cannot load font from pattern.");
        return std::nullopt;
    }

    font.h = (unsigned)(font.xfont->ascent + font.xfont->descent);
    font.dpy = m_dpy;

    return font;
}

static void xfont_free(Fnt const &font) {
    // TODO(dk949): consider making this the destructor for Fnt
    if (font.pattern) FcPatternDestroy(font.pattern);

    XftFontClose(font.dpy, font.xfont);
}

bool Drw::fontset_create(char const *_fonts[], size_t fontcount) {
    std::span<char const *> fonts {_fonts, fontcount};

    bool success = false;
    for (auto font_name : fonts)
        if (auto xfont = xfont_create(font_name)) {
            m_fonts.push_back(*xfont);
            success = true;
        }
    return success;
}

void drw_fontset_free(std::vector<Fnt> &fonts) {
    for (auto const &font : fonts)
        xfont_free(font);
}

Clr Drw::clr_create(char const *clrname) const {
    Clr out;

    if (!XftColorAllocName(m_dpy, DefaultVisual(m_dpy, m_screen), DefaultColormap(m_dpy, m_screen), clrname, &out))
        lg::fatal("error, cannot allocate color '{}'", clrname);

    return out;
}

Color Drw::nameToColor(ColorName const &name) const {
    Color out;
#undef DRW_COLOR_FIELDS_DO
#define DRW_COLOR_FIELDS_DO(f) out.f = clr_create(name.f);
    DRW_COLOR_FIELDS_FOREACH()
    return out;
};

/* Wrapper to create color schemes. The caller has to call free(3) on the
 * returned color scheme when done using it. */
void Drw::setColorScheme(ColorSchemeName clrnames) {
#undef DRW_COLOR_SCHEME_FIELDS_DO
#define DRW_COLOR_SCHEME_FIELDS_DO(f) m_scheme.f = nameToColor(clrnames.f);
    DRW_COLOR_SCHEME_FIELDS_FOREACH()
}

// TODO(dk949): make the bools strongly typed
void Drw::draw_rect(int x, int y, unsigned int w, unsigned int h, bool filled, bool invert) {
    if (!m_current_color) return;

    XSetForeground(m_dpy, m_gc, invert ? currentColor().bg.pixel : currentColor().fg.pixel);
    if (filled)
        XFillRectangle(m_dpy, m_drawable, m_gc, x, y, w, h);
    else
        XDrawRectangle(m_dpy, m_drawable, m_gc, x, y, w - 1, h - 1);
}

// TODO(dk949): make the bools strongly typed
int Drw::draw_text(int x, int y, unsigned int w, unsigned int h, unsigned int lpad, char const *text, bool invert) {
    int ellipsis_x = 0;
    unsigned int tmpw = 0;
    unsigned int ellipsis_w = 0;
    XftDraw *d = nullptr;
    int render = x || y || w || h;
    long utf8codepoint = 0;
    XftResult result;
    int charexists = 0, overflow = 0;

    // TODO(dk949): use an actual UTF-8 library

    /* keep track of a couple codepoints for which we have no match. */
    static constexpr auto nomatches_len = 64;

    static struct {
        long codepoint[nomatches_len];
        unsigned int idx;
    } nomatches;

    static unsigned int ellipsis_width = 0;

    if ((render && (!m_current_color || !w)) || !text) return 0;

    if (!render) {
        w = invert ? 1u : ~0u;
    } else {
        XSetForeground(m_dpy, m_gc, currentColor().invert(invert).bg.pixel);
        XFillRectangle(m_dpy, m_drawable, m_gc, x, y, w, h);
        d = XftDrawCreate(m_dpy, m_drawable, DefaultVisual(m_dpy, m_screen), DefaultColormap(m_dpy, m_screen));
        x += (int)lpad;
        w -= lpad;
    }

    Fnt usedfont = m_fonts.front();
    if (!ellipsis_width && render) ellipsis_width = fontset_getwidth("...");
    while (true) {
        unsigned ew = 0;
        std::size_t ellipsis_len = 0;
        std::size_t utf8strlen = 0;
        char const *utf8str = text;
        std::optional<Fnt> nextfont = std::nullopt;
        while (*text) {
            auto utf8charlen = utf8decode(text, &utf8codepoint, UTF_SIZ);
            for (auto &curfont : m_fonts) {
                charexists = charexists || XftCharExists(m_dpy, curfont.xfont, (FcChar32)utf8codepoint);
                if (charexists) {
                    drw_font_getexts(&curfont, text, utf8charlen, &tmpw, nullptr);
                    if (ew + ellipsis_width <= w) {
                        /* keep track where the ellipsis still fits */
                        ellipsis_x = (int)((unsigned)x + ew);
                        ellipsis_w = w - ew;
                        ellipsis_len = utf8strlen;
                    }

                    if (ew + tmpw > w) {
                        overflow = 1;
                        /* called from drw_fontset_getwidth_clamp():
                         * it wants the width AFTER the overflow
                         */
                        if (!render)
                            x += (int)tmpw;
                        else
                            utf8strlen = ellipsis_len;
                    } else if (curfont == usedfont) {
                        utf8strlen += utf8charlen;
                        text += utf8charlen;
                        ew += tmpw;
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
                auto ty = (unsigned)y + (h - usedfont.h) / 2 + (unsigned)usedfont.xfont->ascent;
                XftDrawStringUtf8(d,
                    &currentColor().invert(invert).fg,
                    usedfont.xfont,
                    x,
                    (int)ty,
                    (XftChar8 *)utf8str,
                    (int)utf8strlen);
            }
            x += (int)ew;
            w -= ew;
        }
        if (render && overflow) draw_text(ellipsis_x, y, ellipsis_w, h, 0, "...", invert);

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
            for (int i = 0; i < nomatches_len; ++i) {
                /* avoid calling XftFontMatch if we know we won't find a match */
                if (utf8codepoint == nomatches.codepoint[i]) {
                    no_match = true;
                    usedfont = m_fonts.front();
                    break;
                }
            }


            if (!no_match) {

                FcCharSet *fccharset = FcCharSetCreate();
                FcCharSetAddChar(fccharset, (FcChar32)utf8codepoint);

                if (!m_fonts.front().pattern) {
                    /* Refer to the comment in xfont_create for more information. */
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
                    auto new_font = xfont_create(match);
                    if (new_font && XftCharExists(m_dpy, new_font->xfont, (FcChar32)utf8codepoint)) {
                        usedfont = *new_font;
                        m_fonts.push_back(usedfont);
                    } else {
                        if (new_font) xfont_free(*new_font);
                        nomatches.codepoint[++nomatches.idx % nomatches_len] = utf8codepoint;
                        usedfont = m_fonts.front();
                    }
                }
            }
        }
    }
    if (d) {
        XftDrawDestroy(d);
    }

    return (int)((unsigned)x + (render ? w : 0));
}

void Drw::map(Window win, int x, int y, unsigned int w, unsigned int h) {
    XCopyArea(m_dpy, m_drawable, win, m_gc, x, y, w, h, x, y);
    XSync(m_dpy, False);
}

unsigned int Drw::fontset_getwidth(char const *text) {
    if (!text) return 0;

    return (unsigned)draw_text(0, 0, 0, 0, 0, text, 0);
}

void drw_font_getexts(Fnt *font, char const *text, std::size_t len, unsigned int *w, unsigned int *h) {
    XGlyphInfo ext;

    if (!font || !text) {
        return;
    }

    XftTextExtentsUtf8(font->dpy, font->xfont, (XftChar8 *)text, (int)len, &ext);
    if (w) {
        *w = (unsigned)ext.xOff;
    }
    if (h) {
        *h = font->h;
    }
}
