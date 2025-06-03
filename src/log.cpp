#include "log.hpp"

#include <cstring>

namespace lg {
FILE *log_file = nullptr;

std::optional<std::filesystem::path> getLogDir(void) {
    auto logsubdir = "dwm/log/";

    char const *xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home) {
        auto path = std::filesystem::path(xdg_cache_home) / logsubdir;
        std::error_code ec;
        if (!std::filesystem::exists(path)) std::filesystem::create_directories(path, ec);

        if (ec == std::errc {}) return path;
        warn("Failed to get XDG_CACHE_HOME ({}): {}", path.c_str(), strerror(errno));
    }

    char const *home = getenv("HOME");
    if (home) {
        auto path = std::filesystem::path(home) / ".cache" / logsubdir;
        std::error_code ec;
        if (!std::filesystem::exists(path)) std::filesystem::create_directories(path, ec);

        if (ec == std::errc {}) return path;
        warn("Failed to get $HOME/.cache directory ({}): {}", path.c_str(), strerror(errno));
    }
    return {};
}

namespace detail {

    char const *datetime(void) {
        static char buf[26];

        time_t timer = time(nullptr);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&timer));
        return buf;
    }

}  // namespace detail
}  // namespace lg
