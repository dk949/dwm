#include "stddir.hpp"

#include "log.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>


namespace fs = std::filesystem;

namespace {
enum struct Dir {
    Config,
    Cache,
    Data,
    State,
    _last,  // NOLINT
};

struct CachedDirs {
private:
    static inline std::array<std::optional<fs::path>, std::to_underlying(Dir::_last)> m_paths {};
    static constexpr auto m_xdg_vars = [] {
        std::array<char const *, std::to_underlying(Dir::_last)> out;  // NOLINT(cppcoreguidelines-pro-type-member-init)
        out[std::to_underlying(Dir::Config)] = "XDG_CONFIG_HOME";
        out[std::to_underlying(Dir::Cache)] = "XDG_CACHE_HOME";
        out[std::to_underlying(Dir::Data)] = "XDG_DATA_HOME";
        out[std::to_underlying(Dir::State)] = "XDG_STATE_HOME";
        return out;
    }();
    static constexpr auto m_fallback_dirs = [] {
        std::array<char const *, std::to_underlying(Dir::_last)> out;  // NOLINT(cppcoreguidelines-pro-type-member-init)
        out[std::to_underlying(Dir::Config)] = ".config";
        out[std::to_underlying(Dir::Cache)] = ".cache";
        out[std::to_underlying(Dir::Data)] = ".local/share";
        out[std::to_underlying(Dir::State)] = ".local/state";
        return out;
    }();

    template<Dir directory>
    static std::optional<fs::path> fromXdg() {
        if (auto const *env_val = std::getenv(m_xdg_vars[std::to_underlying(directory)]))
            return fs::path(env_val);
        else
            return std::nullopt;
    }

    template<Dir directory>
    static fs::path fromFallback() {
        auto const *home = getenv("HOME");
        if (!home) {
            lg::error("Could not get value of $HOME");
            home = "/";
        }
        auto path = fs::path(home);
        path /= m_fallback_dirs[std::to_underlying(directory)];

        return path;
    }
public:

    template<Dir directory>
    static fs::path const &get() {
        static constexpr auto dir_val = std::to_underlying(directory);
        static_assert(dir_val >= 0 && dir_val < std::to_underlying(Dir::_last));
        auto &dir = m_paths[dir_val];
        if (!dir) dir = fromXdg<directory>();
        if (!dir) dir = fromFallback<directory>();
        (*dir) /= "dwm";
        return dir.value();
    }
};

}  // namespace

namespace dir {

fs::path const &config() {
    return CachedDirs::get<Dir::Config>();
}

fs::path const &cache() {
    return CachedDirs::get<Dir::Cache>();
}

fs::path const &data() {
    return CachedDirs::get<Dir::Data>();
}

fs::path const &state() {
    return CachedDirs::get<Dir::State>();
}

}  // namespace dir
