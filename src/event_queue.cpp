#include "event_queue.hpp"

#include "log.hpp"
#include "proc.hpp"
#include "strerror.hpp"
#include "time_utils.hpp"
#include "x_utils.hpp"

#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

template<>
void EventLogger<false>::tickStart() { };
template<>
void EventLogger<false>::tickEnd() { };
template<>
void EventLogger<false>::countInternal() { };
template<>
void EventLogger<false>::countX() { };
template<>
void EventLogger<false>::log() { };

template<>
void EventLogger<true>::tickStart() {
    this_tick_start = chr::high_resolution_clock::now();
    internal_total += internal_this_tick;
    x_total += x_this_tick;
    internal_this_tick = 0;
    x_this_tick = 0;
}

template<>
void EventLogger<true>::tickEnd() {
    auto const this_tick_end = chr::high_resolution_clock::now();
    max_tick_time = std::max(max_tick_time, chr::duration_cast<chr::microseconds>(this_tick_end - this_tick_start));
    x_max_per_tick = std::max(x_max_per_tick, x_this_tick);
    internal_max_per_tick = std::max(internal_max_per_tick, internal_this_tick);
}

template<>

void EventLogger<true>::countInternal() {
    ++internal_this_tick;
}

template<>
void EventLogger<true>::countX() {
    ++x_this_tick;
}

template<>
void EventLogger<true>::log() {
    auto const since = chr::high_resolution_clock::now() - last_log;
    if (since < log_every) return;
    auto const ticks_since = chr::duration_cast<DoubleSec>(since) / chr::duration_cast<DoubleSec>(EventLoop::tick_time);
    auto const fn = [&](auto &total, auto &max, char const *name) {
        lg::debug("({}) {} / {}; {:0.2g} / tick (avg); {} / tick (max); max tick time {}",
            name,
            total,
            chr::duration_cast<DoubleSec>(log_every),
            static_cast<double>(total) / ticks_since,
            max,
            chr::duration_cast<DoubleMSec>(max_tick_time));
        total = 0;
        max = 0;
    };
    fn(internal_total, internal_max_per_tick, "ievents");
    fn(x_total, x_max_per_tick, "xevents");
    max_tick_time = max_tick_time.zero();
    last_log = chr::high_resolution_clock::now();
}

EventLoop::EventLoop(Display *dpy, Window root)
        : m_dpy(dpy)
        , x_socket(ConnectionNumber(m_dpy)) {
    XSetWindowAttributes wa;
    wa.event_mask = SubstructureRedirectMask  //
                  | SubstructureNotifyMask    //
                  | ButtonPressMask           //
                  | PointerMotionMask         //
                  | EnterWindowMask           //
                  | LeaveWindowMask           //
                  | StructureNotifyMask       //
                  | PropertyChangeMask;
    XChangeWindowAttributes(m_dpy, root, CWEventMask, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    on<TerminateEvent>([this](auto) noexcept { m_done = true; });
}

/**
 * Basic design:
 *  * 2 internal queues: active and inactive
 *  * Internal events only get pushed to the active queue
 *  * At the start of the loop:
 *      1. Swap queues
 *      2. Process all events in the inactive queue
 *          a. (optionally) checking for pending X events between each internal event
 *      3. Use https://stackoverflow.com/questions/29001189/how-to-stop-an-x11-event-loop-gracefully-asynchronously
 *         to wait for X events until next frame.
 *          a. After handling all X events, go back to waiting for more if there's time until next frame,
 *             go to 1. if not.
 *  * In the happy case this should look like waking up every 16ms, checking that internal queue is empty,
 *    then going to sleep again.
 *  * X events and internal events can both generate new internal events, these go to the new active queue and
 *    are not processed this frame.
 *  * There will be potential issues with `movemouse` and `resizemouse`, since these process events out of order.
 *    To start this should be OK (there aren't currently any internal events this will interfere with), but
 *    eventually I want to be able to dynamically replace X event handlers.
 */
void EventLoop::run() {
    XSync(m_dpy, False);
    syncSignals();
    while (!m_done) {
        logger.tickStart();
        {
            auto const tick_start = chr::high_resolution_clock::now();
            swapQueues();
            runQueueEvents(m_inactive_queue);
            handleXEvents(tick_start + tick_time);
        }
        logger.tickEnd();
        logger.log();
    }
}

void EventLoop::runQueueEvents(InternalQueue *q) {
    for (auto ev = q->tryPop(); ev; ev = q->tryPop()) {
        logger.countInternal();
        std::visit([this]<typename Ev>(Ev &&e) { return runInternalHandler(std::forward<Ev>(e)); }, *std::move(ev));
    }
}

void EventLoop::handleXEvents(chr::high_resolution_clock::time_point until) {
    flushXEvents();
    for (auto now = chr::high_resolution_clock::now(); now < until; now = chr::high_resolution_clock::now()) {
        fd_set in_fd_set;
        FD_ZERO(&in_fd_set);
        FD_SET(x_socket, &in_fd_set);
        FD_SET(Proc::sfd.get(), &in_fd_set);
        auto const ts = fromChrono(until - now);
        // TODO(dk949): Once we no longer need the timeout, switch to poll
        if (auto bits = pselect(std::max(x_socket, Proc::sfd.get()) + 1, &in_fd_set, nullptr, nullptr, &ts, nullptr);
            bits > 0) {
            if (FD_ISSET(x_socket, &in_fd_set)) flushXEvents();
            if (FD_ISSET(Proc::sfd.get(), &in_fd_set)) handleSignals();
        } else if (bits < 0) {
            lg::error("Error when `select` ing the socket: {}", strError(errno));
            break;
        } else
            break;
    };
}

void EventLoop::swapQueues() {
    // TODO(dk949): This can be also probably done with a CAS loop, but this is much easier and I doubt it matters
    m_active_queue->underLock([&](auto const &) {        //
        m_inactive_queue->underLock([&](auto const &) {  //
            std::swap(m_active_queue, m_inactive_queue);
        });
    });
}

void EventLoop::flushXEvents() {
    while (XPending(m_dpy)) {
        logger.countX();
        XEvent ev;
        if (auto err = XNextEvent(m_dpy, &ev)) {
            lg::error("XNextEvent error: {}", xstrerror(m_dpy, err));
            break;
        }
        auto &&handler = m_x_handlers[static_cast<std::size_t>(ev.type)];
        if (handler) handler(&ev);
    }
}

void EventLoop::handleSignals() {
    signalfd_siginfo siginfo {};
    if (auto bytes_read = read(Proc::sfd.get(), &siginfo, sizeof(siginfo)); bytes_read < 0) {
        lg::error("read error when handling signalfd: {}", strError(errno));
        return;
    } else if (bytes_read != sizeof(siginfo)) {
        lg::error("Failed to read all bytes of siginfo (expected {}, got {}): {}",
            sizeof(siginfo),
            bytes_read,
            strError(errno));
        return;
    }
    while (true)
        switch (auto pid = waitpid(-1, nullptr, WNOHANG)) {
            case 0: return;
            default: lg::debug("Successfully reaped {}", pid); break;
            case -1: lg::error("Failed to reap {}: {}", pid, strError(errno)); return;
        };
}

void EventLoop::syncSignals() {
    signalfd_siginfo dummy {};
    while (true) {
        errno = 0;
        auto bytes_read = read(Proc::sfd.get(), &dummy, sizeof(dummy));
        // TODO(dk949): Maybe also handle EINTR
        if (bytes_read < 0) {
            if (!DWM_IS_EAGAIN(errno)) lg::error("Read error when syncing signalfd: {}", strError(errno));
            break;
        }
    }
    Proc::cleanUpZombies();
}

