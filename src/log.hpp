#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <print>

namespace lg {
extern FILE *log_file;
enum struct Level { Debug, Info, Warn, Error, Fatal };

namespace detail {
    char const *datetime(void);

    constexpr std::string_view log_level_str(Level l) {
        switch (l) {
            case Level::Debug: return "[DWM DBG]";
            case Level::Info: return "[DWM INFO]";
            case Level::Warn: return "[DWM WARN]";
            case Level::Error:
            case Level::Fatal: return "[DWM ERROR]"; ;
            default: return "[!!!]";
        }
    }
}  // namespace detail

template<Level level, typename... Args>
void log(std::format_string<Args...> fmt, Args &&...args) {
    auto file = log_file ? log_file : stderr;
    std::print(file, "{} {}: ", detail::datetime(), detail::log_level_str(level));
    std::println(file, fmt, std::forward<Args>(args)...);
    if (!log_file) fputs("NOTE: logfile unavailable", file);
    fflush(file);

    exit(1);
}

template<typename... Args>
void debug(std::format_string<Args...> fmt, Args &&...args) {
    return log<Level::Debug, Args...>(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void info(std::format_string<Args...> fmt, Args &&...args) {
    return log<Level::Info, Args...>(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void warn(std::format_string<Args...> fmt, Args &&...args) {
    return log<Level::Warn, Args...>(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void error(std::format_string<Args...> fmt, Args &&...args) {
    return log<Level::Error, Args...>(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void fatal(std::format_string<Args...> fmt, Args &&...args) {
    log<Level::Fatal, Args...>(fmt, std::forward<Args>(args)...);
    std::exit(1);
}

/**
 * Get dwm log directory
 *
 * Caller owns returned string
 *
 * If an error occurs, `NULL` is returned
 */
std::optional<std::filesystem::path> getLogDir(void);

}  // namespace lg
