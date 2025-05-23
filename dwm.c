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
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */

#include "dwm.h"

#include "layout.h"
#include "mapping.h"
#include "st.h"

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

#ifdef XINERAMA
#    include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include "drw.h"
#include "util.h"

#ifdef ASOUND
#    include "volc.h"
#endif /* ASOUND */

#include "backlight.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                 \
    ((mask) & ~(numlockmask | LockMask) \
        & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                   \
    (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) \
        * MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define LENGTH(X) (sizeof(X) / sizeof(X)[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X)  ((X)->w + 2 * (X)->bw + gappx)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw + gappx)
#define TAGMASK   ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)  (drw_fontset_getwidth(drw, (X)) + lrpad)
#ifndef VERSION
#    define VERSION "unknown"
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

#define PROGRESS_FADE 0, 0, 0


/* function declarations */

static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachaside(Client *c);
static void attachstack(Client *c);
static int avgheight(Display *dpy);
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
static void pop(Client *);
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
static void sigchld(int unused);
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
static int sw, sh;                                                     /* X display screen geometry width, height */
static int barHeight, blw = 0, selBarNameX = -1, selBarNameWidth = -1; /* bar geometry */
static int lrpad;                                                      /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static int notified = SelfNotifyNone;
static void (*self_notify_handler[SelfNotifyLast])(void) = {
    [SelfNotifyNone] = NULL,
    [SelfNotifyFadeBar] = handle_notifyself_fade_anim,
};
static void (*handler[LASTEvent])(XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [UnmapNotify] = unmapnotify,
};
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
static char *log_dir = NULL;
FILE *log_file = NULL;

/* configuration, allows nested code to access above variables */
#include "config.h"

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
    char const *class;
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
    class = ch.res_class ? ch.res_class : broken;
    instance = ch.res_name ? ch.res_name : broken;

    for (i = 0; i < LENGTH(rules); i++) {
        r = &rules[i];
        if ((!r->title || strstr(c->name, r->title)) && (!r->class || strstr(class, r->class))
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
                        view(&((Arg) {.ui = newtagset}));
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
    *w = MAX(1, *w);
    *h = MAX(1, *h);
    if (interact) {
        if (*x > sw) {
            *x = sw - WIDTH(c);
        }
        if (*y > sh) {
            *y = sh - HEIGHT(c);
        }
        if (*x + *w + 2 * c->bw < 0) {
            *x = 0;
        }
        if (*y + *h + 2 * c->bw < 0) {
            *y = 0;
        }
    } else {
        if (*x >= m->wx + m->ww) {
            *x = m->wx + m->ww - WIDTH(c);
        }
        if (*y >= m->wy + m->wh) {
            *y = m->wy + m->wh - HEIGHT(c);
        }
        if (*x + *w + 2 * c->bw <= m->wx) {
            *x = m->wx;
        }
        if (*y + *h + 2 * c->bw <= m->wy) {
            *y = m->wy;
        }
    }
    if (*h < barHeight) {
        *h = barHeight;
    }
    if (*w < barHeight) {
        *w = barHeight;
    }
    if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
        /* see last two sentences in ICCCM 4.1.2.3 */
        baseismin = c->basew == c->minw && c->baseh == c->minh;
        if (!baseismin) { /* temporarily remove base dimensions */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for aspect limits */
        if (c->mina > 0 && c->maxa > 0) {
            if (c->maxa < (float)*w / *h) {
                *w = *h * c->maxa + 0.5;
            } else if (c->mina < (float)*h / *w) {
                *h = *w * c->mina + 0.5;
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
        *w = MAX(*w + c->basew, c->minw);
        *h = MAX(*h + c->baseh, c->minh);
        if (c->maxw) {
            *w = MIN(*w, c->maxw);
        }
        if (c->maxh) {
            *h = MIN(*h, c->maxh);
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
        if (m->lt[m->sellt]->arrange == monocle)
            st_make_opaque(dpy, termclass, m);
        else
            st_make_transparent(dpy, termclass, m);
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

int avgheight(Display *dpy) {
#ifdef XINERAMA
    if (XineramaIsActive(dpy)) {
        int scr_count;
        XineramaScreenInfo *screens = XineramaQueryScreens(dpy, &scr_count);
        double out = 0;
        for (int i = 0; i < scr_count; i++)
            out += screens->width;
        XFree(screens);
        return out / (double)scr_count;
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
    XMoveResizeWindow(dpy, p->win, p->x, p->y, p->w, p->h);
    configure(p);
    updateclientlist();
}

void unswallow(Client *c) {
    c->win = c->swallowing->win;

    free(c->swallowing);
    c->swallowing = NULL;

    updatetitle(c);
    updatesizehints(c);
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    configure(c);
    setclientstate(c, NormalState);
}

void bright_dec(Arg const *arg) {
    int ret;
    if ((ret = bright_dec_(arg->f))) {
        WARN("Function bright_dec_(const Arg *arg) from backlight.h returned %d", ret);
        return;
    }
    double newval;
    if ((ret = bright_get_(&newval))) {
        WARN("Function bright_get_(const Arg *arg) from backlight.h returned %d", ret);
        return;
    }
    drawprogress(100, (unsigned long long)newval, SchemeBrightProgress);
}

void bright_inc(Arg const *arg) {
    int ret;
    if ((ret = bright_inc_(arg->f))) {
        WARN("Function bright_inc_(const Arg *arg) from backlight.h returned %d", ret);
        return;
    }
    double newval;
    if ((ret = bright_get_(&newval))) {
        WARN("Function bright_get_(const Arg *arg) from backlight.h returned %d", ret);
        return;
    }
    drawprogress(100, (unsigned long long)newval, SchemeBrightProgress);
}

void bright_set(Arg const *arg) {
    int ret;
    if ((ret = bright_set_(arg->f))) {
        WARN("Function bright_set_(const Arg *arg) from backlight.h returned %d", ret);
        return;
    }
    drawprogress(100, (unsigned long long)arg->f, SchemeBrightProgress);
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
        } while (ev->x >= x && ++i < LENGTH(tags));
        if (i < LENGTH(tags)) {
            click = ClkTagBar;
            arg.ui = 1 << i;
        } else if (ev->x < x + blw) {
            click = ClkLtSymbol;
        } else if (ev->x > selmon->ww - (int)TEXTW(stext)) {
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
            buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
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
    Arg a = {.ui = ~0};
    Layout foo = {"", NULL};
    Monitor *m;
    size_t i;

    view(&a);
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
    for (i = 0; i < LENGTH(colors); i++) {
        free(scheme[i]);
    }
    XDestroyWindow(dpy, wmcheckwin);
    drw_free(drw);
    XSync(dpy, False);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
#ifdef ASOUND
    volc_deinit(volc);
#endif /* ASOUND */
    free(log_dir);
    fclose(log_file);
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
    free(mon);
}

void clientmessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;

    Client *c = wintoclient(cme->window);

    if (!c) return;

    if (cme->message_type == netatom[NetWMState]) {
        if (cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen]) {
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
            drw_resize(drw, sw, barHeight);
            updatebars();
            for (m = mons; m; m = m->next) {
                for (c = m->clients; c; c = c->next) {
                    if (c->isfullscreen) {
                        resizeclient(c, m->mx, m->my, m->mw, m->mh);
                    }
                }
                XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, barHeight);
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
                c->x = m->mx + ev->x;
            }
            if (ev->value_mask & CWY) {
                c->oldy = c->y;
                c->y = m->my + ev->y;
            }
            if (ev->value_mask & CWWidth) {
                c->oldw = c->w;
                c->w = ev->width;
            }
            if (ev->value_mask & CWHeight) {
                c->oldh = c->h;
                c->h = ev->height;
            }
            if ((c->x + c->w) > m->mx + m->mw && c->isfloating) {
                c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
            }
            if ((c->y + c->h) > m->my + m->mh && c->isfloating) {
                c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
            }
            if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight))) {
                configure(c);
            }
            if (ISVISIBLE(c)) {
                XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
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
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
    XSync(dpy, False);
}

Monitor *createmon(void) {
    Monitor *m;
    unsigned int i;

    m = ecalloc(1, sizeof(Monitor));
    m->tagset[0] = m->tagset[1] = 1;
    m->mfact = mfact;
    m->nmaster = nmaster;
    m->showbar = showbar;
    m->topbar = topbar;
    m->lt[0] = &layouts[0];
    m->lt[1] = &layouts[1 % LENGTH(layouts)];
    strncpy(m->layoutSymbol, layouts[0].symbol, sizeof m->layoutSymbol);
    m->pertag = ecalloc(1, sizeof(Pertag));
    m->pertag->curtag = m->pertag->prevtag = 1;

    for (i = 0; i <= LENGTH(tags); i++) {
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

    if (!*tc) WARN("Client `%s` was not attached, c->next %s!!!", c->name, c->next ? "is not null" : "is null");
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

// TODO: handle the case where the tags overlap with status
//       (common if monitor is vertical)
void drawbar(Monitor *m) {
    int x;
    int w;
    int tw = 0;
    int boxs = drw->fonts->h / 9;
    int boxw = drw->fonts->h / 6 + 2;
    unsigned int i;
    unsigned int occ = 0;
    unsigned int urg = 0;

    /* draw status first so it can be overdrawn by tags later */
    if (m == selmon) { /* status is only drawn on selected monitor */
        drw_setscheme(drw, scheme[SchemeStatus]);
        tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
        drw_text(drw, m->ww - tw, 0, tw, barHeight, 0, stext, 0);
    }

    for (Client *c = m->clients; c; c = c->next) {
        occ |= c->tags;
        if (c->isurgent) {
            urg |= c->tags;
        }
    }
    x = 0;
    for (i = 0; i < LENGTH(tags); i++) {
        w = TEXTW(tags[i]);
        drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeTagsSel : SchemeTagsNorm]);
        drw_text(drw, x, 0, w, barHeight, lrpad / 2, tags[i], urg & 1 << i);
        if (occ & 1 << i) {
            drw_rect(drw, x + boxs, boxs, boxw, boxw, m == selmon && selmon->sel && selmon->sel->tags & 1 << i, urg & 1 << i);
        }
        x += w;
    }
    w = blw = TEXTW(m->layoutSymbol);
    drw_setscheme(drw, scheme[SchemeTagsNorm]);
    x = drw_text(drw, x, 0, w, barHeight, lrpad / 2, m->layoutSymbol, 0);

    if ((w = m->ww - tw - x) > barHeight) {
        if (m->sel) {
            drw_setscheme(drw, scheme[m == selmon ? SchemeInfoSel : SchemeInfoNorm]);
            drw_text(drw, x, 0, w, barHeight, lrpad / 2, m->sel->name, 0);
            if (m->sel->isfloating) {
                drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
            }
        } else {
            drw_setscheme(drw, scheme[SchemeInfoNorm]);
            drw_rect(drw, x, 0, w, barHeight, 1, 1);
        }
    }
    if (m == selmon) {
        selBarNameX = x;
        selBarNameWidth = w;
    }
    drw_map(drw, m->barwin, 0, 0, m->ww, barHeight);
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

    if (selBarNameX <= 0 || selBarNameWidth <= 0) return;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    if (t != 0) {
        total = t;
        current = c;
        last = now;
        cscheme = s;
    }

    if (total > 0 && (timespecdiff(&now, &last) < progress_fade_time)) {
        int x = selBarNameX, y = 0, w = selBarNameWidth, h = barHeight; /*progress rectangle*/
        int fg = 0;
        int bg = 1;
        drw_setscheme(drw, scheme[cscheme]);

        drw_rect(drw, x, y, w, h, 1, bg);
        drw_rect(drw, x, y, ((double)w * (double)current) / (double)total, h, 1, fg);

        drw_map(drw, selmon->barwin, x, y, w, h);
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

void focusmon(Arg const *arg) {
    Monitor *m;

    if (!mons->next) {
        return;
    }
    if ((m = dirtomon(arg->i)) == selmon) {
        return;
    }
    unfocus(selmon->sel, 0);
    selmon = m;

    /* move cursor to the center of the new monitor */
    XWarpPointer(dpy, 0, selmon->barwin, 0, 0, 0, 0, selmon->ww / 2, selmon->wh / 2);
    focus(NULL);
}

void focusstack(Arg const *arg) {
    Client *c = NULL;
    Client *i;

    if (!selmon->sel) {
        return;
    }
    if (arg->i > 0) {
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
        if (nitems != 0) atom = *(Atom *)p;
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

    if (XGetWindowProperty(dpy,
            w,
            wmatom[WMState],
            0L,
            2L,
            False,
            wmatom[WMState],
            &real,
            &format,
            &n,
            &extra,
            (unsigned char **)&p)
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
    } else {
        if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
            strncpy(text, *list, size - 1);
            XFreeStringList(list);
        }
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
    {
        unsigned int i;
        unsigned int j;
        unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
        KeyCode code;
        XK_r;

        XUngrabKey(dpy, AnyKey, AnyModifier, root);
        for (i = 0; i < LENGTH(keys); i++) {
            if ((code = XKeysymToKeycode(dpy, keys[i].keysym))) {
                for (j = 0; j < LENGTH(modifiers); j++) {
                    XGrabKey(dpy, code, keys[i].mod | modifiers[j], root, True, GrabModeAsync, GrabModeAsync);
                }
            }
        }
    }
}

void setmaster(Arg const *arg) {
    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(arg->i, 0);
    arrange(selmon);
}

void iconify(Arg const *_) {
    if (!XIconifyWindow(dpy, selmon->sel->win, screen)) DEBUG_PRINTF("Could not iconify %s", selmon->sel->name);
}

void incnmaster(Arg const *arg) {
    setmaster(&(Arg) {.i = MAX(selmon->nmaster + arg->i, 0)});
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
            keys[i].func(&(keys[i].arg));
        }
    }
}

void killclient(Arg const *arg) {
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
    Client *c;
    Client *t = NULL;
    Client *term = NULL;
    Window trans = None;
    XWindowChanges wc;

    c = ecalloc(1, sizeof(Client));
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

    if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw) {
        c->x = c->mon->mx + c->mon->mw - WIDTH(c);
    }
    if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh) {
        c->y = c->mon->my + c->mon->mh - HEIGHT(c);
    }
    c->x = MAX(c->x, c->mon->mx);
    /* only fix client y-offset, if the client center might cover the bar */
    c->y = MAX(c->y,
        ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx) && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww))
            ? barHeight
            : c->mon->my);
    c->bw = borderpx;

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
    XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
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

    if (!XGetWindowAttributes(dpy, ev->window, &wa)) {
        return;
    }
    if (wa.override_redirect) {
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
        resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
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

void movemouse(Arg const *arg) {
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
            case MapRequest: handler[ev.type](&ev); break;
            case MotionNotify:
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
                    continue;
                }
                lasttime = ev.xmotion.time;

                nx = ocx + (ev.xmotion.x - x);
                ny = ocy + (ev.xmotion.y - y);
                if (abs(selmon->wx - nx) < snap) {
                    nx = selmon->wx;
                } else if (((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap) {
                    nx = selmon->wx + selmon->ww - WIDTH(c);
                }
                if (abs(selmon->wy - ny) < snap) {
                    ny = selmon->wy;
                } else if (((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap) {
                    ny = selmon->wy + selmon->wh - HEIGHT(c);
                }
                if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
                    && (abs(nx - c->x) > snap || abs(ny - c->y) > snap)) {
                    togglefloating(NULL);
                }
                if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
                    resize(c, nx, ny, c->w, c->h, 1);
                }
                break;
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
    static struct timespec last_print = {0};


    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (last_print.tv_sec == 0 && last_print.tv_nsec == 0) last_print = now;

    calls++;

    if (timespecdiff(&now, &last_print) < 1.0) return;

    DEBUG_PRINTF("%li events/s", calls);
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
            case XA_WM_NORMAL_HINTS: updatesizehints(c); break;
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

void quit(Arg const *arg) {
    running = 0;
    need_restart = 0;
    LOG("Initiating shutdowd");
}

void restart(Arg const *arg) {
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
            gapincr = -2 * borderpx;
            wc.border_width = 0;
        } else {
            gapoffset = gappx;
            gapincr = 2 * gappx;
        }
    }

    c->oldx = c->x;
    c->x = wc.x = x + gapoffset;
    c->oldy = c->y;
    c->y = wc.y = y + gapoffset;
    c->oldw = c->w;
    c->w = wc.width = w - gapincr;
    c->oldh = c->h;
    c->h = wc.height = h - gapincr;

    XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
    configure(c);
    XSync(dpy, False);
}

void resizemouse(Arg const *arg) {
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
            case MapRequest: handler[ev.type](&ev); break;
            case MotionNotify:
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
                    continue;
                }
                lasttime = ev.xmotion.time;

                nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
                nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
                if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
                    && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh) {
                    if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
                        && (abs(nw - c->w) > snap || abs(nh - c->h) > snap)) {
                        togglefloating(NULL);
                    }
                }
                if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
                    resize(c, c->x, c->y, nw, nh, 1);
                }
                break;
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

void rotatestack(Arg const *arg) {
    Client *c = NULL;
    Client *f;

    if (!selmon->sel) {
        return;
    }
    f = selmon->sel;
    if (arg->i > 0) {
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
            if (self_notify_handler[notified]) self_notify_handler[notified]();
        } else {
            if (XNextEvent(dpy, &ev)) break;
            if (handler[ev.type]) handler[ev.type](&ev); /* call handler */
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
        ev.xclient.data.l[0] = proto;
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
        resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
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

void setlayout(Arg const *arg) {
    if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt]) {
        selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
    }
    if (arg && arg->v) {
        selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
    }
    strncpy(selmon->layoutSymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->layoutSymbol);
    if (selmon->sel) {
        arrange(selmon);
    } else {
        drawbar(selmon);
    }
}

void setcfact(Arg const *arg) {
    float f;
    Client *c;

    c = selmon->sel;

    if (!arg || !c || !selmon->lt[selmon->sellt]->arrange) {
        return;
    }
    f = arg->f + c->cfact;
    if (arg->f == 0.0) {
        f = 1.0;
    } else if (f < 0.25 || f > 4.0) {
        return;
    }
    c->cfact = f;
    arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(Arg const *arg) {
    float f;

    if (!arg || !selmon->lt[selmon->sellt]->arrange) {
        return;
    }
    f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
    if (f < 0.05 || f > 0.95) {
        return;
    }
    if (f == 0.0) {
        f = 1.0;
    }
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
    arrange(selmon);
}

void resetmcfact(Arg const *unused) {
    (void)unused;
    if (!selmon->lt[selmon->sellt]->arrange) return;

    selmon->sel->cfact = 1.0;
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = 0.5;
    arrange(selmon);
}

void setup(void) {
    int i;
    XSetWindowAttributes wa;
    Atom utf8string;

    /* clean up any zombies immediately */
    sigchld(0);

    /*Set up logging*/
    log_dir = getLogDir();
    {
        char *log_file_name = buildString(log_dir, "dwm.log", NULL);
        log_file = fopen(log_file_name, "a");
        free(log_file_name);
        if (!log_file) die("could not open log file:");
    }


    /* init screen */
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    drw = drw_create(dpy, screen, root, sw, sh);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts))) {
        die("no fonts could be loaded.");
    }

#ifdef XBACKLIGHT
    if (bright_setup(NULL, bright_steps, bright_time))
#else
    if (bright_setup(bright_file ? bright_file : get_bright_file(), 0, 0))
#endif  // XBACKLIGHT
    {
        die("backlight setup failed");
    }

#ifdef ASOUND
    if (!(volc = volc_init(VOLC_ALL_DEFULTS))) die("volc setup failed");

#endif /* ASOUND */

    lrpad = drw->fonts->h;
    barHeight = drw->fonts->h + 2;
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
    scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
    for (i = 0; i < LENGTH(colors); i++) {
        scheme[i] = drw_scm_create(drw, colors[i], 3);
    }

    {
        // In multimonitor setups with Xinerama, the value of `sh` becomes very
        // big as all monitors are treated as a single screen
        int avg = avgheight(dpy);
        borderpx = avg / 540;
        gappx = avg / 180;
        snap = avg / 67;
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
        XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
    }
}

void sigchld(int unused) {
    (void)unused;
    if (signal(SIGCHLD, sigchld) == SIG_ERR) {
        die("can't install SIGCHLD handler:");
    }
    while (0 < waitpid(-1, NULL, WNOHANG)) {
        ;
    }
}

void redirectChildLog(char **argv) {
    char *file_name = buildString(log_dir, "/", argv[0], ".log", NULL);

    int child_fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (child_fd < 0) {
        die("Could not set up logging for child processes %s: %s", argv[0], strerror(errno));
        return;
    }
    char const div[] = "________________________________________________________________________________\n";
    if (write(child_fd, div, sizeof(div)) < 0) {
        die("Could not write to child log file %s: %s", file_name, strerror(errno));
        goto exit;
    }

    if (dup2(child_fd, STDOUT_FILENO) < 0) {
        die("Could not redirect child stdout to log file %s: %s", file_name, strerror(errno));
        goto exit;
    }
    if (dup2(child_fd, STDERR_FILENO) < 0) {
        die("Could not redirect child stdout to log file %s: %s", file_name, strerror(errno));
        goto exit;
    }
exit:
    close(child_fd);
    free(file_name);
}

void spawn(Arg const *arg) {
    if (arg->v == dmenucmd) {
        dmenumon[0] = '0' + selmon->num;
    }
    if (fork() == 0) {
        if (dpy) {
            close(ConnectionNumber(dpy));
        }
        redirectChildLog((char **)arg->v);
        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        die("failed to spawn %s:", ((char **)arg->v)[0]);
    }
}

void tag(Arg const *arg) {
    if (selmon->sel && arg->ui & TAGMASK) {
        selmon->sel->tags = arg->ui & TAGMASK;
        if (selmon->sel->switchtotag) {
            selmon->sel->switchtotag = 0;
        }
        focus(NULL);
        arrange(selmon);
    }
}

void tagmon(Arg const *arg) {
    if (!selmon->sel || !mons->next) {
        return;
    }
    sendmon(selmon->sel, dirtomon(arg->i));
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
        if (n < m->nmaster)
            mfacts += c->cfact;
        else
            sfacts += c->cfact;

    if (n == 0) return;


    if (n > m->nmaster)
        mw = m->nmaster ? m->ww * m->mfact : 0;
    else
        mw = m->ww;

    for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < m->nmaster) {
            h = (m->wh - my) * (c->cfact / mfacts);
            resize(c, m->wx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
            if (my + HEIGHT(c) < m->wh) {
                my += HEIGHT(c);
                mfacts -= c->cfact;
            }
        } else {
            h = (m->wh - ty) * (c->cfact / sfacts);
            resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw), h - (2 * c->bw), 0);
            if (ty + HEIGHT(c) < m->wh) {
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

void togglebar(Arg const *arg) {
    selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
    updatebarpos(selmon);
    XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, barHeight);
    arrange(selmon);
}

void togglefloating(Arg const *arg) {
    if (!selmon->sel) return;

    if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
        return;

    selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
    if (selmon->sel->isfloating) {
        resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w, selmon->sel->h, 0);
    }
    arrange(selmon);
}

void togglefs(Arg const *arg) {
    if (!selmon->sel) return;
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

void toggletag(Arg const *arg) {
    unsigned int newtags;

    if (!selmon->sel) {
        return;
    }
    newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
    if (newtags) {
        selmon->sel->tags = newtags;
        focus(NULL);
        arrange(selmon);
    }
}

void toggleview(Arg const *arg) {
    unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
    int i;

    if (newtagset) {
        selmon->tagset[selmon->seltags] = newtagset;

        if (newtagset == ~0) {
            selmon->pertag->prevtag = selmon->pertag->curtag;
            selmon->pertag->curtag = 0;
        }

        /* test if the user did not select the same tag */
        if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
            selmon->pertag->prevtag = selmon->pertag->curtag;
            for (i = 0; !(newtagset & 1 << i); i++) {
                ;
            }
            selmon->pertag->curtag = i + 1;
        }

        /* apply settings for this view */
        selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
        selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
        selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
        selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
        selmon->lt[selmon->sellt ^ 1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

        if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag]) {
            togglebar(NULL);
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
    DEBUG_PRINTF("restoring iconified cliend %s", c->name);
    updatetitle(c);
    updatesizehints(c);
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
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
        free(s->swallowing);
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
        XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        setclientstate(c, WithdrawnState);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
    free(c);
    if (!s) {
        arrange(m);
        focus(NULL);
        updateclientlist();
        if (switchtotag) {
            view(&((Arg) {.ui = switchtotag}));
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
    XSetWindowAttributes wa = {.override_redirect = True,
        .background_pixmap = ParentRelative,
        .event_mask = ButtonPressMask | ExposureMask};
    XClassHint ch = {"dwm", "dwm"};
    for (m = mons; m; m = m->next) {
        if (m->barwin) {
            continue;
        }
        m->barwin = XCreateWindow(dpy,
            root,
            m->wx,
            m->by,
            m->ww,
            barHeight,
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
    m->wy = m->my;
    m->wh = m->mh;
    if (m->showbar) {
        m->wh -= barHeight;
        m->by = m->topbar ? m->wy : m->wy + m->wh;
        m->wy = m->topbar ? m->wy + barHeight : m->wy;
    } else {
        m->by = -barHeight;
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
        XineramaScreenInfo *unique = NULL;

        for (n = 0, m = mons; m; m = m->next, n++) {
            ;
        }
        /* only consider unique geometries as separate screens */
        unique = ecalloc(nn, sizeof(XineramaScreenInfo));
        for (i = 0, j = 0; i < nn; i++) {
            if (isuniquegeom(unique, j, &info[i])) {
                memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
            }
        }
        XFree(info);
        nn = j;
        if (n <= nn) { /* new monitors available */
            for (i = 0; i < (nn - n); i++) {
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
                if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my || unique[i].width != m->mw
                    || unique[i].height != m->mh) {
                    dirty = 1;
                    m->num = i;
                    m->mx = m->wx = unique[i].x_org;
                    m->my = m->wy = unique[i].y_org;
                    m->mw = m->ww = unique[i].width;
                    m->mh = m->wh = unique[i].height;
                    updatebarpos(m);
                }
            }
        } else { /* less monitors available nn < n */
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
        }
        free(unique);
    } else
#endif /* XINERAMA */
    {  /* default monitor setup */
        if (!mons) {
            mons = createmon();
        }
        if (mons->mw != sw || mons->mh != sh) {
            dirty = 1;
            mons->mw = mons->ww = sw;
            mons->mh = mons->wh = sh;
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
        for (j = 0; j < modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock)) {
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
        c->mina = (float)size.min_aspect.y / size.min_aspect.x;
        c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
    } else {
        c->maxa = c->mina = 0.0;
    }
    c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void updatestatus(void) {
    if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext))) {
        strcpy(stext, "dwm-" VERSION);
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

void view(Arg const *arg) {
    int i;
    unsigned int tmptag;

    if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags]) {
        return;
    }
    selmon->seltags ^= 1; /* toggle sel tagset */
    if (arg->ui & TAGMASK) {
        selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
        selmon->pertag->prevtag = selmon->pertag->curtag;

        if (arg->ui == ~0) {
            selmon->pertag->curtag = 0;
        } else {
            for (i = 0; !(arg->ui & 1 << i); i++) {
                ;
            }
            selmon->pertag->curtag = i + 1;
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
        togglebar(NULL);
    }

    focus(NULL);
    arrange(selmon);
}

#ifdef ASOUND
void volumechange(Arg const *arg) {
    volc_volume_state_t state;
    if (arg->i == VOL_MT) {
        state = volc_volume_ctl(volc, VOLC_ALL_CHANNELS, VOLC_SAME, VOLC_CHAN_TOGGLE);
    } else {
        state = volc_volume_ctl(volc, VOLC_ALL_CHANNELS, VOLC_INC(arg->i), VOLC_CHAN_ON);
    }

    if (state.err < 0) return;

    drawprogress(100,
        (unsigned long long)state.state.volume,
        state.state.switch_pos ? SchemeInfoProgress : SchemeOffProgress);
}
#endif


pid_t winpid(Window w) {
    pid_t result = 0;

    xcb_res_client_id_spec_t spec = {0};
    spec.client = w;
    spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

    xcb_generic_error_t *e = NULL;
    xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
    xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

    if (!r) {
        return (pid_t)0;
    }

    xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
    for (; i.rem; xcb_res_client_id_value_next(&i)) {
        spec = i.data->spec;
        if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
            uint32_t *t = xcb_res_client_id_value_value(i.data);
            result = *t;
            break;
        }
    }

    free(r);

    if (result == (pid_t)-1) {
        result = 0;
    }
    return result;
}

pid_t getparentprocess(pid_t p) {
    unsigned int v = 0;

#ifdef __linux__
    FILE *f;
    char buf[256];
    snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

    if (!(f = fopen(buf, "r"))) {
        WARN("failed to open stat file %s for process %d: %s", buf, p, strerror(errno));
        return 0;
    }

    int res = fscanf(f, "%*u %*s %*c %u", &v);
    fclose(f);
    if (res != 1) {
        WARN("failed to get child process of %d: %s", p, strerror(errno));
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
     ButesToReturn = MINIMUM(T, 4 * long_length)
     bytes_left = N - (Offs + BytesToReturn)

    The  returned  value starts at byte index Offs in the property (indexing from zero), and its length in bytes is L.
    If the value for long_offset causes L to be negative, a BadValue error results.  The value of bytes_after_return is
    A, giving the number of trailing unread bytes in the stored property.

       */
    long offset = 0, length = 0;
    Bool delete = False;
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
        delete,
        req_type,
        &actual_type,
        &format,
        &nitems,
        &bytes_left,
        &data);
    if (format != 32) DEBUG_PRINTF("wrong format: %d", format);
    if (req_type != actual_type) DEBUG_PRINTF("wrong type:  expected %lu got %lu", req_type, actual_type);
    DEBUG_PRINTF("nitems = %lu, bytes_left = %lu", nitems, bytes_left);
    *size = length = bytes_left;
    XGetWindowProperty(dpy,
        c->win,
        netatom[NetWMIcon],
        offset,
        length,
        delete,
        req_type,
        &actual_type,
        &format,
        &nitems,
        &bytes_left,
        &data);
    {
        uint32_t *begin = (uint32_t *)data;
        uint32_t *end = (uint32_t *)((uint8_t *)data + *size);
        int pos = 0;
        for (uint32_t *it = begin; it != end; ++it, pos++) {
            if (pos % 2) continue;
            begin[pos / 2] = *it;
        }
    }
    return (uint32_t *)data;
}

static void dump_ppm(uint32_t *data, uint32_t w, uint32_t h, char const *path) {
    typedef union ARGB {
        struct {
            uint8_t b;
            uint8_t g;
            uint8_t r;
            uint8_t a;
        };

        uint32_t argb;
    } ARGB;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        DEBUG_PRINTF("Could not open file %s for writing", path);
        return;
    }
    fprintf(fp, "P6 %d %d 255\n", w, h);
    ARGB *argb_data = (ARGB *)data;
    int x = 0, y = 0;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            fwrite(&(argb_data[y * w + x].r), sizeof(uint8_t), 1, fp);
            fwrite(&(argb_data[y * w + x].g), sizeof(uint8_t), 1, fp);
            fwrite(&(argb_data[y * w + x].b), sizeof(uint8_t), 1, fp);
        }
        // fputc('\n', fp);
    }
    DEBUG_PRINTF("After icon, data = %dx%d", (int)data[y * w + x + 0], (int)data[y * w + x + 1]);
    fclose(fp);
}

static __attribute_maybe_unused__ void dump_raw(uint8_t *data, size_t size, char const *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        DEBUG_PRINTF("Could not open file %s for writing", path);
        return;
    }

    fwrite(data, sizeof(data[0]), size, fp);

    fclose(fp);
}

static void iconifyclient(Client *c) {
    char *icon_name;
    XGetIconName(dpy, c->win, &icon_name);
    DEBUG_PRINTF("%s wants to iconify. Icon name: %s", c->name, icon_name);
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
        DEBUG_PRINTF("icon is %dx%d, %lu bytes", (int)icon[0], (int)icon[1], size);
        dump_ppm(icon + 2, icon[0], icon[1], "/home/davidk/.cache/dwm/icon.ppm");

        XFree(icon);
    } else {
        DEBUG_PRINTF("No icon for client %s", c->name);
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
                if(selmon->sel == c) return c;
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
int xerror(Display *dpy, XErrorEvent *ee) {
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
    WARN("fatal error: request code=%d, error code=%d", ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee) {
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
    die("another window manager is already running");
    return -1;
}

void zoom(Arg const *arg) {
    Client *c = selmon->sel;

    if (!selmon->lt[selmon->sellt]->arrange || (selmon->sel && selmon->sel->isfloating)) {
        return;
    }
    if (c == nexttiled(selmon->clients)) {
        if (!c || !(c = nexttiled(c->next))) {
            return;
        }
    }
    pop(c);
}

int main(int argc, char *argv[]) {
    if (argc == 2 && !strcmp("-v", argv[1])) {
        puts("dwm-" VERSION);
        return 0;
    } else if (argc != 1) {
        fputs("usage: dwm [-v]", stderr);
        return 1;
    }
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
        WARN("no locale support");
    }
    if (!(dpy = XOpenDisplay(NULL))) {
        die("cannot open display");
    }
    if (!(xcon = XGetXCBConnection(dpy))) {
        die("cannot get xcb connection");
    }
    checkotherwm();
    setup();
#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec", NULL) == -1) die("pledge");
#endif /* __OpenBSD__ */
    scan();
    LOG("Starting DWM");
    run();
    cleanup();
    XCloseDisplay(dpy);
    if (need_restart) {
        LOG("Restarting dwm\n"
            "________________________________________________________________________________\n");
        if (execvp(argv[0], argv)) die("could not restart dwm:");
    }

    LOG("Shutdown complete");
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
    mw = m->ww;
    mx = 0;
    my = 0;
    tw = mw;

    if (n > m->nmaster) {
        /* go mfact box in the center if more than nmaster clients */
        mw = m->nmaster ? m->ww * m->mfact : 0;
        tw = m->ww - mw;

        if (n - m->nmaster > 1) {
            /* only one client */
            mx = (m->ww - mw) / 2;
            tw = (m->ww - mw) / 2;
        }
    }

    oty = 0;
    ety = 0;
    for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < m->nmaster) {
            /* nmaster clients are stacked vertically, in the center
             * of the screen */
            h = (m->wh - my) / (MIN(n, m->nmaster) - i);
            resize(c, m->wx + mx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
            my += HEIGHT(c);
        } else {
            /* stack clients are stacked vertically */
            if ((i - m->nmaster) % 2) {
                h = (m->wh - ety) / ((1 + n - i) / 2);
                resize(c, m->wx, m->wy + ety, tw - (2 * c->bw), h - (2 * c->bw), 0);
                ety += HEIGHT(c);
            } else {
                h = (m->wh - oty) / ((1 + n - i) / 2);
                resize(c, m->wx + mx + mw, m->wy + oty, tw - (2 * c->bw), h - (2 * c->bw), 0);
                oty += HEIGHT(c);
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
    if (n > m->nmaster) {
        /* go mfact box in the center if more than nmaster clients */
        if (m->ww > m->wh) {
            mw = m->nmaster ? m->ww * m->mfact : 0;
            mh = m->nmaster ? m->wh * 0.9 : 0;
        } else {
            mh = m->nmaster ? m->wh * m->mfact : 0;
            mw = m->nmaster ? m->ww * 0.9 : 0;
        }
        mx = mxo = (m->ww - mw) / 2;
        my = myo = (m->wh - mh) / 2;
    } else {
        /* go fullscreen if all clients are in the master area */
        mh = m->wh;
        mw = m->ww;
        mx = mxo = 0;
        my = myo = 0;
    }

    for (i = tx = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
        if (i < m->nmaster) {
            /* nmaster clients are stacked horizontally, in the center
             * of the screen */
            w = (mw + mxo - mx) / (MIN(n, m->nmaster) - i);
            resize(c, m->wx + mx, m->wy + my, w - (2 * c->bw), mh - (2 * c->bw), 0);
            mx += WIDTH(c);
        } else {
            /* stack clients are stacked horizontally */
            w = (m->ww - tx) / (n - i);
            resize(c, m->wx + tx, m->wy, w - (2 * c->bw), m->wh - (2 * c->bw), 0);
            tx += WIDTH(c);
        }
    }
}
