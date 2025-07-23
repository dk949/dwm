#ifndef DWM_HPP
#define DWM_HPP

#include "layout.hpp"
#include "xidptr.hpp"

#include <X11/X.h>
#include <X11/Xutil.h>

struct Pertag;
struct Monitor;
struct Client;

struct ClassHint {
    XPtr<char> instance_hint;
    XPtr<char> class_hint;

    [[nodiscard]]
    static ClassHint fromX(XClassHint ch) {
        return {XPtr<char> {ch.res_name}, XPtr<char> {ch.res_class}};
    }
};

struct Monitor {
    char layoutSymbol[16];
    float mfact;
    int nmaster;
    int num;
    int bar_y;                                               /* bar geometry */
    int monitor_x, monitor_y, monitor_width, monitor_height; /* screen size */
    int window_x, window_y, window_width, window_height;
    unsigned int seltags;
    unsigned int sellt;
    unsigned int tagset[2];
    int showbar;
    int topbar;
    Client *clients;
    Client *sel;
    Client *stack;
    Monitor *next;
    Window barwin;
    Layout const *lt[2];
    Pertag *pertag;
};

struct Client {
    char name[256];
    float mina, maxa;
    float cfact;
    int x, y, w, h;
    int oldx, oldy, oldw, oldh;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh;
    int hintsvalid;
    int bw, oldbw;
    unsigned int tags;
    unsigned int switchtotag;
    int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, isterminal, noswallow;
    pid_t pid;
    Client *next;
    Client *snext;
    Client *swallowing;
    Monitor *mon;
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
