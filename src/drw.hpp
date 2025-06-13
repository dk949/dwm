/* See LICENSE file for copyright and license details. */
#ifndef DWM_DRW_HPP
#define DWM_DRW_HPP

#include "xidptr.hpp"

#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include <span>
#include <utility>

enum struct CurShape : unsigned { Normal = XC_left_ptr, Resize = XC_sizing, Move = XC_fleur };

struct Cursors {
private:
    XidPtr m_normal;
    XidPtr m_resize;
    XidPtr m_move;

    static void freeCursor(Display *dpy, Cursor cursor) {
        XFreeCursor(dpy, cursor);
    }

public:

    [[nodiscard]]
    Cursor normal() const {
        return m_normal.get();
    }

    [[nodiscard]]
    Cursor resize() const {
        return m_normal.get();
    }

    [[nodiscard]]
    Cursor move() const {
        return m_normal.get();
    }

    explicit Cursors(Display *dpy)
            : m_normal(dpy, XCreateFontCursor(dpy, std::to_underlying(CurShape::Normal)), freeCursor)
            , m_resize(dpy, XCreateFontCursor(dpy, std::to_underlying(CurShape::Normal)), freeCursor)
            , m_move(dpy, XCreateFontCursor(dpy, std::to_underlying(CurShape::Normal)), freeCursor) { };
};

struct Cur {
    Cursor cursor;
};

struct Fnt {
    Display *dpy;
    unsigned int h;
    XftFont *xfont;
    FcPattern *pattern;
    struct Fnt *next;
};

enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */

using Clr = XftColor;

struct Drw {
private:
    unsigned int m_screen_width;
    unsigned int m_screen_height;
    Display *m_dpy;
    int m_screen;
    Window m_root;
    Drawable m_drawable;
    GC m_gc;
    Clr *m_scheme;
    Fnt *m_fonts;
    Cursors m_cursors;

public:
    Drw(Display *dpy, int screen, Window win, unsigned int width, unsigned int height);
    ~Drw();

    void resize(unsigned int w, unsigned int h);
    unsigned int fontset_getwidth_clamp(char const *text, unsigned int n);
    Fnt *fontset_create(char const *fonts[], size_t fontcount);

    void clr_create(Clr *dest, char const *clrname);
    Clr *scm_create(std::span<char const *const> clrnames);

    unsigned int fontset_getwidth(char const *text);

    int draw_text(int x, int y, unsigned int w, unsigned int h, unsigned int lpad, char const *text, unsigned invert);
    void draw_rect(int x, int y, unsigned int w, unsigned int h, int filled, int invert);
    void map(Window win, int x, int y, unsigned int w, unsigned int h);

    inline void set_fontset(Fnt *set) {
        m_fonts = set;
    }

    inline void set_scheme(Clr *scm) {
        m_scheme = scm;
    }

    inline Fnt const *fonts() const {
        return m_fonts;
    }

private:
    Fnt *xfont_create(char const *fontname, FcPattern *fontpattern);
};

/* Fnt abstraction */
void drw_fontset_free(Fnt *set);
void drw_font_getexts(Fnt *font, char const *text, std::size_t len, unsigned int *w, unsigned int *h);

#endif  // DWM_DRW_HPP
