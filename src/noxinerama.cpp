#include "xinerama.hpp"

bool xineramaIsActive(Display *) {
    return false;
}

ScreenInfoPtr::~ScreenInfoPtr() = default;  // NOLINT

ScreenInfo ScreenInfoPtr::operator[](std::size_t) const noexcept {
    // TODO(dk949): give this a sensible value
    return {};
}

void ScreenInfoPtr::free() noexcept { }

ScreenInfoPtr ScreenInfoPtr::query(Display *) {
    return {nullptr, 1};
}
