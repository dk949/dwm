#include "procstat.hpp"

#include "file.hpp"
#include "log.hpp"
#include "strerror.hpp"

#include <unistd.h>

#include <array>
#include <charconv>
#include <climits>
#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

static_assert(std::is_same_v<pid_t, int>);
#define stringize2(x) #x
#define STRINGIZE(x)  stringize2(x)

static constexpr auto task_comm_len = 16;
static constexpr auto max_pid_size = std::string_view {STRINGIZE(INT_MAX)}.size();

pid_t getPpid(pid_t child) {
    if (child <= 0) return 0;
    static std::string path;
    path.clear();
    path.reserve(PATH_MAX);
    std::format_to(std::back_inserter(path), "/proc/{}/stat", child);
    auto stat = FilePtr {fopen(path.c_str(), "r")};
    if (!stat) {
        std::println("Could not open {}", path);
        return 0;
    }
    /*pid + space + ( + comm_len + ) + space + state + space + ppid + space */
    static constexpr auto read_size = max_pid_size + 1 + (1 + task_comm_len + 1) + 1 + 1 + 1 + max_pid_size + 1;
    std::array<char, read_size> buf;  // NOLINT(cppcoreguidelines-pro-type-member-init)
    auto const actual_read = fread(buf.data(), 1, buf.size(), stat.get());
    if (actual_read != read_size) {
        lg::error("Could not get ppid of {}: {}", child, strError(errno));
        return 0;
    }
    std::string_view file_stat {buf};
    auto const cmd_end = file_stat.rfind(')');
    if (cmd_end == file_stat.npos) {
        lg::error("Failed to parse {}: No command", path);
        return 0;
    }
    file_stat = file_stat.substr(cmd_end + 1);
    if (file_stat.size() < 1 + 1 + 1 + 1 + 1) {
        lg::error("Failed to parse {}: Command too long", path);
        return 0;
    }
    if (file_stat.front() != ' ') {
        lg::error("Failed to parse {}: No space after command", path);
        return 0;
    }
    file_stat = file_stat.substr(2);
    if (file_stat.front() != ' ') {
        lg::error("Failed to parse {}: No space after state", path);
        return 0;
    }
    file_stat = file_stat.substr(1);

    auto const space = file_stat.find(' ');
    if (space == file_stat.npos) {
        lg::error("Failed to parse {}: No space after ppid", path);
        return 0;
    }
    file_stat = file_stat.substr(0, space);
    if (file_stat.empty()) {
        lg::error("Failed to parse {}: No ppid", path);
        return 0;
    }
    pid_t out = 0;
    auto const [ptr, ec] = std::from_chars(file_stat.begin(), file_stat.end(), out);
    if (ec != std::errc {}) {
        lg::error("Failed to parse {}: {}", path, std::make_error_code(ec).message());
        return 0;
    }
    if (ptr != file_stat.end()) {
        lg::error("Failed to parse {}: Excess characters after ppid", path);
        return 0;
    }

    return out;
}
