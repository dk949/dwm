#ifndef DWM_BOOLENUM_HPP
#define DWM_BOOLENUM_HPP

#define BOOLEAN_ENUM(Type)                                              \
    enum struct Type : bool;                                            \
    constexpr Type operator!(Type e) {                                  \
        return Type {!std::to_underlying(e)};                           \
    }                                                                   \
    constexpr Type operator&&(Type e1, Type e2) {                       \
        return Type {std::to_underlying(e1) && std::to_underlying(e2)}; \
    }                                                                   \
    constexpr bool operator&&(bool b, Type e) {                         \
        return b && std::to_underlying(e);                              \
    }                                                                   \
    constexpr Type operator&&(Type e, bool b) {                         \
        return Type {std::to_underlying(e) && b};                       \
    }                                                                   \
    constexpr Type operator||(Type e1, Type e2) {                       \
        return Type {std::to_underlying(e1) || std::to_underlying(e2)}; \
    }                                                                   \
    constexpr bool operator||(bool b, Type e) {                         \
        return b || std::to_underlying(e);                              \
    }                                                                   \
    constexpr Type operator||(Type e, bool b) {                         \
        return Type {std::to_underlying(e) || b};                       \
    }                                                                   \
    enum struct Type : bool

#endif  // DWM_BOOLENUM_HPP
