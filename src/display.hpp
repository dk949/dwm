#ifndef DWM_DISPLAY_HPP
#define DWM_DISPLAY_HPP

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
enum struct Orientation { Normal, Inverted, Left, Right };

struct Pos {
    int x;
    int y;
};

struct Scale {
    int x;
    int y;
};

struct DisplayOff { };

struct DisplayConfig {
    Orientation orientation;
    std::string mode;
    Pos pos;
    Scale scale;
    bool primary;
};

using MaybeDisplayConfig = std::variant<DisplayConfig, DisplayOff>;
using ArrangementConfig = std::unordered_map<std::string, MaybeDisplayConfig>;


void saveDisplayConf(ArrangementConfig const &);
std::optional<ArrangementConfig> loadDisplayConf();

#endif  // DWM_DISPLAY_HPP
