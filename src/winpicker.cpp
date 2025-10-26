#include "winpicker.hpp"

#include "config.hpp"
#include "dwm.hpp"
#include "log.hpp"

#include <ut/trim/trim.hpp>
#include <X11/Xlib.h>

#include <algorithm>
#include <bitset>
#include <charconv>
#include <format>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
namespace rng = std::ranges;
namespace vws = std::views;
static constexpr auto tagmask = ((1u << tags.size()) - 1);

// NOLINTBEGIN(readability-magic-numbers)

// syntax: '[' tag (',' tag)* (':' mon)? ']' instance '(' name ')'

[[nodiscard]]
static std::string encodeClientName(
    Display *dpy, Client const *c, Monitors::difference_type mon_idx, bool needs_mon) noexcept {
    std::string out;
    out.reserve(15 + std::char_traits<char>::length(c->name));
    out.push_back('[');
    auto tagset = std::bitset<sizeof(c->tags) * 8>(c->tags & tagmask);
    for (auto i = 0uz; i < std::min(tags.size(), tagset.size()); ++i) {
        if (tagset[i]) {
            if (out.back() != '[') out.push_back(',');
            std::format_to(std::back_inserter(out), "{}", i);
        }
    }
    if (needs_mon) std::format_to(std::back_inserter(out), ":{}", mon_idx);

    auto class_hint = c->classHint(dpy);
    std::format_to(std::back_inserter(out), "] {} ({})", class_hint.instance_hint.get(), c->name);

    return out;
}

static constexpr auto invalid_mon_tag = SIZE_MAX;

struct DecodedClient {
    std::bitset<tags.size()> tagset;
    std::size_t mon = invalid_mon_tag;
    std::string_view instance;
    std::string_view name;
    bool operator==(DecodedClient const &) const = default;
};

static constexpr auto invalid_client = DecodedClient {};

[[nodiscard]]
static DecodedClient decodeClientName(std::string_view name) noexcept {
    DecodedClient out;
    name = ut::trim(name);
    if (name.empty() || name.front() != '[') return invalid_client;
    while (true) {
        name = name.substr(1);
        std::size_t tag = 0;
        auto [ptr, errc] = std::from_chars(name.begin(), name.end(), tag);
        if (errc != std::errc {} || ptr == name.end() || ptr == name.begin()) return invalid_client;
        out.tagset[tag] = true;
        name = std::string_view {ptr, name.end()};
        if (*ptr != ',') break;
    }
    if (name.front() == ':') {
        name = name.substr(1);
        std::size_t mon = 0;
        auto [ptr, errc] = std::from_chars(name.begin(), name.end(), mon);
        if (errc != std::errc {} || ptr == name.end() || ptr == name.begin()) return invalid_client;
        out.mon = mon;
        name = std::string_view {ptr, name.end()};
    } else
        out.mon = 0;

    if (name.size() < 1 + 1 + 2 || name[0] != ']' || name[1] != ' ') return invalid_client;
    name = name.substr(2);
    auto client_name_start_idx = name.find('(');
    if (client_name_start_idx == name.npos) return invalid_client;
    auto instance = name.substr(0, client_name_start_idx);
    if (instance.empty() || instance.back() != ' ') return invalid_client;
    instance.remove_suffix(1);
    out.instance = instance;
    auto client_name = name.substr(client_name_start_idx);
    if (client_name.size() < 2 || client_name.front() != '(' || client_name.back() != ')') return invalid_client;
    client_name.remove_prefix(1);
    client_name.remove_suffix(1);
    out.name = client_name;

    return out;
}

[[nodiscard]]
static Client *decodedToClient(Display *dpy, Monitors const &mons, DecodedClient const &decoded) noexcept {
    if (decoded == invalid_client) return nullptr;
    if (mons.size() <= decoded.mon) return nullptr;
    auto const &mon = mons[decoded.mon];
    auto tagset = static_cast<unsigned>(decoded.tagset.to_ulong());
    Client *candidate = nullptr;
    for (Client *client = mon->clients; client; client = client->next) {
        if (client->tags != tagset) continue;
        if (std::string_view {client->name} != decoded.name) continue;
        auto class_hint = client->classHint(dpy);
        if (std::string_view {class_hint.instance_hint.get()} != decoded.instance) {
            candidate = client;
            continue;
        }
        return client;
    }
    if (candidate) {
        auto class_hint = candidate->classHint(dpy);
        lg::warn("Inexact window match: expected instance '{}', actual instance '{}'",
            decoded.instance,
            class_hint.instance_hint.get());
    }
    return candidate;
}

std::vector<std::string> winpickerCreateDmenuCommand(Display *dpy, Monitors const &mons, int current_mon) noexcept {
    auto total_client_count =
        rng::fold_left(mons, 0uz, [](auto count, auto const &mon) noexcept { return count + mon->clients->count(); });
    std::vector<std::string> args;
    args.reserve(13 + total_client_count);
    args.emplace_back("dmenu");
    args.emplace_back("-i");  // case insensitive
    args.emplace_back("-m");
    args.push_back(std::to_string(current_mon));
    args.emplace_back("-fn");
    args.emplace_back(dmenufont);
    args.emplace_back("-l");
    args.emplace_back("20");
    args.emplace_back("-c");
    args.emplace_back("-bw");
    args.emplace_back("3");
    args.emplace_back("-o");
    args.emplace_back("0.8");
    args.emplace_back("-it");
    auto needs_mon = mons.size() > 1;
    for (auto const &[mon_idx, mon] : mons | vws::enumerate)
        for (Client *client = mon->clients; client; client = client->next)
            args.push_back(encodeClientName(dpy, client, mon_idx, needs_mon));

    return args;
}

std::optional<std::pair<Client *, std::size_t>> winpickerMatchClient(Display *dpy,
    Monitors const &mons,
    std::string_view dmenu_str) noexcept {
    auto decoded = decodeClientName(dmenu_str);
    auto *client = decodedToClient(dpy, mons, decoded);
    if (!client) return std::nullopt;
    return std::pair {client, decoded.mon};
}

// NOLINTEND(readability-magic-numbers)
