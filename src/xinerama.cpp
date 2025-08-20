
#include "xinerama.hpp"

#include "log.hpp"

#include <X11/extensions/Xinerama.h>
#include <X11/Xlib.h>

bool xineramaIsActive(Display *dpy) {
    return XineramaIsActive(dpy) == True;
}

ScreenInfoPtr::~ScreenInfoPtr() {
    free();
}

ScreenInfo ScreenInfoPtr::operator[](std::size_t idx) const noexcept {
    if (!m_infos) {
        lg::error("Trying to index ScreenInfo with no xinerama active!");
        return {};
    }
    auto const *infos = static_cast<XineramaScreenInfo const *>(m_infos);
    return {
        .screen_number = infos[idx].screen_number,
        .x_org = infos[idx].x_org,
        .y_org = infos[idx].y_org,
        .width = infos[idx].width,
        .height = infos[idx].height,
    };
}

void ScreenInfoPtr::free() noexcept {
    if (m_infos) XFree(static_cast<XineramaScreenInfo *>(m_infos));
}

ScreenInfoPtr ScreenInfoPtr::query(Display *dpy) {
    int count = 0;
    auto *infos = XineramaQueryScreens(dpy, &count);
    return ScreenInfoPtr {dpy, infos, count};
}
