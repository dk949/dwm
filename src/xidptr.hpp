#ifndef DWM_SRCXIDPTRXIDPTR_HPP
#define DWM_SRCXIDPTRXIDPTR_HPP

#include <X11/X.h>
#include <X11/Xlib.h>

#include <concepts>
#include <functional>
#include <memory>
#include <utility>

template<typename T>
struct XDeleter {
    void operator()(T *ptr) {
        XFree(ptr);
    }
};

template<typename T>
using XPtr = std::unique_ptr<T, XDeleter<T>>;

struct XidPtr {
private:
    XID m_xid = None;
    std::function<void(Display *, XID)> m_destructor;
    Display *m_dpy;
public:

    template<std::invocable<Display *, XID> D>
    explicit XidPtr(Display *dpy, D destructor)
            : m_destructor(std::move(destructor))
            , m_dpy(dpy) { }

    template<std::invocable<Display *, XID> D>
    XidPtr(Display *dpy, XID xid, D destructor)
            : m_xid(xid)
            , m_destructor(std::move(destructor))
            , m_dpy(dpy) { }

    XidPtr &operator=(XidPtr &&other) {
        if (this != &other) {
            reset();
            std::swap(m_xid, other.m_xid);
            std::swap(m_destructor, other.m_destructor);
            std::swap(m_dpy, other.m_dpy);
        }
        return *this;
    }

    XidPtr(XidPtr &&other) {
        *this = std::move(other);
    }

    ~XidPtr() {
        reset();
    }

    XidPtr(XidPtr const &) = delete;
    XidPtr &operator=(XidPtr const &) = delete;

    XID get() const {
        return m_xid;
    }

    void reset(XID new_xid = None) {
        if (m_xid != None) m_destructor(m_dpy, m_xid);
        m_xid = new_xid;
    }

    XID release() {
        return std::exchange(m_xid, None);
    }
};

#endif  // DWM_SRCXIDPTRXIDPTR_HPP
