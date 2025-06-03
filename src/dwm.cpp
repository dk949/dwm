/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.hpp.
 *
 * To understand everything else, start reading main().
 */

#include "dwm.hpp"

#include "layout.hpp"
#include "log.hpp"
#include "mapping.hpp"

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>

#ifdef XINERAMA
#    include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include "drw.hpp"
#include "util.hpp"

#ifdef ASOUND
#    include "volc.hpp"
#endif /* ASOUND */

#include "backlight.hpp"

#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                 \
    ((mask) & ~(numlockmask | LockMask) \
        & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                                                        \
    (std::max(0, std::min((x) + (w), (m)->window_x + (m)->window_width) - std::max((x), (m)->window_x)) \
        * std::max(0, std::min((y) + (h), (m)->window_y + (m)->window_height) - std::max((y), (m)->window_y)))
#define LENGTH(X) (sizeof(X) / sizeof(X)[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X)  ((unsigned)(X)->w + 2 * (unsigned)(X)->bw + gappx)
#define HEIGHT(X) ((unsigned)(X)->h + 2 * (unsigned)(X)->bw + gappx)
#define TAGMASK   ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)  (drw_fontset_getwidth(drw, (X)) + (unsigned)lrpad)
#ifndef dwm_version
#    define dwm_version "unknown"
#endif

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */

enum {
    NetSupported,
    NetWMName,
    NetWMState,
    NetWMCheck,
    NetWMFullscreen,
    NetActiveWindow,
    NetWMWindowType,
    NetWMWindowTypeDialog,
    NetClientList,
    NetWMIcon,
    NetLast
}; /* EWMH atoms */

enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMChangeState, WMLast }; /* default atoms */

enum {
    SelfNotifyNone = 0,
    SelfNotifyFadeBar,

    // Has to be last
    SelfNotifyLast,
};

using SelfNotifyCallback = void (*)();
using NotifyCallback = void (*)(XEvent *);

#define PROGRESS_FADE 0, 0, 0


/* function declarations */

static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachaside(Client *c);
static void attachstack(Client *c);
static int avgheight(void);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
/// Remove client `c` from the list of clients on the monitor `c` is on
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawprogress(unsigned long long total, unsigned long long current, int scheme);
static void enqueue(Client *c);
static void enqueuestack(Client *c);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static pid_t getparentprocess(pid_t p);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void iconifyclient(Client *c);
static int isdescprocess(pid_t p, pid_t c);
static void keypress(XEvent *e);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static Client *nexttagged(Client *c);
static Client *nexttiled(Client *c);
static void redirectChildLog(char **argv);
static void notifyself(int type);
static void handle_notifyself_fade_anim(void);
static void pop(Client *c);
static void print_event_stats(void);
static void propertynotify(XEvent *e);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static Client *swallowingclient(Window w);
static Client *termforwin(Client const *c);
static double timespecdiff(const struct timespec *a, const struct timespec *b);
static void unfocus(Client *c, int setfocus);
static void uniconifyclient(Client *c);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);


static pid_t winpid(Window w);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static void wmchange(Client *c, XClientMessageEvent *cme);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);

/* variables */
static char const broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;                                                   /* X display screen geometry width, height */
static int bar_height, sel_bar_name_x = -1, sel_bar_name_width = -1; /* bar geometry */
static int lrpad;                                                    /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static int notified = SelfNotifyNone;
static constexpr auto self_notify_handler = [] {
    std::array<SelfNotifyCallback, 2> out;
    out[SelfNotifyNone] = NULL;
    out[SelfNotifyFadeBar] = handle_notifyself_fade_anim;
    return out;
}();
// static void (*handler[LASTEvent])(XEvent *) = {
static constexpr auto handler = [] {
    std::array<NotifyCallback, LASTEvent> out {};
    out[ButtonPress] = buttonpress;
    out[ClientMessage] = clientmessage;
    out[ConfigureRequest] = configurerequest;
    out[ConfigureNotify] = configurenotify;
    out[DestroyNotify] = destroynotify;
    out[EnterNotify] = enternotify;
    out[Expose] = expose;
    out[FocusIn] = focusin;
    out[KeyPress] = keypress;
    out[MappingNotify] = mappingnotify;
    out[MapRequest] = maprequest;
    out[MotionNotify] = motionnotify;
    out[PropertyNotify] = propertynotify;
    out[UnmapNotify] = unmapnotify;
    return out;
}();
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1, need_restart = 0;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
#ifdef ASOUND
static volc_t *volc;
#endif /* ASOUND */
static xcb_connection_t *xcon;
static std::optional<std::filesystem::path> log_dir;

/* configuration, allows nested code to access above variables */
#include "config.hpp"

struct Pertag {
    unsigned int curtag, prevtag;              /* current and previous tag */
    int nmasters[LENGTH(tags) + 1];            /* number of windows in master area */
    float mfacts[LENGTH(tags) + 1];            /* mfacts per tag */
    unsigned int sellts[LENGTH(tags) + 1];     /* selected layouts */
    Layout const *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
    int showbars[LENGTH(tags) + 1];            /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
    char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
void applyrules(Client *c) {
    char const *class_;
    char const *instance;
    unsigned int i;
    unsigned int newtagset;
    Rule const *r;
    Monitor *m;
    XClassHint ch = {NULL, NULL};

    /* rule matching */
    c->isfloating = 0;
    c->tags = 0;
    XGetClassHint(dpy, c->win, &ch);
    class_ = ch.res_class ? ch.res_class : broken;
    instance = ch.res_name ? ch.res_name : broken;

    for (i = 0; i < LENGTH(rules); i++) {
        r = &rules[i];
        if ((!r->title || strstr(c->name, r->title)) && (!r->class_ || strstr(class_, r->class_))
            && (!r->instance || strstr(instance, r->instance))) {
            c->isterminal = r->isterminal;
            c->isfloating = r->isfloating;
            c->tags |= r->tags;
            for (m = mons; m && m->num != r->monitor; m = m->next) {
                ;
            }
            if (m) {
                c->mon = m;
            }
            if (r->switchtotag) {
                selmon = c->mon;
                if (r->switchtotag == 2 || r->switchtotag == 4) {
                    newtagset = c->mon->tagset[c->mon->seltags] ^ c->tags;
                } else {
                    newtagset = c->tags;
                }

                if (newtagset && !(c->tags & c->mon->tagset[c->mon->seltags])) {
                    if (r->switchtotag == 3 || r->switchtotag == 4) {
                        c->switchtotag = c->mon->tagset[c->mon->seltags];
                    }
                    if (r->switchtotag == 1 || r->switchtotag == 3) {
                        view(Arg {.ui = newtagset});
                    } else {
                        c->mon->tagset[c->mon->seltags] = newtagset;
                        arrange(c->mon);
                    }
                }
            }
        }
    }
    if (ch.res_class) {
        XFree(ch.res_class);
    }
    if (ch.res_name) {
        XFree(ch.res_name);
    }
    c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact) {
    int baseismin;
    Monitor *m = c->mon;

    /* set minimum possible */
    *w = std::max(1, *w);
    *h = std::max(1, *h);
    if (interact) {
        if (*x > sw) {
            *x = (int)((unsigned)sw - WIDTH(c));
        }
        if (*y > sh) {
            *y = (int)((unsigned)sh - HEIGHT(c));
        }
        if (*x + *w + 2 * c->bw < 0) {
            *x = 0;
        }
        if (*y + *h + 2 * c->bw < 0) {
            *y = 0;
        }
    } else {
        if (*x >= m->window_x + m->window_width) {
            *x = (int)((unsigned)(m->window_x + m->window_width) - WIDTH(c));
        }
        if (*y >= m->window_y + m->window_height) {
            *y = (int)((unsigned)(m->window_y + m->window_height) - HEIGHT(c));
        }
        if (*x + *w + 2 * c->bw <= m->window_x) {
            *x = m->window_x;
        }
        if (*y + *h + 2 * c->bw <= m->window_y) {
            *y = m->window_y;
        }
    }
    if (*h < bar_height) {
        *h = bar_height;
    }
    if (*w < bar_height) {
        *w = bar_height;
    }
    if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
        if (!c->hintsvalid) updatesizehints(c);
        /* see last two sentences in ICCCM 4.1.2.3 */
        baseismin = c->basew == c->minw && c->baseh == c->minh;
        if (!baseismin) { /* temporarily remove base dimensions */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for aspect limits */
        if (c->mina > 0 && c->maxa > 0) {
            if (c->maxa < (float)*w / (float)*h) {
                *w = (int)((float)*h * c->maxa + 0.5f);
            } else if (c->mina < (float)*h / (float)*w) {
                *h = (int)((float)*w * c->mina + 0.5f);
            }
        }
        if (baseismin) { /* increment calculation requires this */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for increment value */
        if (c->incw) {
            *w -= *w % c->incw;
        }
        if (c->inch) {
            *h -= *h % c->inch;
        }
        /* restore base dimensions */
        *w = std::max(*w + c->basew, c->minw);
        *h = std::max(*h + c->baseh, c->minh);
        if (c->maxw) {
            *w = std::min(*w, c->maxw);
        }
        if (c->maxh) {
            *h = std::min(*h, c->maxh);
        }
    }
    return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor *m) {
    if (m) {
        showhide(m->stack);
    } else {
        for (m = mons; m; m = m->next) {
            showhide(m->stack);
        }
    }
    if (m) {
        arrangemon(m);
        restack(m);
    } else {
        for (m = mons; m; m = m->next) {
            arrangemon(m);
        }
    }
}

void arrangemon(Monitor *m) {
    strncpy(m->layoutSymbol, m->lt[m->sellt]->symbol, sizeof m->layoutSymbol);
    if (m->lt[m->sellt]->arrange) {
        m->lt[m->sellt]->arrange(m);
    }
}

void attach(Client *c) {
    c->next = c->mon->clients;
    c->mon->clients = c;
}

void attachaside(Client *c) {
    Client *at = nexttagged(c);
    if (!at) {
        attach(c);
        return;
    }
    c->next = at->next;
    at->next = c;
}

void attachstack(Client *c) {
    c->snext = c->mon->stack;
    c->mon->stack = c;
}

int avgheight(void) {
#ifdef XINERAMA
    if (XineramaIsActive(dpy)) {
        int scr_count;
        XineramaScreenInfo *screens = XineramaQueryScreens(dpy, &scr_count);
        double out = 0;
        for (int i = 0; i < scr_count; i++)
            out += screens->width;
        XFree(screens);
        return (int)(out / (double)scr_count);
    } else
#endif
    {
        return sh;
    }
}

void swallow(Client *p, Client *c) {
    if (c->noswallow || c->isterminal) {
        return;
    }

    detach(c);
    detachstack(c);

    setclientstate(c, WithdrawnState);
    XUnmapWindow(dpy, p->win);

    p->swallowing = c;
    c->mon = p->mon;

    Window w = p->win;
    p->win = c->win;
    c->win = w;
    updatetitle(p);
    arrange(p->mon);
    XMoveResizeWindow(dpy, p->win, p->x, p->y, (unsigned)p->w, (unsigned)p->h);
    configure(p);
    updateclientlist();
}

void unswallow(Client *c) {
    c->win = c->swallowing->win;

    delete c->swallowing;
    c->swallowing = NULL;

    updatetitle(c);
    updatesizehints(c);
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    XMoveResizeWindow(dpy, c->win, c->x, c->y, (unsigned)c->w, (unsigned)c->h);
    configure(c);
    setclientstate(c, NormalState);
}

void bright_dec(Arg const &arg) {
    int ret;
    if ((ret = bright_dec_((double)arg.f))) {
        lg::warn("Function bright_dec_(const Arg *arg) from backlight.hpp returned {}", ret);
        return;
    }
    double newval;
    if ((ret = bright_get_(&newval))) {
        lg::warn("Function bright_get_(const Arg *arg) from backlight.hpp returned {}", ret);
        return;
    }
    drawprogress(100, (unsigned long long)newval, SchemeBrightProgress);
}

void bright_inc(Arg const &arg) {
    int ret;
    if ((ret = bright_inc_((double)arg.f))) {
        lg::warn("Function bright_inc_(const Arg *arg) from backlight.hpp returned {}", ret);
        return;
    }
    double newval;
    if ((ret = bright_get_(&newval))) {
        lg::warn("Function bright_get_(const Arg *arg) from backlight.hpp returned {}", ret);
        return;
    }
    drawprogress(100, (unsigned long long)newval, SchemeBrightProgress);
}

void bright_set(Arg const &arg) {
    int ret;
    if ((ret = bright_set_((double)arg.f))) {
        lg::warn("Function bright_set_(const Arg *arg) from backlight.hpp returned {}", ret);
        return;
    }
    drawprogress(100, (unsigned long long)arg.f, SchemeBrightProgress);
}

void buttonpress(XEvent *e) {
    unsigned int i;
    unsigned int x;
    unsigned int click;
    Arg arg = {0};
    Client *c;
    Monitor *m;
    XButtonPressedEvent *ev = &e->xbutton;

    click = ClkRootWin;
    /* focus monitor if necessary */
    if ((m = wintomon(ev->window)) && m != selmon) {
        unfocus(selmon->sel, 1);
        selmon = m;
        focus(NULL);
    }
    if (ev->window == selmon->barwin) {
        i = x = 0;
        do {
            x += TEXTW(tags[i]);
        } while ((unsigned)ev->x >= x && ++i < LENGTH(tags));
        if (i < LENGTH(tags)) {
            click = ClkTagBar;
            arg.ui = 1 << i;
        } else if ((unsigned)ev->x < x + TEXTW(selmon->layoutSymbol)) {
            click = ClkLtSymbol;
        } else if (ev->x > selmon->window_width - (int)TEXTW(stext)) {
            click = ClkStatusText;
        } else {
            click = ClkWinTitle;
        }
    } else if ((c = wintoclient(ev->window))) {
        focus(c);
        restack(selmon);
        XAllowEvents(dpy, ReplayPointer, CurrentTime);
        click = ClkClientWin;
    }
    for (i = 0; i < LENGTH(buttons); i++) {
        if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
            && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state)) {
            buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? arg : buttons[i].arg);
        }
    }
}

void checkotherwm(void) {
    xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XSync(dpy, False);
}

void cleanup(void) {
    Layout foo = {"", NULL};
    Monitor *m;
    size_t i;

    view(Arg {.ui = ~0u});
    selmon->lt[selmon->sellt] = &foo;
    for (m = mons; m; m = m->next) {
        while (m->stack) {
            unmanage(m->stack, 0);  // XXX: Potential problens
        }
    }
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    while (mons) {
        cleanupmon(mons);
    }
    for (i = 0; i < CurLast; i++) {
        drw_cur_free(drw, cursor[i]);
    }
    for (i = 0; i < LENGTH(colors); i++)
        delete[] scheme[i];
    delete[] scheme;

    XDestroyWindow(dpy, wmcheckwin);
    drw_free(drw);
    XSync(dpy, False);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
#ifdef ASOUND
    volc_deinit(volc);
#endif /* ASOUND */
    fclose(lg::log_file);
}

void cleanupmon(Monitor *mon) {
    Monitor *m;

    if (mon == mons) {
        mons = mons->next;
    } else {
        for (m = mons; m && m->next != mon; m = m->next) {
            ;
        }
        m->next = mon->next;
    }
    XUnmapWindow(dpy, mon->barwin);
    XDestroyWindow(dpy, mon->barwin);
    delete mon->pertag;
    delete mon;
}

void clientmessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;

    Client *c = wintoclient(cme->window);

    if (!c) return;

    if (cme->message_type == netatom[NetWMState]) {
        if ((Atom)cme->data.l[1] == netatom[NetWMFullscreen] || (Atom)cme->data.l[2] == netatom[NetWMFullscreen]) {
            setfullscreen(c,
                (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                    || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
        }
    } else if (cme->message_type == netatom[NetActiveWindow]) {
        if (c != selmon->sel && !c->isurgent) {
            seturgent(c, 1);
        }
    } /*else if (cme->message_type == wmatom[WMChangeState]) {
        wmchange(c, cme);
    }*/
}

void configure(Client *c) {
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = dpy;
    ce.event = c->win;
    ce.window = c->win;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->w;
    ce.height = c->h;
    ce.border_width = c->bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e) {
    Monitor *m;
    Client *c;
    XConfigureEvent *ev = &e->xconfigure;
    int dirty;

    /* TODO: updategeom handling sucks, needs to be simplified */
    if (ev->window == root) {
        dirty = (sw != ev->width || sh != ev->height);
        sw = ev->width;
        sh = ev->height;
        if (updategeom() || dirty) {
            drw_resize(drw, (unsigned)sw, (unsigned)bar_height);
            updatebars();
            for (m = mons; m; m = m->next) {
                for (c = m->clients; c; c = c->next) {
                    if (c->isfullscreen) {
                        resizeclient(c, m->monitor_x, m->monitor_y, m->monitor_width, m->monitor_height);
                    }
                }
                XMoveResizeWindow(dpy, m->barwin, m->window_x, m->bar_y, (unsigned)m->window_width, (unsigned)bar_height);
            }
            focus(NULL);
            arrange(NULL);
        }
    }
}

void configurerequest(XEvent *e) {
    Client *c;
    Monitor *m;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    if ((c = wintoclient(ev->window))) {
        if (ev->value_mask & CWBorderWidth) {
            c->bw = ev->border_width;
        } else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
            m = c->mon;
            if (ev->value_mask & CWX) {
                c->oldx = c->x;
                c->x = m->monitor_x + ev->x;
            }
            if (ev->value_mask & CWY) {
                c->oldy = c->y;
                c->y = m->monitor_y + ev->y;
            }
            if (ev->value_mask & CWWidth) {
                c->oldw = c->w;
                c->w = ev->width;
            }
            if (ev->value_mask & CWHeight) {
                c->oldh = c->h;
                c->h = ev->height;
            }
            if ((c->x + c->w) > m->monitor_x + m->monitor_width && c->isfloating) {
                c->x = (int)((unsigned)m->monitor_x
                             + ((unsigned)(m->monitor_width / 2) - WIDTH(c) / 2)); /* center in x direction */
            }
            if ((c->y + c->h) > m->monitor_y + m->monitor_height && c->isfloating) {
                c->y = (int)((unsigned)m->monitor_y
                             + ((unsigned)(m->monitor_height / 2) - HEIGHT(c) / 2)); /* center in y direction */
            }
            if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight))) {
                configure(c);
            }
            if (ISVISIBLE(c)) {
                XMoveResizeWindow(dpy, c->win, c->x, c->y, (unsigned)c->w, (unsigned)c->h);
            }
        } else {
            configure(c);
        }
    } else {
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, (unsigned int)ev->value_mask, &wc);
    }
    XSync(dpy, False);
}

Monitor *createmon(void) {

    Monitor *m = new Monitor {};
    m->tagset[0] = m->tagset[1] = 1;
    m->mfact = mfact;
    m->nmaster = nmaster;
    m->showbar = showbar;
    m->topbar = topbar;
    m->lt[0] = &layouts[0];
    m->lt[1] = &layouts[1 % LENGTH(layouts)];
    strncpy(m->layoutSymbol, layouts[0].symbol, sizeof m->layoutSymbol);
    m->pertag = new Pertag {};
    m->pertag->curtag = m->pertag->prevtag = 1;

    for (unsigned int i = 0; i <= LENGTH(tags); i++) {
        m->pertag->nmasters[i] = m->nmaster;
        m->pertag->mfacts[i] = m->mfact;

        m->pertag->ltidxs[i][0] = m->lt[0];
        m->pertag->ltidxs[i][1] = m->lt[1];
        m->pertag->sellts[i] = m->sellt;

        m->pertag->showbars[i] = m->showbar;
    }

    return m;
}

void destroynotify(XEvent *e) {
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    if ((c = wintoclient(ev->window))) {
        unmanage(c, 1);
    } else if ((c = swallowingclient(ev->window))) {
        unmanage(c->swallowing, 1);
    }
}

void detach(Client *c) {
    Client **tc;

    for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next) { }

    if (!*tc) lg::warn("Client `{}` was not attached, c->next {}!!!", c->name, c->next ? "is not null" : "is null");
    *tc = c->next;
}

void detachstack(Client *c) {
    Client **tc;
    Client *t;

    for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext) { }
    *tc = c->snext;

    if (c == c->mon->sel) {
        for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext) { }
        c->mon->sel = t;
    }
}

Monitor *dirtomon(int dir) {
    Monitor *m = NULL;

    if (dir > 0) {
        if (!(m = selmon->next)) {
            m = mons;
        }
    } else if (selmon == mons) {
        for (m = mons; m->next; m = m->next) {
            ;
        }
    } else {
        for (m = mons; m->next != selmon; m = m->next) {
            ;
        }
    }
    return m;
}

// TODO(dk949): handle the case where the tags overlap with status
//              (common if monitor is vertical)
void drawbar(Monitor *m) {
    int x;
    int w;
    int text_width = 0;
    int boxs = (int)(drw->fonts->h / 9u);
    int boxw = (int)(drw->fonts->h / 6u + 2u);
    unsigned int i;
    unsigned int occ = 0;
    unsigned int urg = 0;

    if (!m->showbar) return;

    /* draw status first so it can be overdrawn by tags later */
    if (m == selmon) { /* status is only drawn on selected monitor */
        drw_setscheme(drw, scheme[SchemeStatus]);
        text_width = (int)(TEXTW(stext) - (unsigned)lrpad + 2); /* 2px right padding */
        drw_text(drw, m->window_width - text_width, 0, (unsigned)text_width, (unsigned)bar_height, 0, stext, 0);
    }

    for (Client *c = m->clients; c; c = c->next) {
        occ |= c->tags;
        if (c->isurgent) {
            urg |= c->tags;
        }
    }
    x = 0;
    for (i = 0; i < LENGTH(tags); i++) {
        w = (int)TEXTW(tags[i]);
        drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeTagsSel : SchemeTagsNorm]);
        drw_text(drw, x, 0, (unsigned)w, (unsigned)bar_height, (unsigned)(lrpad / 2), tags[i], urg & 1 << i);
        if (occ & 1 << i) {
            drw_rect(drw,
                x + boxs,
                boxs,
                (unsigned)boxw,
                (unsigned)boxw,
                m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
                (int)(urg & 1 << i));
        }
        x += w;
    }
    w = (int)TEXTW(m->layoutSymbol);
    drw_setscheme(drw, scheme[SchemeTagsNorm]);
    x = drw_text(drw, x, 0, (unsigned)w, (unsigned)bar_height, (unsigned)(lrpad / 2), m->layoutSymbol, 0);

    if ((w = m->window_width - text_width - x) > bar_height) {
        if (m->sel) {
            drw_setscheme(drw, scheme[m == selmon ? SchemeInfoSel : SchemeInfoNorm]);
            drw_text(drw, x, 0, (unsigned)w, (unsigned)bar_height, (unsigned)(lrpad / 2), m->sel->name, 0);
            if (m->sel->isfloating) {
                drw_rect(drw, x + boxs, boxs, (unsigned)boxw, (unsigned)boxw, m->sel->isfixed, 0);
            }
        } else {
            drw_setscheme(drw, scheme[SchemeInfoNorm]);
            drw_rect(drw, x, 0, (unsigned)w, (unsigned)bar_height, 1, 1);
        }
    }
    if (m == selmon) {
        sel_bar_name_x = x;
        sel_bar_name_width = w;
    }
    drw_map(drw, m->barwin, 0, 0, (unsigned)m->window_width, (unsigned)bar_height);
    drawprogress(PROGRESS_FADE);
}

void drawbars(void) {
    for (Monitor *m = mons; m; m = m->next) {
        drawbar(m);
    }
}

void drawprogress(unsigned long long t, unsigned long long c, int s) {
    static unsigned long long total;
    static unsigned long long current;
    static struct timespec last;
    static int cscheme;

    if (sel_bar_name_x <= 0 || sel_bar_name_width <= 0) return;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    if (t != 0) {
        total = t;
        current = c;
        last = now;
        cscheme = s;
    }

    if (total > 0 && (timespecdiff(&now, &last) < progress_fade_time)) {
        int x = sel_bar_name_x, y = 0, w = sel_bar_name_width, h = bar_height; /*progress rectangle*/
        int fg = 0;
        int bg = 1;
        drw_setscheme(drw, scheme[cscheme]);

        drw_rect(drw, x, y, (unsigned)w, (unsigned)h, 1, bg);
        drw_rect(drw, x, y, (unsigned)(((double)w * (double)current) / (double)total), (unsigned)h, 1, fg);

        drw_map(drw, selmon->barwin, x, y, (unsigned)w, (unsigned)h);
        notifyself(SelfNotifyFadeBar);
    }
}

void enqueue(Client *c) {
    Client *l;
    for (l = c->mon->clients; l && l->next; l = l->next) { }
    if (l) {
        l->next = c;
        c->next = NULL;
    }
}

void enqueuestack(Client *c) {
    Client *l;
    for (l = c->mon->stack; l && l->snext; l = l->snext) {
        ;
    }
    if (l) {
        l->snext = c;
        c->snext = NULL;
    }
}

void enternotify(XEvent *e) {
    Client *c;
    Monitor *m;
    XCrossingEvent *ev = &e->xcrossing;

    if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root) {
        return;
    }
    c = wintoclient(ev->window);
    m = c ? c->mon : wintomon(ev->window);
    if (m != selmon) {
        unfocus(selmon->sel, 1);
        selmon = m;
    } else if (!c || c == selmon->sel) {
        return;
    }
    focus(c);
}

void expose(XEvent *e) {
    Monitor *m;
    XExposeEvent *ev = &e->xexpose;

    if (ev->count == 0 && (m = wintomon(ev->window))) {
        drawbar(m);
    }
}

void focus(Client *c) {
    if (!c || !ISVISIBLE(c)) {
        for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext) {
            ;
        }
    }
    if (selmon->sel && selmon->sel != c) {
        unfocus(selmon->sel, 0);
    }
    if (c) {
        if (c->mon != selmon) {
            selmon = c->mon;
        }
        if (c->isurgent) {
            seturgent(c, 0);
        }
        detachstack(c);
        attachstack(c);
        grabbuttons(c, 1);
        XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
        setfocus(c);
    } else {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
    selmon->sel = c;
    drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e) {
    XFocusChangeEvent *ev = &e->xfocus;

    if (selmon->sel && ev->window != selmon->sel->win) {
        setfocus(selmon->sel);
    }
}

void focusmon(Arg const &arg) {
    Monitor *m;

    if (!mons->next) {
        return;
    }
    if ((m = dirtomon(arg.i)) == selmon) {
        return;
    }
    unfocus(selmon->sel, 0);
    selmon = m;

    /* move cursor to the center of the new monitor */
    XWarpPointer(dpy, 0, selmon->barwin, 0, 0, 0, 0, selmon->window_width / 2, selmon->window_height / 2);
    focus(NULL);
}

void focusstack(Arg const &arg) {
    Client *c = NULL;
    Client *i;

    if (!selmon->sel || selmon->sel->isfullscreen) {
        return;
    }
    if (arg.i > 0) {
        for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next) {
            ;
        }
        if (!c) {
            for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next) {
                ;
            }
        }
    } else {
        for (i = selmon->clients; i != selmon->sel; i = i->next) {
            if (ISVISIBLE(i)) {
                c = i;
            }
        }
        if (!c) {
            for (; i; i = i->next) {
                if (ISVISIBLE(i)) {
                    c = i;
                }
            }
        }
    }
    if (c) {
        focus(c);
        restack(selmon);
    }
}

Atom getatomprop(Client *c, Atom prop) {
    int fmt;
    unsigned long bytes_left, nitems;
    unsigned char *p = NULL;
    Atom type;
    Atom atom = None;

    if (!XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM, &type, &fmt, &nitems, &bytes_left, &p)
        && p) {
        // If nitems is 0, no property was returned
        if (nitems != 0) atom = *(Atom *)(void *)p;
        XFree(p);
    }
    return atom;
}

int getrootptr(int *x, int *y) {
    int di;
    unsigned int dui;
    Window dummy;

    return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w) {
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n;
    unsigned long extra;
    Atom real;

    if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState], &real, &format, &n, &extra, &p)
        != Success) {
        return -1;
    }
    if (n != 0) {
        result = *p;
    }
    XFree(p);
    return result;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
    char **list = NULL;
    int n;
    XTextProperty name;

    if (!text || size == 0) {
        return 0;
    }
    text[0] = '\0';
    if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems) {
        return 0;
    }
    if (name.encoding == XA_STRING) {
        strncpy(text, (char *)name.value, size - 1);
    } else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
        strncpy(text, *list, size - 1);
        XFreeStringList(list);
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return 1;
}

void grabbuttons(Client *c, int focused) {
    updatenumlockmask();
    {
        unsigned int i;
        unsigned int j;
        unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        if (!focused) {
            XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
        }
        for (i = 0; i < LENGTH(buttons); i++) {
            if (buttons[i].click == ClkClientWin) {
                for (j = 0; j < LENGTH(modifiers); j++) {
                    XGrabButton(dpy,
                        buttons[i].button,
                        buttons[i].mask | modifiers[j],
                        c->win,
                        False,
                        BUTTONMASK,
                        GrabModeAsync,
                        GrabModeSync,
                        None,
                        None);
                }
            }
        }
    }
}

void grabkeys(void) {
    updatenumlockmask();
    unsigned int modifiers[] = {
        0,
        LockMask,
        numlockmask,
        numlockmask | LockMask,
    };
    int start, end, skip;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XDisplayKeycodes(dpy, &start, &end);
    KeySym *syms = XGetKeyboardMapping(dpy, (KeyCode)start, end - start + 1, &skip);
    if (!syms) return;
    for (int k = start; k <= end; k++) {
        for (auto const &key : keys) {
            /* skip modifier codes, we do that ourselves */
            if (key.keysym == syms[(k - start) * skip]) {
                for (auto const &mod : modifiers) {
                    XGrabKey(dpy, k, key.mod | mod, root, True, GrabModeAsync, GrabModeAsync);
                }
            }
        }
    }
    XFree(syms);
}

void setmaster(Arg const &arg) {
    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = std::max(arg.i, 0);
    arrange(selmon);
}

void iconify(Arg const &arg) {
    (void)arg;
    if (!XIconifyWindow(dpy, selmon->sel->win, screen)) lg::debug("Could not iconify {}", selmon->sel->name);
}

void incnmaster(Arg const &arg) {
    setmaster(Arg {.i = std::max(selmon->nmaster + arg.i, 0)});
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info) {
    while (n--) {
        if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org && unique[n].width == info->width
            && unique[n].height == info->height) {
            return 0;
        }
    }
    return 1;
}
#endif /* XINERAMA */

void keypress(XEvent *e) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev;

    ev = &e->xkey;
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"  // FIXME: Look for alternatives
    keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
    for (i = 0; i < LENGTH(keys); i++) {
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func) {
            keys[i].func((keys[i].arg));
        }
    }
}

void killclient(Arg const &arg) {
    (void)arg;
    if (!selmon->sel) {
        return;
    }
    if (!sendevent(selmon->sel, wmatom[WMDelete])) {
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, selmon->sel->win);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
}

void manage(Window w, XWindowAttributes *wa) {
    Client *t = NULL;
    Client *term = NULL;
    Window trans = None;
    XWindowChanges wc;

    Client *c = new Client {};
    c->win = w;
    c->pid = winpid(w);
    /* geometry */
    c->x = c->oldx = wa->x;
    c->y = c->oldy = wa->y;
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;
    c->cfact = 1.0;

    updatetitle(c);
    if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
        c->mon = t->mon;
        c->tags = t->tags;
    } else {
        c->mon = selmon;
        applyrules(c);
        term = termforwin(c);
    }

    if ((unsigned)c->x + WIDTH(c) > (unsigned)(c->mon->window_x + c->mon->window_width))
        c->x = (int)((unsigned)(c->mon->window_x + c->mon->window_width) - WIDTH(c));
    if ((unsigned)c->y + HEIGHT(c) > (unsigned)(c->mon->window_y + c->mon->window_height))
        c->y = (int)((unsigned)(c->mon->window_y + c->mon->window_height) - HEIGHT(c));
    c->x = std::max(c->x, c->mon->window_x);
    c->y = std::max(c->y, c->mon->window_y);



    c->bw = (int)borderpx;

    wc.border_width = c->bw;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
    configure(c); /* propagates border_width, if size doesn't change */
    updatewindowtype(c);
    updatesizehints(c);
    updatewmhints(c);
    XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    grabbuttons(c, 0);
    if (!c->isfloating) {
        c->isfloating = c->oldstate = trans != None || c->isfixed;
    }
    if (c->isfloating) {
        XRaiseWindow(dpy, c->win);
    }
    attachaside(c);
    attachstack(c);
    XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
    XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, (unsigned)c->w, (unsigned)c->h); /* some windows require this */
    setclientstate(c, NormalState);
    if (c->mon == selmon) {
        unfocus(selmon->sel, 0);
    }
    c->mon->sel = c;
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    if (term) {
        swallow(term, c);
    }
    focus(NULL);
}

void mappingnotify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard) {
        grabkeys();
    }
}

void maprequest(XEvent *e) {
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect) {
        return;
    }
    if (!wintoclient(ev->window)) {
        manage(ev->window, &wa);
    }
}

void monocle(Monitor *m) {
    unsigned int n = 0;
    Client *c;

    for (c = m->clients; c; c = c->next) {
        if (ISVISIBLE(c)) {
            n++;
        }
    }
    if (n > 0) { /* override layout symbol */
        snprintf(m->layoutSymbol, sizeof m->layoutSymbol, "[%d]", n);
    }
    for (c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
        resize(c, m->window_x, m->window_y, m->window_width - 2 * c->bw, m->window_height - 2 * c->bw, 0);
    }
}

void motionnotify(XEvent *e) {
    static Monitor *mon = NULL;
    Monitor *m;
    XMotionEvent *ev = &e->xmotion;

    if (ev->window != root) {
        return;
    }
    if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
        unfocus(selmon->sel, 1);
        selmon = m;
        focus(NULL);
    }
    mon = m;
}

void movemouse(Arg const &arg) {
    (void)arg;
    int x;
    int y;
    int ocx;
    int ocy;
    int nx;
    int ny;
    Client *c;
    Monitor *m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel)) {
        return;
    }
    if (c->isfullscreen) { /* no support moving fullscreen windows by mouse */
        return;
    }
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurMove]->cursor, CurrentTime)
        != GrabSuccess) {
        return;
    }
    if (!getrootptr(&x, &y)) {
        return;
    }
    do {
        XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
        switch (ev.type) {
            case ConfigureRequest:
            case Expose:
            case MapRequest: handler[(size_t)ev.type](&ev); break;
            case MotionNotify:
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
                    continue;
                }
                lasttime = ev.xmotion.time;

                nx = ocx + (ev.xmotion.x - x);
                ny = ocy + (ev.xmotion.y - y);
                if ((unsigned)abs(selmon->window_x - nx) < snap) {
                    nx = selmon->window_x;
                } else if (((unsigned)(selmon->window_x + selmon->window_width) - ((unsigned)nx + WIDTH(c))) < snap) {
                    nx = (int)((unsigned)(selmon->window_x + selmon->window_width) - WIDTH(c));
                }
                if ((unsigned)abs(selmon->window_y - ny) < snap) {
                    ny = selmon->window_y;
                } else if (((unsigned)(selmon->window_y + selmon->window_height) - ((unsigned)ny + HEIGHT(c))) < snap) {
                    ny = (int)((unsigned)(selmon->window_y + selmon->window_height) - HEIGHT(c));
                }
                if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
                    && ((unsigned)abs(nx - c->x) > snap || (unsigned)abs(ny - c->y) > snap)) {
                    togglefloating({});
                }
                if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
                    resize(c, nx, ny, c->w, c->h, 1);
                }
                break;
            default: lg::warn("Unexpected event type {} in movemouse", ev.type); break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(dpy, CurrentTime);
    if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

Client *nexttagged(Client *c) {
    Client *walked = c->mon->clients;
    for (; walked && (walked->isfloating || !ISVISIBLEONTAG(walked, c->tags)); walked = walked->next) {
        ;
    }
    return walked;
}

Client *nexttiled(Client *c) {
    for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next) {
        ;
    }
    return c;
}

void pop(Client *c) {
    detach(c);
    attach(c);
    focus(c);
    arrange(c->mon);
}

static void print_event_stats(void) {
    static long calls = 0;
    static timespec last_print {};


    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (last_print.tv_sec == 0 && last_print.tv_nsec == 0) last_print = now;

    calls++;

    if (timespecdiff(&now, &last_print) < 1.0) return;

    lg::debug("{} events/s", calls);
    calls = 0;
    last_print = now;
}

void propertynotify(XEvent *e) {
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
        updatestatus();
    } else if (ev->state == PropertyDelete) {
        return; /* ignore */
    } else if ((c = wintoclient(ev->window))) {
        switch (ev->atom) {
            default: break;
            case XA_WM_TRANSIENT_FOR:
                if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans))
                    && (c->isfloating = (wintoclient(trans)) != NULL)) {
                    arrange(c->mon);
                }
                break;
            case XA_WM_NORMAL_HINTS: c->hintsvalid = false; break;
            case XA_WM_HINTS:
                updatewmhints(c);
                drawbars();
                break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
            updatetitle(c);
            if (c == c->mon->sel) {
                drawbar(c->mon);
            }
        }
        if (ev->atom == netatom[NetWMWindowType]) {
            updatewindowtype(c);
        }
    }
}

void quit(Arg const &arg) {
    (void)arg;
    running = 0;
    need_restart = 0;
    lg::info("Initiating shutdowd");
}

void restart(Arg const &arg) {
    (void)arg;
    running = 0;
    need_restart = 1;
}

Monitor *recttomon(int x, int y, int w, int h) {
    Monitor *m;
    Monitor *r = selmon;
    int a;
    int area = 0;

    for (m = mons; m; m = m->next) {
        if ((a = INTERSECT(x, y, w, h, m)) > area) {
            area = a;
            r = m;
        }
    }
    return r;
}

void resize(Client *c, int x, int y, int w, int h, int interact) {
    if (applysizehints(c, &x, &y, &w, &h, interact)) resizeclient(c, x, y, w, h);
}

void resizeclient(Client *c, int x, int y, int w, int h) {
    XWindowChanges wc;
    unsigned int n;
    unsigned int gapoffset;
    unsigned int gapincr;
    Client *nbc;
    Monitor *mon = c->mon;

    wc.border_width = c->bw;

    /* Get number of clients for the selected monitor */
    for (n = 0, nbc = nexttiled(mon->clients); nbc; nbc = nexttiled(nbc->next), n++) { }

    /* Do nothing if layout is floating */
    if (c->isfloating || mon->lt[mon->sellt]->arrange == NULL) {
        gapincr = gapoffset = 0;
    } else {
        /* Remove border and gap if layout is monocle or only one client */
        if (mon->lt[mon->sellt]->arrange == monocle || n == 1) {
            gapoffset = 0;
            gapincr = -2u * borderpx;
            wc.border_width = 0;
        } else {
            gapoffset = gappx;
            gapincr = 2 * gappx;
        }
    }

    c->oldx = c->x;
    c->x = wc.x = (int)((unsigned)x + gapoffset);
    c->oldy = c->y;
    c->y = wc.y = (int)((unsigned)y + gapoffset);
    c->oldw = c->w;
    c->w = wc.width = (int)((unsigned)w - gapincr);
    c->oldh = c->h;
    c->h = wc.height = (int)((unsigned)h - gapincr);

    XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
    configure(c);
    XSync(dpy, False);
}

void resizemouse(Arg const &arg) {
    (void)arg;
    int ocx;
    int ocy;
    int nw;
    int nh;
    Client *c;
    Monitor *m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel)) {
        return;
    }
    if (c->isfullscreen) { /* no support resizing fullscreen windows by mouse */
        return;
    }
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurResize]->cursor, CurrentTime)
        != GrabSuccess) {
        return;
    }
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    do {
        XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
        switch (ev.type) {
            case ConfigureRequest:
            case Expose:
            case MapRequest: handler[(size_t)ev.type](&ev); break;
            case MotionNotify:
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
                    continue;
                }
                lasttime = ev.xmotion.time;

                nw = std::max(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
                nh = std::max(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
                if (c->mon->window_x + nw >= selmon->window_x
                    && c->mon->window_x + nw <= selmon->window_x + selmon->window_width
                    && c->mon->window_y + nh >= selmon->window_y
                    && c->mon->window_y + nh <= selmon->window_y + selmon->window_height) {
                    if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
                        && ((unsigned)abs(nw - c->w) > snap || (unsigned)abs(nh - c->h) > snap)) {
                        togglefloating({});
                    }
                }
                if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
                    resize(c, c->x, c->y, nw, nh, 1);
                }
                break;
            default: lg::warn("Unknown event type {} in resizemouse", ev.type); break;
        }
    } while (ev.type != ButtonRelease);
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {
        ;
    }
    if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

void restack(Monitor *m) {
    Client *c;
    XEvent ev;
    XWindowChanges wc;

    drawbar(m);
    if (!m->sel) {
        return;
    }
    if (m->sel->isfloating || !m->lt[m->sellt]->arrange) {
        XRaiseWindow(dpy, m->sel->win);
    }
    if (m->lt[m->sellt]->arrange) {
        wc.stack_mode = Below;
        wc.sibling = m->barwin;
        for (c = m->stack; c; c = c->snext) {
            if (!c->isfloating && ISVISIBLE(c)) {
                XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
                wc.sibling = c->win;
            }
        }
    }
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {
        ;
    }
}

void rotatestack(Arg const &arg) {
    Client *c = NULL;
    Client *f;

    if (!selmon->sel) {
        return;
    }
    f = selmon->sel;
    if (arg.i > 0) {
        for (c = nexttiled(selmon->clients); c && nexttiled(c->next); c = nexttiled(c->next)) {
            ;
        }
        if (c) {
            detach(c);
            attach(c);
            detachstack(c);
            attachstack(c);
        }
    } else {
        if ((c = nexttiled(selmon->clients))) {
            detach(c);
            enqueue(c);
            detachstack(c);
            enqueuestack(c);
        }
    }
    if (c) {
        arrange(selmon);
        // unfocus(f, 1);
        focus(f);
        restack(selmon);
    }
}

void run(void) {
    XEvent ev;
    /* main event loop */
    XSync(dpy, False);
    while (1) {
        if (!running) break;
        // Only handle self notify events if no X events need handling
        if (notified && XPending(dpy) == 0) {
            if (self_notify_handler[(size_t)notified]) self_notify_handler[(size_t)notified]();
        } else {
            if (XNextEvent(dpy, &ev)) break;
            if (handler[(size_t)ev.type]) handler[(size_t)ev.type](&ev); /* call handler */
        }
        IF_EVENT_TRACE {
            print_event_stats();
        }
    }
}

void scan(void) {
    unsigned int i;
    unsigned int num;
    Window d1;
    Window d2;
    Window *wins = NULL;
    XWindowAttributes wa;

    if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect
                || XGetTransientForHint(dpy, wins[i], &d1)) {
                continue;
            }
            if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState) {
                manage(wins[i], &wa);
            }
        }
        for (i = 0; i < num; i++) { /* now the transients */
            if (!XGetWindowAttributes(dpy, wins[i], &wa)) {
                continue;
            }
            if (XGetTransientForHint(dpy, wins[i], &d1)
                && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)) {
                manage(wins[i], &wa);
            }
        }
        if (wins) {
            XFree(wins);
        }
    }
}

void handle_notifyself_fade_anim(void) {
    notified = SelfNotifyNone;
    drawprogress(PROGRESS_FADE);
    struct timespec requested_time = {.tv_sec = 0, .tv_nsec = (long)((1.0 / 60.0) * 1e9)};
    nanosleep(&requested_time, NULL);
}

void notifyself(int type) {
    notified = type;
}

void sendmon(Client *c, Monitor *m) {
    if (c->mon == m) return;

    unfocus(c, 1);
    detach(c);
    detachstack(c);
    c->mon = m;
    c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
    attachaside(c);
    attachstack(c);
    focus(NULL);
    arrange(NULL);
    if (c->switchtotag) {
        c->switchtotag = 0;
    }
}

void setclientstate(Client *c, long state) {
    long data[] = {state, None};

    XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32, PropModeReplace, (unsigned char *)data, 2);
}

int sendevent(Client *c, Atom proto) {
    int n;
    Atom *protocols;
    int exists = 0;
    XEvent ev;

    if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
        while (!exists && n--) {
            exists = protocols[n] == proto;
        }
        XFree(protocols);
    }
    if (exists) {
        ev.type = ClientMessage;
        ev.xclient.window = c->win;
        ev.xclient.message_type = wmatom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = (long)proto;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, c->win, False, NoEventMask, &ev);
    }
    return exists;
}

void setfocus(Client *c) {
    if (!c->neverfocus) {
        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
        XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->win), 1);
    }
    sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client *c, int fullscreen) {
    if (fullscreen && !c->isfullscreen) {
        XChangeProperty(dpy,
            c->win,
            netatom[NetWMState],
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *)&netatom[NetWMFullscreen],
            1);
        c->isfullscreen = 1;
        c->oldstate = c->isfloating;
        c->oldbw = c->bw;
        c->bw = 0;
        c->isfloating = 1;
        resizeclient(c, c->mon->monitor_x, c->mon->monitor_y, c->mon->monitor_width, c->mon->monitor_height);
        XRaiseWindow(dpy, c->win);
    } else if (!fullscreen && c->isfullscreen) {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)0, 0);
        c->isfullscreen = 0;
        c->isfloating = c->oldstate;
        c->bw = c->oldbw;
        c->x = c->oldx;
        c->y = c->oldy;
        c->w = c->oldw;
        c->h = c->oldh;
        resizeclient(c, c->x, c->y, c->w, c->h);
        arrange(c->mon);
    }
}

void setlayout(Arg const &arg) {
    if (!arg.v || arg.v != selmon->lt[selmon->sellt]) {
        selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
    }
    if (arg.v) {
        selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg.v;
    }
    strncpy(selmon->layoutSymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->layoutSymbol);
    if (selmon->sel) {
        arrange(selmon);
    } else {
        drawbar(selmon);
    }
}

void setcfact(Arg const &arg) {
    float f;
    Client *c;

    c = selmon->sel;

    if (!c || !selmon->lt[selmon->sellt]->arrange) {
        return;
    }
    f = arg.f + c->cfact;
    if (arg.f == 0.0f) {
        f = 1.0;
    } else if (f < 0.25f || f > 4.0f) {
        return;
    }
    c->cfact = f;
    arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(Arg const &arg) {
    float f;

    if (!selmon->lt[selmon->sellt]->arrange) {
        return;
    }
    f = arg.f < 1.0f ? arg.f + selmon->mfact : arg.f - 1.0f;
    if (f < 0.05f || f > 0.95f) {
        return;
    }
    if (f == 0.0f) {
        f = 1.0f;
    }
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
    arrange(selmon);
}

void resetmcfact(Arg const &unused) {
    (void)unused;
    if (!selmon->lt[selmon->sellt]->arrange) return;

    selmon->sel->cfact = 1.0;
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = 0.5;
    arrange(selmon);
}

void setup(void) {
    XSetWindowAttributes wa;
    Atom utf8string;
    struct sigaction sa;

    /* do not transform children into zombies when they terminate */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, NULL);

    /* clean up any zombies (inherited from .xinitrc etc) immediately */
    while (waitpid(-1, NULL, WNOHANG)) { }

    /*Set up logging*/
    log_dir = lg::getLogDir();
    if (log_dir) {
        auto log_file_name = *log_dir / "dwm.log";
        lg::log_file = fopen(log_file_name.c_str(), "a");
        if (!lg::log_file) lg::fatal("could not open log file: {}", std::strerror(errno));
    } else {
        lg::fatal("Could not obtain log dir");
    }


    /* init screen */
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    drw = drw_create(dpy, screen, root, (unsigned)sw, (unsigned)sh);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts))) {
        lg::fatal("no fonts could be loaded.");
    }

#ifdef XBACKLIGHT
    if (bright_setup(NULL, bright_steps, bright_time))
#else
    if (bright_setup(bright_file ? bright_file : get_bright_file(), 0, 0))
#endif  // XBACKLIGHT
    {
        lg::fatal("backlight setup failed");
    }

#ifdef ASOUND
    if (!(volc = volc_init(VOLC_ALL_DEFULTS))) lg::fatal("volc setup failed");

#endif /* ASOUND */

    lrpad = (int)drw->fonts->h;
    bar_height = (int)drw->fonts->h + 2;
    updategeom();
    /* init atoms */
    utf8string = XInternAtom(dpy, "UTF8_STRING", False);
    wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
    wmatom[WMChangeState] = XInternAtom(dpy, "WM_CHANGE_STATE", False);
    wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
    netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
    netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
    netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    netatom[NetWMIcon] = XInternAtom(dpy, "_NET_WM_ICON", False);

    /* init cursors */
    cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
    cursor[CurResize] = drw_cur_create(drw, XC_sizing);
    cursor[CurMove] = drw_cur_create(drw, XC_fleur);
    /* init appearance */
    scheme = new Clr *[LENGTH(colors)];
    for (size_t i = 0; i < LENGTH(colors); i++) {
        scheme[i] = drw_scm_create(drw, colors[i]);
    }

    {
        // In multimonitor setups with Xinerama, the value of `sh` becomes very
        // big as all monitors are treated as a single screen
        int avg = avgheight();
        borderpx = (unsigned)avg / 540;
        gappx = (unsigned)avg / 180;
        snap = (unsigned)avg / 67;
    }

    /* init bars */
    updatebars();
    updatestatus();
    /* supporting window for NetWMCheck */
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *)"dwm", 3);
    XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    /* EWMH support per view */
    XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)netatom, NetLast);
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    /* select events */
    wa.cursor = cursor[CurNormal]->cursor;
    wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | PointerMotionMask
                  | EnterWindowMask | LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    grabkeys();
    focus(NULL);
}

void seturgent(Client *c, int urg) {
    XWMHints *wmh;

    c->isurgent = urg;
    if (!(wmh = XGetWMHints(dpy, c->win))) {
        return;
    }
    wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
    XSetWMHints(dpy, c->win, wmh);
    XFree(wmh);
}

void showhide(Client *c) {
    if (!c) {
        return;
    }
    if (ISVISIBLE(c)) {
        /* show clients top down */
        XMoveWindow(dpy, c->win, c->x, c->y);
        if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen) {
            resize(c, c->x, c->y, c->w, c->h, 0);
        }
        showhide(c->snext);
    } else {
        /* hide clients bottom up */
        showhide(c->snext);
        XMoveWindow(dpy, c->win, (int)(WIDTH(c) * -2u), c->y);
    }
}

void redirectChildLog(char **argv) {
    if (!log_dir) return;
    auto file_name = *log_dir / argv[0];
    file_name.replace_extension(".log");

    int child_fd = open(file_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (child_fd < 0) {
        lg::warn("Could not set up logging for child processes {}: {}", argv[0], strerror(errno));
        return;
    }
    char const div[] = "________________________________________________________________________________\n";
    if (write(child_fd, div, sizeof(div)) < 0) {
        close(child_fd);
        lg::warn("Could not write to child log file {}: {}", file_name.c_str(), strerror(errno));
        goto exit;
    }

    if (dup2(child_fd, STDOUT_FILENO) < 0) {
        lg::warn("Could not redirect child stdout to log file {}: {}", file_name.c_str(), strerror(errno));
        goto exit;
    }
    if (dup2(child_fd, STDERR_FILENO) < 0) {
        lg::warn("Could not redirect child stdout to log file {}: {}", file_name.c_str(), strerror(errno));
        goto exit;
    }
exit:
    close(child_fd);
}

void spawn(Arg const &arg) {
    struct sigaction sa;
    if (arg.v == dmenucmd) dmenumon[0] = (char)('0' + selmon->num);

    if (fork() == 0) {
        if (dpy) {
            close(ConnectionNumber(dpy));
        }
        redirectChildLog((char **)arg.v);
        setsid();
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = SIG_DFL;
        sigaction(SIGCHLD, &sa, NULL);
        execvp(((char **)arg.v)[0], (char **)arg.v);
        lg::fatal("failed to spawn {}:", ((char **)arg.v)[0]);
    }
}

void tag(Arg const &arg) {
    if (selmon->sel && arg.ui & TAGMASK) {
        selmon->sel->tags = arg.ui & TAGMASK;
        if (selmon->sel->switchtotag) {
            selmon->sel->switchtotag = 0;
        }
        focus(NULL);
        arrange(selmon);
    }
}

void tagmon(Arg const &arg) {
    if (!selmon->sel || !mons->next) {
        return;
    }
    sendmon(selmon->sel, dirtomon(arg.i));
}

void tile(Monitor *m) {
    unsigned int i;
    unsigned int n;
    unsigned int h;
    unsigned int mw;
    unsigned int my;
    unsigned int ty;
    float mfacts = 0;
    float sfacts = 0;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
        if (n < (unsigned)m->nmaster)
            mfacts += c->cfact;
        else
            sfacts += c->cfact;

    if (n == 0) return;


    if (n > (unsigned)m->nmaster)
        mw = m->nmaster ? (unsigned)((float)m->window_width * m->mfact) : 0;
    else
        mw = (unsigned)m->window_width;

    for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < (unsigned)m->nmaster) {
            h = (unsigned)((float)((unsigned)m->window_height - my) * (c->cfact / mfacts));
            resize(c,
                m->window_x,
                (int)((unsigned)m->window_y + my),
                (int)(mw - (unsigned)(2 * c->bw)),
                (int)(h - (2 * (unsigned)c->bw)),
                0);
            // TODO(dk949): This is a guard against creating too many clients.
            //              Do something if there's too many clients!
            if (my + HEIGHT(c) < (unsigned)m->window_height) {
                my += HEIGHT(c);
                mfacts -= c->cfact;
            }
        } else {
            h = (unsigned)((float)((unsigned)m->window_height - ty) * (c->cfact / sfacts));
            resize(c,
                (int)((unsigned)m->window_x + mw),
                (int)((unsigned)m->window_y + ty),
                (int)((unsigned)m->window_width - mw - (2 * (unsigned)c->bw)),
                (int)(h - (2 * (unsigned)c->bw)),
                0);
            if (ty + HEIGHT(c) < (unsigned)m->window_height) {
                ty += HEIGHT(c);
                sfacts -= c->cfact;
            }
        }
    }
}

double timespecdiff(const struct timespec *a, const struct timespec *b) {
    double a_sec = (double)a->tv_sec + ((double)a->tv_nsec * 1e-9);
    double b_sec = (double)b->tv_sec + ((double)b->tv_nsec * 1e-9);
    double diff = a_sec - b_sec;
    return (diff >= 0) ? diff : -diff;
}

void togglebar(Arg const &arg) {
    (void)arg;
    selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
    updatebarpos(selmon);
    XMoveResizeWindow(dpy,
        selmon->barwin,
        selmon->window_x,
        selmon->bar_y,
        (unsigned)selmon->window_width,
        (unsigned)bar_height);
    arrange(selmon);
}

void togglefloating(Arg const &arg) {
    (void)arg;
    if (!selmon->sel) return;

    if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
        return;

    selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
    if (selmon->sel->isfloating) {
        resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w, selmon->sel->h, 0);
    }
    arrange(selmon);
}

void togglefs(Arg const &arg) {
    (void)arg;
    if (!selmon->sel) return;
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

void toggletag(Arg const &arg) {
    unsigned int newtags;

    if (!selmon->sel) {
        return;
    }
    newtags = selmon->sel->tags ^ (arg.ui & TAGMASK);
    if (newtags) {
        selmon->sel->tags = newtags;
        focus(NULL);
        arrange(selmon);
    }
}

void toggleview(Arg const &arg) {
    unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg.ui & TAGMASK);
    int i;

    if (newtagset) {
        selmon->tagset[selmon->seltags] = newtagset;

        if (newtagset == ~0u) {
            selmon->pertag->prevtag = selmon->pertag->curtag;
            selmon->pertag->curtag = 0;
        }

        /* test if the user did not select the same tag */
        if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
            selmon->pertag->prevtag = selmon->pertag->curtag;
            for (i = 0; !(newtagset & 1 << i); i++) {
                ;
            }
            selmon->pertag->curtag = (unsigned)(i + 1);
        }

        /* apply settings for this view */
        selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
        selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
        selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
        selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
        selmon->lt[selmon->sellt ^ 1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

        if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag]) {
            togglebar({});
        }

        focus(NULL);
        arrange(selmon);
    }
}

void unfocus(Client *c, int setfocus) {
    if (!c) return;

    grabbuttons(c, 0);
    XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
    if (setfocus) {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
}

static void uniconifyclient(Client *c) {
    lg::debug("restoring iconified cliend {}", c->name);
    updatetitle(c);
    updatesizehints(c);
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    XMoveResizeWindow(dpy, c->win, c->x, c->y, (unsigned)c->w, (unsigned)c->h);
    configure(c);
    setclientstate(c, NormalState);
    attachstack(c);
    attach(c);
}

void unmanage(Client *c, int destroyed) {
    Monitor *m = c->mon;
    unsigned int switchtotag = c->switchtotag;
    XWindowChanges wc;

    if (c->swallowing) {
        unswallow(c);
        return;
    }

    Client *s = swallowingclient(c->win);
    if (s) {
        delete s->swallowing;
        s->swallowing = NULL;
        arrange(m);
        focus(NULL);
        return;
    }

    detach(c);
    detachstack(c);
    if (!destroyed) {
        wc.border_width = c->oldbw;
        XGrabServer(dpy); /* avoid race conditions */
        XSetErrorHandler(xerrordummy);
        XSelectInput(dpy, c->win, NoEventMask);
        XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        setclientstate(c, WithdrawnState);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
    delete c;
    if (!s) {
        arrange(m);
        focus(NULL);
        updateclientlist();
        if (switchtotag) {
            view(Arg {.ui = switchtotag});
        }
    }

    /*if (c->switchtotag) {*/
    /*Arg a = { .ui = c->switchtotag };*/
    /*view(&a);*/
    /*}*/
}

void unmapnotify(XEvent *e) {
    Client *c;
    XUnmapEvent *ev = &e->xunmap;

    if ((c = wintoclient(ev->window))) {
        if (ev->send_event) {
            setclientstate(c, WithdrawnState);
        } else {
            unmanage(c, 0);
        }
    }
}

void updatebars(void) {
    Monitor *m;
    XSetWindowAttributes wa = {
        .background_pixmap = ParentRelative,
        .background_pixel = 0,
        .border_pixmap = 0,
        .border_pixel = 0,
        .bit_gravity = 0,
        .win_gravity = 0,
        .backing_store = 0,
        .backing_planes = 0,
        .backing_pixel = 0,
        .save_under = 0,
        .event_mask = ButtonPressMask | ExposureMask,
        .do_not_propagate_mask = 0,
        .override_redirect = True,
        .colormap = 0,
        .cursor = 0,
    };



    char dwm_class_name[] = "dwm";
    XClassHint ch = {dwm_class_name, dwm_class_name};
    for (m = mons; m; m = m->next) {
        if (m->barwin) {
            continue;
        }
        m->barwin = XCreateWindow(dpy,
            root,
            m->window_x,
            m->bar_y,
            (unsigned)m->window_width,
            (unsigned)bar_height,
            0,
            DefaultDepth(dpy, screen),
            CopyFromParent,
            DefaultVisual(dpy, screen),
            CWOverrideRedirect | CWBackPixmap | CWEventMask,
            &wa);
        XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
        XMapRaised(dpy, m->barwin);
        XSetClassHint(dpy, m->barwin, &ch);
    }
}

void updatebarpos(Monitor *m) {
    m->window_y = m->monitor_y;
    m->window_height = m->monitor_height;
    if (m->showbar) {
        m->window_height -= bar_height;
        m->bar_y = m->topbar ? m->window_y : m->window_y + m->window_height;
        m->window_y = m->topbar ? m->window_y + bar_height : m->window_y;
    } else {
        m->bar_y = -bar_height;
    }
}

void updateclientlist(void) {
    Client *c;
    Monitor *m;

    XDeleteProperty(dpy, root, netatom[NetClientList]);
    for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) {
            XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
        }
    }
}

int updategeom(void) {
    int dirty = 0;

#ifdef XINERAMA
    if (XineramaIsActive(dpy)) {
        int i;
        int j;
        int n;
        int nn;
        Client *c;
        Monitor *m;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);

        for (n = 0, m = mons; m; m = m->next, n++) {
            ;
        }
        /* only consider unique geometries as separate screens */
        XineramaScreenInfo *unique = new XineramaScreenInfo[(size_t)nn];
        for (i = 0, j = 0; i < nn; i++) {
            if (isuniquegeom(unique, (size_t)j, &info[i])) {
                memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
            }
        }
        XFree(info);
        nn = j;
        /* new monitors available */
        for (i = n; i < nn; i++) {
            for (m = mons; m && m->next; m = m->next) {
                ;
            }
            if (m) {
                m->next = createmon();
            } else {
                mons = createmon();
            }
        }
        for (i = 0, m = mons; i < nn && m; m = m->next, i++) {
            if (i >= n || unique[i].x_org != m->monitor_x || unique[i].y_org != m->monitor_y
                || unique[i].width != m->monitor_width || unique[i].height != m->monitor_height) {
                dirty = 1;
                m->num = i;
                m->monitor_x = m->window_x = unique[i].x_org;
                m->monitor_y = m->window_y = unique[i].y_org;
                m->monitor_width = m->window_width = unique[i].width;
                m->monitor_height = m->window_height = unique[i].height;
                updatebarpos(m);
            }
        }
        /* less monitors available nn < n */
        for (i = nn; i < n; i++) {
            for (m = mons; m && m->next; m = m->next) {
                ;
            }
            while ((c = m->clients)) {
                dirty = 1;
                m->clients = c->next;
                detachstack(c);
                c->mon = mons;
                attach(c);
                attachaside(c);
                attachstack(c);
            }
            if (m == selmon) {
                selmon = mons;
            }
            cleanupmon(m);
        }
        delete[] unique;
    } else
#endif /* XINERAMA */
    {  /* default monitor setup */
        if (!mons) {
            mons = createmon();
        }
        if (mons->monitor_width != sw || mons->monitor_height != sh) {
            dirty = 1;
            mons->monitor_width = mons->window_width = sw;
            mons->monitor_height = mons->window_height = sh;
            updatebarpos(mons);
        }
    }
    if (dirty) {
        selmon = mons;
        selmon = wintomon(root);
    }
    return dirty;
}

void updatenumlockmask(void) {
    unsigned int i;
    unsigned int j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < (unsigned)modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * (unsigned)modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock)) {
                numlockmask = (1 << i);
            }
        }
    }
    XFreeModifiermap(modmap);
}

void updatesizehints(Client *c) {
    long msize;
    XSizeHints size;

    if (!XGetWMNormalHints(dpy, c->win, &size, &msize)) {
        /* size is uninitialized, ensure that size.flags aren't used */
        size.flags = PSize;
    }
    if (size.flags & PBaseSize) {
        c->basew = size.base_width;
        c->baseh = size.base_height;
    } else if (size.flags & PMinSize) {
        c->basew = size.min_width;
        c->baseh = size.min_height;
    } else {
        c->basew = c->baseh = 0;
    }
    if (size.flags & PResizeInc) {
        c->incw = size.width_inc;
        c->inch = size.height_inc;
    } else {
        c->incw = c->inch = 0;
    }
    if (size.flags & PMaxSize) {
        c->maxw = size.max_width;
        c->maxh = size.max_height;
    } else {
        c->maxw = c->maxh = 0;
    }
    if (size.flags & PMinSize) {
        c->minw = size.min_width;
        c->minh = size.min_height;
    } else if (size.flags & PBaseSize) {
        c->minw = size.base_width;
        c->minh = size.base_height;
    } else {
        c->minw = c->minh = 0;
    }
    if (size.flags & PAspect) {
        c->mina = (float)size.min_aspect.y / (float)size.min_aspect.x;
        c->maxa = (float)size.max_aspect.x / (float)size.max_aspect.y;
    } else {
        c->maxa = c->mina = 0.0;
    }
    c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
    c->hintsvalid = true;
}

void updatestatus(void) {
    if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext))) {
        strcpy(stext, "dwm-" dwm_version);
    }
    drawbar(selmon);
}

void updatetitle(Client *c) {
    if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name)) {
        gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
    }
    if (c->name[0] == '\0') { /* hack to mark broken clients */
        strcpy(c->name, broken);
    }
}

void updatewindowtype(Client *c) {
    Atom state = getatomprop(c, netatom[NetWMState]);
    Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

    if (state == netatom[NetWMFullscreen]) {
        setfullscreen(c, 1);
    }
    if (wtype == netatom[NetWMWindowTypeDialog]) {
        c->isfloating = 1;
    }
}

void updatewmhints(Client *c) {
    XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, c->win))) {
        if (c == selmon->sel && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->win, wmh);
        } else {
            c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
        }
        if (wmh->flags & InputHint) {
            c->neverfocus = !wmh->input;
        } else {
            c->neverfocus = 0;
        }
        XFree(wmh);
    }
}

void view(Arg const &arg) {
    int i;
    unsigned int tmptag;

    if ((arg.ui & TAGMASK) == selmon->tagset[selmon->seltags]) {
        return;
    }
    selmon->seltags ^= 1; /* toggle sel tagset */
    if (arg.ui & TAGMASK) {
        selmon->tagset[selmon->seltags] = arg.ui & TAGMASK;
        selmon->pertag->prevtag = selmon->pertag->curtag;

        if (arg.ui == ~0u) {
            selmon->pertag->curtag = 0;
        } else {
            for (i = 0; !(arg.ui & 1 << i); i++) {
                ;
            }
            selmon->pertag->curtag = (unsigned)(i + 1);
        }
    } else {
        tmptag = selmon->pertag->prevtag;
        selmon->pertag->prevtag = selmon->pertag->curtag;
        selmon->pertag->curtag = tmptag;
    }

    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt ^ 1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag]) {
        togglebar({});
    }

    focus(NULL);
    arrange(selmon);
}

#ifdef ASOUND
void volumechange(Arg const &arg) {
    volc_volume_state_t state;
    if (arg.i == VOL_MT) {
        state = volc_volume_ctl(volc, VOLC_ALL_CHANNELS, VOLC_SAME, VOLC_CHAN_TOGGLE);
    } else {
        state = volc_volume_ctl(volc, VOLC_ALL_CHANNELS, VOLC_INC((float)arg.i), VOLC_CHAN_ON);
    }

    if (state.err < 0) return;

    drawprogress(100,
        (unsigned long long)state.state.volume,
        state.state.switch_pos ? SchemeInfoProgress : SchemeOffProgress);
}
#endif


pid_t winpid(Window w) {
    pid_t result = 0;

    xcb_res_client_id_spec_t spec {};
    spec.client = (uint32_t)w;
    spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

    xcb_generic_error_t *e = NULL;
    xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
    xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

    if (!r) return 0;


    xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
    for (; i.rem; xcb_res_client_id_value_next(&i)) {
        spec = i.data->spec;
        if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
            uint32_t *t = xcb_res_client_id_value_value(i.data);
            result = (pid_t)*t;
            break;
        }
    }

    free(r);

    if (result == -1) result = 0;

    return result;
}

pid_t getparentprocess(pid_t p) {
    unsigned int v = 0;

#ifdef __linux__
    FILE *f;
    char buf[256];
    snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

    if (!(f = fopen(buf, "r"))) {
        lg::warn("failed to open stat file {} for process {}: {}", buf, p, strerror(errno));
        return 0;
    }

    int res = fscanf(f, "%*u %*s %*c %u", &v);
    fclose(f);
    if (res != 1) {
        lg::warn("failed to get child process of {}: {}", p, strerror(errno));
        return 0;
    }
#endif /* __linux__ */

    return (pid_t)v;
}

static uint32_t *geticon(Client *c, unsigned long *size) {
    /*
    It also  returns a value to bytes_after_return and nitems_return, by defining the following values:
     N = actual length of the stored property in bytes (even if the format is 16 or 32)
     Offs = 4 * offset
     T = N - Offs
     ButesToReturn = std::minIMUM(T, 4 * long_length)
     bytes_left = N - (Offs + BytesToReturn)

    The  returned  value starts at byte index Offs in the property (indexing from zero), and its length in bytes is L.
    If the value for long_offset causes L to be negative, a BadValue error results.  The value of bytes_after_return
    is A, giving the number of trailing unread bytes in the stored property.

       */
    long offset = 0, length = 0;
    Bool delete_ = False;
    Atom req_type = XA_CARDINAL;
    Atom actual_type;
    int format;
    unsigned long nitems, bytes_left;
    unsigned char *data;
    XGetWindowProperty(dpy,
        c->win,
        netatom[NetWMIcon],
        offset,
        length,
        delete_,
        req_type,
        &actual_type,
        &format,
        &nitems,
        &bytes_left,
        &data);
    if (format != 32) lg::debug("wrong format: {}", format);
    if (req_type != actual_type) lg::debug("wrong type:  expected {} got {}", req_type, actual_type);
    lg::debug("nitems = {}, bytes_left = {}", nitems, bytes_left);
    *size = (unsigned long)(length = (long)bytes_left);
    XGetWindowProperty(dpy,
        c->win,
        netatom[NetWMIcon],
        offset,
        length,
        delete_,
        req_type,
        &actual_type,
        &format,
        &nitems,
        &bytes_left,
        &data);
    {
        uint32_t *begin = (uint32_t *)(void *)data;
        uint32_t *end = (uint32_t *)(void *)((uint8_t *)data + *size);
        int pos = 0;
        for (uint32_t *it = begin; it != end; ++it, pos++) {
            if (pos % 2) continue;
            begin[pos / 2] = *it;
        }
    }
    return (uint32_t *)(void *)data;
}

static __attribute_maybe_unused__ void dump_raw(uint8_t *data, size_t size, char const *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        lg::debug("Could not open file {} for writing", path);
        return;
    }

    fwrite(data, sizeof(data[0]), size, fp);

    fclose(fp);
}

static void iconifyclient(Client *c) {
    char *icon_name;
    XGetIconName(dpy, c->win, &icon_name);
    lg::debug("{} wants to iconify. Icon name: {}", c->name, icon_name);
    XFree(icon_name);

    detach(c);
    detachstack(c);

    setclientstate(c, IconicState);
    XUnmapWindow(dpy, c->win);

    arrange(c->mon);
    updateclientlist();
    unsigned long size;
    uint32_t *icon;
    if ((icon = geticon(c, &size))) {
        lg::debug("icon is {}x{}, {} bytes", (int)icon[0], (int)icon[1], size);
        XFree(icon);
    } else {
        lg::debug("No icon for client {}", c->name);
    }

    delay(1000000 * 5, (void (*)(void *))uniconifyclient, c);
}

int isdescprocess(pid_t p, pid_t c) {
    while (p != c && c != 0) {
        c = getparentprocess(c);
    }

    return (int)c;
}

Client *termforwin(Client const *w) {
    if (!w->pid || w->isterminal) {
        return NULL;
    }

    Client *out = NULL;
    for (Monitor *m = mons; m; m = m->next) {
        for (Client *c = m->clients; c; c = c->next) {
            if (c->isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid)) {
                if (selmon->sel == c) return c;
                out = c;
            }
        }
    }

    return out;
}

Client *swallowingclient(Window w) {
    Client *c;
    Monitor *m;

    for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) {
            if (c->swallowing && c->swallowing->win == w) {
                return c;
            }
        }
    }

    return NULL;
}

Client *wintoclient(Window w) {
    Client *c;
    Monitor *m;

    for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) {
            if (c->win == w) {
                return c;
            }
        }
    }
    return NULL;
}

Monitor *wintomon(Window w) {
    int x;
    int y;
    Client *c;
    Monitor *m;

    if (w == root && getrootptr(&x, &y)) {
        return recttomon(x, y, 1, 1);
    }
    for (m = mons; m; m = m->next) {
        if (w == m->barwin) {
            return m;
        }
    }
    if ((c = wintoclient(w))) {
        return c->mon;
    }
    return selmon;
}

static __attribute_used__ void wmchange(Client *c, XClientMessageEvent *cme) {
    if (cme->format != 32 || cme->data.l[0] != IconicState)
        // Only handling iconification
        return;

    iconifyclient(c);
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *d, XErrorEvent *ee) {
    if (ee->error_code == BadWindow || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
        || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
        || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
        || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
        || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
        || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
        || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
        || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable)) {
        return 0;
    }
    lg::warn("fatal error: request code={}, error code={}", ee->request_code, ee->error_code);
    return xerrorxlib(d, ee); /* may call exit */
}

int xerrordummy(Display *_dpy, XErrorEvent *_ee) {
    (void)_dpy, (void)_ee;
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *_dpy, XErrorEvent *_ee) {
    (void)_dpy, (void)_ee;
    lg::fatal("another window manager is already running");
    return -1;
}

void zoom(Arg const &arg) {
    (void)arg;
    Client *c = selmon->sel;

    if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating) return;

    if (c == nexttiled(selmon->clients) || !(c = nexttiled(c->next))) return;

    pop(c);
}

int main(int argc, char *argv[]) {
    if (argc == 2 && !strcmp("-v", argv[1])) {
        puts("dwm-" dwm_version);
        return 0;
    } else if (argc != 1) {
        fputs("usage: dwm [-v]", stderr);
        return 1;
    }
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
        lg::warn("no locale support");
    }
    if (!(dpy = XOpenDisplay(NULL))) {
        lg::fatal("cannot open display");
    }
    if (!(xcon = XGetXCBConnection(dpy))) {
        lg::fatal("cannot get xcb connection");
    }
    checkotherwm();
    setup();
#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec", NULL) == -1) die("pledge");
#endif /* __OpenBSD__ */
    scan();
    lg::info("Starting DWM");
    run();
    cleanup();
    XCloseDisplay(dpy);
    if (need_restart) {
        lg::info("Restarting dwm\n"
                 "________________________________________________________________________________\n");
        if (execvp(argv[0], argv)) lg::fatal("could not restart dwm:");
    }

    lg::info("Shutdown complete");
    return EXIT_SUCCESS;
}

void centeredmaster(Monitor *m) {
    unsigned int i;
    unsigned int n;
    unsigned int h;
    unsigned int mw;
    unsigned int mx;
    unsigned int my;
    unsigned int oty;
    unsigned int ety;
    unsigned int tw;
    Client *c;

    /* count number of clients in the selected monitor */
    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
        ;
    }
    if (n == 0) {
        return;
    }

    /* initialize areas */
    mw = (unsigned)m->window_width;
    mx = 0;
    my = 0;
    tw = mw;

    if (n > (unsigned)m->nmaster) {
        /* go mfact box in the center if more than nmaster clients */
        mw = m->nmaster ? (unsigned)((float)m->window_width * m->mfact) : 0;
        tw = (unsigned)m->window_width - mw;

        if (n - (unsigned)m->nmaster > 1) {
            /* only one client */
            mx = ((unsigned)m->window_width - mw) / 2;
            tw = ((unsigned)m->window_width - mw) / 2;
        }
    }

    oty = 0;
    ety = 0;
    for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < (unsigned)m->nmaster) {
            /* nmaster clients are stacked vertically, in the center
             * of the screen */
            h = ((unsigned)m->window_height - my) / (std::min(n, (unsigned)m->nmaster) - i);
            resize(c,
                (int)((unsigned)m->window_x + mx),
                (int)((unsigned)m->window_y + my),
                (int)(mw - (unsigned)(2 * c->bw)),
                (int)(h - (unsigned)(2 * c->bw)),
                0);
            // TODO(dk949): This is a guard against creating too many clients.
            //              Do something if there's too many clients!
            // TODO(dk949): make this cfact aware
            if (my + HEIGHT(c) < (unsigned)m->window_height) my += HEIGHT(c);
        } else {
            /* stack clients are stacked vertically */
            if ((i - (unsigned)m->nmaster) % 2) {
                h = ((unsigned)m->window_height - ety) / ((1 + n - i) / 2);
                resize(c,
                    m->window_x,
                    (int)((unsigned)m->window_y + ety),
                    (int)(tw - (unsigned)(2 * c->bw)),
                    (int)(h - (unsigned)(2 * c->bw)),
                    0);
                // TODO(dk949): This is a guard against creating too many clients.
                //              Do something if there's too many clients!
                // TODO(dk949): make this cfact aware
                if (ety + HEIGHT(c) < (unsigned)m->window_height) ety += HEIGHT(c);
            } else {
                h = ((unsigned)m->window_height - oty) / ((1 + n - i) / 2);
                resize(c,
                    (int)((unsigned)m->window_x + mx + mw),
                    (int)((unsigned)m->window_y + oty),
                    (int)(tw - (unsigned)(2 * c->bw)),
                    (int)(h - (unsigned)(2 * c->bw)),
                    0);
                if (oty + HEIGHT(c) < (unsigned)m->window_height) oty += HEIGHT(c);
            }
        }
    }
}

void centeredfloatingmaster(Monitor *m) {
    unsigned int i;
    unsigned int n;
    unsigned int w;
    unsigned int mh;
    unsigned int mw;
    unsigned int mx;
    unsigned int mxo;
    unsigned int my;
    unsigned int myo;
    unsigned int tx;
    Client *c;

    /* count number of clients in the selected monitor */
    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
        ;
    }
    if (n == 0) {
        return;
    }

    /* initialize nmaster area */
    if (n > (unsigned)m->nmaster) {
        /* go mfact box in the center if more than nmaster clients */
        if (m->window_width > m->window_height) {
            mw = m->nmaster ? (unsigned)((float)m->window_width * m->mfact) : 0;
            mh = m->nmaster ? (unsigned)(m->window_height * 0.9) : 0;
        } else {
            mh = m->nmaster ? (unsigned)((float)m->window_height * m->mfact) : 0;
            mw = m->nmaster ? (unsigned)(m->window_width * 0.9) : 0;
        }
        mx = mxo = ((unsigned)m->window_width - mw) / 2;
        my = myo = ((unsigned)m->window_height - mh) / 2;
    } else {
        /* go fullscreen if all clients are in the master area */
        mh = (unsigned)m->window_height;
        mw = (unsigned)m->window_width;
        mx = mxo = 0;
        my = myo = 0;
    }

    for (i = tx = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < (unsigned)m->nmaster) {
            /* nmaster clients are stacked horizontally, in the center
             * of the screen */
            w = (mw + mxo - mx) / (std::min(n, (unsigned)m->nmaster) - i);
            resize(c,
                (int)((unsigned)m->window_x + mx),
                (int)((unsigned)m->window_y + my),
                (int)(w - (unsigned)(2 * c->bw)),
                (int)(mh - (unsigned)(2 * c->bw)),
                0);
            mx += WIDTH(c);
        } else {
            /* stack clients are stacked horizontally */
            w = ((unsigned)m->window_width - tx) / (n - i);
            resize(c,
                (int)((unsigned)m->window_x + tx),
                m->window_y,
                (int)(w - (unsigned)(2 * c->bw)),
                m->window_height - (2 * c->bw),
                0);
            tx += WIDTH(c);
        }
    }
}
