#ifndef DWM_HPP
#define DWM_HPP

#include "boolenum.hpp"
#include "layout.hpp"
#include "xidptr.hpp"

#include <ut/static_string/static_string.hpp>
#include <X11/X.h>
#include <X11/Xutil.h>

#include <cstddef>
#include <format>
#include <stdexcept>

struct Pertag;
struct Monitor;
struct Client;

BOOLEAN_ENUM(FullScreen) {off = false, on = true};

template<typename T>
struct Rect {
    T x;
    T y;
    T w;
    T h;

    T operator[](std::size_t idx) {
        switch (idx) {
            case 0: return x;
            case 1: return y;
            case 2: return w;
            case 3: return h;
            default: throw std::runtime_error(std::format("Bad Rect access: {} > 4", idx));
        }
    }
};

struct ClassHint {
    XPtr<char> instance_hint;
    XPtr<char> class_hint;

    [[nodiscard]]
    static ClassHint fromX(XClassHint ch) {
        return {XPtr<char> {ch.res_name}, XPtr<char> {ch.res_class}};
    }
};

using MonitorRef = std::shared_ptr<Monitor>;
using WeakMonitorRef = std::weak_ptr<Monitor>;
using Monitors = std::vector<MonitorRef>;

struct Monitor {
    char layoutSymbol[16];
    float mfact;
    int nmaster;
    int num;
    int bar_y; /* bar geometry */
    Rect<int> monitor_size;
    Rect<int> window_size;
    unsigned int seltags;
    unsigned int sellt;
    unsigned int tagset[2];
    bool showbar;
    int topbar;
    Client *clients;
    Client *sel;
    Client *stack;
    Window barwin;
    Layout const *lt[2];
    Pertag *pertag;
};

struct ClientProps {
    bool isfixed;
    bool isfloating;
    bool isurgent;
    bool neverfocus;
    bool old_float_state;
    FullScreen isfullscreen;
    bool isterminal;
    bool noswallow;
};

struct Client {
    ut::StaticString<256> name;  // NOLINT readability-magic-numbers
    float mina, maxa;
    float cfact;
    Rect<int> size;
    Rect<int> old_size;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh;
    bool hintsvalid;
    int bw, oldbw;
    unsigned int tags;
    unsigned int switchtotag;
    ClientProps props;
    pid_t pid;
    Client *next;
    Client *snext;
    Client *swallowing;
    MonitorRef mon;
    Window win;

    [[nodiscard]]
    ClassHint classHint(Display *dpy) const {
        XClassHint ch;
        auto status = XGetClassHint(dpy, win, &ch);
        if (!status) return ClassHint {nullptr, nullptr};
        return ClassHint::fromX(ch);
    }

    [[nodiscard]]
    std::size_t count() const {
        std::size_t count = 0;
        for (auto const *client = this; client; client = client->next, ++count) { }
        return count;
    }

    void configure() const;
    void applyrules();
    void resizeclient(Rect<int> new_size);
    bool applysizehints(Rect<int> *size, bool interact);
    void resize(Rect<int> size, bool interact);
    void unfocus(bool setfocus);
    void setfocus();
    void setfullscreen(FullScreen fullscreen);
    void grabbuttons(bool focused) const;
    [[nodiscard]]
    bool sendevent(Atom proto) const;
    [[nodiscard]]
    Atom getatomprop(Atom prop) const;

    [[nodiscard]]
    bool isVisibleOnTag(unsigned tag) const {
        return (tags & tag) != 0u;
    }

    [[nodiscard]]
    bool isVisible() const {
        return isVisibleOnTag(mon->tagset[mon->seltags]);
    }
};

#endif  // DWM_HPP
