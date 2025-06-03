/* See LICENSE file for copyright and license details. */
#ifndef DWM_UTIL_HPP
#define DWM_UTIL_HPP



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

#include <stdarg.h>
#include <stddef.h>

constexpr auto between(auto x, auto a, auto b) {
    return a <= x && x <= b;
}

void delay(int delay_for, void (*fn)(void *), void *arg);

#endif  // DWM_UTIL_HPP
