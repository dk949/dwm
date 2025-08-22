/* See LICENSE file for copyright and license details. */
#ifndef DWM_UTIL_HPP
#define DWM_UTIL_HPP



#include <algorithm>
#include <functional>
#ifndef NDEBUG
#    define IF_DEBUG if (true)
#    ifdef DWM_TRACE_EVENTS
#        define IF_EVENT_TRACE if (true)
#    else
#        define IF_EVENT_TRACE if (false)
#    endif
#else  // NDEBUG
#    define IF_DEBUG       if (false)
#    define IF_EVENT_TRACE if (false)
#endif  // NDEBUG

constexpr auto between(auto x, auto a, auto b) {
    return a <= x && x <= b;
}

void delay(int delay_for, void (*fn)(void *), void *arg);

template<typename R, typename E, typename Cmp>
bool contains(R const &r, E &&_match, Cmp const &cmp = std::equal_to<E> {}) {
    return std::ranges::find_if(r, [&, match = std::forward<E>(_match)](E const &e) { return cmp(match, e); })
        == std::ranges::end(r);
}

#endif  // DWM_UTIL_HPP
