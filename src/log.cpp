#include "log.hpp"

#include "strerror.hpp"

#include <noticeboard/backend.hpp>
#include <noticeboard/noticeboard.hpp>
#include <X11/Xlib.h>

#include <cstdlib>
#include <format>
#include <optional>

static constexpr auto info_expiry = nb::ExpireTime {900};

static std::string_view getIcon(lg::Level level) {
    switch (level) {
        case lg::Level::Breakpoint:
        case lg::Level::Debug:
        case lg::Level::Info: return ICONDIR "/dwm-icon.svg";
        case lg::Level::Warn: return ICONDIR "/dwm-warning.svg";
        case lg::Level::Error:
        case lg::Level::Fatal: return ICONDIR "/dwm-error.svg";
        default: std::abort();
    }
}

static nb::Notice &getNotice(lg::Level level) {
    static std::optional<nb::Notice> null_notice {};
    static std::optional<nb::Notice> info_notice {};
    static std::optional<nb::Notice> warn_notice {};
    static std::optional<nb::Notice> error_notice {};
    switch (level) {
        case lg::Level::Breakpoint:
        case lg::Level::Debug:
            if (!null_notice) null_notice = nb::Notice {"dwm", nb::Backend::Null};
            return *null_notice;
        case lg::Level::Info:
            if (!info_notice) {
                info_notice = nb::Notice {"dwm"};
                info_notice->icon = getIcon(level);
                info_notice->expire_time = info_expiry;
            }
            return *info_notice;
        case lg::Level::Warn:
            if (!warn_notice) {
                warn_notice = nb::Notice {"dwm"};
                warn_notice->urgency = nb::Urgency::Critical;
                warn_notice->icon = getIcon(level);
            }
            return *warn_notice;
        case lg::Level::Error:
        case lg::Level::Fatal:
            if (!error_notice) {
                error_notice = nb::Notice {"dwm"};
                error_notice->urgency = nb::Urgency::Critical;
                error_notice->icon = getIcon(level);
            }
            return *error_notice;
        default: std::abort();
    }
}

namespace lg {
FILE *log_file = nullptr;

void sendNotice(Level level, std::string_view header, std::string_view body) {
    // TODO(dk949): handle exception properly
    try {
        getNotice(level).send(header, body);
    } catch (...) { }
}

std::optional<std::filesystem::path> getLogDir() {
    constexpr auto const *logsubdir = "dwm/log/";

    char const *xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home) {
        auto path = std::filesystem::path(xdg_cache_home) / logsubdir;
        std::error_code err;
        if (!std::filesystem::exists(path)) std::filesystem::create_directories(path, err);

        if (err == std::errc {}) return path;
        warn("Failed to get XDG_CACHE_HOME ({}): {}", path.c_str(), strError(errno));
    }

    char const *home = getenv("HOME");
    if (home) {
        auto path = std::filesystem::path(home) / ".cache" / logsubdir;
        std::error_code err;
        if (!std::filesystem::exists(path)) std::filesystem::create_directories(path, err);

        if (err == std::errc {}) return path;
        warn("Failed to get $HOME/.cache directory ({}): {}", path.c_str(), strError(errno));
    }
    return {};
}

std::filesystem::path setupLogging() {
    auto log_dir = lg::getLogDir();
    if (log_dir) {
        auto log_file_name = *log_dir / "dwm.log";
        lg::log_file = fopen(log_file_name.c_str(), "a");
        if (!lg::log_file) lg::fatal("could not open log file: {}", strError(errno));
        return *log_dir;
    } else {
        lg::fatal("Could not obtain log dir");
    }
}

}  // namespace lg
