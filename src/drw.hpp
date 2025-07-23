/* See LICENSE file for copyright and license details. */
#ifndef DWM_DRW_HPP
#define DWM_DRW_HPP

#include "colors.hpp"
#include "xidptr.hpp"

#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include <optional>
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
        return m_resize.get();
    }

    [[nodiscard]]
    Cursor move() const {
        return m_move.get();
    }

    explicit Cursors(Display *dpy)
            : m_normal(dpy, XCreateFontCursor(dpy, std::to_underlying(CurShape::Normal)), freeCursor)
            , m_resize(dpy, XCreateFontCursor(dpy, std::to_underlying(CurShape::Resize)), freeCursor)
            , m_move(dpy, XCreateFontCursor(dpy, std::to_underlying(CurShape::Move)), freeCursor) { };
};

struct Fnt {
    Display *dpy;
    unsigned int h;
    XftFont *xfont = nullptr;
    FcPattern *pattern = nullptr;
    bool operator==(Fnt const &) const = default;
};

enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */

using Clr = XftColor;

struct Drw {
private:
    ColorScheme m_scheme;
    Color const *m_current_color = nullptr;
    unsigned int m_screen_width;
    unsigned int m_screen_height;
    Display *m_dpy;
    int m_screen;
    Window m_root;
    Drawable m_drawable;
    GC m_gc;
    std::vector<Fnt> m_fonts;
    Cursors m_cursors;

public:
    Drw(Display *dpy, int screen, Window win, unsigned int width, unsigned int height);
    Drw(Drw const &) = delete;
    Drw &operator=(Drw const &) = delete;
    Drw(Drw &&) = delete;
    Drw &operator=(Drw &&) = delete;
    ~Drw();

    void resize(unsigned int w, unsigned int h);
    [[nodiscard]]
    unsigned int fontset_getwidth_clamp(char const *text, unsigned int n);
    bool fontset_create(char const *fonts[], size_t fontcount);


    void setColorScheme(ColorSchemeName clrnames);

    unsigned int fontset_getwidth(char const *text);

    int draw_text(int x, int y, unsigned int w, unsigned int h, unsigned int lpad, char const *text, unsigned invert);
    void draw_rect(int x, int y, unsigned int w, unsigned int h, int filled, int invert);
    void map(Window win, int x, int y, unsigned int w, unsigned int h);

    inline void setColor(Color const *col) {
        m_current_color = col;
    }

    [[nodiscard]]
    inline ColorScheme const &scheme() const {
        return m_scheme;
    }

    [[nodiscard]]
    inline Color const &currentColor() const {
        return *m_current_color;
    }

    [[nodiscard]]
    inline Fnt const &fonts() const {
        return m_fonts.front();
    }

    [[nodiscard]]
    Cursors const &cursors() const {
        return m_cursors;
    }

private:
    std::optional<Fnt> xfont_create(char const *fontname);
    std::optional<Fnt> xfont_create(FcPattern *fontpattern);
    Clr clr_create(char const *clrname) const;
    Color nameToColor(ColorName const &name) const;
};

/* Fnt abstraction */
void drw_fontset_free(std::vector<Fnt> &set);
void drw_font_getexts(Fnt *font, char const *text, std::size_t len, unsigned int *w, unsigned int *h);

#endif  // DWM_DRW_HPP
