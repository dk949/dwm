/* See LICENSE file for copyright and license details. */
#include "drw.hpp"

#include "colors.hpp"
#include "log.hpp"
#include "util.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
Fnt *Drw::xfont_create(char const *fontname, FcPattern *fontpattern) {
    Fnt *font;
    XftFont *xfont = nullptr;
    FcPattern *pattern = nullptr;

    if (fontname) {
        /* Using the pattern found at font->xfont->pattern does not yield the
         * same substitution results as using the pattern returned by
         * FcNameParse; using the latter results in the desired fallback
         * behaviour whereas the former just results in missing-character
         * rectangles being drawn, at least with some fonts. */
        if (!(xfont = XftFontOpenName(m_dpy, m_screen, fontname))) {
            lg::warn("cannot load font from name: '{}'", fontname);
            return nullptr;
        }
        if (!(pattern = FcNameParse((FcChar8 *)fontname))) {
            lg::warn("cannot parse font name to pattern: '{}'", fontname);
            XftFontClose(m_dpy, xfont);
            return nullptr;
        }
    } else if (fontpattern) {
        if (!(xfont = XftFontOpenPattern(m_dpy, fontpattern))) {
            lg::warn("error, cannot load font from pattern.");
            return nullptr;
        }
    } else {
        lg::fatal("no font specified.");
    }

    font = new Fnt {};
    font->xfont = xfont;
    font->pattern = pattern;
    font->h = (unsigned)(xfont->ascent + xfont->descent);
    font->dpy = m_dpy;

    return font;
}

static void xfont_free(Fnt *font) {
    if (!font) {
        return;
    }
    if (font->pattern) {
        FcPatternDestroy(font->pattern);
    }
    XftFontClose(font->dpy, font->xfont);
    delete font;
}

Fnt *Drw::fontset_create(char const *fonts[], size_t fontcount) {
    Fnt *cur;
    Fnt *ret = nullptr;
    size_t i;

    // TODO(dk949): check how this is used
    if (!fonts) return nullptr;


    for (i = 1; i <= fontcount; i++) {
        if ((cur = xfont_create(fonts[fontcount - i], nullptr))) {
            cur->next = ret;
            ret = cur;
        }
    }
    // TODO(dk949): check if this is necessary
    m_fonts = ret;
    return ret;
}

void drw_fontset_free(Fnt *font) {
    if (font) {
        drw_fontset_free(font->next);
        xfont_free(font);
    }
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

void Drw::draw_rect(int x, int y, unsigned int w, unsigned int h, int filled, int invert) {
    if (!m_current_color) return;

    XSetForeground(m_dpy, m_gc, invert ? currentColor().bg.pixel : currentColor().fg.pixel);
    if (filled)
        XFillRectangle(m_dpy, m_drawable, m_gc, x, y, w, h);
    else
        XDrawRectangle(m_dpy, m_drawable, m_gc, x, y, w - 1, h - 1);
}

int Drw::draw_text(int x, int y, unsigned int w, unsigned int h, unsigned int lpad, char const *text, unsigned invert) {
    int ellipsis_x = 0;
    unsigned int tmpw, ellipsis_w = 0;
    XftDraw *d = nullptr;
    Fnt *curfont;
    Fnt *nextfont;
    int render = x || y || w || h;
    long utf8codepoint = 0;
    FcCharSet *fccharset;
    FcPattern *fcpattern;
    FcPattern *match;
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

    if ((render && (!m_current_color || !w)) || !text || !m_fonts) return 0;

    if (!render) {
        w = invert ? invert : ~invert;
    } else {
        XSetForeground(m_dpy, m_gc, currentColor().invert(invert).bg.pixel);
        XFillRectangle(m_dpy, m_drawable, m_gc, x, y, w, h);
        d = XftDrawCreate(m_dpy, m_drawable, DefaultVisual(m_dpy, m_screen), DefaultColormap(m_dpy, m_screen));
        x += (int)lpad;
        w -= lpad;
    }

    Fnt *usedfont = m_fonts;
    if (!ellipsis_width && render) ellipsis_width = fontset_getwidth("...");
    while (true) {
        unsigned ew = 0;
        std::size_t ellipsis_len = 0;
        std::size_t utf8strlen = 0;
        char const *utf8str = text;
        nextfont = nullptr;
        while (*text) {
            auto utf8charlen = utf8decode(text, &utf8codepoint, UTF_SIZ);
            for (curfont = m_fonts; curfont; curfont = curfont->next) {
                charexists = charexists || XftCharExists(m_dpy, curfont->xfont, (FcChar32)utf8codepoint);
                if (charexists) {
                    drw_font_getexts(curfont, text, utf8charlen, &tmpw, nullptr);
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
                auto ty = (unsigned)y + (h - usedfont->h) / 2 + (unsigned)usedfont->xfont->ascent;
                XftDrawStringUtf8(d,
                    &currentColor().invert(invert).fg,
                    usedfont->xfont,
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
            usedfont = nextfont;
        } else {
            /* Regardless of whether or not a fallback font is found, the
             * character must be drawn. */
            charexists = 1;

            for (int i = 0; i < nomatches_len; ++i) {
                /* avoid calling XftFontMatch if we know we won't find a match */
                if (utf8codepoint == nomatches.codepoint[i]) goto no_match;
            }



            fccharset = FcCharSetCreate();
            FcCharSetAddChar(fccharset, (FcChar32)utf8codepoint);

            if (!m_fonts->pattern) {
                /* Refer to the comment in xfont_create for more information. */
                lg::fatal("the first font in the cache must be loaded from a font string.");
            }

            fcpattern = FcPatternDuplicate(m_fonts->pattern);
            FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
            FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);

            FcConfigSubstitute(nullptr, fcpattern, FcMatchPattern);
            FcDefaultSubstitute(fcpattern);
            match = XftFontMatch(m_dpy, m_screen, fcpattern, &result);

            FcCharSetDestroy(fccharset);
            FcPatternDestroy(fcpattern);

            if (match) {
                usedfont = xfont_create(nullptr, match);
                if (usedfont && XftCharExists(m_dpy, usedfont->xfont, (FcChar32)utf8codepoint)) {
                    for (curfont = m_fonts; curfont->next; curfont = curfont->next)
                        ; /* NOP */
                    curfont->next = usedfont;
                } else {
                    xfont_free(usedfont);
                    nomatches.codepoint[++nomatches.idx % nomatches_len] = utf8codepoint;
no_match:
                    usedfont = m_fonts;
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

unsigned int Drw::fontset_getwidth_clamp(char const *text, unsigned int n) {
    unsigned int tmp = 0;
    if (m_fonts && text && n) tmp = (unsigned)draw_text(0, 0, 0, 0, 0, text, n);
    return std::min(n, tmp);
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
