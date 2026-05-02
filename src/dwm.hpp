#ifndef DWM_HPP
#define DWM_HPP

#include "boolenum.hpp"
#include "layout.hpp"
#include "log.hpp"
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
BOOLEAN_ENUM(IsUrgent) {no = false, yes = true};
BOOLEAN_ENUM(IsDestroyed) {no = false, yes = true};

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
    static ClassHint fromX(XClassHint class_hint) {
        return {XPtr<char> {class_hint.res_name}, XPtr<char> {class_hint.res_class}};
    }
};

using MonitorRef = std::shared_ptr<Monitor>;
using WeakMonitorRef = std::weak_ptr<Monitor>;
using Monitors = std::vector<MonitorRef>;

struct Monitor {
    ut::StaticString<16> layoutSymbol;  // NOLINT readability-magic-numbers
    float master_factor;
    int nmaster;
    int num;
    int bar_y; /* bar geometry */
    Rect<int> monitor_size;
    Rect<int> window_size;
    unsigned int sel_tags;
    unsigned int sel_layout;
    std::array<unsigned int, 2> tagset;
    bool showbar;
    int topbar;
    Client *clients;
    Client *sel;
    Client *stack;
    Window barwin;
    std::array<Layout const *, 2> layout_slots;
    Pertag *pertag;
};

struct ClientProps {
    bool isfixed;
    bool isfloating;
    IsUrgent isurgent;
    bool neverfocus;
    bool old_float_state;
    FullScreen isfullscreen;
    bool isterminal;
    bool noswallow;
};

struct Client {
    ut::StaticString<256> name;  // NOLINT readability-magic-numbers
    float min_aspect, max_aspect;
    float client_factor;
    Rect<int> size;
    Rect<int> old_size;
    int base_width, base_height, inc_width, inc_height, max_width, max_height, min_width, min_height;
    bool hintsvalid;
    int border_width, old_border_width;
    unsigned int tags;
    unsigned int switchtotag;
    ClientProps props;
    pid_t pid;
    Client *next;
    Client *snext;
    Client *swallowing;
    WeakMonitorRef mon;
    Window win;

    [[nodiscard]]
    ClassHint classHint(Display *dpy) const {
        XClassHint raw_hint;
        auto status = XGetClassHint(dpy, win, &raw_hint);
        if (!status) return ClassHint {nullptr, nullptr};
        return ClassHint::fromX(raw_hint);
    }

    [[nodiscard]]
    std::size_t count() const {
        std::size_t count = 0;
        for (auto const *client = this; client; client = client->next, ++count) { }
        return count;
    }

    void configure() const;
    void applyRules();
    void resizeClient(Rect<int> new_size);
    bool applySizeHints(Rect<int> *size, bool interact);
    void resize(Rect<int> size, bool interact);
    void unfocus(bool set_focus);
    void setFocus();
    void setFullscreen(FullScreen fullscreen);
    void setUrgent(IsUrgent urg);
    void updateSizeHints();
    void updateTitle();
    void updateWindowType();
    void updateWmHints();
    void grabButtons(bool focused) const;
    void setClientState(long state) const;
    [[nodiscard]]
    MonitorRef getMon();
    [[nodiscard]]
    bool sendEvent(Atom proto) const;
    [[nodiscard]]
    Atom getAtomProp(Atom prop) const;
    [[nodiscard]]
    int getWidth() const;
    [[nodiscard]]
    int getHeight() const;

    [[nodiscard]]
    bool isVisibleOnTag(unsigned tag) const {
        return (tags & tag) != 0u;
    }

    [[nodiscard]]
    bool isVisible() const {
        if (auto monitor = mon.lock())
            return isVisibleOnTag(monitor->tagset[monitor->sel_tags]);
        else
            lg::warn("Trying to query visibility of the client '{}' on a deleted monitor", name.view());
        return false;
    }
};

struct RootPointer {
    int x;
    int y;
    bool success;

    constexpr operator bool() const noexcept {  // NOLINT(google-explicit-constructor)
        return success;
    }
};

template<>
struct std::tuple_size<RootPointer> : std::integral_constant<std::size_t, 2> { };

template<>
struct std::tuple_element<0, RootPointer> {
    using type = int;
};

template<>
struct std::tuple_element<1, RootPointer> {
    using type = int;
};

template<std::size_t I>
constexpr int &get(RootPointer &ptr) {
    static_assert(I < 2);
    if constexpr (I == 0)
        return ptr.x;
    else
        return ptr.y;
}

template<std::size_t I>
constexpr int get(RootPointer ptr) {
    static_assert(I < 2);
    if constexpr (I == 0)
        return ptr.x;
    else
        return ptr.y;
}

#endif  // DWM_HPP
