#ifndef DWM_EVENT_TIME_UTILS_HPP
#define DWM_EVENT_TIME_UTILS_HPP

#include <chrono>
#include <ctime>
namespace chr = std::chrono;
// NOLINTNEXTLINE(google-global-names-in-headers) // Yes, but this is too convenient :)
using namespace std::chrono_literals;

// https://embeddedartistry.com/blog/2019/01/31/converting-between-timespec-stdchrono/

template<typename Rep, typename Period>
[[nodiscard]]
constexpr std::timespec fromChrono(chr::duration<Rep, Period> dur) {
    auto nano = chr::duration_cast<chr::nanoseconds>(dur);
    auto secs = chr::duration_cast<chr::seconds>(dur);
    nano -= secs;
    return {.tv_sec = secs.count(), .tv_nsec = nano.count()};
}

template<typename T = chr::nanoseconds>
[[nodiscard]]
constexpr T fromTimespec(std::timespec ts) {
    auto nano = chr::nanoseconds(ts.tv_nsec);
    auto secs = chr::seconds(ts.tv_sec);
    return chr::duration_cast<T>(secs + nano);
}

using DoubleSec = chr::duration<double>;
using DoubleMSec = chr::duration<double, std::milli>;

// NOLINTBEGIN(readability-magic-numbers) // tests
static_assert(fromChrono(2s).tv_sec == 2 && fromChrono(2s).tv_nsec == 0);
static_assert(fromChrono(2.5s).tv_sec == 2 && fromChrono(2.5s).tv_nsec == 500'000'000);
static_assert(fromTimespec({.tv_sec = 2, .tv_nsec = 500'000'000}) == 2.5s);
static_assert(fromTimespec<chr::seconds>({.tv_sec = 2, .tv_nsec = 0}) == 2s);
// NOLINTEND(readability-magic-numbers)

#endif  // DWM_EVENT_TIME_UTILS_HPP
