#ifndef DWM_HPP
#define DWM_HPP

#include "layout.hpp"
#include "xidptr.hpp"

#include <X11/X.h>
#include <X11/Xutil.h>

#include <cstddef>
#include <format>
#include <stdexcept>

struct Pertag;
struct Monitor;
struct Client;

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
using MonitorPtr = std::shared_ptr<Monitor>;
using WeakMonitorPtr = std::weak_ptr<Monitor>;
using Monitors = std::vector<MonitorPtr>;

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

struct Client {
    char name[256];
    float mina, maxa;
    float cfact;
    // TODO(dk949): How about a Rectangle struct???
    Rect<int> size;
    Rect<int> old_size;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh;
    bool hintsvalid;
    int bw, oldbw;
    unsigned int tags;
    unsigned int switchtotag;
    // TODO(dk949): combine these into a ClientConfig?
    bool isfixed;
    bool isfloating;
    bool isurgent;
    bool neverfocus;
    bool old_float_state;
    bool isfullscreen;
    bool isterminal;
    bool noswallow;
    pid_t pid;
    Client *next;
    Client *snext;
    Client *swallowing;
    MonitorPtr mon;
    Window win;

    [[nodiscard]]
    ClassHint classHint(Display *dpy) {
        XClassHint ch;
        XGetClassHint(dpy, win, &ch);
        return ClassHint::fromX(ch);
    }
};

#define ISVISIBLEONTAG(C, T) (((C)->tags & (T)))
#define ISVISIBLE(C)         ISVISIBLEONTAG(C, (C)->mon->tagset[(C)->mon->seltags])

#endif  // DWM_HPP
