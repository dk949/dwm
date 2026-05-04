#include "display.hpp"

#include "log.hpp"
#include "stddir.hpp"

#include <nlohmann/json.hpp>
#include <X11/X.h>

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
namespace fs = std::filesystem;

using json = nlohmann::json;
static constexpr auto incomaptible_type = 302;
static constexpr auto indent = 2;
static constexpr auto display_conf_file = "display.json";

template<typename... Args>
static auto mkError(std::format_string<Args...> fmt, Args &&...args) {
    return std::unexpected(std::format(fmt, std::forward<Args>(args)...));
}

static bool isDisplayOff(json const &data) {
    return data.is_string() && data.get<std::string_view>() == "off";
}

static void to_json(json &data, Pos const &pos) {
    data = json(std::pair {pos.x, pos.y});
}

static void from_json(json const &data, Pos &pos) {
    data.at(0).get_to(pos.x);
    data.at(1).get_to(pos.y);
}

static void to_json(json &data, Scale const &scale) {
    data = json(std::pair {scale.x, scale.y});
}

static void from_json(json const &data, Scale &scale) {
    data.at(0).get_to(scale.x);
    data.at(1).get_to(scale.y);
}

static void to_json(json &data, DisplayConfig const &cfg) {
    data = json::object();
    data["orientation"] = cfg.orientation;
    data["mode"] = cfg.mode;
    data["pos"] = cfg.pos;
    data["scale"] = cfg.scale;
    data["primary"] = cfg.primary;
}

static void from_json(json const &data, DisplayConfig &cfg) {
    data.at("orientation").get_to(cfg.orientation);
    data.at("mode").get_to(cfg.mode);
    data.at("pos").get_to(cfg.pos);
    data.at("scale").get_to(cfg.scale);
    data.at("primary").get_to(cfg.primary);
}

static void to_json(json &data, DisplayOff const &) {
    data = "off";
}

static void from_json(json const &data, DisplayOff &val) {
    if (!isDisplayOff(data)) throw json::type_error::create(incomaptible_type, "Expected value 'off'", &data);
    val = {};
}

static void to_json(json &data, MaybeDisplayConfig const &cfg) {
    std::visit([&](auto const &value) { data = value; }, cfg);
}

static void from_json(json const &data, MaybeDisplayConfig &val) {
    if (isDisplayOff(data))
        val = data.get<DisplayOff>();
    else
        val = data.get<DisplayConfig>();
}

namespace {
struct ConfDir {
private:
    static inline std::optional<fs::path> m_file {};
public:
    static fs::path const &file() {
        if (!m_file) m_file = dir::config() / display_conf_file;
        fs::create_directories(dir::config());
        return m_file.value();
    }
};
}  // namespace

void saveDisplayConf(ArrangementConfig const &cfg) {
    try {
        std::ofstream ofs {ConfDir::file()};
        ofs << json {cfg}.dump(indent);
    } catch (std::exception const &e) {
        lg::error("Could save display conf: {}", e.what());
    }
}

std::optional<ArrangementConfig> loadDisplayConf() {
    try {
        std::ifstream ifs {ConfDir::file()};
        return json::parse(ifs).get<ArrangementConfig>();
    } catch (std::exception const &e) {
        lg::error("Could load display conf: {}", e.what());
        return std::nullopt;
    }
}
