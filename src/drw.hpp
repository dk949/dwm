/* See LICENSE file for copyright and license details. */
#ifndef DWM_DRW_HPP
#define DWM_DRW_HPP

#include "colors.hpp"
#include "dwm.hpp"
#include "xidptr.hpp"

#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include <optional>
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
    Display *dpy = nullptr;
    int h = 0;
    XftFont *xfont = nullptr;
    FcPattern *pattern = nullptr;
    bool operator==(Fnt const &) const = default;
};

enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */

using Clr = XftColor;

struct Drw {
private:
    ColorScheme m_scheme {};
    Color const *m_current_color = nullptr;
    int m_screen_width;
    int m_screen_height;
    Display *m_dpy;
    int m_screen;
    Window m_root;
    Drawable m_drawable;
    GC m_gc;
    std::vector<Fnt> m_fonts;
    Cursors m_cursors;

public:
    Drw(Display *dpy, int screen, Window win, int width, int height);
    Drw(Drw const &) = delete;
    Drw &operator=(Drw const &) = delete;
    Drw(Drw &&) = delete;
    Drw &operator=(Drw &&) = delete;
    ~Drw();

    void resize(int w, int h);
    [[nodiscard]]
    bool fontsetCreate(std::span<char const *const> fonts);


    void setColorScheme(ColorSchemeName clrnames);

    int fontsetGetwidth(char const *text);

    int drawText(int x, int y, int w, int h, int left_pad, char const *text, bool invert);
    void drawRect(int x, int y, int w, int h, bool filled, bool invert);
    void map(Window win, Rect<int> dims);

    void setColor(Color const *col) {
        m_current_color = col;
    }

    [[nodiscard]]
    ColorScheme const &scheme() const {
        return m_scheme;
    }

    [[nodiscard]]
    Color const &currentColor() const {
        return *m_current_color;
    }

    [[nodiscard]]
    Fnt const &fonts() const {
        return m_fonts.front();
    }

    [[nodiscard]]
    Cursors const &cursors() const {
        return m_cursors;
    }

private:
    std::optional<Fnt> xfontCreate(char const *fontname);
    std::optional<Fnt> xfontCreate(FcPattern *fontpattern);
    Clr clrCreate(char const *clrname) const;
    Color nameToColor(ColorName const &name) const;
};

/* Fnt abstraction */
void drwFontsetFree(std::vector<Fnt> &set);
void drwFontGetexts(Fnt *font, char const *text, std::size_t len, int *w, int *h);

#endif  // DWM_DRW_HPP
