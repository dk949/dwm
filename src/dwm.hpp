#ifndef DWM_HPP
#define DWM_HPP

#include "layout.hpp"

#include <signal.h>
#include <X11/X.h>

struct Pertag;
struct Monitor;
struct Client;

struct Monitor {
    char layoutSymbol[16];
    float mfact;
    int nmaster;
    int num;
    int by;             /* bar geometry */
    int mx, my, mw, mh; /* screen size */
    int wx, wy, ww, wh; /* window area  */
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
};

#define ISVISIBLEONTAG(C, T) (((C)->tags & (T)))
#define ISVISIBLE(C)         ISVISIBLEONTAG(C, (C)->mon->tagset[(C)->mon->seltags])

#endif  // DWM_HPP
