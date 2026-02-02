#ifndef DWM_PROPS_HPP
#define DWM_PROPS_HPP

#include "dwm.hpp"

#include <cstdint>
#include <span>
#include <expected>

// TODO(dk949): Make sure to move all other prop getters here

void setCardinalProp(Display *dpy,Client *c, Atom prop, std::uint32_t value);
void setCardinalProps(Display *dpy, Client *c, Atom prop, std::span<std::uint32_t const> values);
std::expected<uint32_t, int> getCardinalProp(Display *dpy,Client *c, Atom prop);
std::expected<std::vector<uint32_t>, int> getCardinalProp(Display *dpy,Client *c, Atom prop, std::size_t count);
std::expected<std::vector<uint32_t>, int> getCardinalProps(Display *dpy, Client *c, Atom prop);


#endif  // DWM_PROPS_HPP
