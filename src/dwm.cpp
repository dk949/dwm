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

#include "event_queue.hpp"
#include "file.hpp"
#include "layout.hpp"
#include "log.hpp"
#include "mapping.hpp"
#include "xinerama.hpp"

#include <fcntl.h>
#include <project/config.hpp>
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
#include <cerrno>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <numeric>
#include <print>
#include <ranges>
#include <thread>
#include <utility>
namespace rng = std::ranges;
namespace vws = std::views;

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
#define INTERSECT(x, y, w, h, m)                                                                                   \
    (std::max(0, std::min((x) + (w), (m)->window_size.x + (m)->window_size.w) - std::max((x), (m)->window_size.x)) \
        * std::max(0, std::min((y) + (h), (m)->window_size.y + (m)->window_size.h) - std::max((y), (m)->window_size.y)))
#define LENGTH(X) (sizeof(X) / sizeof(X)[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X)  ((unsigned)(X)->size.w + 2 * (unsigned)(X)->bw + gappx)
#define HEIGHT(X) ((unsigned)(X)->size.h + 2 * (unsigned)(X)->bw + gappx)
#define TAGMASK   ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)  (drw->fontset_getwidth((X)) + (unsigned)lrpad)

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

#define PROGRESS_FADE 0, 0, 0


/* function declarations */

static void applyrules(Client *c);
static bool applysizehints(Client *c, Rect<int> *rect, int interact);
static void arrange(MonitorPtr const &m);
static void arrangemon(MonitorPtr const &m);
static void attach(Client *c);
static void attachaside(Client *c);
static void attachstack(Client *c);
static int avgheight();
static void buttonpress(XEvent *e);
static void checkotherwm();
static void cleanup();
static void cleanupmon(MonitorPtr const &mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static MonitorPtr createmon();
static void destroynotify(XEvent *e);
/// Remove client `c` from the list of clients on the monitor `c` is on
static void detach(Client *c);
static void detachstack(Client *c);
static MonitorPtr dirtomon(int dir);
static void drawbar(MonitorPtr m);
static void drawbars();
static void drawprogress(unsigned long long total, unsigned long long current, Color const *color);
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
static void grabkeys();
static void iconifyclient(Client *c);
static void installEventHandlers();
static int isdescprocess(pid_t p, pid_t c);
static void keypress(XEvent *e);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static Client *nexttagged(Client *c);
static Client *nexttiled(Client *c);
static void redirectChildLog(char **argv);
static void handle_notifyself_fade_anim(FadeBarEvent);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static MonitorPtr recttomon(int x, int y, int w, int h);
static void resize(Client *c, Rect<int> size, int interact);
static void resizeclient(Client *c, Rect<int> new_size);
static void restack(MonitorPtr const &m);
static void scan();
static bool sendevent(Client *c, Atom proto);
static void sendmon(Client *c, MonitorPtr m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, bool fullscreen);
static void setup();
static void seturgent(Client *c, bool urg);
static void showhide(Client *c);
static Client *swallowingclient(Window w);
static Client *termforwin(Client const *w);
static double timespecdiff(const struct timespec *a, const struct timespec *b);
static void unfocus(Client *c, int setfocus);
static void uniconifyclient(Client *c);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(MonitorPtr m);
static void updatebars();
static void updateclientlist();
static int updategeom();
static void updatenumlockmask();
static void updatesizehints(Client *c);
static void updatestatus();
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);


static pid_t winpid(Window w);
static Client *wintoclient(Window w);
static MonitorPtr wintomon(Window w);
static void wmchange(Client *c, XClientMessageEvent *cme);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);

/* configuration, allows nested code to access above variables */
#include "config.hpp"

/* variables */
static char const broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;                                                   /* X display screen geometry width, height */
static int bar_height, sel_bar_name_x = -1, sel_bar_name_width = -1; /* bar geometry */
static int lrpad;                                                    /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static Atom wmatom[WMLast], netatom[NetLast];
static int need_restart = 0;
static Display *dpy;
static Drw *drw;
static Monitors mons;
static MonitorPtr selmon;
static Window root, wmcheckwin;
#ifdef ASOUND
static volc_t *volc;
#endif /* ASOUND */
static xcb_connection_t *xcon;
static std::filesystem::path log_dir;
static std::unique_ptr<EventLoop> loop = nullptr;

struct Pertag {
    unsigned int curtag, prevtag;              /* current and previous tag */
    int nmasters[LENGTH(tags) + 1];            /* number of windows in master area */
    float mfacts[LENGTH(tags) + 1];            /* mfacts per tag */
    unsigned int sellts[LENGTH(tags) + 1];     /* selected layouts */
    Layout const *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
    bool showbars[LENGTH(tags) + 1];           /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
    char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
void applyrules(Client *c) {
    unsigned int i;
    unsigned int newtagset;
    Rule const *r;

    /* rule matching */
    c->props.isfloating = false;
    c->tags = 0;
    auto ch = c->classHint(dpy);
    char const *class_ = ch.class_hint ? ch.class_hint.get() : broken;
    char const *instance = ch.instance_hint ? ch.instance_hint.get() : broken;

    for (i = 0; i < LENGTH(rules); i++) {
        r = &rules[i];
        if ((!r->title || strstr(c->name, r->title)) && (!r->class_ || strstr(class_, r->class_))
            && (!r->instance || strstr(instance, r->instance))) {
            c->props.isterminal = r->isterminal;
            c->props.isfloating = r->isfloating;
            c->props.noswallow = r->noswallow;
            c->tags |= r->tags;
            auto mon_it = rng::find_if(mons, [&](MonitorPtr const &m) noexcept { return m->num == r->monitor; });
            if (mon_it != mons.end()) c->mon = *mon_it;

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
    c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

bool applysizehints(Client *c, Rect<int> *size, int interact) {
    auto &m = c->mon;

    /* set minimum possible */
    size->w = std::max(1, size->w);
    size->h = std::max(1, size->h);
    if (interact) {
        if (size->x > sw) {
            size->x = (int)((unsigned)sw - WIDTH(c));
        }
        if (size->y > sh) {
            size->y = (int)((unsigned)sh - HEIGHT(c));
        }
        if (size->x + size->w + 2 * c->bw < 0) {
            size->x = 0;
        }
        if (size->y + size->h + 2 * c->bw < 0) {
            size->y = 0;
        }
    } else {
        if (size->x >= m->window_size.x + m->window_size.w) {
            size->x = (int)((unsigned)(m->window_size.x + m->window_size.w) - WIDTH(c));
        }
        if (size->y >= m->window_size.y + m->window_size.h) {
            size->y = (int)((unsigned)(m->window_size.y + m->window_size.h) - HEIGHT(c));
        }
        if (size->x + size->w + 2 * c->bw <= m->window_size.x) {
            size->x = m->window_size.x;
        }
        if (size->y + size->h + 2 * c->bw <= m->window_size.y) {
            size->y = m->window_size.y;
        }
    }
    size->h = std::max(size->h, bar_height);
    size->w = std::max(size->w, bar_height);
    if (resizehints || c->props.isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
        if (!c->hintsvalid) updatesizehints(c);
        /* see last two sentences in ICCCM 4.1.2.3 */
        bool baseismin = c->basew == c->minw && c->baseh == c->minh;
        if (!baseismin) { /* temporarily remove base dimensions */
            size->w -= c->basew;
            size->h -= c->baseh;
        }
        /* adjust for aspect limits */
        if (c->mina > 0 && c->maxa > 0) {
            if (c->maxa < (float)size->w / (float)size->h) {
                size->w = (int)(((float)size->h * c->maxa) + 0.5f);
            } else if (c->mina < (float)size->h / (float)size->w) {
                size->h = (int)(((float)size->w * c->mina) + 0.5f);
            }
        }
        if (baseismin) { /* increment calculation requires this */
            size->w -= c->basew;
            size->h -= c->baseh;
        }
        /* adjust for increment value */
        if (c->incw) {
            size->w -= size->w % c->incw;
        }
        if (c->inch) {
            size->h -= size->h % c->inch;
        }
        /* restore base dimensions */
        size->w = std::max(size->w + c->basew, c->minw);
        size->h = std::max(size->h + c->baseh, c->minh);
        if (c->maxw) {
            size->w = std::min(size->w, c->maxw);
        }
        if (c->maxh) {
            size->h = std::min(size->h, c->maxh);
        }
    }
    return size->x != c->size.x || size->y != c->size.y || size->w != c->size.w || size->h != c->size.h;
}

void arrange(MonitorPtr const &m) {
    if (m) {
        showhide(m->stack);
    } else {
        for (auto const &mon : mons)
            showhide(mon->stack);
    }
    if (m) {
        arrangemon(m);
        restack(m);
    } else {
        for (auto const &mon : mons)
            arrangemon(mon);
    }
}

void arrangemon(MonitorPtr const &m) {
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

int avgheight() {
    if (xineramaIsActive(dpy)) {
        auto screens = ScreenInfoPtr::query(dpy);
        double out = 0;
        for (auto i = 0uz; i < screens.count(); i++)
            out += screens[i].width;
        return static_cast<int>(out / static_cast<double>(screens.count()));
    } else {
        return sh;
    }
}

void swallow(Client *p, Client *c) {
    if (c->props.noswallow || c->props.isterminal) {
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
    XMoveResizeWindow(dpy, p->win, p->size.x, p->size.y, (unsigned)p->size.w, (unsigned)p->size.h);
    configure(p);
    updateclientlist();
}

void unswallow(Client *c) {
    c->win = c->swallowing->win;

    delete c->swallowing;
    c->swallowing = nullptr;

    updatetitle(c);
    updatesizehints(c);
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    XMoveResizeWindow(dpy, c->win, c->size.x, c->size.y, (unsigned)c->size.w, (unsigned)c->size.h);
    configure(c);
    setclientstate(c, NormalState);
}

void bright_dec(Arg const &arg) {
    int ret;
    if ((ret = bright_dec_((double)arg.f))) return;

    double newval;
    if ((ret = bright_get_(&newval))) return;

    drawprogress(100, (unsigned long long)newval, &drw->scheme().bright_progress);
}

void bright_inc(Arg const &arg) {
    int ret;
    if ((ret = bright_inc_((double)arg.f))) return;

    double newval;
    if ((ret = bright_get_(&newval))) return;

    drawprogress(100, (unsigned long long)newval, &drw->scheme().bright_progress);
}

void bright_set(Arg const &arg) {
    int ret;
    if ((ret = bright_set_((double)arg.f))) return;

    drawprogress(100, (unsigned long long)arg.f, &drw->scheme().bright_progress);
}

void buttonpress(XEvent *e) {
    unsigned int i;
    unsigned int x;
    unsigned int click;
    Arg arg = {0};
    Client *c;
    MonitorPtr m;
    XButtonPressedEvent *ev = &e->xbutton;

    click = ClkRootWin;
    /* focus monitor if necessary */
    if ((m = wintomon(ev->window)) && m != selmon) {
        unfocus(selmon->sel, 1);
        selmon = m;
        focus(nullptr);
    }
    if (ev->window == selmon->barwin) {
        i = x = 0;
        do {
            x += TEXTW(tags[i]);
        } while (std::cmp_greater_equal(ev->x, x) && ++i < LENGTH(tags));
        if (i < LENGTH(tags)) {
            click = ClkTagBar;
            arg.ui = 1 << i;
        } else if ((unsigned)ev->x < x + TEXTW(selmon->layoutSymbol)) {
            click = ClkLtSymbol;
        } else if (ev->x > selmon->window_size.w - (int)TEXTW(stext)) {
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

void checkotherwm() {
    xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XSync(dpy, False);
}

void cleanup() {
    Layout foo = {.symbol = "", .arrange = nullptr};

    view(Arg {.ui = ~0u});
    selmon->lt[selmon->sellt] = &foo;
    for (auto const &m : mons) {
        while (m->stack) {
            unmanage(m->stack, 0);  // XXX: Potential problems
        }
    }
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (auto const &m : mons | vws::reverse)
        cleanupmon(m);
    mons.clear();


    XDestroyWindow(dpy, wmcheckwin);
    delete drw;
    XSync(dpy, False);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
#ifdef ASOUND
    volc_deinit(volc);
#endif /* ASOUND */
}

void cleanupmon(MonitorPtr const &mon) {
    // TODO(dk949): this needs to go in the Monitor destructor!
    XUnmapWindow(dpy, mon->barwin);
    XDestroyWindow(dpy, mon->barwin);
    delete mon->pertag;
}

void clientmessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;

    Client *c = wintoclient(cme->window);

    if (!c) return;

    if (cme->message_type == netatom[NetWMState]) {
        if (std::cmp_equal(cme->data.l[1], netatom[NetWMFullscreen])
            || std::cmp_equal(cme->data.l[2], netatom[NetWMFullscreen])) {
            setfullscreen(c,
                (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                    || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->props.isfullscreen)));
        }
    } else if (cme->message_type == netatom[NetActiveWindow]) {
        if (c != selmon->sel && !c->props.isurgent) {
            seturgent(c, true);
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
    ce.x = c->size.x;
    ce.y = c->size.y;
    ce.width = c->size.w;
    ce.height = c->size.h;
    ce.border_width = c->bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e) {
    Client *c;
    XConfigureEvent *ev = &e->xconfigure;

    // TODO(dk949): Figure out what this means??
    /* TODO: updategeom handling sucks, needs to be simplified */
    if (ev->window != root) return;

    bool dirty = sw != ev->width || sh != ev->height;
    sw = ev->width;
    sh = ev->height;
    if (updategeom() || dirty) {
        drw->resize((unsigned)sw, (unsigned)bar_height);
        updatebars();
        for (auto const &m : mons) {
            for (c = m->clients; c; c = c->next) {
                if (c->props.isfullscreen) {
                    resizeclient(c, m->monitor_size);
                }
            }
            XMoveResizeWindow(dpy, m->barwin, m->window_size.x, m->bar_y, (unsigned)m->window_size.w, (unsigned)bar_height);
        }
        focus(nullptr);
        arrange(nullptr);
    }
}

void configurerequest(XEvent *e) {
    Client *c;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    if ((c = wintoclient(ev->window))) {
        if (ev->value_mask & CWBorderWidth) {
            c->bw = ev->border_width;
        } else if (c->props.isfloating || !selmon->lt[selmon->sellt]->arrange) {
            auto const &m = c->mon;
            if (ev->value_mask & CWX) {
                c->old_size.x = c->size.x;
                c->size.x = m->monitor_size.x + ev->x;
            }
            if (ev->value_mask & CWY) {
                c->old_size.y = c->size.y;
                c->size.y = m->monitor_size.y + ev->y;
            }
            if (ev->value_mask & CWWidth) {
                c->old_size.w = c->size.w;
                c->size.w = ev->width;
            }
            if (ev->value_mask & CWHeight) {
                c->old_size.h = c->size.h;
                c->size.h = ev->height;
            }
            if ((c->size.x + c->size.w) > m->monitor_size.x + m->monitor_size.w && c->props.isfloating) {
                c->size.x = (int)((unsigned)m->monitor_size.x
                                  + ((unsigned)(m->monitor_size.w / 2) - WIDTH(c) / 2)); /* center in x direction */
            }
            if ((c->size.y + c->size.h) > m->monitor_size.y + m->monitor_size.h && c->props.isfloating) {
                c->size.y = (int)((unsigned)m->monitor_size.y
                                  + ((unsigned)(m->monitor_size.h / 2) - HEIGHT(c) / 2)); /* center in y direction */
            }
            if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight))) {
                configure(c);
            }
            if (ISVISIBLE(c)) {
                XMoveResizeWindow(dpy, c->win, c->size.x, c->size.y, (unsigned)c->size.w, (unsigned)c->size.h);
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

MonitorPtr createmon() {
    // TODO(dk949): Some of this should probably be in Monitor constructor

    auto m = std::make_shared<Monitor>();
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

MonitorPtr dirtomon(int dir) {
    auto mon_it = rng::find(mons, selmon);
    if (mon_it == mons.end()) return nullptr;
    if (dir > 0) {
        if (mon_it == mons.end() + 1)
            return mons.front();
        else
            return *std::next(mon_it);
    } else {
        if (mon_it == mons.begin())
            return mons.back();
        else
            return *std::prev(mon_it);
    }
}

// TODO(dk949): handle the case where the tags overlap with status
//              (common if monitor is vertical)
void drawbar(MonitorPtr m) {
    int x;
    int w;
    int text_width = 0;
    auto boxs = (int)(drw->fonts().h / 9u);
    auto boxw = (int)((drw->fonts().h / 6u) + 2u);
    unsigned int i;
    unsigned int occ = 0;
    unsigned int urg = 0;

    if (!m->showbar) return;

    /* draw status first so it can be overdrawn by tags later */
    if (m == selmon) {                                          /* status is only drawn on selected monitor */
        drw->setColor(&drw->scheme().status);
        text_width = (int)(TEXTW(stext) - (unsigned)lrpad + 2); /* 2px right padding */
        drw->draw_text(m->window_size.w - text_width, 0, (unsigned)text_width, (unsigned)bar_height, 0, stext, false);
    }

    for (Client *c = m->clients; c; c = c->next) {
        occ |= c->tags;
        if (c->props.isurgent) {
            urg |= c->tags;
        }
    }
    x = 0;
    for (i = 0; i < LENGTH(tags); i++) {
        w = (int)TEXTW(tags[i]);
        if (m->tagset[m->seltags] & 1 << i)
            drw->setColor(&drw->scheme().tags_sel);
        else
            drw->setColor(&drw->scheme().tags_norm);

        drw->draw_text(x, 0, (unsigned)w, (unsigned)bar_height, (unsigned)(lrpad / 2), tags[i], (urg & 1 << i) != 0u);
        if (occ & 1 << i) {
            drw->draw_rect(x + boxs,
                boxs,
                (unsigned)boxw,
                (unsigned)boxw,
                m == selmon && (selmon->sel != nullptr) && ((selmon->sel->tags & 1 << i) != 0u),
                ((int)(urg & 1 << i)) != 0);
        }
        x += w;
    }
    w = (int)TEXTW(m->layoutSymbol);
    drw->setColor(&drw->scheme().tags_norm);
    x = drw->draw_text(x, 0, (unsigned)w, (unsigned)bar_height, (unsigned)(lrpad / 2), m->layoutSymbol, false);

    if ((w = m->window_size.w - text_width - x) > bar_height) {
        if (m->sel) {
            drw->setColor(m == selmon ? &drw->scheme().info_sel : &drw->scheme().info_norm);
            drw->draw_text(x, 0, (unsigned)w, (unsigned)bar_height, (unsigned)(lrpad / 2), m->sel->name, false);
            if (m->sel->props.isfloating) {
                drw->draw_rect(x + boxs, boxs, (unsigned)boxw, (unsigned)boxw, m->sel->props.isfixed, false);
            }
        } else {
            drw->setColor(&drw->scheme().info_norm);
            drw->draw_rect(x, 0, (unsigned)w, (unsigned)bar_height, /*filled*/ true, /*invert*/ true);
        }
    }
    if (m == selmon) {
        sel_bar_name_x = x;
        sel_bar_name_width = w;
    }
    drw->map(m->barwin, 0, 0, (unsigned)m->window_size.w, (unsigned)bar_height);
    drawprogress(PROGRESS_FADE);
}

void drawbars() {
    for (auto const &mon : mons) {
        drawbar(mon);
    }
}

// TODO(dk949): THIS NEEDS TO BE FIXED!!!!!
//              (also handle_notifyself_fade_anim)
void drawprogress(unsigned long long t, unsigned long long c, Color const *color) {
    static unsigned long long total;
    static unsigned long long current;
    static struct timespec last;
    static Color const *cscheme;

    if (sel_bar_name_x <= 0 || sel_bar_name_width <= 0) return;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    if (t != 0) {
        total = t;
        current = c;
        last = now;
        cscheme = color;
    }

    if (total > 0 && (timespecdiff(&now, &last) < progress_fade_time)) {
        int x = sel_bar_name_x;
        int y = 0;
        int w = sel_bar_name_width;
        int h = bar_height; /*progress rectangle*/
        int fg = 0;
        int bg = 1;
        drw->setColor(cscheme);

        drw->draw_rect(x, y, (unsigned)w, (unsigned)h, true, bg != 0);
        drw->draw_rect(x, y, (unsigned)(((double)w * (double)current) / (double)total), (unsigned)h, true, fg != 0);

        drw->map(selmon->barwin, x, y, (unsigned)w, (unsigned)h);
        loop->push(FadeBarEvent());
    }
}

void enqueue(Client *c) {
    Client *l;
    for (l = c->mon->clients; l && l->next; l = l->next) { }
    if (l) {
        l->next = c;
        c->next = nullptr;
    }
}

void enqueuestack(Client *c) {
    Client *l;
    for (l = c->mon->stack; l && l->snext; l = l->snext) {
        ;
    }
    if (l) {
        l->snext = c;
        c->snext = nullptr;
    }
}

void enternotify(XEvent *e) {
    Client *c;
    XCrossingEvent *ev = &e->xcrossing;

    if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root) {
        return;
    }
    c = wintoclient(ev->window);
    auto mon = c ? c->mon : wintomon(ev->window);
    if (mon != selmon) {
        unfocus(selmon->sel, 1);
        selmon = mon;
    } else if (!c || c == selmon->sel) {
        return;
    }
    focus(c);
}

void expose(XEvent *e) {
    MonitorPtr m;
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
        if (c->props.isurgent) {
            seturgent(c, false);
        }
        detachstack(c);
        attachstack(c);
        grabbuttons(c, 1);
        XSetWindowBorder(dpy, c->win, drw->scheme().sel.border.pixel);
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
    MonitorPtr m;

    if (mons.size() == 1) {
        return;
    }
    if ((m = dirtomon(arg.i)) == selmon) {
        return;
    }
    unfocus(selmon->sel, 0);
    selmon = m;

    /* move cursor to the center of the new monitor */
    XWarpPointer(dpy, 0, selmon->barwin, 0, 0, 0, 0, selmon->window_size.w / 2, selmon->window_size.h / 2);
    focus(nullptr);
}

void focusstack(Arg const &arg) {
    Client *c = nullptr;
    Client *i;

    if (!selmon->sel || selmon->sel->props.isfullscreen) {
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
    unsigned long bytes_left;
    unsigned long nitems;
    unsigned char *p = nullptr;
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
    unsigned char *p = nullptr;
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
    char **list = nullptr;
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

void grabkeys() {
    updatenumlockmask();
    unsigned int modifiers[] = {
        0,
        LockMask,
        numlockmask,
        numlockmask | LockMask,
    };
    int start;
    int end;
    int skip;

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

static int isuniquegeom(std::span<ScreenInfo const> unique, size_t n, ScreenInfo info) {
    while (n--) {
        if (unique[n].x_org == info.x_org && unique[n].y_org == info.y_org && unique[n].width == info.width
            && unique[n].height == info.height) {
            return 0;
        }
    }
    return 1;
}

void keypress(XEvent *e) {

    XKeyEvent *ev = &e->xkey;

    KeySym keysym = XLookupKeysym(ev, 0);
    for (unsigned int i = 0; i < LENGTH(keys); i++) {
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
    Client *t = nullptr;
    Client *term = nullptr;
    Window trans = None;
    XWindowChanges wc;

    auto *c = new Client {};
    c->win = w;
    c->pid = winpid(w);
    /* geometry */
    c->size.x = c->old_size.x = wa->x;
    c->size.y = c->old_size.y = wa->y;
    c->size.w = c->old_size.w = wa->width;
    c->size.h = c->old_size.h = wa->height;
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

    if ((unsigned)c->size.x + WIDTH(c) > (unsigned)(c->mon->window_size.x + c->mon->window_size.w))
        c->size.x = (int)((unsigned)(c->mon->window_size.x + c->mon->window_size.w) - WIDTH(c));
    if ((unsigned)c->size.y + HEIGHT(c) > (unsigned)(c->mon->window_size.y + c->mon->window_size.h))
        c->size.y = (int)((unsigned)(c->mon->window_size.y + c->mon->window_size.h) - HEIGHT(c));
    c->size.x = std::max(c->size.x, c->mon->window_size.x);
    c->size.y = std::max(c->size.y, c->mon->window_size.y);



    c->bw = (int)borderpx;

    wc.border_width = c->bw;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, drw->scheme().norm.border.pixel);
    configure(c); /* propagates border_width, if size doesn't change */
    updatewindowtype(c);
    updatesizehints(c);
    updatewmhints(c);
    XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    grabbuttons(c, 0);
    if (!c->props.isfloating) {
        c->props.isfloating = c->props.old_float_state = trans != None || c->props.isfixed;
    }
    if (c->props.isfloating) {
        XRaiseWindow(dpy, c->win);
    }
    attachaside(c);
    attachstack(c);
    XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
    XMoveResizeWindow(dpy,
        c->win,
        c->size.x + (2 * sw),
        c->size.y,
        (unsigned)c->size.w,
        (unsigned)c->size.h); /* some windows require this */
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
    focus(nullptr);
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

void monocle(MonitorPtr const &m) {
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
        resize(c,
            {
                m->window_size.x,
                m->window_size.y,
                m->window_size.w - (2 * c->bw),
                m->window_size.h - (2 * c->bw),
            },
            0);
    }
}

void motionnotify(XEvent *e) {
    // TODO(dk949): get rid of this static variable!!!
    //              Currently made it weak_ptr, to avoid keeping a monitor pointer alive
    //              for the duration of the program.
    static WeakMonitorPtr mon;
    MonitorPtr m;
    XMotionEvent *ev = &e->xmotion;

    if (ev->window != root) {
        return;
    }
    m = recttomon(ev->x_root, ev->y_root, 1, 1);
    if (!mon.expired() && mon.lock() != m) {
        unfocus(selmon->sel, 1);
        selmon = m;
        focus(nullptr);
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
    MonitorPtr m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel)) {
        return;
    }
    if (c->props.isfullscreen) { /* no support moving fullscreen windows by mouse */
        return;
    }
    restack(selmon);
    ocx = c->size.x;
    ocy = c->size.y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, drw->cursors().move(), CurrentTime)
        != GrabSuccess) {
        return;
    }
    if (!getrootptr(&x, &y)) {
        return;
    }
    do {
        XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
        switch (ev.type) {
            // TODO(dk949): make sure these don't actually need special treatment
            case ButtonRelease:
            case NoExpose: break;
            case ConfigureRequest: loop->exec<ConfigureRequest>(&ev); break;
            case Expose: loop->exec<Expose>(&ev); break;
            case MapRequest: loop->exec<MapRequest>(&ev); break;
            case MotionNotify:
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
                    continue;
                }
                lasttime = ev.xmotion.time;

                nx = ocx + (ev.xmotion.x - x);
                ny = ocy + (ev.xmotion.y - y);
                if (std::cmp_less(abs(selmon->window_size.x - nx), snap)) {
                    nx = selmon->window_size.x;
                } else if (((unsigned)(selmon->window_size.x + selmon->window_size.w) - ((unsigned)nx + WIDTH(c)))
                           < snap) {
                    nx = (int)((unsigned)(selmon->window_size.x + selmon->window_size.w) - WIDTH(c));
                }
                if (std::cmp_less(abs(selmon->window_size.y - ny), snap)) {
                    ny = selmon->window_size.y;
                } else if (((unsigned)(selmon->window_size.y + selmon->window_size.h) - ((unsigned)ny + HEIGHT(c)))
                           < snap) {
                    ny = (int)((unsigned)(selmon->window_size.y + selmon->window_size.h) - HEIGHT(c));
                }
                if (!c->props.isfloating && selmon->lt[selmon->sellt]->arrange
                    && (std::cmp_greater(abs(nx - c->size.x), snap) || std::cmp_greater(abs(ny - c->size.y), snap))) {
                    togglefloating({});
                }
                if (!selmon->lt[selmon->sellt]->arrange || c->props.isfloating) {
                    resize(c, {nx, ny, c->size.w, c->size.h}, 1);
                }
                break;
            default: lg::warn("Unexpected event type {} in movemouse", ev.type); break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(dpy, CurrentTime);
    if ((m = recttomon(c->size.x, c->size.y, c->size.w, c->size.h)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(nullptr);
    }
}

Client *nexttagged(Client *c) {
    Client *walked = c->mon->clients;
    for (; walked && (walked->props.isfloating || !ISVISIBLEONTAG(walked, c->tags)); walked = walked->next) {
        ;
    }
    return walked;
}

Client *nexttiled(Client *c) {
    for (; c && (c->props.isfloating || !ISVISIBLE(c)); c = c->next) {
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
                if (!c->props.isfloating && (XGetTransientForHint(dpy, c->win, &trans))
                    && (c->props.isfloating = (wintoclient(trans)) != nullptr)) {
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
    loop->push(TerminateEvent());
    need_restart = 0;
    lg::info("Initiating shutdowd");
}

void restart(Arg const &arg) {
    (void)arg;
    loop->push(TerminateEvent());
    need_restart = 1;
}

// TODO(dk949): this should take aaRect<int>
MonitorPtr recttomon(int x, int y, int w, int h) {
    MonitorPtr r = selmon;
    int a;
    int area = 0;

    // TODO(dk949): This is probably a max reduction
    for (auto const &mon : mons) {
        if ((a = INTERSECT(x, y, w, h, mon)) > area) {
            area = a;
            r = mon;
        }
    }
    return r;
}

void resize(Client *c, Rect<int> new_size, int interact) {
    if (applysizehints(c, &new_size, interact)) resizeclient(c, new_size);
}

void resizeclient(Client *c, Rect<int> new_size) {
    XWindowChanges wc;
    unsigned int n;
    unsigned int gapoffset;
    unsigned int gapincr;
    Client *nbc;
    auto mon = c->mon;

    wc.border_width = c->bw;

    /* Get number of clients for the selected monitor */
    for (n = 0, nbc = nexttiled(mon->clients); nbc; nbc = nexttiled(nbc->next), n++) { }

    /* Do nothing if layout is floating */
    if (c->props.isfloating || mon->lt[mon->sellt]->arrange == nullptr) {
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

    c->old_size.x = c->size.x;
    c->size.x = wc.x = (int)((unsigned)new_size.x + gapoffset);
    c->old_size.y = c->size.y;
    c->size.y = wc.y = (int)((unsigned)new_size.y + gapoffset);
    c->old_size.w = c->size.w;
    c->size.w = wc.width = (int)((unsigned)new_size.w - gapincr);
    c->old_size.h = c->size.h;
    c->size.h = wc.height = (int)((unsigned)new_size.h - gapincr);

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
    MonitorPtr m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel)) {
        return;
    }
    if (c->props.isfullscreen) { /* no support resizing fullscreen windows by mouse */
        return;
    }
    restack(selmon);
    ocx = c->size.x;
    ocy = c->size.y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, drw->cursors().resize(), CurrentTime)
        != GrabSuccess) {
        return;
    }
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->size.w + c->bw - 1, c->size.h + c->bw - 1);
    do {
        XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
        switch (ev.type) {
            // TODO(dk949): make sure these don't actually need special treatment
            case ButtonRelease:
            case NoExpose: break;
            case ConfigureRequest: loop->exec<ConfigureRequest>(&ev); break;
            case Expose: loop->exec<Expose>(&ev); break;
            case MapRequest: loop->exec<MapRequest>(&ev); break;
            case MotionNotify:
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
                    continue;
                }
                lasttime = ev.xmotion.time;

                nw = std::max(ev.xmotion.x - ocx - (2 * c->bw) + 1, 1);
                nh = std::max(ev.xmotion.y - ocy - (2 * c->bw) + 1, 1);
                if (c->mon->window_size.x + nw >= selmon->window_size.x
                    && c->mon->window_size.x + nw <= selmon->window_size.x + selmon->window_size.w
                    && c->mon->window_size.y + nh >= selmon->window_size.y
                    && c->mon->window_size.y + nh <= selmon->window_size.y + selmon->window_size.h) {
                    if (!c->props.isfloating && selmon->lt[selmon->sellt]->arrange
                        && (std::cmp_greater(abs(nw - c->size.w), snap) || std::cmp_greater(abs(nh - c->size.h), snap))) {
                        togglefloating({});
                    }
                }
                if (!selmon->lt[selmon->sellt]->arrange || c->props.isfloating) {
                    resize(c, {c->size.x, c->size.y, nw, nh}, 1);
                }
                break;
            default: lg::warn("Unknown event type {} in resizemouse", ev.type); break;
        }
    } while (ev.type != ButtonRelease);
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->size.w + c->bw - 1, c->size.h + c->bw - 1);
    XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {
        ;
    }
    if ((m = recttomon(c->size.x, c->size.y, c->size.w, c->size.h)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(nullptr);
    }
}

void restack(MonitorPtr const &m) {
    Client *c;
    XEvent ev;
    XWindowChanges wc;

    drawbar(m);
    if (!m->sel) {
        return;
    }
    if (m->sel->props.isfloating || !m->lt[m->sellt]->arrange) {
        XRaiseWindow(dpy, m->sel->win);
    }
    if (m->lt[m->sellt]->arrange) {
        wc.stack_mode = Below;
        wc.sibling = m->barwin;
        for (c = m->stack; c; c = c->snext) {
            if (!c->props.isfloating && ISVISIBLE(c)) {
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
    Client *c = nullptr;
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

void scan() {
    unsigned int i;
    unsigned int num;
    Window d1;
    Window d2;
    Window *wins = nullptr;
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

void handle_notifyself_fade_anim(FadeBarEvent) {
    drawprogress(PROGRESS_FADE);
    std::this_thread::sleep_for(EventLoop::tick_time);
}

void sendmon(Client *c, MonitorPtr m) {
    if (c->mon == m) return;

    unfocus(c, 1);
    detach(c);
    detachstack(c);
    c->mon = m;
    c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
    attachaside(c);
    attachstack(c);
    focus(nullptr);
    arrange(nullptr);
    if (c->switchtotag) {
        c->switchtotag = 0;
    }
}

void setclientstate(Client *c, long state) {
    long data[] = {state, None};

    XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32, PropModeReplace, (unsigned char *)data, 2);
}

bool sendevent(Client *c, Atom proto) {
    int n;
    Atom *protocols;
    bool exists = false;
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
    if (!c->props.neverfocus) {
        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
        XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->win), 1);
    }
    sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client *c, bool fullscreen) {
    if (fullscreen && !c->props.isfullscreen) {
        XChangeProperty(dpy,
            c->win,
            netatom[NetWMState],
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *)&netatom[NetWMFullscreen],
            1);
        c->props.isfullscreen = true;
        c->props.old_float_state = c->props.isfloating;
        c->oldbw = c->bw;
        c->bw = 0;
        c->props.isfloating = true;
        resizeclient(c, c->mon->monitor_size);
        XRaiseWindow(dpy, c->win);
    } else if (!fullscreen && c->props.isfullscreen) {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, nullptr, 0);
        c->props.isfullscreen = false;
        c->props.isfloating = c->props.old_float_state;
        c->bw = c->oldbw;
        c->size.x = c->old_size.x;
        c->size.y = c->old_size.y;
        c->size.w = c->old_size.w;
        c->size.h = c->old_size.h;
        resizeclient(c, c->size);
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

void setup() {
    Atom utf8string;
    struct sigaction sa;

    /* do not transform children into zombies when they terminate */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, nullptr);

    /* clean up any zombies (inherited from .xinitrc etc) immediately */
    while (waitpid(-1, nullptr, WNOHANG)) { }


    /* init screen */
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    drw = new Drw(dpy, screen, root, (unsigned)sw, (unsigned)sh);
    if (!drw->fontset_create(fonts, LENGTH(fonts))) {
        lg::fatal("no fonts could be loaded.");
    }

    if (bright_setup(get_bright_set_file(), get_bright_get_file(), get_bright_max_file())) {
        lg::fatal("backlight setup failed");
    }

#ifdef ASOUND
    if (!(volc = volc_init(VOLC_ALL_DEFULTS))) lg::fatal("volc setup failed");

#endif /* ASOUND */

    lrpad = (int)drw->fonts().h;
    bar_height = (int)drw->fonts().h + 2;
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

    drw->setColorScheme(colors);

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
    XSetWindowAttributes wa;
    wa.cursor = drw->cursors().normal();
    XChangeWindowAttributes(dpy, root, CWCursor, &wa);
    loop = std::make_unique<EventLoop>(dpy, root);
    installEventHandlers();
    grabkeys();
    focus(nullptr);
}

void seturgent(Client *c, bool urg) {
    XWMHints *wmh;

    c->props.isurgent = urg;
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
        XMoveWindow(dpy, c->win, c->size.x, c->size.y);
        if ((!c->mon->lt[c->mon->sellt]->arrange || c->props.isfloating) && !c->props.isfullscreen) {
            resize(c, c->size, 0);
        }
        showhide(c->snext);
    } else {
        /* hide clients bottom up */
        showhide(c->snext);
        XMoveWindow(dpy, c->win, (int)(WIDTH(c) * -2u), c->size.y);
    }
}

void redirectChildLog(char **argv) {
    auto file_name = log_dir / argv[0];
    file_name.replace_extension(".log");

    auto child_fd = FDPtr {open(file_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)};
    if (child_fd.get() < 0) {
        lg::warn("Could not set up logging for child processes {}: {}", argv[0], strerror(errno));
        return;
    }
    char const div[] = "________________________________________________________________________________\n";
    if (write(child_fd.get(), div, sizeof(div)) < 0) {
        lg::warn("Could not write to child log file {}: {}", file_name.c_str(), strerror(errno));
        return;
    }

    if (dup2(child_fd.get(), STDOUT_FILENO) < 0) {
        lg::warn("Could not redirect child stdout to log file {}: {}", file_name.c_str(), strerror(errno));
        return;
    }
    if (dup2(child_fd.get(), STDERR_FILENO) < 0) {
        lg::warn("Could not redirect child stdout to log file {}: {}", file_name.c_str(), strerror(errno));
        return;
    }
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
        sigaction(SIGCHLD, &sa, nullptr);
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
        focus(nullptr);
        arrange(selmon);
    }
}

void tagmon(Arg const &arg) {
    if (!selmon->sel || mons.size() == 1) {
        return;
    }
    sendmon(selmon->sel, dirtomon(arg.i));
}

void tile(MonitorPtr const &m) {
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
        if (std::cmp_less(n, m->nmaster))
            mfacts += c->cfact;
        else
            sfacts += c->cfact;

    if (n == 0) return;


    if (std::cmp_greater(n, m->nmaster))
        mw = m->nmaster ? (unsigned)((float)m->window_size.w * m->mfact) : 0;
    else
        mw = (unsigned)m->window_size.w;

    for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (std::cmp_less(i, m->nmaster)) {
            h = (unsigned)((float)((unsigned)m->window_size.h - my) * (c->cfact / mfacts));
            resize(c,
                {
                    m->window_size.x,
                    (int)((unsigned)m->window_size.y + my),
                    (int)(mw - (unsigned)(2 * c->bw)),
                    (int)(h - (2 * (unsigned)c->bw)),
                },
                0);
            // TODO(dk949): This is a guard against creating too many clients.
            //              Do something if there's too many clients!
            if (my + HEIGHT(c) < (unsigned)m->window_size.h) {
                my += HEIGHT(c);
                mfacts -= c->cfact;
            }
        } else {
            h = (unsigned)((float)((unsigned)m->window_size.h - ty) * (c->cfact / sfacts));
            resize(c,
                {
                    (int)((unsigned)m->window_size.x + mw),
                    (int)((unsigned)m->window_size.y + ty),
                    (int)((unsigned)m->window_size.w - mw - (2 * (unsigned)c->bw)),
                    (int)(h - (2 * (unsigned)c->bw)),
                },
                0);
            if (ty + HEIGHT(c) < (unsigned)m->window_size.h) {
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
        selmon->window_size.x,
        selmon->bar_y,
        (unsigned)selmon->window_size.w,
        (unsigned)bar_height);
    arrange(selmon);
}

void togglefloating(Arg const &arg) {
    (void)arg;
    if (!selmon->sel) return;

    if (selmon->sel->props.isfullscreen) /* no support for fullscreen windows */
        return;

    selmon->sel->props.isfloating = !selmon->sel->props.isfloating || selmon->sel->props.isfixed;
    if (selmon->sel->props.isfloating) {
        resize(selmon->sel, selmon->sel->size, 0);
    }
    arrange(selmon);
}

void togglefs(Arg const &) {
    if (!selmon->sel) return;
    setfullscreen(selmon->sel, !selmon->sel->props.isfullscreen);
}

void toggletag(Arg const &arg) {
    unsigned int newtags;

    if (!selmon->sel) {
        return;
    }
    newtags = selmon->sel->tags ^ (arg.ui & TAGMASK);
    if (newtags) {
        selmon->sel->tags = newtags;
        focus(nullptr);
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

        focus(nullptr);
        arrange(selmon);
    }
}

void unfocus(Client *c, int setfocus) {
    if (!c) return;

    grabbuttons(c, 0);
    XSetWindowBorder(dpy, c->win, drw->scheme().norm.border.pixel);
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
    XMoveResizeWindow(dpy, c->win, c->size.x, c->size.y, (unsigned)c->size.w, (unsigned)c->size.h);
    configure(c);
    setclientstate(c, NormalState);
    attachstack(c);
    attach(c);
}

void unmanage(Client *c, int destroyed) {
    auto m = c->mon;
    unsigned int switchtotag = c->switchtotag;
    XWindowChanges wc;

    if (c->swallowing) {
        unswallow(c);
        return;
    }

    Client *s = swallowingclient(c->win);
    if (s) {
        delete s->swallowing;
        s->swallowing = nullptr;
        arrange(m);
        focus(nullptr);
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
        focus(nullptr);
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

void updatebars() {
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
    for (auto const &mon : mons) {
        if (mon->barwin) {
            continue;
        }
        mon->barwin = XCreateWindow(dpy,
            root,
            mon->window_size.x,
            mon->bar_y,
            (unsigned)mon->window_size.w,
            (unsigned)bar_height,
            0,
            DefaultDepth(dpy, screen),
            CopyFromParent,
            DefaultVisual(dpy, screen),
            CWOverrideRedirect | CWBackPixmap | CWEventMask,
            &wa);
        XDefineCursor(dpy, mon->barwin, drw->cursors().normal());
        XMapRaised(dpy, mon->barwin);
        XSetClassHint(dpy, mon->barwin, &ch);
    }
}

void updatebarpos(MonitorPtr m) {
    m->window_size.y = m->monitor_size.y;
    m->window_size.h = m->monitor_size.h;
    if (m->showbar) {
        m->window_size.h -= bar_height;
        m->bar_y = m->topbar ? m->window_size.y : m->window_size.y + m->window_size.h;
        m->window_size.y = m->topbar ? m->window_size.y + bar_height : m->window_size.y;
    } else {
        m->bar_y = -bar_height;
    }
}

void updateclientlist() {
    Client *c;

    XDeleteProperty(dpy, root, netatom[NetClientList]);
    for (auto const &mon : mons) {
        for (c = mon->clients; c; c = c->next) {
            XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
        }
    }
}

int updategeom() {
    int dirty = 0;

    if (xineramaIsActive(dpy)) {
        std::size_t j = 0;
        Client *c;
        auto info = ScreenInfoPtr::query(dpy);
        std::size_t num_screen_infos = info.count();
        /* only consider unique geometries as separate screens */
        // auto *unique = new ScreenInfo[num_screen_infos];
        std::vector<ScreenInfo> unique;
        unique.resize(num_screen_infos);
        for (size_t i = 0; i < num_screen_infos; i++) {
            if (isuniquegeom(unique, j, info[i])) {
                unique[j++] = info[i];
            }
        }
        num_screen_infos = j;
        /* new monitors available */
        for (auto i = mons.size(); i < num_screen_infos; i++) {
            mons.push_back(createmon());
        }
        for (auto const &[mon, u, i] : vws::zip(mons | vws::take(num_screen_infos), unique, vws::iota(0))) {
            if (u.x_org != mon->monitor_size.x     //
                || u.y_org != mon->monitor_size.y  //
                || u.width != mon->monitor_size.w  //
                || u.height != mon->monitor_size.h) {
                dirty = 1;
                mon->num = i;
                mon->monitor_size.x = mon->window_size.x = u.x_org;
                mon->monitor_size.y = mon->window_size.y = u.y_org;
                mon->monitor_size.w = mon->window_size.w = u.width;
                mon->monitor_size.h = mon->window_size.h = u.height;
                updatebarpos(mon);
            }
        }
        /* less monitors available nn < n */
        for (auto const &mon : mons | vws::drop(num_screen_infos) | vws::reverse) {
            while ((c = mon->clients)) {
                dirty = 1;
                mon->clients = c->next;
                detachstack(c);
                c->mon = mons.front();
                attach(c);
                attachaside(c);
                attachstack(c);
            }
            if (mon == selmon) selmon = mons.front();
            cleanupmon(mon);
        }
        if (auto erase_start = mons.begin() + (long)num_screen_infos; erase_start <= mons.end())
            mons.erase(erase_start, mons.end());
    } else { /* default monitor setup */
        if (mons.empty()) {
            mons.push_back(createmon());
        }
        if (mons.front()->monitor_size.w != sw || mons.front()->monitor_size.h != sh) {
            dirty = 1;
            mons.front()->monitor_size.w = mons.front()->window_size.w = sw;
            mons.front()->monitor_size.h = mons.front()->window_size.h = sh;
            updatebarpos(mons.front());
        }
    }
    if (dirty) {
        selmon = mons.front();
        selmon = wintomon(root);
    }
    return dirty;
}

void updatenumlockmask() {
    unsigned int i;
    unsigned int j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++) {
        for (j = 0; std::cmp_less(j, modmap->max_keypermod); j++) {
            if (modmap->modifiermap[(i * (unsigned)modmap->max_keypermod) + j] == XKeysymToKeycode(dpy, XK_Num_Lock)) {
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
    c->props.isfixed = c->maxw != 0 && c->maxh != 0 && c->maxw == c->minw && c->maxh == c->minh;
    c->hintsvalid = true;
}

void updatestatus() {
    if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext))) {
        strcpy(stext, "dwm-");
        strncat(stext, dwm::version::full.data(), dwm::version::full.size());
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
        setfullscreen(c, true);
    }
    if (wtype == netatom[NetWMWindowTypeDialog]) {
        c->props.isfloating = true;
    }
}

void updatewmhints(Client *c) {
    XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, c->win))) {
        if (c == selmon->sel && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->win, wmh);
        } else {
            c->props.isurgent = (wmh->flags & XUrgencyHint) != 0;
        }
        if (wmh->flags & InputHint) {
            c->props.neverfocus = wmh->input == 0;
        } else {
            c->props.neverfocus = false;
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

    focus(nullptr);
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
        state.state.switch_pos ? &drw->scheme().info_progress : &drw->scheme().off_progress);
}
#endif


pid_t winpid(Window w) {
    pid_t result = 0;

    xcb_res_client_id_spec_t spec {};
    spec.client = (uint32_t)w;
    spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

    xcb_generic_error_t *e = nullptr;
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
    char buf[256];
    snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);


    auto f = FilePtr {fopen(buf, "r")};

    if (!f) {
        lg::warn("failed to open stat file {} for process {}: {}", buf, p, strerror(errno));
        return 0;
    }

    int res = fscanf(f.get(), "%*u %*s %*c %u", &v);
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
    long offset = 0;
    long length = 0;
    Bool delete_ = False;
    Atom req_type = XA_CARDINAL;
    Atom actual_type;
    int format;
    unsigned long nitems;
    unsigned long bytes_left;
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
        auto *begin = (uint32_t *)(void *)data;
        auto *end = (uint32_t *)(void *)((uint8_t *)data + *size);
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

    delay(1'000'000 * 5, (void (*)(void *))uniconifyclient, c);
}

void installEventHandlers() {
    loop->on<ButtonPress>(buttonpress);
    loop->on<ClientMessage>(clientmessage);
    loop->on<ConfigureRequest>(configurerequest);
    loop->on<ConfigureNotify>(configurenotify);
    loop->on<DestroyNotify>(destroynotify);
    loop->on<EnterNotify>(enternotify);
    loop->on<Expose>(expose);
    loop->on<FocusIn>(focusin);
    loop->on<KeyPress>(keypress);
    loop->on<MappingNotify>(mappingnotify);
    loop->on<MapRequest>(maprequest);
    loop->on<MotionNotify>(motionnotify);
    loop->on<PropertyNotify>(propertynotify);
    loop->on<UnmapNotify>(unmapnotify);
    loop->on<FadeBarEvent>(handle_notifyself_fade_anim);
}

int isdescprocess(pid_t p, pid_t c) {
    while (p != c && c != 0) {
        c = getparentprocess(c);
    }

    return (int)c;
}

Client *termforwin(Client const *w) {
    if (!w->pid || w->props.isterminal) {
        return nullptr;
    }

    Client *out = nullptr;
    for (auto const &mon : mons) {
        for (Client *c = mon->clients; c; c = c->next) {
            if (c->props.isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid)) {
                if (selmon->sel == c) return c;
                out = c;
            }
        }
    }

    return out;
}

Client *swallowingclient(Window w) {
    for (auto const &mon : mons) {
        for (Client *c = mon->clients; c; c = c->next) {
            if (c->swallowing && c->swallowing->win == w) {
                return c;
            }
        }
    }

    return nullptr;
}

Client *wintoclient(Window w) {

    for (auto const &mon : mons) {
        for (Client *c = mon->clients; c; c = c->next) {
            if (c->win == w) {
                return c;
            }
        }
    }
    return nullptr;
}

MonitorPtr wintomon(Window w) {
    int x;
    int y;

    if (w == root && getrootptr(&x, &y)) {
        return recttomon(x, y, 1, 1);
    }
    if (auto mon_it = rng::find_if(mons, [&](auto const &m) noexcept { return w == m->barwin; }); mon_it != mons.end()) {
        return *mon_it;
    }
    if (Client *c = wintoclient(w)) {
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

    if (!selmon->lt[selmon->sellt]->arrange || !c || c->props.isfloating) return;

    if (c == nexttiled(selmon->clients) || !(c = nexttiled(c->next))) return;

    pop(c);
}

int main(int argc, char *argv[]) {
    log_dir = lg::setupLogging();
    lg::debug("Setup logging");

    if (argc == 2 && !strcmp("-v", argv[1])) {
        std::println("dwm-{}", dwm::version::full);
        return 0;
    } else if (argc != 1) {
        fputs("usage: dwm [-v]", stderr);
        return 1;
    }
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
        lg::warn("no locale support");
    }
    if (!(dpy = XOpenDisplay(nullptr))) {
        lg::fatal("cannot open display");
    }
    if (!(xcon = XGetXCBConnection(dpy))) {
        lg::fatal("cannot get xcb connection");
    }
    checkotherwm();
    setup();
#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec", nullptr) == -1) die("pledge");
#endif /* __OpenBSD__ */
    scan();
    lg::info("DWM ({}{})", dwm::version::full, dwm::version::is_debug ? "-debug" : "");
    loop->run();
    cleanup();
    XCloseDisplay(dpy);
    if (need_restart) {
        lg::info("Restarting dwm\n");
        fclose(lg::log_file);
        if (execvp(argv[0], argv)) lg::fatal("could not restart dwm:");
    }

    lg::info("Shutdown complete");
    fclose(lg::log_file);
    return EXIT_SUCCESS;
}

void centeredmaster(MonitorPtr const &m) {
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
    mw = (unsigned)m->window_size.w;
    mx = 0;
    my = 0;
    tw = mw;

    if (std::cmp_greater(n, m->nmaster)) {
        /* go mfact box in the center if more than nmaster clients */
        mw = m->nmaster ? (unsigned)((float)m->window_size.w * m->mfact) : 0;
        tw = (unsigned)m->window_size.w - mw;

        if (n - (unsigned)m->nmaster > 1) {
            /* only one client */
            mx = ((unsigned)m->window_size.w - mw) / 2;
            tw = ((unsigned)m->window_size.w - mw) / 2;
        }
    }

    oty = 0;
    ety = 0;
    for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (std::cmp_less(i, m->nmaster)) {
            /* nmaster clients are stacked vertically, in the center
             * of the screen */
            h = ((unsigned)m->window_size.h - my) / (std::min(n, (unsigned)m->nmaster) - i);
            resize(c,
                {
                    (int)((unsigned)m->window_size.x + mx),
                    (int)((unsigned)m->window_size.y + my),
                    (int)(mw - (unsigned)(2 * c->bw)),
                    (int)(h - (unsigned)(2 * c->bw)),
                },
                0);
            // TODO(dk949): This is a guard against creating too many clients.
            //              Do something if there's too many clients!
            // TODO(dk949): make this cfact aware
            if (my + HEIGHT(c) < (unsigned)m->window_size.h) my += HEIGHT(c);
        } else {
            /* stack clients are stacked vertically */
            if ((i - (unsigned)m->nmaster) % 2) {
                h = ((unsigned)m->window_size.h - ety) / ((1 + n - i) / 2);
                resize(c,
                    {
                        m->window_size.x,
                        (int)((unsigned)m->window_size.y + ety),
                        (int)(tw - (unsigned)(2 * c->bw)),
                        (int)(h - (unsigned)(2 * c->bw)),
                    },
                    0);
                // TODO(dk949): This is a guard against creating too many clients.
                //              Do something if there's too many clients!
                // TODO(dk949): make this cfact aware
                if (ety + HEIGHT(c) < (unsigned)m->window_size.h) ety += HEIGHT(c);
            } else {
                h = ((unsigned)m->window_size.h - oty) / ((1 + n - i) / 2);
                resize(c,
                    {
                        (int)((unsigned)m->window_size.x + mx + mw),
                        (int)((unsigned)m->window_size.y + oty),
                        (int)(tw - (unsigned)(2 * c->bw)),
                        (int)(h - (unsigned)(2 * c->bw)),
                    },
                    0);
                if (oty + HEIGHT(c) < (unsigned)m->window_size.h) oty += HEIGHT(c);
            }
        }
    }
}

void centeredfloatingmaster(MonitorPtr const &m) {
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
    if (std::cmp_greater(n, m->nmaster)) {
        /* go mfact box in the center if more than nmaster clients */
        if (m->window_size.w > m->window_size.h) {
            mw = m->nmaster ? (unsigned)((float)m->window_size.w * m->mfact) : 0;
            mh = m->nmaster ? (unsigned)(m->window_size.h * 0.9) : 0;
        } else {
            mh = m->nmaster ? (unsigned)((float)m->window_size.h * m->mfact) : 0;
            mw = m->nmaster ? (unsigned)(m->window_size.w * 0.9) : 0;
        }
        mx = mxo = ((unsigned)m->window_size.w - mw) / 2;
        my = myo = ((unsigned)m->window_size.h - mh) / 2;
    } else {
        /* go fullscreen if all clients are in the master area */
        mh = (unsigned)m->window_size.h;
        mw = (unsigned)m->window_size.w;
        mx = mxo = 0;
        my = myo = 0;
    }

    for (i = tx = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (std::cmp_less(i, m->nmaster)) {
            /* nmaster clients are stacked horizontally, in the center
             * of the screen */
            w = (mw + mxo - mx) / (std::min(n, (unsigned)m->nmaster) - i);
            resize(c,
                {
                    (int)((unsigned)m->window_size.x + mx),
                    (int)((unsigned)m->window_size.y + my),
                    (int)(w - (unsigned)(2 * c->bw)),
                    (int)(mh - (unsigned)(2 * c->bw)),
                },
                0);
            mx += WIDTH(c);
        } else {
            /* stack clients are stacked horizontally */
            w = ((unsigned)m->window_size.w - tx) / (n - i);
            resize(c,
                {
                    (int)((unsigned)m->window_size.x + tx),
                    m->window_size.y,
                    (int)(w - (unsigned)(2 * c->bw)),
                    m->window_size.h - (2 * c->bw),
                },
                0);
            tx += WIDTH(c);
        }
    }
}
