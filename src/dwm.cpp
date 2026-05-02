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

#include "colors.hpp"
#include "drw.hpp"
#include "event_queue.hpp"
#include "layout.hpp"
#include "log.hpp"
#include "mapping.hpp"
#include "proc.hpp"
#include "procstat.hpp"
#include "util.hpp"
#include "variant_utils.hpp"
#include "winpicker.hpp"
#include "xidptr.hpp"
#include "xinerama.hpp"

#include <project/config.hpp>
#include <sched.h>
#include <unistd.h>
#include <ut/resource/resource.hpp>
#include <ut/static_string/static_string.hpp>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <xcb/xcb.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <clocale>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>


#ifdef ASOUND
#    include "volc.hpp"
#endif /* ASOUND */

#include "backlight.hpp"

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>
namespace rng = std::ranges;
namespace vws = std::views;

static constexpr auto CLEANMASK(std::unsigned_integral auto mask, std::unsigned_integral auto nlmask) {
    return ((mask) & ~(nlmask | toUnsigned(LockMask))
            & (toUnsigned(ShiftMask)       //
                | toUnsigned(ControlMask)  //
                | toUnsigned(Mod1Mask)     //
                | toUnsigned(Mod2Mask)     //
                | toUnsigned(Mod3Mask)     //
                | toUnsigned(Mod4Mask)     //
                | toUnsigned(Mod5Mask)));
}

static constexpr auto INTERSECT(
    std::integral auto x, std::integral auto y, std::integral auto w, std::integral auto h, MonitorRef const &mon) {
    return std::max(0, std::min(x + w, mon->window_size.x + mon->window_size.w) - std::max(x, mon->window_size.x))
         * std::max(0, std::min(y + h, mon->window_size.y + mon->window_size.h) - std::max(y, mon->window_size.y));
}

template<std::integral I>
static constexpr auto INTERSECT(Rect<I> rect, MonitorRef const &mon) {
    return INTERSECT(rect.x, rect.y, rect.w, rect.h, mon);
}

#define TEXTW(X) (drw->fontsetGetwidth((X)) + text_padding)

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
    NetOpacity,
    NetBypassComp,
    NetOpaqueRegion,
    NetLast
}; /* EWMH atoms */

enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMChangeState, WMLast }; /* default atoms */

#define PROGRESS_FADE 0, 0, 0


/* function declarations */

static void arrange(MonitorRef const &mon);
static void arrangeMon(MonitorRef const &mon);
static void attach(Client *client);
static void attachAside(Client *client);
static void attachStack(Client *client);
static int avgHeight();
static void buttonPress(XEvent *e);
static void checkOtherWm();
static void cleanup();
static void cleanupMon(MonitorRef const &mon);
static void clientMessage(XEvent *e);
static void configureNotify(XEvent *e);
static void configureRequest(XEvent *e);
static MonitorRef createMon();
static void destroyNotify(XEvent *e);
/// Remove client `client` from the list of clients on the monitor `client` is on
static void detach(Client *client);
static void detachStack(Client *client);
static MonitorRef dirToMon(int dir);
static void drawBar(MonitorRef const &mon);
static void drawBars();
static void drawProgress(unsigned long long total, unsigned long long current, Color const *color);
static Client *ensureUnattached(Client *client);
static void enqueue(Client *client);
static void enqueueStack(Client *client);
static void enterNotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *client);
static void focusIn(XEvent *e);
static RootPointer getRootPtr();
static long getState(Window w);
static bool getTextProp(Window w, Atom atom, char *text, std::size_t size);
static void grabKeys();
static void iconifyClient(Client *client);
static void installEventHandlers();
static bool isDescProcess(pid_t parent, pid_t child);
static void keyPress(XEvent *e);
static void manage(Window w, XWindowAttributes *attrs);
static void mappingNotify(XEvent *e);
static void mapRequest(XEvent *e);
static void motionNotify(XEvent *e);
static Client *nextTagged(Client *client);
static Client *nextTiled(Client *client);
static void handle_notifyself_fade_anim(FadeBarEvent);
static void pop(Client *client);
static void propertyNotify(XEvent *e);
static MonitorRef rectToMon(Rect<int> rect);
static void restack(MonitorRef const &mon);
static void rrOutputChange(XEvent *e);
static void scan();
static void sendMon(Client *client, MonitorRef const &mon);
static void setup();
static void showHide(Client *client);
static void swallow(Client *parent, Client *child);
static Client *swallowingClient(Window w);
static Client *termForWin(Client const *w);
static double timespecDiff(const struct timespec *lhs, const struct timespec *rhs);
static void uniconifyClient(Client *client);
static void unmanage(Client *client, IsDestroyed destroyed);
static void unmapNotify(XEvent *e);
static void unswallow(Client *client);
static void updateBarPos(MonitorRef const &mon);
static void updateBars();
static void updateClientList();
static bool updateGeom();
static void updateNumLockMask();
static void updateStatus();


static pid_t winPid(Window w);
static Client *winToClient(Window w);
static MonitorRef winToMon(Window w);
static void wmChange(Client *client, XClientMessageEvent *cme);
static int dwmErrorHandler(Display *dpy, XErrorEvent *err_event);
static int xErrorDummy(Display *dpy, XErrorEvent *err_event);
static int xErrorStart(Display *dpy, XErrorEvent *err_event);

/* configuration, allows nested code to access above variables */
#include "config.hpp"

/* variables */

constexpr auto TAGMASK = (1uz << tag_symbols.size()) - 1uz;
constexpr auto BUTTONMASK = toUnsigned(ButtonPressMask) | toUnsigned(ButtonReleaseMask);
constexpr auto MOUSEMASK = BUTTONMASK | toUnsigned(PointerMotionMask);
constexpr auto cfact_min = 0.25f;
constexpr auto cfact_max = 4.0f;
constexpr auto mfact_min = 0.05f;
constexpr auto mfact_max = 0.95f;
constexpr auto full_bar = 100;
static constexpr ut::StaticString broken = "broken";
static char stext[256];
static int screen;
static int screen_w, screen_h;                                       /* X display screen geometry width, height */
static int bar_height, sel_bar_name_x = -1, sel_bar_name_width = -1; /* bar geometry */
static int text_padding;                                             /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static std::array<Atom, WMLast> wmatom;
static std::array<Atom, NetLast> netatom;
static bool need_restart = false;
static Display *dpy;
static Drw *drw;
static Monitors mons;
static MonitorRef selmon;
static Window root, wmcheckwin;
#ifdef ASOUND
static volc_t *volc;
#endif /* ASOUND */
static xcb_connection_t *xcon;
static std::filesystem::path log_dir;
static std::unique_ptr<EventLoop> loop = nullptr;
static int borderpx; /* border pixel of windows */
static int gappx;    /* gaps between windows */
static int snap;     /* snap pixel */

struct Pertag {
    unsigned int curtag, prevtag;                                             /* current and previous tag */
    std::array<int, tag_symbols.size() + 1> nmasters;                         /* number of windows in master area */
    std::array<float, tag_symbols.size() + 1> master_factors;                 /* master factor per tag */
    std::array<unsigned int, tag_symbols.size() + 1> sellts;                  /* selected layouts */
    std::array<std::array<Layout const *, 2>, tag_symbols.size() + 1> ltidxs; /* matrix of tags and layouts indexes  */
    std::array<bool, tag_symbols.size() + 1> showbars;                        /* display bar for the current tag */
};

static_assert(tag_symbols.size() <= std::numeric_limits<unsigned int>::digits - 1,
    "All tags have to fit into an unsigned int bit array");

/* function implementations */
void Client::applyRules() {

    /* rule matching */
    props.isfloating = false;
    tags = 0;
    auto hints = classHint(dpy);
    char const *class_ = hints.class_hint ? hints.class_hint.get() : broken.data();
    char const *instance = hints.instance_hint ? hints.instance_hint.get() : broken.data();

    for (auto const &rule : rules) {
        if ((!rule.title || name.contains(rule.title)) && (!rule.class_ || strstr(class_, rule.class_))
            && (!rule.instance || strstr(instance, rule.instance))) {
            props.isterminal = rule.isterminal;
            props.isfloating = rule.isfloating;
            props.noswallow = rule.noswallow;
            tags |= rule.tags;
            auto mon_it =
                rng::find_if(mons, [&](MonitorRef const &monitor) noexcept { return monitor->num == rule.monitor; });
            if (mon_it != mons.end()) mon = *mon_it;

            if (rule.switchtotag) {
                selmon = getMon();
                auto const newtagset = (rule.switchtotag == 2 || rule.switchtotag == 4)  //
                                         ? (getMon()->tagset[getMon()->sel_tags] ^ tags)
                                         : tags;
                if (newtagset != 0u && ((tags & getMon()->tagset[getMon()->sel_tags]) == 0u)) {
                    if (rule.switchtotag == 3 || rule.switchtotag == 4) {
                        switchtotag = getMon()->tagset[getMon()->sel_tags];
                    }
                    if (rule.switchtotag == 1 || rule.switchtotag == 3) {
                        view(newtagset);
                    } else {
                        getMon()->tagset[getMon()->sel_tags] = newtagset;
                        arrange(getMon());
                    }
                }
            }
        }
    }
    tags = tags & TAGMASK ? tags & TAGMASK : getMon()->tagset[getMon()->sel_tags];
}

bool Client::applySizeHints(Rect<int> *new_size, bool interact) {

    /* set minimum possible */
    new_size->w = std::max(1, new_size->w);
    new_size->h = std::max(1, new_size->h);
    if (interact) {
        if (new_size->x > screen_w) {
            new_size->x = screen_w - getWidth();
        }
        if (new_size->y > screen_h) {
            new_size->y = screen_h - getHeight();
        }
        if (new_size->x + new_size->w + (2 * border_width) < 0) {
            new_size->x = 0;
        }
        if (new_size->y + new_size->h + (2 * border_width) < 0) {
            new_size->y = 0;
        }
    } else {
        if (new_size->x >= getMon()->window_size.x + getMon()->window_size.w) {
            new_size->x = getMon()->window_size.x + getMon()->window_size.w - getWidth();
        }
        if (new_size->y >= getMon()->window_size.y + getMon()->window_size.h) {
            new_size->y = getMon()->window_size.y + getMon()->window_size.h - getHeight();
        }
        if (new_size->x + new_size->w + (2 * border_width) <= getMon()->window_size.x) {
            new_size->x = getMon()->window_size.x;
        }
        if (new_size->y + new_size->h + (2 * border_width) <= getMon()->window_size.y) {
            new_size->y = getMon()->window_size.y;
        }
    }
    new_size->h = std::max(new_size->h, bar_height);
    new_size->w = std::max(new_size->w, bar_height);
    if (resizehints || props.isfloating || (getMon()->layout_slots[getMon()->sel_layout]->arrange == nullptr)) {
        if (!hintsvalid) updateSizeHints();
        /* see last two sentences in ICCCM 4.1.2.3 */
        bool baseismin = base_width == min_width && base_height == min_height;
        if (!baseismin) { /* temporarily remove base dimensions */
            new_size->w -= base_width;
            new_size->h -= base_height;
        }
        /* adjust for aspect limits */
        if (min_aspect > 0 && max_aspect > 0) {

            if (max_aspect < static_cast<float>(new_size->w) / static_cast<float>(new_size->h)) {
                new_size->w = static_cast<int>(std::lround(static_cast<float>(new_size->h) * max_aspect));
            } else if (min_aspect < static_cast<float>(new_size->h) / static_cast<float>(new_size->w)) {
                new_size->h = static_cast<int>(std::lround(static_cast<float>(new_size->w) * min_aspect));
            }
        }
        if (baseismin) { /* increment calculation requires this */
            new_size->w -= base_width;
            new_size->h -= base_height;
        }
        /* adjust for increment value */
        if (inc_width) {
            new_size->w -= new_size->w % inc_width;
        }
        if (inc_height) {
            new_size->h -= new_size->h % inc_height;
        }
        /* restore base dimensions */
        new_size->w = std::max(new_size->w + base_width, min_width);
        new_size->h = std::max(new_size->h + base_height, min_height);
        if (max_width) {
            new_size->w = std::min(new_size->w, max_width);
        }
        if (max_height) {
            new_size->h = std::min(new_size->h, max_height);
        }
    }
    return new_size->x != size.x || new_size->y != size.y || new_size->w != size.w || new_size->h != size.h;
}

void arrange(MonitorRef const &mon) {
    if (mon) {
        showHide(mon->stack);
    } else {
        for (auto const &each_mon : mons)
            showHide(each_mon->stack);
    }
    if (mon) {
        arrangeMon(mon);
        restack(mon);
    } else {
        for (auto const &each_mon : mons)
            arrangeMon(each_mon);
    }
}

void arrangeMon(MonitorRef const &mon) {
    strncpy(mon->layoutSymbol.data(), mon->layout_slots[mon->sel_layout]->symbol, mon->layoutSymbol.max_size() - 1);
    if (mon->layout_slots[mon->sel_layout]->arrange) {
        mon->layout_slots[mon->sel_layout]->arrange(mon);
    }
}

void attach(Client *client) {
    client->next = client->getMon()->clients;
    client->getMon()->clients = client;
}

void attachAside(Client *client) {
    Client *anchor = nextTagged(client);
    if (!anchor) {
        attach(client);
        return;
    }
    client->next = anchor->next;
    anchor->next = client;
}

void attachStack(Client *client) {
    client->snext = client->getMon()->stack;
    client->getMon()->stack = client;
}

int avgHeight() {
    if (xineramaIsActive(dpy)) {
        auto screens = ScreenInfoPtr::query(dpy);
        double out = 0;
        for (auto i = 0uz; i < screens.count(); i++)
            out += screens[i].width;
        return static_cast<int>(out / static_cast<double>(screens.count()));
    } else {
        return screen_h;
    }
}

void swallow(Client *parent, Client *child) {
    if (child->props.noswallow || child->props.isterminal) {
        return;
    }

    detach(child);
    detachStack(child);

    child->setClientState(WithdrawnState);
    XUnmapWindow(dpy, parent->win);

    parent->swallowing = child;
    child->mon = parent->mon;

    Window swap_win = parent->win;
    parent->win = child->win;
    child->win = swap_win;
    parent->updateTitle();
    arrange(parent->getMon());
    XMoveResizeWindow(dpy,
        parent->win,
        parent->size.x,
        parent->size.y,
        static_cast<unsigned>(parent->size.w),
        static_cast<unsigned>(parent->size.h));
    parent->configure();
    updateClientList();
}

void unswallow(Client *client) {
    client->win = client->swallowing->win;

    delete ensureUnattached(client->swallowing);
    client->swallowing = nullptr;

    client->updateTitle();
    client->updateSizeHints();
    arrange(client->getMon());
    XMapWindow(dpy, client->win);
    XMoveResizeWindow(dpy,
        client->win,
        client->size.x,
        client->size.y,
        static_cast<unsigned>(client->size.w),
        static_cast<unsigned>(client->size.h));
    client->configure();
    client->setClientState(NormalState);
}

void bright_dec(double arg) {
    if (brightDec_(arg) != BacklightError::Ok) return;

    auto newval = std::nan("");
    if (brightGet_(&newval) != BacklightError::Ok) return;

    drawProgress(full_bar, static_cast<unsigned long long>(newval), &drw->scheme().bright_progress);
}

void bright_inc(double arg) {
    if (brightInc_(arg) != BacklightError::Ok) return;

    auto newval = std::nan("");
    if (brightGet_(&newval) != BacklightError::Ok) return;

    drawProgress(full_bar, static_cast<unsigned long long>(newval), &drw->scheme().bright_progress);
}

void bright_set(double arg) {
    if (brightSet_(arg) != BacklightError::Ok) return;

    drawProgress(full_bar, static_cast<unsigned long long>(arg), &drw->scheme().bright_progress);
}

void dmenu_run() {
    static std::array<char, 8> mon {};  // NOLINT(readability-magic-numbers)
    // clang-format off
    static std::array cmd = {
        "dmenu_run",
        "-m", std::as_const(mon).data(),
        "-fn", dmenufont,
        "-l", dmenulines,
        "-c",
        "-bw", dmenu_border,
        "-x",
        "-o", dmenuopacity,
        cmd_end,
    };
    // clang-format on
    auto [ptr, ec] = std::to_chars(mon.begin(), mon.end() - 1, selmon->num);
    if (ec != std::errc {}) {
        lg::error("Could not assign dmenu monitor: {}, defaulting to 0", std::make_error_code(ec).message());
        mon[0] = '0';
        mon[1] = 0;
    } else
        *ptr = 0;
    spawn(cmd.data());
}

void buttonPress(XEvent *e) {
    Arg arg = {0};
    XButtonPressedEvent *ev = &e->xbutton;

    auto click = Click::RootWin;
    /* focus monitor if necessary */
    if (MonitorRef mon = winToMon(ev->window); mon && mon != selmon) {
        if (selmon->sel) selmon->sel->unfocus(true);
        selmon = mon;
        focus(nullptr);
    }
    if (ev->window == selmon->barwin) {
        unsigned int idx = 0;
        int x = 0;
        do {
            x += TEXTW(tag_symbols[idx]);
        } while (std::cmp_greater_equal(ev->x, x) && ++idx < tag_symbols.size());
        if (idx < tag_symbols.size()) {
            click = Click::TagBar;
            arg = 1u << idx;
        } else if (ev->x < x + TEXTW(selmon->layoutSymbol.data())) {
            click = Click::LtSymbol;
        } else if (ev->x > selmon->window_size.w - TEXTW(stext)) {
            click = Click::StatusText;
        } else {
            click = Click::WinTitle;
        }
    } else if (auto *client = winToClient(ev->window)) {
        focus(client);
        restack(selmon);
        XAllowEvents(dpy, ReplayPointer, CurrentTime);
        click = Click::ClientWin;
    }
    for (auto const &button : buttons)
        if (click == button.click && button.button == ev->button
            && CLEANMASK(button.mask, numlockmask) == CLEANMASK(ev->state, numlockmask)) {
            auto fn_arg = click == Click::TagBar && button.arg.index() == 0 ? arg : button.arg;
            if (!variantInvoke(button.func, fn_arg))
                lg::error("Could not run button mapping: function index is {}, but arg is {}",
                    button.func.index(),
                    fn_arg.index());
        }
}

void checkOtherWm() {
    xerrorxlib = XSetErrorHandler(xErrorStart);
    /* this causes an error if some other window manager is running */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    XSetErrorHandler(dwmErrorHandler);
    XSync(dpy, False);
}

void cleanup() {
    Layout foo = {.symbol = "", .arrange = nullptr};

    view(~0u);
    selmon->layout_slots[selmon->sel_layout] = &foo;
    for (auto const &mon : mons) {
        while (mon->stack) {
            unmanage(mon->stack, IsDestroyed::no);  // XXX: Potential problems
        }
    }
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (auto const &mon : mons | vws::reverse)
        cleanupMon(mon);
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

void cleanupMon(MonitorRef const &mon) {
    // TODO(dk949): this needs to go in the Monitor destructor!
    XUnmapWindow(dpy, mon->barwin);
    XDestroyWindow(dpy, mon->barwin);
    delete mon->pertag;
}

void clientMessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;

    Client *client = winToClient(cme->window);

    if (!client) return;

    if (cme->message_type == netatom[NetWMState]) {
        if (std::cmp_equal(cme->data.l[1], netatom[NetWMFullscreen])
            || std::cmp_equal(cme->data.l[2], netatom[NetWMFullscreen])) {
            client->setFullscreen(
                (FullScreen {cme->data.l[0] == 1} /* _NET_WM_STATE_ADD    */
                    || (FullScreen {cme->data.l[0] == 2} /* _NET_WM_STATE_TOGGLE */ && !client->props.isfullscreen)));
        }
    } else if (cme->message_type == netatom[NetActiveWindow]) {
        if (client != selmon->sel && !client->props.isurgent) {
            client->setUrgent(IsUrgent::yes);
        }
    } /*else if (cme->message_type == wmatom[WMChangeState]) {
        wmChange(client, cme);
    }*/
}

void Client::configure() const {
    auto cfg_event = XConfigureEvent {
        .type = ConfigureNotify,
        .serial = 0,      // UNUSED
        .send_event = 0,  // UNUSED
        .display = dpy,
        .event = win,
        .window = win,
        .x = size.x,
        .y = size.y,
        .width = size.w,
        .height = size.h,
        .border_width = border_width,
        .above = None,
        .override_redirect = False,
    };

    XSendEvent(dpy, win, False, StructureNotifyMask, reinterpret_cast<XEvent *>(&cfg_event));
}

void configureNotify(XEvent *e) {
    XConfigureEvent *ev = &e->xconfigure;

    // TODO(dk949): Figure out what this means??
    /* TODO: updateGeom handling sucks, needs to be simplified */
    if (ev->window != root) return;

    bool dirty = screen_w != ev->width || screen_h != ev->height;
    screen_w = ev->width;
    screen_h = ev->height;
    if (updateGeom() || dirty) {
        drw->resize(screen_w, bar_height);
        updateBars();
        for (auto const &mon : mons) {
            for (Client *client = mon->clients; client; client = client->next) {
                if (client->props.isfullscreen == FullScreen::on) {
                    client->resizeClient(mon->monitor_size);
                }
            }
            XMoveResizeWindow(dpy,
                mon->barwin,
                mon->window_size.x,
                mon->bar_y,
                static_cast<unsigned>(mon->window_size.w),
                static_cast<unsigned>(bar_height));
        }
        focus(nullptr);
        arrange(nullptr);
    }
}

void rrOutputChange(XEvent *e) {
    auto *ev = reinterpret_cast<XRROutputChangeNotifyEvent *>(e);
    if (ev->subtype != RRNotify_OutputChange) return;
    auto *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) {
        lg::warn("randr: no screen resources");
        return;
    }
    auto *info = XRRGetOutputInfo(dpy, res, ev->output);
    char const *state = ev->connection == RR_Connected    ? "connected"
                      : ev->connection == RR_Disconnected ? "disconnected"
                                                          : "unknown";
    lg::info("randr: output '{}' {}", info ? info->name : "(unknown)", state);
    if (info) XRRFreeOutputInfo(info);
    XRRFreeScreenResources(res);
}

void configureRequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges changes;

    if (auto *client = winToClient(ev->window)) {
        if (ev->value_mask & CWBorderWidth) {
            client->border_width = ev->border_width;
        } else if (client->props.isfloating || !selmon->layout_slots[selmon->sel_layout]->arrange) {
            auto mon = client->getMon();
            if (ev->value_mask & CWX) {
                client->old_size.x = client->size.x;
                client->size.x = mon->monitor_size.x + ev->x;
            }
            if (ev->value_mask & CWY) {
                client->old_size.y = client->size.y;
                client->size.y = mon->monitor_size.y + ev->y;
            }
            if (ev->value_mask & CWWidth) {
                client->old_size.w = client->size.w;
                client->size.w = ev->width;
            }
            if (ev->value_mask & CWHeight) {
                client->old_size.h = client->size.h;
                client->size.h = ev->height;
            }
            if ((client->size.x + client->size.w) > mon->monitor_size.x + mon->monitor_size.w
                && client->props.isfloating) {
                client->size.x = mon->monitor_size.x
                               + ((mon->monitor_size.w / 2) - (client->getWidth() / 2)); /* center in x direction */
            }
            if ((client->size.y + client->size.h) > mon->monitor_size.y + mon->monitor_size.h
                && client->props.isfloating) {
                client->size.y = mon->monitor_size.y
                               + ((mon->monitor_size.h / 2) - (client->getHeight() / 2)); /* center in y direction */
            }
            if ((ev->value_mask & (toUnsigned(CWX) | toUnsigned(CWY)))
                && !(ev->value_mask & (toUnsigned(CWWidth) | toUnsigned(CWHeight)))) {
                client->configure();
            }
            if (client->isVisible()) {
                XMoveResizeWindow(dpy,
                    client->win,
                    client->size.x,
                    client->size.y,
                    static_cast<unsigned>(client->size.w),
                    static_cast<unsigned>(client->size.h));
            }
        } else {
            client->configure();
        }
    } else {
        changes.x = ev->x;
        changes.y = ev->y;
        changes.width = ev->width;
        changes.height = ev->height;
        changes.border_width = ev->border_width;
        changes.sibling = ev->above;
        changes.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, static_cast<unsigned int>(ev->value_mask), &changes);
    }
    XSync(dpy, False);
}

MonitorRef createMon() {
    // TODO(dk949): Some of this should probably be in Monitor constructor

    auto mon = std::make_shared<Monitor>();
    mon->tagset[0] = mon->tagset[1] = 1;
    mon->master_factor = master_factor;
    mon->nmaster = nmaster;
    mon->showbar = showbar;
    mon->topbar = topbar;
    mon->layout_slots[0] = &layouts[0];
    mon->layout_slots[1] = &layouts[1 % layouts.size()];
    strncpy(mon->layoutSymbol.data(), layouts[0].symbol, mon->layoutSymbol.max_size());
    mon->pertag = new Pertag {};
    mon->pertag->curtag = mon->pertag->prevtag = 1;

    for (unsigned int i = 0; i <= tag_symbols.size(); i++) {
        mon->pertag->nmasters[i] = mon->nmaster;
        mon->pertag->master_factors[i] = mon->master_factor;

        mon->pertag->ltidxs[i][0] = mon->layout_slots[0];
        mon->pertag->ltidxs[i][1] = mon->layout_slots[1];
        mon->pertag->sellts[i] = mon->sel_layout;

        mon->pertag->showbars[i] = mon->showbar;
    }

    return mon;
}

void destroyNotify(XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    if (auto *client = winToClient(ev->window)) {
        unmanage(client, IsDestroyed::yes);
    } else if (auto *swallower = swallowingClient(ev->window)) {
        unmanage(swallower->swallowing, IsDestroyed::yes);
    }
}

void detach(Client *client) {
    Client **prev;

    for (prev = &client->getMon()->clients; *prev && *prev != client; prev = &(*prev)->next) { }

    if (!*prev)
        lg::warn("Client `{}` was not attached, client->next {}!!!",
            client->name,
            client->next ? "is not null" : "is null");
    *prev = client->next;
}

void detachStack(Client *client) {
    Client **prev;
    Client *iter;

    for (prev = &client->getMon()->stack; *prev && *prev != client; prev = &(*prev)->snext) { }
    *prev = client->snext;

    if (client == client->getMon()->sel) {
        for (iter = client->getMon()->stack; iter && !iter->isVisible(); iter = iter->snext) { }
        client->getMon()->sel = iter;
    }
}

MonitorRef dirToMon(int dir) {
    auto mon_it = rng::find(mons, selmon);
    if (mon_it == mons.end()) return nullptr;
    if (dir > 0) {
        if (mon_it == mons.end() - 1)
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
void drawBar(MonitorRef const &mon) {
    int text_width = 0;
    auto const boxs = drw->fonts().h / 9;
    auto const boxw = (drw->fonts().h / 6) + 2;
    unsigned int occ = 0;
    unsigned int urg = 0;

    if (!mon->showbar) return;

    /* draw status first so it can be overdrawn by tags later */
    if (mon == selmon) {                              /* status is only drawn on selected monitor */
        drw->setColor(&drw->scheme().status);
        text_width = TEXTW(stext) - text_padding + 2; /* 2px right padding */
        drw->drawText(mon->window_size.w - text_width, 0, text_width, bar_height, 0, stext, false);
    }

    for (Client *client = mon->clients; client; client = client->next) {
        occ |= client->tags;
        if (client->props.isurgent == IsUrgent::yes) {
            urg |= client->tags;
        }
    }
    int x = 0;
    for (unsigned i = 0; i < tag_symbols.size(); i++) {
        int w = TEXTW(tag_symbols[i]);
        if (mon->tagset[mon->sel_tags] & 1u << i)
            drw->setColor(&drw->scheme().tags_sel);
        else
            drw->setColor(&drw->scheme().tags_norm);

        drw->drawText(x, 0, w, bar_height, text_padding / 2, tag_symbols[i], (urg & 1u << i) != 0u);
        if (occ & 1u << i) {
            drw->drawRect(x + boxs,
                boxs,
                boxw,
                boxw,
                mon == selmon && (selmon->sel != nullptr) && ((selmon->sel->tags & 1u << i) != 0u),
                (urg & 1u << i) != 0);
        }
        x += w;
    }
    drw->setColor(&drw->scheme().tags_norm);
    x = drw->drawText(x, 0, TEXTW(mon->layoutSymbol.data()), bar_height, text_padding / 2, mon->layoutSymbol.data(), false);

    int w = mon->window_size.w - text_width - x;
    if (w > bar_height) {
        if (mon->sel) {
            drw->setColor(mon == selmon ? &drw->scheme().info_sel : &drw->scheme().info_norm);
            drw->drawText(x, 0, w, bar_height, text_padding / 2, mon->sel->name.data(), false);
            if (mon->sel->props.isfloating) {
                drw->drawRect(x + boxs, boxs, boxw, boxw, mon->sel->props.isfixed, false);
            }
        } else {
            drw->setColor(&drw->scheme().info_norm);
            drw->drawRect(x, 0, w, bar_height, /*filled*/ true, /*invert*/ true);
        }
    }
    if (mon == selmon) {
        sel_bar_name_x = x;
        sel_bar_name_width = w;
    }
    drw->map(mon->barwin, {.x = 0, .y = 0, .w = mon->window_size.w, .h = bar_height});
    drawProgress(PROGRESS_FADE);
}

void drawBars() {
    for (auto const &mon : mons) {
        drawBar(mon);
    }
}

// TODO(dk949): THIS NEEDS TO BE FIXED!!!!!
//              (also handle_notifyself_fade_anim)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void drawProgress(unsigned long long new_total, unsigned long long new_current, Color const *color) {
    static unsigned long long total;
    static unsigned long long current;
    static struct timespec last;
    static Color const *cscheme;

    if (sel_bar_name_x <= 0 || sel_bar_name_width <= 0) return;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);  // NOLINT(misc-include-cleaner)

    if (new_total != 0) {
        total = new_total;
        current = new_current;
        last = now;
        cscheme = color;
    }

    if (total > 0 && (timespecDiff(&now, &last) < progress_fade_time)) {
        int x = sel_bar_name_x;
        int y = 0;
        int w = sel_bar_name_width;
        int h = bar_height; /*progress rectangle*/
        int fg = 0;
        int bg = 1;
        drw->setColor(cscheme);

        drw->drawRect(x, y, w, h, true, bg != 0);
        drw->drawRect(x,
            y,
            static_cast<int>((static_cast<double>(w) * static_cast<double>(current)) / static_cast<double>(total)),
            h,
            true,
            fg != 0);

        drw->map(selmon->barwin, {.x = x, .y = y, .w = w, .h = h});
        loop->push(FadeBarEvent());
    }
}

Client *ensureUnattached(Client *client) {
    for (auto const &mon : mons)
        for (auto *cc = mon->clients; cc; cc = cc->next)
            if (cc == client) lg::error("Client {} still attached", client->name.view());
    return client;
}

void enqueue(Client *client) {
    Client *last;
    for (last = client->getMon()->clients; last && last->next; last = last->next) { }
    if (last) {
        last->next = client;
        client->next = nullptr;
    }
}

void enqueueStack(Client *client) {
    Client *last;
    for (last = client->getMon()->stack; last && last->snext; last = last->snext) { }
    if (last) {
        last->snext = client;
        client->snext = nullptr;
    }
}

void enterNotify(XEvent *e) {
    Client *client;
    XCrossingEvent *ev = &e->xcrossing;

    if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root) {
        return;
    }
    client = winToClient(ev->window);
    auto mon = client ? client->getMon() : winToMon(ev->window);
    if (mon != selmon) {
        if (selmon->sel) selmon->sel->unfocus(true);
        selmon = mon;
    } else if (!client || client == selmon->sel) {
        return;
    }
    focus(client);
}

void expose(XEvent *e) {
    XExposeEvent *ev = &e->xexpose;

    if (ev->count == 0)
        if (auto mon = winToMon(ev->window)) drawBar(mon);
}

void focus(Client *client) {
    if (!client || !client->isVisible()) {
        for (client = selmon->stack; client && !client->isVisible(); client = client->snext) {
            ;
        }
    }
    if (selmon->sel && selmon->sel != client) selmon->sel->unfocus(false);

    if (client) {
        auto mon_ptr = client->getMon();
        if (mon_ptr != selmon) selmon = mon_ptr;

        if (client->props.isurgent == IsUrgent::yes) client->setUrgent(IsUrgent::no);

        detachStack(client);
        attachStack(client);
        client->grabButtons(true);
        XSetWindowBorder(dpy, client->win, drw->scheme().sel.border.pixel);
        client->setFocus();
    } else {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
    selmon->sel = client;
    drawBars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusIn(XEvent *e) {
    XFocusChangeEvent *ev = &e->xfocus;

    if (selmon->sel && ev->window != selmon->sel->win) {
        selmon->sel->setFocus();
    }
}

void focusMon(int arg) {
    if (mons.size() == 1) return;
    MonitorRef mon;
    auto selmon_it = rng::find(mons, selmon);
    if (selmon_it == mons.end()) lg::fatal("Selected monitor is not in the monitor array");
    auto selmon_idx = std::distance(mons.begin(), selmon_it);
    selmon_idx += arg;
    if (std::ssize(mons) <= selmon_idx) selmon_idx = 0;
    if (selmon_idx < 0) selmon_idx = std::ssize(mons) - 1;

    focusMonAbs(static_cast<unsigned>(selmon_idx));
}

void focusMonAbs(unsigned arg) {
    if (mons.size() == 1) return;
    if (mons.size() <= arg) return;
    if (mons[arg] == selmon) return;
    if (selmon->sel) selmon->sel->unfocus(false);
    selmon = mons[arg];
    /* move cursor to the center of the new monitor */
    XWarpPointer(dpy, 0, selmon->barwin, 0, 0, 0, 0, selmon->window_size.w / 2, selmon->window_size.h / 2);
    focus(nullptr);
}

void focusStack(int arg) {
    Client *client = nullptr;
    Client *iter;

    if (!selmon->sel || selmon->sel->props.isfullscreen) {
        return;
    }
    if (arg > 0) {
        for (client = selmon->sel->next; client && !client->isVisible(); client = client->next) {
            ;
        }
        if (!client) {
            for (client = selmon->clients; client && !client->isVisible(); client = client->next) {
                ;
            }
        }
    } else {
        for (iter = selmon->clients; iter != selmon->sel; iter = iter->next) {
            if (iter->isVisible()) {
                client = iter;
            }
        }
        if (!client) {
            for (; iter; iter = iter->next) {
                if (iter->isVisible()) {
                    client = iter;
                }
            }
        }
    }
    if (client) {
        focus(client);
        restack(selmon);
    }
}

Atom Client::getAtomProp(Atom prop) const {
    int fmt;
    unsigned long bytes_left;
    unsigned long nitems;
    unsigned char *prop_data = nullptr;
    Atom type;
    Atom atom = None;

    if (!XGetWindowProperty(dpy, win, prop, 0L, sizeof atom, False, XA_ATOM, &type, &fmt, &nitems, &bytes_left, &prop_data)
        && prop_data) {
        // If nitems is 0, no property was returned
        if (nitems != 0) atom = *reinterpret_cast<Atom *>(prop_data);
        XFree(prop_data);
    }
    return atom;
}

RootPointer getRootPtr() {
    int dummy_int {};
    unsigned int dui {};
    Window dummy {};
    int x = 0;
    int y = 0;
    auto succ = XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, &dummy_int, &dummy_int, &dui) == True;
    return {.x = x, .y = y, .success = succ};
}

long getState(Window w) {
    int format;
    long result = -1;
    unsigned char *prop_data = nullptr;
    unsigned long nitems;
    unsigned long extra;
    Atom real;

    if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState], &real, &format, &nitems, &extra, &prop_data)
        != Success) {
        return -1;
    }
    if (nitems != 0) {
        result = *prop_data;
    }
    XFree(prop_data);
    return result;
}

bool getTextProp(Window w, Atom atom, char *text, std::size_t size) {
    char **list = nullptr;
    int count;
    XTextProperty name;

    if (!text || size == 0) return false;

    text[0] = '\0';
    if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems) return false;

    if (name.encoding == XA_STRING) {
        strncpy(text, reinterpret_cast<char *>(name.value), size - 1);
    } else if (XmbTextPropertyToTextList(dpy, &name, &list, &count) >= Success && count > 0 && *list) {
        strncpy(text, *list, size - 1);
        XFreeStringList(list);
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return true;
}

void Client::grabButtons(bool focused) const {
    updateNumLockMask();
    {
        std::array modifiers {0u, toUnsigned(LockMask), numlockmask, numlockmask | LockMask};
        XUngrabButton(dpy, AnyButton, AnyModifier, win);
        if (!focused) {
            XGrabButton(dpy, AnyButton, AnyModifier, win, False, BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
        }
        for (auto const &button : buttons) {
            if (button.click == Click::ClientWin) {
                for (unsigned int modifier : modifiers) {
                    XGrabButton(dpy,
                        button.button,
                        button.mask | modifier,
                        win,
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

void grabKeys() {
    updateNumLockMask();
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
    KeySym *syms = XGetKeyboardMapping(dpy, static_cast<KeyCode>(start), end - start + 1, &skip);
    if (!syms) return;
    for (int k = start; k <= end; k++) {
        for (auto const &key : keys) {
            /* skip modifier codes, we do that ourselves */
            if (key.keysym == syms[(static_cast<ptrdiff_t>(k - start) * skip)]) {
                for (auto const &mod : modifiers) {
                    XGrabKey(dpy, k, key.mod | mod, root, True, GrabModeAsync, GrabModeAsync);
                }
            }
        }
    }
    XFree(syms);
}

void setMaster(int arg) {
    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = std::max(arg, 0);
    arrange(selmon);
}

void iconify() {
    if (!XIconifyWindow(dpy, selmon->sel->win, screen)) lg::debug("Could not iconify {}", selmon->sel->name);
}

void incNmaster(int arg) {
    setMaster(std::max(selmon->nmaster + arg, 0));
}

static int isuniquegeom(std::span<ScreenInfo const> unique, size_t count, ScreenInfo info) {
    while (count--) {
        if (unique[count].x_org == info.x_org && unique[count].y_org == info.y_org && unique[count].width == info.width
            && unique[count].height == info.height) {
            return 0;
        }
    }
    return 1;
}

void keyPress(XEvent *e) {

    XKeyEvent *ev = &e->xkey;

    KeySym keysym = XLookupKeysym(ev, 0);
    for (auto const &key : keys)
        if (keysym == key.keysym && CLEANMASK(key.mod, numlockmask) == CLEANMASK(ev->state, numlockmask))
            if (!variantInvoke(key.func, key.arg)) {
                lg::error("Could not run key mapping: function index is {}, but arg is {}",
                    key.func.index(),
                    key.arg.index());
            }
}

void killClient() {
    if (!selmon->sel) {
        return;
    }
    if (!selmon->sel->sendEvent(wmatom[WMDelete])) {
        XGrabServer(dpy);
        XSetErrorHandler(xErrorDummy);
        XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, selmon->sel->win);
        XSync(dpy, False);
        XSetErrorHandler(dwmErrorHandler);
        XUngrabServer(dpy);
    }
}

void manage(Window w, XWindowAttributes *attrs) {
    Client *trans_for = nullptr;
    Client *term = nullptr;
    Window trans = None;
    XWindowChanges changes;

    auto *client = new Client {};
    client->win = w;
    client->pid = winPid(w);
    /* geometry */
    client->size.x = client->old_size.x = attrs->x;
    client->size.y = client->old_size.y = attrs->y;
    client->size.w = client->old_size.w = attrs->width;
    client->size.h = client->old_size.h = attrs->height;
    client->old_border_width = attrs->border_width;
    client->client_factor = 1.0;

    client->updateTitle();
    if (XGetTransientForHint(dpy, w, &trans)
        && (trans_for = winToClient(trans))) {  // NOLINT(bugprone-assignment-in-if-condition)
        client->mon = trans_for->mon;
        client->tags = trans_for->tags;
    } else {
        client->mon = selmon;
        client->applyRules();
        term = termForWin(client);
    }

    if (client->size.x + client->getWidth() > client->getMon()->window_size.x + client->getMon()->window_size.w)
        client->size.x = client->getMon()->window_size.x + client->getMon()->window_size.w - client->getWidth();
    if (client->size.y + client->getHeight() > client->getMon()->window_size.y + client->getMon()->window_size.h)
        client->size.y = client->getMon()->window_size.y + client->getMon()->window_size.h - client->getHeight();
    client->size.x = std::max(client->size.x, client->getMon()->window_size.x);
    client->size.y = std::max(client->size.y, client->getMon()->window_size.y);



    client->border_width = borderpx;

    changes.border_width = client->border_width;
    XConfigureWindow(dpy, w, CWBorderWidth, &changes);
    XSetWindowBorder(dpy, w, drw->scheme().norm.border.pixel);
    client->configure(); /* propagates border_width, if size doesn't change */
    client->updateWindowType();
    client->updateSizeHints();
    client->updateWmHints();
    XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    client->grabButtons(false);
    if (!client->props.isfloating) {
        client->props.isfloating = client->props.old_float_state = trans != None || client->props.isfixed;
    }
    if (client->props.isfloating) {
        XRaiseWindow(dpy, client->win);
    }
    attachAside(client);
    attachStack(client);
    XChangeProperty(dpy,
        root,
        netatom[NetClientList],
        XA_WINDOW,
        32,
        PropModeAppend,
        reinterpret_cast<unsigned char const *>(&client->win),
        1);
    XMoveResizeWindow(dpy,
        client->win,
        client->size.x + (2 * screen_w),
        client->size.y,
        static_cast<unsigned>(client->size.w),
        static_cast<unsigned>(client->size.h)); /* some windows require this */
    client->setClientState(NormalState);
    if (client->getMon() == selmon && selmon->sel) selmon->sel->unfocus(false);

    client->getMon()->sel = client;
    arrange(client->getMon());
    XMapWindow(dpy, client->win);
    if (term) {
        swallow(term, client);
    }
    focus(nullptr);
}

void mappingNotify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard) {
        grabKeys();
    }
}

void mapRequest(XEvent *e) {
    static XWindowAttributes attrs;
    XMapRequestEvent *ev = &e->xmaprequest;

    if (!XGetWindowAttributes(dpy, ev->window, &attrs) || attrs.override_redirect) {
        return;
    }
    if (!winToClient(ev->window)) {
        manage(ev->window, &attrs);
    }
}

void monocle(MonitorRef const &mon) {
    unsigned int count = 0;
    Client *client;

    for (client = mon->clients; client; client = client->next) {
        if (client->isVisible()) {
            count++;
        }
    }
    if (count > 0) { /* override layout symbol */
                     // TODO(dk949): Replace with std::format_to ?
        (void)snprintf(mon->layoutSymbol.data(), mon->layoutSymbol.max_size(), "[%d]", count);
    }
    for (client = nextTiled(mon->clients); client; client = nextTiled(client->next)) {
        client->resize(
            {
                mon->window_size.x,
                mon->window_size.y,
                mon->window_size.w - (2 * client->border_width),
                mon->window_size.h - (2 * client->border_width),
            },
            false);
    }
}

void motionNotify(XEvent *e) {
    // TODO(dk949): get rid of this static variable!!!
    //              Currently made it weak_ptr, to avoid keeping a monitor pointer alive
    //              for the duration of the program.
    static WeakMonitorRef last_mon;
    MonitorRef cur_mon;
    XMotionEvent *ev = &e->xmotion;

    if (ev->window != root) {
        return;
    }
    cur_mon = rectToMon({ev->x_root, ev->y_root, 1, 1});
    if (!last_mon.expired() && last_mon.lock() != cur_mon) {
        if (selmon->sel) selmon->sel->unfocus(true);
        selmon = cur_mon;
        focus(nullptr);
    }
    last_mon = cur_mon;
}

void movemouse() {
    int ocx;
    int ocy;
    int new_x;
    int new_y;
    XEvent ev;
    Time lasttime = 0;
    Client *client = selmon->sel;
    if (!client) return;

    if (client->props.isfullscreen == FullScreen::on) { /* no support moving fullscreen windows by mouse */
        return;
    }
    restack(selmon);
    ocx = client->size.x;
    ocy = client->size.y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, drw->cursors().move(), CurrentTime)
        != GrabSuccess) {
        return;
    }
    if (auto [x, y] = getRootPtr()) {
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

                    new_x = ocx + (ev.xmotion.x - x);
                    new_y = ocy + (ev.xmotion.y - y);
                    if (std::cmp_less(abs(selmon->window_size.x - new_x), snap)) {
                        new_x = selmon->window_size.x;
                    } else if (((selmon->window_size.x + selmon->window_size.w) - (new_x + client->getWidth())) < snap) {
                        new_x = selmon->window_size.x + selmon->window_size.w - client->getWidth();
                    }
                    if (std::cmp_less(abs(selmon->window_size.y - new_y), snap)) {
                        new_y = selmon->window_size.y;
                    } else if (selmon->window_size.y + selmon->window_size.h - (new_y + client->getHeight()) < snap) {
                        new_y = selmon->window_size.y + selmon->window_size.h - client->getHeight();
                    }
                    if (!client->props.isfloating && selmon->layout_slots[selmon->sel_layout]->arrange
                        && (std::cmp_greater(abs(new_x - client->size.x), snap)
                            || std::cmp_greater(abs(new_y - client->size.y), snap))) {
                        toggleFloating();
                    }
                    if (!selmon->layout_slots[selmon->sel_layout]->arrange || client->props.isfloating) {
                        client->resize({new_x, new_y, client->size.w, client->size.h}, true);
                    }
                    break;
                default: lg::warn("Unexpected event type {} in movemouse", ev.type); break;
            }
        } while (ev.type != ButtonRelease);
        XUngrabPointer(dpy, CurrentTime);
        if (auto mon = rectToMon(client->size); mon != selmon) {
            sendMon(client, mon);
            selmon = mon;
            focus(nullptr);
        }
    }
}

Client *nextTagged(Client *client) {
    Client *walked = client->getMon()->clients;
    for (; walked && (walked->props.isfloating || !walked->isVisibleOnTag(client->tags)); walked = walked->next) { }
    return walked;
}

Client *nextTiled(Client *client) {
    for (; client && (client->props.isfloating || !client->isVisible()); client = client->next) {
        ;
    }
    return client;
}

void pop(Client *client) {
    detach(client);
    attach(client);
    focus(client);
    arrange(client->getMon());
}

void propertyNotify(XEvent *e) {
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
        updateStatus();
    } else if (ev->state == PropertyDelete) {
        return; /* ignore */
    } else if (auto *client = winToClient(ev->window)) {
        switch (ev->atom) {
            default: break;
            case XA_WM_TRANSIENT_FOR:
                if (!client->props.isfloating && (XGetTransientForHint(dpy, client->win, &trans))) {
                    client->props.isfloating = winToClient(trans) != nullptr;
                    if (client->props.isfloating) arrange(client->getMon());
                }
                break;
            case XA_WM_NORMAL_HINTS: client->hintsvalid = false; break;
            case XA_WM_HINTS:
                client->updateWmHints();
                drawBars();
                break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
            client->updateTitle();
            if (client == client->getMon()->sel) drawBar(client->getMon());
        }
        if (ev->atom == netatom[NetWMWindowType]) {
            client->updateWindowType();
        }
    }
}

void quit() {
    loop->terminate();
    need_restart = false;
    lg::info("Initiating shutdowd");
}

void restart() {
    loop->terminate();
    need_restart = true;
}

MonitorRef rectToMon(Rect<int> rect) {
    return *rng::max_element(mons,
        [&](auto const &lhs, auto const &rhs) { return INTERSECT(rect, lhs) < INTERSECT(rect, rhs); });
}

void Client::resize(Rect<int> new_size, bool interact) {
    if (applySizeHints(&new_size, interact)) resizeClient(new_size);
}

void Client::resizeClient(Rect<int> new_size) {
    XWindowChanges changes;
    int gapoffset = 0;
    int gapincr = 0;

    changes.border_width = border_width;

    /* Get number of clients for the selected monitor */
    auto count = [&] {
        unsigned int out = 0;
        for (auto *nbc = nextTiled(getMon()->clients); nbc; nbc = nextTiled(nbc->next), out++) { }
        return out;
    }();

    /* Do nothing if layout is floating */
    if (!props.isfloating && getMon()->layout_slots[getMon()->sel_layout]->arrange != nullptr) {
        /* Remove border and gap if layout is monocle or only one client */
        if (getMon()->layout_slots[getMon()->sel_layout]->arrange == monocle || count == 1) {
            gapincr = -2 * borderpx;
            changes.border_width = 0;
        } else {
            gapoffset = gappx;
            gapincr = 2 * gappx;
        }
    }

    old_size.x = size.x;
    size.x = changes.x = new_size.x + gapoffset;
    old_size.y = size.y;
    size.y = changes.y = new_size.y + gapoffset;
    old_size.w = size.w;
    size.w = changes.width = new_size.w - gapincr;
    old_size.h = size.h;
    size.h = changes.height = new_size.h - gapincr;

    XConfigureWindow(dpy, win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &changes);
    configure();
    XSync(dpy, False);
}

void resizeMouse() {
    int ocx;
    int ocy;
    int new_w;
    int new_h;
    Client *client;
    XEvent ev;
    Time lasttime = 0;

    client = selmon->sel;
    if (!client) return;

    if (client->props.isfullscreen == FullScreen::on) { /* no support resizing fullscreen windows by mouse */
        return;
    }
    restack(selmon);
    ocx = client->size.x;
    ocy = client->size.y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, drw->cursors().resize(), CurrentTime)
        != GrabSuccess) {
        return;
    }
    XWarpPointer(dpy,
        None,
        client->win,
        0,
        0,
        0,
        0,
        client->size.w + client->border_width - 1,
        client->size.h + client->border_width - 1);
    do {
        XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
        switch (ev.type) {
            // TODO(dk949): make sure these don't actually need special treatment
            case ButtonRelease:
            case NoExpose: break;
            case ConfigureRequest: loop->exec<ConfigureRequest>(&ev); break;
            case Expose: loop->exec<Expose>(&ev); break;
            case MapRequest: loop->exec<MapRequest>(&ev); break;
            case MotionNotify: {
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
                    continue;
                }
                lasttime = ev.xmotion.time;

                new_w = std::max(ev.xmotion.x - ocx - (2 * client->border_width) + 1, 1);
                new_h = std::max(ev.xmotion.y - ocy - (2 * client->border_width) + 1, 1);
                auto mon_ptr = client->getMon();
                if (mon_ptr->window_size.x + new_w >= selmon->window_size.x
                    && mon_ptr->window_size.x + new_w <= selmon->window_size.x + selmon->window_size.w
                    && mon_ptr->window_size.y + new_h >= selmon->window_size.y
                    && mon_ptr->window_size.y + new_h <= selmon->window_size.y + selmon->window_size.h) {
                    if (!client->props.isfloating && selmon->layout_slots[selmon->sel_layout]->arrange
                        && (std::cmp_greater(abs(new_w - client->size.w), snap)
                            || std::cmp_greater(abs(new_h - client->size.h), snap))) {
                        toggleFloating();
                    }
                }
                if (!selmon->layout_slots[selmon->sel_layout]->arrange || client->props.isfloating) {
                    client->resize({client->size.x, client->size.y, new_w, new_h}, true);
                }
            } break;
            default: lg::warn("Unknown event type {} in resizeMouse", ev.type); break;
        }
    } while (ev.type != ButtonRelease);
    XWarpPointer(dpy,
        None,
        client->win,
        0,
        0,
        0,
        0,
        client->size.w + client->border_width - 1,
        client->size.h + client->border_width - 1);
    XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {
        ;
    }
    if (auto mon = rectToMon(client->size); mon != selmon) {
        sendMon(client, mon);
        selmon = mon;
        focus(nullptr);
    }
}

void restack(MonitorRef const &mon) {
    Client *client;
    XEvent ev;
    XWindowChanges changes;

    drawBar(mon);
    if (!mon->sel) {
        return;
    }
    if (mon->sel->props.isfloating || !mon->layout_slots[mon->sel_layout]->arrange) {
        XRaiseWindow(dpy, mon->sel->win);
    }
    if (mon->layout_slots[mon->sel_layout]->arrange) {
        changes.stack_mode = Below;
        changes.sibling = mon->barwin;
        for (client = mon->stack; client; client = client->snext) {
            if (!client->props.isfloating && client->isVisible()) {
                XConfigureWindow(dpy, client->win, CWSibling | CWStackMode, &changes);
                changes.sibling = client->win;
            }
        }
    }
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {
        ;
    }
}

void rotateStack(int arg) {
    Client *client = nullptr;
    Client *focused;

    if (!selmon->sel) {
        return;
    }
    focused = selmon->sel;
    if (arg > 0) {
        for (client = nextTiled(selmon->clients); client && nextTiled(client->next); client = nextTiled(client->next)) {
            ;
        }
        if (client) {
            detach(client);
            attach(client);
            detachStack(client);
            attachStack(client);
        }
    } else {
        client = nextTiled(selmon->clients);
        if (client) {
            detach(client);
            enqueue(client);
            detachStack(client);
            enqueueStack(client);
        }
    }
    if (client) {
        arrange(selmon);
        // focused->unfocus( 1);
        focus(focused);
        restack(selmon);
    }
}

void scan() {
    unsigned int idx;
    unsigned int num;
    Window root_ret;
    Window parent_ret;
    Window *wins = nullptr;
    XWindowAttributes attrs;

    if (XQueryTree(dpy, root, &root_ret, &parent_ret, &wins, &num)) {
        for (idx = 0; idx < num; idx++) {
            if (!XGetWindowAttributes(dpy, wins[idx], &attrs) || attrs.override_redirect
                || XGetTransientForHint(dpy, wins[idx], &root_ret)) {
                continue;
            }
            if (attrs.map_state == IsViewable || getState(wins[idx]) == IconicState) {
                manage(wins[idx], &attrs);
            }
        }
        for (idx = 0; idx < num; idx++) { /* now the transients */
            if (!XGetWindowAttributes(dpy, wins[idx], &attrs)) {
                continue;
            }
            if (XGetTransientForHint(dpy, wins[idx], &root_ret)
                && (attrs.map_state == IsViewable || getState(wins[idx]) == IconicState)) {
                manage(wins[idx], &attrs);
            }
        }
        if (wins) {
            XFree(wins);
        }
    }
}

void handle_notifyself_fade_anim(FadeBarEvent) {
    drawProgress(PROGRESS_FADE);
}

void sendMon(Client *client, MonitorRef const &mon) {
    if (client->getMon() == mon) return;

    client->unfocus(true);
    detach(client);
    detachStack(client);
    client->mon = mon;
    client->tags = mon->tagset[mon->sel_tags]; /* assign tags of target monitor */
    attachAside(client);
    attachStack(client);
    focus(nullptr);
    arrange(nullptr);
    if (client->switchtotag) {
        client->switchtotag = 0;
    }
}

void Client::setClientState(long state) const {
    std::array data {state, None};

    XChangeProperty(dpy,
        win,
        wmatom[WMState],
        wmatom[WMState],
        32,
        PropModeReplace,
        reinterpret_cast<unsigned char const *>(data.data()),
        2);
}

bool Client::sendEvent(Atom proto) const {
    int count;
    Atom *protocols;
    bool exists = false;
    XEvent ev;

    if (XGetWMProtocols(dpy, win, &protocols, &count)) {
        while (!exists && count--) {
            exists = protocols[count] == proto;
        }
        XFree(protocols);
    }
    if (!exists) return false;
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = static_cast<long>(proto);
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, win, False, NoEventMask, &ev);
    return exists;
}

void Client::setFocus() {
    if (!props.neverfocus) {
        XSetInputFocus(dpy, win, RevertToPointerRoot, CurrentTime);
        XChangeProperty(dpy,
            root,
            netatom[NetActiveWindow],
            XA_WINDOW,
            32,
            PropModeReplace,
            reinterpret_cast<unsigned char const *>(&win),
            1);
    }
    (void)sendEvent(wmatom[WMTakeFocus]);
}

void Client::setFullscreen(FullScreen fullscreen) {
    if (fullscreen == FullScreen::on && !props.isfullscreen) {
        props.isfullscreen = FullScreen::on;
        XChangeProperty(dpy,
            win,
            netatom[NetWMState],
            XA_ATOM,
            32,
            PropModeReplace,
            reinterpret_cast<unsigned char const *>(&netatom[NetWMFullscreen]),
            1);
        props.isfullscreen = FullScreen::on;
        props.old_float_state = props.isfloating;
        old_border_width = border_width;
        border_width = 0;
        props.isfloating = true;
        resizeClient(getMon()->monitor_size);
        XRaiseWindow(dpy, win);
    } else if (fullscreen == FullScreen::off && props.isfullscreen) {
        XChangeProperty(dpy, win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, nullptr, 0);
        props.isfullscreen = FullScreen::off;
        props.isfloating = props.old_float_state;
        border_width = old_border_width;
        size.x = old_size.x;
        size.y = old_size.y;
        size.w = old_size.w;
        size.h = old_size.h;
        resizeClient(size);
        arrange(getMon());
    }
}

void setLayout(Layout const *arg) {
    if (!arg || arg != selmon->layout_slots[selmon->sel_layout])
        selmon->sel_layout = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1u;

    if (arg)
        selmon->layout_slots[selmon->sel_layout] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sel_layout] =
            arg;

    strncpy(selmon->layoutSymbol.data(),
        selmon->layout_slots[selmon->sel_layout]->symbol,
        selmon->layoutSymbol.max_size() - 1);
    if (selmon->sel)
        arrange(selmon);
    else
        drawBar(selmon);
}

void setCfact(float arg) {
    Client *client = selmon->sel;

    if (!client || !selmon->layout_slots[selmon->sel_layout]->arrange) return;

    float new_factor = arg + client->client_factor;
    if (arg == 0.0f)
        new_factor = 1.0;
    else if (new_factor < cfact_min || new_factor > cfact_max)
        return;

    client->client_factor = new_factor;
    arrange(selmon);
}

/* arg > 1.0 will set master_factor absolutely */
void setMfact(float arg) {

    if (!selmon->layout_slots[selmon->sel_layout]->arrange) return;

    float new_factor = arg < 1.0f ? arg + selmon->master_factor : arg - 1.0f;
    if (new_factor < mfact_min || new_factor > mfact_max) return;
    if (new_factor == 0.0f)  // TODO(dk949): This is never executed?
        new_factor = 1.0f;

    selmon->master_factor = selmon->pertag->master_factors[selmon->pertag->curtag] = new_factor;
    arrange(selmon);
}

void resetMcfact() {
    if (!selmon->layout_slots[selmon->sel_layout]->arrange) return;

    selmon->sel->client_factor = 1.0;
    selmon->master_factor = selmon->pertag->master_factors[selmon->pertag->curtag] = 0.5;
    arrange(selmon);
}

void setup() {
    Atom utf8string;

    Proc::setupSignals();
    if constexpr (dwm::version::is_debug) Proc::setupDebugging();

    /* init screen */
    screen = DefaultScreen(dpy);
    screen_w = DisplayWidth(dpy, screen);
    screen_h = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    drw = new Drw(dpy, screen, root, screen_w, screen_h);
    if (!drw->fontsetCreate(fonts)) {
        lg::fatal("no fonts could be loaded.");
    }

    if (brightSetup(get_bright_set_file(), get_bright_get_file(), get_bright_max_file()) != BacklightError::Ok) {
        lg::fatal("backlight setup failed");
    }

#ifdef ASOUND
    volc = volc_init(VOLC_ALL_DEFULTS);
    if (!volc) lg::fatal("volc setup failed");

#endif /* ASOUND */

    text_padding = drw->fonts().h;
    bar_height = drw->fonts().h + 2;
    updateGeom();
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
    netatom[NetOpacity] = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    netatom[NetBypassComp] = XInternAtom(dpy, "_NET_WM_BYPASS_COMPOSITOR", False);
    netatom[NetOpaqueRegion] = XInternAtom(dpy, "_NET_WM_OPAQUE_REGION", False);

    drw->setColorScheme(colors);

    {
        // In multimonitor setups with Xinerama, the value of `screen_h` becomes very
        // big as all monitors are treated as a single screen
        int avg = avgHeight();
        borderpx = avg / 540;
        gappx = avg / 180;
        snap = avg / 67;
    }

    /* init bars */
    updateBars();
    updateStatus();
    /* supporting window for NetWMCheck */
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy,
        wmcheckwin,
        netatom[NetWMCheck],
        XA_WINDOW,
        32,
        PropModeReplace,
        reinterpret_cast<unsigned char const *>(&wmcheckwin),
        1);
    XChangeProperty(dpy,
        wmcheckwin,
        netatom[NetWMName],
        utf8string,
        8,
        PropModeReplace,
        reinterpret_cast<unsigned char const *>("dwm"),
        3);
    XChangeProperty(dpy,
        root,
        netatom[NetWMCheck],
        XA_WINDOW,
        32,
        PropModeReplace,
        reinterpret_cast<unsigned char const *>(&wmcheckwin),
        1);
    /* EWMH support per view */
    XChangeProperty(dpy,
        root,
        netatom[NetSupported],
        XA_ATOM,
        32,
        PropModeReplace,
        reinterpret_cast<unsigned char const *>(netatom.data()),
        NetLast);
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    /* select events */
    XSetWindowAttributes attrs;
    attrs.cursor = drw->cursors().normal();
    XChangeWindowAttributes(dpy, root, CWCursor, &attrs);
    loop = std::make_unique<EventLoop>(dpy, root);
    installEventHandlers();
    grabKeys();
    focus(nullptr);
}

void Client::setUrgent(IsUrgent urg) {
    props.isurgent = urg;
    if (auto wmh = XPtr<XWMHints>(XGetWMHints(dpy, win))) {
        wmh->flags = urg == IsUrgent::yes ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
        XSetWMHints(dpy, win, wmh.get());
    }
}

void showHide(Client *client) {
    if (!client) {
        return;
    }
    if (client->isVisible()) {
        /* show clients top down */
        XMoveWindow(dpy, client->win, client->size.x, client->size.y);
        if ((!client->getMon()->layout_slots[client->getMon()->sel_layout]->arrange || client->props.isfloating)
            && !client->props.isfullscreen) {
            client->resize(client->size, false);
        }
        showHide(client->snext);
    } else {
        /* hide clients bottom up */
        showHide(client->snext);
        XMoveWindow(dpy, client->win, client->getWidth() * -2, client->size.y);
    }
}

void spawn(char const *const *arg) {
    Proc::spawnDetached(dpy, arg);
}

void tag(unsigned arg) {
    if (selmon->sel && arg & TAGMASK) {
        selmon->sel->tags = arg & TAGMASK;
        if (selmon->sel->switchtotag) {
            selmon->sel->switchtotag = 0;
        }
        focus(nullptr);
        arrange(selmon);
    }
}

void tagMon(int arg) {
    if (!selmon->sel || mons.size() == 1) {
        return;
    }
    sendMon(selmon->sel, dirToMon(arg));
}

void tile(MonitorRef const &mon) {
    float master_factor_sum = 0;
    float stack_factor_sum = 0;

    int count = 0;
    for (auto const *client = nextTiled(mon->clients); client; client = nextTiled(client->next), count++)
        if (std::cmp_less(count, mon->nmaster))
            master_factor_sum += client->client_factor;
        else
            stack_factor_sum += client->client_factor;

    if (count == 0) return;


    int master_w = 0;
    if (std::cmp_greater(count, mon->nmaster))
        master_w = mon->nmaster ? static_cast<int>(static_cast<float>(mon->window_size.w) * mon->master_factor) : 0;
    else
        master_w = mon->window_size.w;

    int master_y = 0;
    int stack_y = 0;
    int idx = 0;
    for (auto *client = nextTiled(mon->clients); client; client = nextTiled(client->next), idx++) {
        if (std::cmp_less(idx, mon->nmaster)) {
            auto const h = static_cast<int>(
                static_cast<float>(mon->window_size.h - master_y) * (client->client_factor / master_factor_sum));
            client->resize(
                {
                    mon->window_size.x,
                    mon->window_size.y + master_y,
                    master_w - (2 * client->border_width),
                    h - (2 * client->border_width),
                },
                false);
            // TODO(dk949): This is a guard against creating too many clients.
            //              Do something if there's too many clients!
            if (master_y + client->getHeight() < mon->window_size.h) {
                master_y += client->getHeight();
                master_factor_sum -= client->client_factor;
            }
        } else {
            auto const h = static_cast<int>(
                static_cast<float>(mon->window_size.h - stack_y) * (client->client_factor / stack_factor_sum));
            client->resize(
                {
                    mon->window_size.x + master_w,
                    mon->window_size.y + stack_y,
                    mon->window_size.w - master_w - (2 * client->border_width),
                    h - (2 * client->border_width),
                },
                false);
            if (stack_y + client->getHeight() < mon->window_size.h) {
                stack_y += client->getHeight();
                stack_factor_sum -= client->client_factor;
            }
        }
    }
}

double timespecDiff(const struct timespec *lhs, const struct timespec *rhs) {
    static constexpr double nano = static_cast<double>(std::chrono::nanoseconds::period::num)
                                 / std::chrono::nanoseconds::period::den;
    double lhs_sec = static_cast<double>(lhs->tv_sec) + (static_cast<double>(lhs->tv_nsec) * nano);
    double rhs_sec = static_cast<double>(rhs->tv_sec) + (static_cast<double>(rhs->tv_nsec) * nano);
    double diff = lhs_sec - rhs_sec;
    return (diff >= 0) ? diff : -diff;
}

void toggleBar() {
    selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
    updateBarPos(selmon);
    XMoveResizeWindow(dpy,
        selmon->barwin,
        selmon->window_size.x,
        selmon->bar_y,
        static_cast<unsigned>(selmon->window_size.w),
        static_cast<unsigned>(bar_height));
    arrange(selmon);
}

void toggleFloating() {
    if (!selmon->sel) return;

    if (selmon->sel->props.isfullscreen == FullScreen::on) /* no support for fullscreen windows */
        return;

    selmon->sel->props.isfloating = !selmon->sel->props.isfloating || selmon->sel->props.isfixed;
    if (selmon->sel->props.isfloating) {
        selmon->sel->resize(selmon->sel->size, false);
    }
    arrange(selmon);
}

void toggleFs() {
    if (!selmon->sel) return;
    selmon->sel->setFullscreen(!selmon->sel->props.isfullscreen);
}

void toggleTag(unsigned arg) {
    unsigned int newtags;

    if (!selmon->sel) {
        return;
    }
    newtags = selmon->sel->tags ^ (arg & TAGMASK);
    if (newtags) {
        selmon->sel->tags = newtags;
        focus(nullptr);
        arrange(selmon);
    }
}

void toggleView(unsigned arg) {
    unsigned int newtagset = selmon->tagset[selmon->sel_tags] ^ (arg & TAGMASK);

    if (newtagset) {
        selmon->tagset[selmon->sel_tags] = newtagset;

        if (newtagset == ~0u) {
            selmon->pertag->prevtag = selmon->pertag->curtag;
            selmon->pertag->curtag = 0;
        }

        /* test if the user did not select the same tag */
        if (!(newtagset & 1u << (selmon->pertag->curtag - 1u))) {
            selmon->pertag->prevtag = selmon->pertag->curtag;
            selmon->pertag->curtag = static_cast<unsigned>(std::countr_zero(newtagset) + 1);
        }

        /* apply settings for this view */
        selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
        selmon->master_factor = selmon->pertag->master_factors[selmon->pertag->curtag];
        selmon->sel_layout = selmon->pertag->sellts[selmon->pertag->curtag];
        selmon->layout_slots[selmon->sel_layout] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sel_layout];
        selmon->layout_slots[selmon->sel_layout ^ 1u] =
            selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sel_layout ^ 1u];

        if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag]) {
            toggleBar();
        }

        focus(nullptr);
        arrange(selmon);
    }
}

void Client::unfocus(bool set_focus) {
    grabButtons(false);
    XSetWindowBorder(dpy, win, drw->scheme().norm.border.pixel);
    if (set_focus) {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
}

[[maybe_unused]]
static void uniconifyClient(Client *client) {
    lg::debug("restoring iconified cliend {}", client->name);
    client->updateTitle();
    client->updateSizeHints();
    arrange(client->getMon());
    XMapWindow(dpy, client->win);
    XMoveResizeWindow(dpy,
        client->win,
        client->size.x,
        client->size.y,
        static_cast<unsigned>(client->size.w),
        static_cast<unsigned>(client->size.h));
    client->configure();
    client->setClientState(NormalState);
    attachStack(client);
    attach(client);
}

void unmanage(Client *client, IsDestroyed destroyed) {
    auto mon = client->getMon();
    unsigned int switchtotag = client->switchtotag;

    if (client->swallowing) {
        unswallow(client);
        return;
    }

    Client *swallower = swallowingClient(client->win);
    if (swallower) {
        delete ensureUnattached(swallower->swallowing);
        swallower->swallowing = nullptr;
        arrange(mon);
        focus(nullptr);
        return;
    }

    detach(client);
    detachStack(client);
    if (destroyed == IsDestroyed::no) {
        XWindowChanges changes;
        changes.border_width = client->old_border_width;
        XGrabServer(dpy); /* avoid race conditions */
        XSetErrorHandler(xErrorDummy);
        XSelectInput(dpy, client->win, NoEventMask);
        XConfigureWindow(dpy, client->win, CWBorderWidth, &changes); /* restore border */
        XUngrabButton(dpy, AnyButton, AnyModifier, client->win);
        client->setClientState(WithdrawnState);
        XSync(dpy, False);
        XSetErrorHandler(dwmErrorHandler);
        XUngrabServer(dpy);
    }
    delete ensureUnattached(client);
    if (!swallower) {
        arrange(mon);
        focus(nullptr);
        updateClientList();
        if (switchtotag) {
            view(switchtotag);
        }
    }

    /*if (client->switchtotag) {*/
    /*Arg a = { .ui = client->switchtotag };*/
    /*view(&a);*/
    /*}*/
}

void unmapNotify(XEvent *e) {
    XUnmapEvent *ev = &e->xunmap;
    Client *client = winToClient(ev->window);
    if (!client) return;
    if (ev->send_event)
        client->setClientState(WithdrawnState);
    else
        unmanage(client, IsDestroyed::no);
}

void updateBars() {
    XSetWindowAttributes attrs = {
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
    XClassHint class_hint = {dwm_class_name, dwm_class_name};
    for (auto const &mon : mons) {
        if (mon->barwin) {
            continue;
        }
        mon->barwin = XCreateWindow(dpy,
            root,
            mon->window_size.x,
            mon->bar_y,
            static_cast<unsigned>(mon->window_size.w),
            static_cast<unsigned>(bar_height),
            0,
            DefaultDepth(dpy, screen),
            CopyFromParent,
            DefaultVisual(dpy, screen),
            CWOverrideRedirect | CWBackPixmap | CWEventMask,
            &attrs);
        XDefineCursor(dpy, mon->barwin, drw->cursors().normal());
        XMapRaised(dpy, mon->barwin);
        XSetClassHint(dpy, mon->barwin, &class_hint);
    }
}

void updateBarPos(MonitorRef const &mon) {
    mon->window_size.y = mon->monitor_size.y;
    mon->window_size.h = mon->monitor_size.h;
    if (mon->showbar) {
        mon->window_size.h -= bar_height;
        mon->bar_y = mon->topbar ? mon->window_size.y : mon->window_size.y + mon->window_size.h;
        mon->window_size.y = mon->topbar ? mon->window_size.y + bar_height : mon->window_size.y;
    } else {
        mon->bar_y = -bar_height;
    }
}

void updateClientList() {
    Client *client;

    XDeleteProperty(dpy, root, netatom[NetClientList]);
    for (auto const &mon : mons) {
        for (client = mon->clients; client; client = client->next) {
            XChangeProperty(dpy,
                root,
                netatom[NetClientList],
                XA_WINDOW,
                32,
                PropModeAppend,
                reinterpret_cast<unsigned char const *>(&client->win),
                1);
        }
    }
}

bool updateGeom() {
    bool dirty = false;

    if (xineramaIsActive(dpy)) {
        std::size_t unique_idx = 0;
        auto info = ScreenInfoPtr::query(dpy);
        std::size_t num_screen_infos = info.count();
        /* only consider unique geometries as separate screens */
        // auto *unique = new ScreenInfo[num_screen_infos];
        std::vector<ScreenInfo> unique;
        unique.resize(num_screen_infos);
        for (size_t idx = 0; idx < num_screen_infos; idx++)
            if (isuniquegeom(unique, unique_idx, info[idx])) unique[unique_idx++] = info[idx];


        num_screen_infos = unique_idx;
        /* new monitors available */
        for (auto idx = mons.size(); idx < num_screen_infos; idx++)
            mons.push_back(createMon());

        for (auto const &[mon, scr, idx] : vws::zip(mons | vws::take(num_screen_infos), unique, vws::iota(0)))
            if (scr.x_org != mon->monitor_size.x     //
                || scr.y_org != mon->monitor_size.y  //
                || scr.width != mon->monitor_size.w  //
                || scr.height != mon->monitor_size.h) {
                dirty = true;
                mon->num = idx;
                mon->monitor_size.x = mon->window_size.x = scr.x_org;
                mon->monitor_size.y = mon->window_size.y = scr.y_org;
                mon->monitor_size.w = mon->window_size.w = scr.width;
                mon->monitor_size.h = mon->window_size.h = scr.height;
                updateBarPos(mon);
            }

        /* less monitors available nn < n */
        for (auto const &mon : mons | vws::drop(num_screen_infos) | vws::reverse) {
            Client *client = nullptr;
            while ((client = mon->clients)) {
                dirty = true;
                mon->clients = client->next;
                detachStack(client);
                client->mon = mons.front();
                attach(client);
                attachAside(client);
                attachStack(client);
            }
            if (mon == selmon) selmon = mons.front();
            cleanupMon(mon);
        }
        if (auto erase_start = mons.begin() + static_cast<long>(num_screen_infos); erase_start <= mons.end())
            mons.erase(erase_start, mons.end());
    } else { /* default monitor setup */
        if (mons.empty()) mons.push_back(createMon());

        if (mons.front()->monitor_size.w != screen_w || mons.front()->monitor_size.h != screen_h) {
            dirty = true;
            mons.front()->monitor_size.w = mons.front()->window_size.w = screen_w;
            mons.front()->monitor_size.h = mons.front()->window_size.h = screen_h;
            updateBarPos(mons.front());
        }
    }
    if (dirty) {
        selmon = mons.front();
        selmon = winToMon(root);
    }
    return dirty;
}

void updateNumLockMask() {
    static constexpr auto modmap_count = 8;

    numlockmask = 0;
    auto modmap = ut::Resource {XGetModifierMapping(dpy), [](XModifierKeymap *map) { XFreeModifiermap(map); }};
    for (unsigned i = 0; i < modmap_count; ++i)
        for (unsigned j = 0; std::cmp_less(j, modmap->max_keypermod); ++j)
            if (modmap->modifiermap[(i * static_cast<unsigned>(modmap->max_keypermod)) + j]
                == XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = 1u << i;
}

void Client::updateSizeHints() {
    long user_size;
    XSizeHints size_hints;

    if (!XGetWMNormalHints(dpy, win, &size_hints, &user_size)) {
        /* size is uninitialized, ensure that size.flags aren't used */
        size_hints.flags = PSize;
    }
    if (size_hints.flags & PBaseSize) {
        base_width = size_hints.base_width;
        base_height = size_hints.base_height;
    } else if (size_hints.flags & PMinSize) {
        base_width = size_hints.min_width;
        base_height = size_hints.min_height;
    } else {
        base_width = base_height = 0;
    }
    if (size_hints.flags & PResizeInc) {
        inc_width = size_hints.width_inc;
        inc_height = size_hints.height_inc;
    } else {
        inc_width = inc_height = 0;
    }
    if (size_hints.flags & PMaxSize) {
        max_width = size_hints.max_width;
        max_height = size_hints.max_height;
    } else {
        max_width = max_height = 0;
    }
    if (size_hints.flags & PMinSize) {
        min_width = size_hints.min_width;
        min_height = size_hints.min_height;
    } else if (size_hints.flags & PBaseSize) {
        min_width = size_hints.base_width;
        min_height = size_hints.base_height;
    } else {
        min_width = min_height = 0;
    }
    if (size_hints.flags & PAspect) {
        min_aspect = static_cast<float>(size_hints.min_aspect.y) / static_cast<float>(size_hints.min_aspect.x);
        max_aspect = static_cast<float>(size_hints.max_aspect.x) / static_cast<float>(size_hints.max_aspect.y);
    } else {
        max_aspect = min_aspect = 0.0;
    }
    props.isfixed = max_width != 0 && max_height != 0 && max_width == min_width && max_height == min_height;
    hintsvalid = true;
}

void updateStatus() {
    if (!getTextProp(root, XA_WM_NAME, stext, sizeof(stext)))
        std::format_to_n(stext, sizeof(stext), "dwm-{}", dwm::version::full);

    drawBar(selmon);
}

void Client::updateTitle() {
    if (!getTextProp(win, netatom[NetWMName], name.data(), name.max_size()))
        getTextProp(win, XA_WM_NAME, name.data(), name.max_size());

    if (name[0] == '\0') /* hack to mark broken clients */
        rng::copy(broken, name.begin());
}

void Client::updateWindowType() {
    Atom state = getAtomProp(netatom[NetWMState]);
    Atom wtype = getAtomProp(netatom[NetWMWindowType]);

    if (state == netatom[NetWMFullscreen]) {
        setFullscreen(FullScreen::on);
    }
    if (wtype == netatom[NetWMWindowTypeDialog]) {
        props.isfloating = true;
    }
}

void Client::updateWmHints() {

    if (auto wmh = XPtr<XWMHints>(XGetWMHints(dpy, win))) {
        if (this == selmon->sel && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, win, wmh.get());
        } else {
            props.isurgent = IsUrgent((wmh->flags & XUrgencyHint) != 0);
        }
        if (wmh->flags & InputHint) {
            props.neverfocus = wmh->input == 0;
        } else {
            props.neverfocus = false;
        }
    }
}

void winPicker() {
    lg::debug("winPicker start");
    auto total_client_count =
        rng::fold_left(mons, 0uz, [](auto count, auto const &mon) noexcept { return count + mon->clients->count(); });
    if (total_client_count == 0) return;
    auto args = winPickerCreateDmenuCommand(dpy, mons, selmon->num);
    loop->spawn(std::move(args),
        EventLoop::SpawnConfig {.keep_stdout = true, .keep_stderr = true},
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        [](std::optional<std::string> const &out, std::optional<std::string> const &err, int status) noexcept {
            if (status == 1 && err->empty()) {
                lg::debug("No window selected");
                return;
            }
            if (status) {
                lg::error("Failed to select window: code {}, {}", status, !err->empty() ? *err : "unknown error");
                return;
            }
            if (auto matched = winPickerMatchClient(dpy, mons, *out); matched && mons.size() > matched->second) {
                auto [client, mon_idx] = *matched;
                focusMonAbs(static_cast<unsigned>(mon_idx));
                view(client->tags);
                focus(client);
                restack(selmon);
            } else {
                lg::warn("Could not find requested window");
                return;
            }
        });
}

void view(unsigned arg) {

    if ((arg & TAGMASK) == selmon->tagset[selmon->sel_tags]) return;

    selmon->sel_tags ^= 1u; /* toggle sel tagset */
    if (arg & TAGMASK) {
        selmon->tagset[selmon->sel_tags] = arg & TAGMASK;
        selmon->pertag->prevtag = selmon->pertag->curtag;

        if (arg == ~0u)
            selmon->pertag->curtag = 0;
        else
            selmon->pertag->curtag = static_cast<unsigned>(std::countr_zero(arg) + 1);
    } else
        std::swap(selmon->pertag->prevtag, selmon->pertag->prevtag);


    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->master_factor = selmon->pertag->master_factors[selmon->pertag->curtag];
    selmon->sel_layout = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->layout_slots[selmon->sel_layout] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sel_layout];
    selmon->layout_slots[selmon->sel_layout ^ 1u] =
        selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sel_layout ^ 1u];

    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag]) toggleBar();


    focus(nullptr);
    arrange(selmon);
}

void shiftView(int dir) {
    if (dir == 0) return;
    unsigned arg = selmon->tagset[selmon->sel_tags] & TAGMASK;
    auto const last_set = static_cast<unsigned>(std::countl_zero(arg));
    auto const first_set = static_cast<unsigned>(std::countr_zero(arg));

    if (dir < 0) {
        if (first_set == 0) return;
        arg >>= 1u;
    } else {
        if (last_set <= std::numeric_limits<unsigned>::digits - tag_symbols.size()) return;
        arg <<= 1u;
    }

    view(arg);
}

#ifdef ASOUND
void volumeChange(float arg) {
    auto const state = arg == VOL_MT ? volc_volume_ctl(volc, VOLC_ALL_CHANNELS, VOLC_SAME, VOLC_CHAN_TOGGLE)
                                     : volc_volume_ctl(volc, VOLC_ALL_CHANNELS, VOLC_INC(arg), VOLC_CHAN_ON);

    if (state.err < 0) return;

    drawProgress(full_bar,
        static_cast<unsigned long long>(state.state.volume),
        state.state.switch_pos ? &drw->scheme().info_progress : &drw->scheme().off_progress);
}
#endif


pid_t winPid(Window w) {
    xcb_res_client_id_spec_t spec {};
    spec.client = static_cast<uint32_t>(w);
    spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

    xcb_generic_error_t *e = nullptr;
    xcb_res_query_client_ids_cookie_t client = xcb_res_query_client_ids(xcon, 1, &spec);

    auto reply = ut::malloced(xcb_res_query_client_ids_reply(xcon, client, &e));

    if (!reply) return 0;

    for (xcb_res_client_id_value_iterator_t iter = xcb_res_query_client_ids_ids_iterator(reply.get());  //
        iter.rem;
        xcb_res_client_id_value_next(&iter)) {
        spec = iter.data->spec;
        if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
            uint32_t *pid_ptr = xcb_res_client_id_value_value(iter.data);
            auto result = static_cast<pid_t>(*pid_ptr);
            return result == -1 ? pid_t {} : result;
        }
    }

    return pid_t {};
}

MonitorRef Client::getMon() {
    if (auto monitor = mon.lock(); !monitor) {
        lg::warn("Client '{}' was abandoned on a deleted monitor, moved to first monitor.", name.view());
        mon = mons.front();
        attachAside(this);
        attachStack(this);
        return mons.front();
    } else
        return monitor;
}

[[nodiscard]]
int Client::getWidth() const {
    return size.w + (2 * border_width) + gappx;
}

[[nodiscard]]
int Client::getHeight() const {
    return size.h + (2 * border_width) + gappx;
}

static uint32_t *geticon(Client *client, unsigned long *size) {
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
        client->win,
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
    length = static_cast<long>(bytes_left);
    *size = bytes_left;
    XGetWindowProperty(dpy,
        client->win,
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
        auto *begin = reinterpret_cast<uint32_t *>(data);
        auto *end = reinterpret_cast<uint32_t *>(data + *size);
        int pos = 0;
        for (uint32_t *it = begin; it != end; ++it, pos++) {
            if (pos % 2) continue;
            begin[pos / 2] = *it;
        }
    }
    return reinterpret_cast<uint32_t *>(data);
}

static void iconifyClient(Client *client) {
    // TODO(dk949): Make this actually work?
    char *icon_name;
    XGetIconName(dpy, client->win, &icon_name);
    lg::debug("{} wants to iconify. Icon name: {}", client->name, icon_name);
    XFree(icon_name);

    detach(client);
    detachStack(client);

    client->setClientState(IconicState);
    XUnmapWindow(dpy, client->win);

    arrange(client->getMon());
    updateClientList();
    unsigned long size;
    if (uint32_t *icon = geticon(client, &size)) {
        lg::debug("icon is {}x{}, {} bytes", icon[0], icon[1], size);
        XFree(icon);
    } else {
        lg::debug("No icon for client {}", client->name);
    }
}

void installEventHandlers() {
    loop->on<ButtonPress>(buttonPress);
    loop->on<ClientMessage>(clientMessage);
    loop->on<ConfigureRequest>(configureRequest);
    loop->on<ConfigureNotify>(configureNotify);
    loop->on<DestroyNotify>(destroyNotify);
    loop->on<EnterNotify>(enterNotify);
    loop->on<Expose>(expose);
    loop->on<FocusIn>(focusIn);
    loop->on<KeyPress>(keyPress);
    loop->on<MappingNotify>(mappingNotify);
    loop->on<MapRequest>(mapRequest);
    loop->on<MotionNotify>(motionNotify);
    loop->on<PropertyNotify>(propertyNotify);
    loop->on<UnmapNotify>(unmapNotify);
    loop->on<FadeBarEvent>(handle_notifyself_fade_anim);
    if (auto base = loop->xrandrEventBase(); base >= 0) loop->onExtension(base + RRNotify, rrOutputChange);
}

bool isDescProcess(pid_t parent, pid_t child) {
    while (parent != child && child != 0)
        child = getPpid(child);

    return child != 0;
}

Client *termForWin(Client const *w) {
    if (!w->pid || w->props.isterminal) {
        return nullptr;
    }

    Client *out = nullptr;
    for (auto const &mon : mons) {
        for (Client *client = mon->clients; client; client = client->next) {
            if (client->props.isterminal && !client->swallowing && client->pid && isDescProcess(client->pid, w->pid)) {
                if (selmon->sel == client) return client;
                out = client;
            }
        }
    }

    return out;
}

Client *swallowingClient(Window w) {
    for (auto const &mon : mons) {
        for (Client *client = mon->clients; client; client = client->next) {
            if (client->swallowing && client->swallowing->win == w) {
                return client;
            }
        }
    }

    return nullptr;
}

Client *winToClient(Window w) {

    for (auto const &mon : mons) {
        for (Client *client = mon->clients; client; client = client->next) {
            if (client->win == w) {
                return client;
            }
        }
    }
    return nullptr;
}

MonitorRef winToMon(Window w) {

    if (w == root)
        if (auto [x, y] = getRootPtr()) return rectToMon({x, y, 1, 1});

    if (auto mon_it = rng::find_if(mons, [&](auto const &mon) noexcept { return w == mon->barwin; });
        mon_it != mons.end())
        return *mon_it;

    if (Client *client = winToClient(w)) return client->getMon();

    return selmon;
}

[[maybe_unused]]
static void wmChange(Client *client, XClientMessageEvent *cme) {
    if (cme->format != 32 || cme->data.l[0] != IconicState)
        // Only handling iconification
        return;

    iconifyClient(client);
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int dwmErrorHandler(Display *display, XErrorEvent *err_event) {
    if (err_event->error_code == BadWindow
        || (err_event->request_code == X_SetInputFocus && err_event->error_code == BadMatch)
        || (err_event->request_code == X_PolyText8 && err_event->error_code == BadDrawable)
        || (err_event->request_code == X_PolyFillRectangle && err_event->error_code == BadDrawable)
        || (err_event->request_code == X_PolySegment && err_event->error_code == BadDrawable)
        || (err_event->request_code == X_ConfigureWindow && err_event->error_code == BadMatch)
        || (err_event->request_code == X_GrabButton && err_event->error_code == BadAccess)
        || (err_event->request_code == X_GrabKey && err_event->error_code == BadAccess)
        || (err_event->request_code == X_CopyArea && err_event->error_code == BadDrawable)) {
        return 0;
    }
    lg::warn("fatal error: request code={}, error code={}", err_event->request_code, err_event->error_code);
    return xerrorxlib(display, err_event); /* may call exit */
}

int xErrorDummy(Display *, XErrorEvent *) {
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xErrorStart(Display *, XErrorEvent *) {
    lg::fatal("another window manager is already running");
    return -1;
}

void zoom() {
    Client *client = selmon->sel;

    if (!selmon->layout_slots[selmon->sel_layout]->arrange || !client || client->props.isfloating) return;

    if (client == nextTiled(selmon->clients)) return;
    client = nextTiled(client->next);
    if (!client) return;

    pop(client);
}

int main(int argc, char *argv[]) {
    log_dir = lg::setupLogging();
    lg::debug("Setup logging");

    if (argc == 2 && !strcmp("-v", argv[1])) {
        std::println("dwm-{}", dwm::version::full);
        return 0;
    } else if (argc != 1) {
        (void)fputs("usage: dwm [-v]", stderr);
        return 1;
    }
    // NOLINTNEXTLINE(concurrency-mt-unsafe) // This is not in MT context
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
        lg::warn("no locale support");
    }
    dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        lg::fatal("cannot open display");
    }
    xcon = XGetXCBConnection(dpy);
    if (!xcon) {
        lg::fatal("cannot get xcb connection");
    }
    checkOtherWm();
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
        lg::info("Restarting dwm\n---------------------------");
        (void)fclose(lg::log_file);
        if (execvp(argv[0], argv)) lg::fatal("could not restart dwm:");
    }

    lg::info("Shutdown complete\n---------------------------");
    (void)fclose(lg::log_file);
    return EXIT_SUCCESS;
}

void centeredMaster(MonitorRef const &mon) {
    /* count number of clients in the selected monitor */
    int count = 0;
    for (auto const *client = nextTiled(mon->clients); client; client = nextTiled(client->next), count++) { }
    if (count == 0) return;


    /* initialize areas */
    int master_w = mon->window_size.w;
    int master_x = 0;
    int master_y = 0;
    int tile_w = master_w;

    if (std::cmp_greater(count, mon->nmaster)) {
        /* go master_factor box in the center if more than nmaster clients */
        master_w = mon->nmaster ? static_cast<int>(static_cast<float>(mon->window_size.w) * mon->master_factor) : 0;
        tile_w = mon->window_size.w - master_w;

        if (count - mon->nmaster > 1) {
            /* only one client */
            master_x = (mon->window_size.w - master_w) / 2;
            tile_w = (mon->window_size.w - master_w) / 2;
        }
    }

    int odd_y = 0;
    int even_y = 0;
    int idx = 0;
    for (auto *client = nextTiled(mon->clients); client; client = nextTiled(client->next), idx++) {
        if (std::cmp_less(idx, mon->nmaster)) {
            /* nmaster clients are stacked vertically, in the center
             * of the screen */
            auto const h = (mon->window_size.h - master_y) / (std::min(count, mon->nmaster) - idx);
            client->resize(
                {
                    mon->window_size.x + master_x,
                    mon->window_size.y + master_y,
                    master_w - (2 * client->border_width),
                    h - (2 * client->border_width),
                },
                false);
            // TODO(dk949): This is a guard against creating too many clients.
            //              Do something if there's too many clients!
            // TODO(dk949): make this client_factor aware
            if (master_y + client->getHeight() < mon->window_size.h) master_y += client->getHeight();
        } else {
            /* stack clients are stacked vertically */
            if ((idx - mon->nmaster) % 2) {
                auto const h = (mon->window_size.h - even_y) / ((1 + count - idx) / 2);
                client->resize(
                    {
                        mon->window_size.x,
                        mon->window_size.y + even_y,
                        tile_w - (2 * client->border_width),
                        h - (2 * client->border_width),
                    },
                    false);
                // TODO(dk949): This is a guard against creating too many clients.
                //              Do something if there's too many clients!
                // TODO(dk949): make this client_factor aware
                if (even_y + client->getHeight() < mon->window_size.h) even_y += client->getHeight();
            } else {
                auto const h = (mon->window_size.h - odd_y) / ((1 + count - idx) / 2);
                client->resize(
                    {
                        mon->window_size.x + master_x + master_w,
                        mon->window_size.y + odd_y,
                        tile_w - (2 * client->border_width),
                        h - (2 * client->border_width),
                    },
                    false);
                if (odd_y + client->getHeight() < mon->window_size.h) odd_y += client->getHeight();
            }
        }
    }
}
