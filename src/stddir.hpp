#ifndef DWM_STDDIR_HPP
#define DWM_STDDIR_HPP

#include <filesystem>

namespace dir {
[[nodiscard]]
std::filesystem::path const &config();
[[nodiscard]]
std::filesystem::path const &cache();
[[nodiscard]]
std::filesystem::path const &data();
[[nodiscard]]
std::filesystem::path const &state();
}  // namespace dir



#endif  // DWM_STDDIR_HPP
