/* See LICENSE file for copyright and license details. */
#ifndef DWM_DRW_HPP
#define DWM_DRW_HPP

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include <span>

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
    unsigned int w, h;
    Display *dpy;
    int screen;
    Window root;
    Drawable drawable;
    GC gc;
    Clr *scheme;
    Fnt *fonts;
};

/* Drawable abstraction */
Drw *drw_create(Display *dpy, int screen, Window win, unsigned int w, unsigned int h);
void drw_resize(Drw *drw, unsigned int w, unsigned int h);
void drw_free(Drw *drw);
unsigned int drw_fontset_getwidth_clamp(Drw *drw, char const *text, unsigned int n);

/* Fnt abstraction */
Fnt *drw_fontset_create(Drw *drw, char const *fonts[], size_t fontcount);
void drw_fontset_free(Fnt *set);
unsigned int drw_fontset_getwidth(Drw *drw, char const *text);
void drw_font_getexts(Fnt *font, char const *text, std::size_t len, unsigned int *w, unsigned int *h);

/* Colorscheme abstraction */
void drw_clr_create(Drw *drw, Clr *dest, char const *clrname);
Clr *drw_scm_create(Drw *drw, std::span<char const *const> clrnames);

/* Cursor abstraction */
Cur *drw_cur_create(Drw *drw, int shape);
void drw_cur_free(Drw *drw, Cur *cursor);

/* Drawing context manipulation */
void drw_setfontset(Drw *drw, Fnt *set);
void drw_setscheme(Drw *drw, Clr *scm);

/* Drawing functions */
void drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int invert);
int drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, unsigned int lpad, char const *text, unsigned invert);

/* Map functions */
void drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h);

#endif  // DWM_DRW_HPP
