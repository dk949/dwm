#ifndef DWM_EVENT_QUEUE_HPP
#define DWM_EVENT_QUEUE_HPP
#include "type_utils.hpp"

#include <project/config.hpp>
#include <sys/select.h>
#include <ut/mt_queue/mt_queue.hpp>
#include <X11/X.h>
#include <X11/Xlib.h>

#include <array>
#include <chrono>
#include <functional>
#include <ratio>
#include <utility>
#include <variant>

struct FadeBarEvent { };

struct TerminateEvent { };

using InternalEvent = std::variant<FadeBarEvent, TerminateEvent>;

using InternalQueue = ut::MtQueue<InternalEvent>;

template<bool active>
struct EventLogger {
    static constexpr auto log_every = std::chrono::seconds {1};
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> last_log;
    std::chrono::time_point<std::chrono::high_resolution_clock> this_tick_start;
    std::chrono::microseconds max_tick_time {};
    std::size_t internal_max_per_tick {};
    std::size_t internal_this_tick {};
    std::size_t internal_total {};
    std::size_t x_max_per_tick {};
    std::size_t x_this_tick {};
    std::size_t x_total {};
public:

    void tickStart();
    void tickEnd();
    void countInternal();
    void countX();
    void log();
};

struct EventLoop {
    static constexpr auto ticks_per_second = 60;
    static constexpr auto tick_time = std::chrono::microseconds {
        static_cast<std::chrono::microseconds::rep>((1.0 / ticks_per_second) * std::micro().den)};

private:
    template<typename T>
    using EvFn = std::function<void(T)>;

    // TODO(dk949): Make this more type-safe (use tuple for xevents too)
    std::array<std::function<void(XEvent *)>, LASTEvent> m_x_handlers;
    map_tuple_types_t<variant_to_tuple_t<InternalEvent>, EvFn> m_intern_handlers;

    std::array<InternalQueue, 2> m_queues;
    InternalQueue *m_active_queue = &m_queues[0];
    InternalQueue *m_inactive_queue = &m_queues[1];

    EventLogger<dwm::log_events> logger;

    Display *m_dpy;
    int x_socket;

    bool m_done = false;

public:
    explicit EventLoop(Display *dpy, Window root);

    EventLoop(EventLoop const &) = delete;
    EventLoop &operator=(EventLoop const &) = delete;
    EventLoop(EventLoop &&) = delete;
    EventLoop &operator=(EventLoop &&) = delete;
    ~EventLoop() = default;

    template<int Ev, typename Fn>
    auto on(Fn &&fn) {
        static_assert(Ev < LASTEvent);
        return std::exchange(m_x_handlers[Ev], std::forward<Fn>(fn));
    }

    template<InVariant<InternalEvent> Ev, typename Fn>
    auto on(Fn &&fn) {
        return std::exchange(  //
            std::get<variant_index_v<InternalEvent, Ev>>(m_intern_handlers),
            std::forward<Fn>(fn));
    }

    template<int Ev>
    void exec(XEvent *ev) {
        if (m_x_handlers[Ev]) m_x_handlers[Ev](ev);
    }

    template<InVariant<InternalEvent> Ev>
    void exec(Ev &&ev) {
        runInternalHandler(std::forward<Ev>(ev));
    }

    template<InVariant<InternalEvent> Ev>
    void push(Ev &&ev) {
        m_active_queue->push(InternalEvent {std::forward<Ev>(ev)});
    }

    void run();

private:
    template<InVariant<InternalEvent> Ev>
    void runInternalHandler(Ev ev) {
        auto &&fn = std::get<variant_index_v<InternalEvent, std::remove_cvref_t<Ev>>>(m_intern_handlers);
        if (fn) fn(std::move(ev));
    }

    void runQueueEvents(InternalQueue *q);
    void handleXEvents(std::chrono::high_resolution_clock::time_point until);
    void flushXEvents();
    static void handleSignals();
    static void syncSignals();
    void swapQueues();
};

#endif  // DWM_EVENT_QUEUE_HPP
