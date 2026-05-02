#ifndef DWM_WINPICKER_HPP
#define DWM_WINPICKER_HPP

#include "dwm.hpp"

#include <X11/Xlib.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

[[nodiscard]]
std::vector<std::string> winPickerCreateDmenuCommand(Display *dpy, Monitors const &mons, int current_mon) noexcept;
[[nodiscard]]
std::optional<std::pair<Client *, std::size_t>> winPickerMatchClient(Display *dpy,
    Monitors const &mons,
    std::string_view dmenu_str) noexcept;

#endif  // DWM_WINPICKER_HPP
