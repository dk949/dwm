#include "props.hpp"

#include "log.hpp"
#include "x_utils.hpp"

#include <X11/Xatom.h>

#include <limits>
#include <ranges>
#include <utility>

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

static constexpr auto UINT32_FORMAT = 32;

void setCardinalProp(Display *dpy, Client *c, Atom prop, std::uint32_t value) {
    setCardinalProps(dpy, c, prop, std::array {value});
}

void setCardinalProps(Display *dpy, Client *c, Atom prop, std::span<std::uint32_t const> values) {
    if (prop == None) return;
    if (values.size() == 0 || !std::in_range<int>(values.size())) {
        lg::error("Cannot set {} CARDINALS, length out of range", values.size());
        return;
    }
    // static constexpr auto
    std::vector<unsigned long> new_data;
    new_data.resize(values.size());
    for (std::size_t i = 0; i < new_data.size(); ++i)
        new_data[i] = values[i];

    auto const *data = reinterpret_cast<unsigned char const *>(new_data.data());
    auto const nelements = static_cast<int>(values.size());

    XChangeProperty(dpy, c->win, prop, XA_CARDINAL, UINT32_FORMAT, PropModeReplace, data, nelements);
    XSync(dpy, False);
}

static std::expected<std::vector<uint32_t>, int> getCardinalPropImpl(
    Display *dpy, Client *c, Atom prop, std::size_t count, bool too_few_props_is_error) {
    if (prop == None) return std::unexpected(Success);
    if (count == 0 || !std::in_range<long>(count)) {
        lg::error("Cannot get {} CARDINALS, length out of range", count);
        return std::unexpected(Success);
    }

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long nitems = 0;
    unsigned long bytes_after = 0;
    XPtr<unsigned char> prop_ret;

    int status = XGetWindowProperty(dpy,
        c->win,
        prop,
        0L,
        static_cast<long>(count),
        False,
        XA_CARDINAL,
        &actual_type,
        &actual_format,
        &nitems,
        &bytes_after,
        std::out_ptr(prop_ret));

    if (status != Success) return std::unexpected(status);
    if (actual_type == None) return std::unexpected(PropGetDoesNotExistError);
    if (actual_type != XA_CARDINAL) return std::unexpected(PropGetTypeError);
    if (actual_format != UINT32_FORMAT) return std::unexpected(PropGetFormatError);
    if (nitems == 0) return std::unexpected(PropGetNoItemError);
    if (too_few_props_is_error && nitems < count) lg::warn("Expected {} cardinals, got {}", count, nitems);

    auto *longs = reinterpret_cast<unsigned long *>(prop_ret.get());
    std::vector<uint32_t> out;
    out.resize(nitems);
    for (std::size_t i = 0; i < nitems; ++i)
        out[i] = static_cast<uint32_t>(longs[i]);

    return out;
}

std::expected<uint32_t, int> getCardinalProp(Display *dpy, Client *c, Atom prop) {
    return getCardinalProp(dpy, c, prop, 1).transform([](auto const &vec) { return vec.at(0); });
}

std::expected<std::vector<uint32_t>, int> getCardinalProp(Display *dpy, Client *c, Atom prop, std::size_t count) {
    return getCardinalPropImpl(dpy, c, prop, count, true);
}

std::expected<std::vector<uint32_t>, int> getCardinalProps(Display *dpy, Client *c, Atom prop) {
    return getCardinalPropImpl(dpy, c, prop, std::numeric_limits<long>::max(), false);
}

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
