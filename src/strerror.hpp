#ifndef DWM_STRERROR_HPP
#define DWM_STRERROR_HPP


#include <cerrno>
#include <string_view>
std::string_view strError(int errnum);

// On Linux EAGAIN and EWOULDBLOCK are the same value. This causes a GCC warning
#if EAGAIN == EWOULDBLOCK
#    define DWM_IS_EAGAIN(x) ((x) == EAGAIN)
#else
#    define DWM_IS_EAGAIN(x) ((x) == EAGAIN || (x) == EWOULDBLOCK)
#endif

#endif  // DWM_STRERROR_HPP
