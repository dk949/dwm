#include "strerror.hpp"

#include <array>
#include <cstring>

std::string_view strError(int errnum) {
    static constexpr auto errbufsz = 1024;
    static thread_local std::array<char, errbufsz> errbuf;
    // NOLINTNEXTLINE(readability-qualified-auto): res could be an int in which case `auto *` wouldn't be valid
    auto const res = strerror_r(errnum, errbuf.data(), errbufsz);
    if constexpr (std::is_same_v<decltype(res), int>)
        return errbuf.data();
    else
        return res;
}
