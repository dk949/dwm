#ifndef DWM_BOOLENUM_HPP
#define DWM_BOOLENUM_HPP

#define BOOLEAN_ENUM(Type)                                                \
    enum struct Type : bool;                                              \
    constexpr Type operator!(Type val) {                                  \
        return Type {!std::to_underlying(val)};                           \
    }                                                                     \
    constexpr Type operator&&(Type lhs, Type rhs) {                       \
        return Type {std::to_underlying(lhs) && std::to_underlying(rhs)}; \
    }                                                                     \
    constexpr bool operator&&(bool lhs, Type rhs) {                       \
        return lhs && std::to_underlying(rhs);                            \
    }                                                                     \
    constexpr Type operator&&(Type lhs, bool rhs) {                       \
        return Type {std::to_underlying(lhs) && rhs};                     \
    }                                                                     \
    constexpr Type operator||(Type lhs, Type rhs) {                       \
        return Type {std::to_underlying(lhs) || std::to_underlying(rhs)}; \
    }                                                                     \
    constexpr bool operator||(bool lhs, Type rhs) {                       \
        return lhs || std::to_underlying(rhs);                            \
    }                                                                     \
    constexpr Type operator||(Type lhs, bool rhs) {                       \
        return Type {std::to_underlying(lhs) || rhs};                     \
    }                                                                     \
    enum struct Type : bool

#endif  // DWM_BOOLENUM_HPP
