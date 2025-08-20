#ifndef DWM_XINERAMA_HPP
#define DWM_XINERAMA_HPP

#include <X11/Xlib.h>

#include <utility>

bool xineramaIsActive(Display *);

struct ScreenInfo {
    int screen_number;
    short x_org;
    short y_org;
    short width;
    short height;
};

struct ScreenInfoPtr {
private:
    Display *m_dpy = nullptr;
    void *m_infos = nullptr;
    int m_count = 1;
    void free() noexcept;

    ScreenInfoPtr(Display *dpy, void *infos, int count)
            : m_dpy(dpy)
            , m_infos(infos)
            , m_count(count) { }
public:
    ~ScreenInfoPtr();

    ScreenInfoPtr(ScreenInfoPtr const &) = delete;
    ScreenInfoPtr &operator=(ScreenInfoPtr const &) = delete;

    ScreenInfoPtr(ScreenInfoPtr &&other) noexcept {
        *this = std::move(other);
    }

    ScreenInfoPtr &operator=(ScreenInfoPtr &&other) noexcept {
        if (this != &other) {
            free();
            m_infos = std::exchange(other.m_infos, nullptr);
        }
        return *this;
    }

    [[nodiscard]]
    ScreenInfo operator[](std::size_t idx) const noexcept;

    [[nodiscard]]
    std::size_t count() const noexcept {
        return static_cast<std::size_t>(m_count);
    }

    static ScreenInfoPtr query(Display *);
};

#endif  // DWM_XINERAMA_HPP
