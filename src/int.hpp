#ifndef DWM_INT_HPP
#define DWM_INT_HPP

#include "log.hpp"

#include <ut/demangle/demangle.hpp>

#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

enum struct IntErrorKind : uint8_t { DivByZero, Underflow, Overflow };

template<std::integral BackingInt>
struct BasicIntError {
    IntErrorKind kind;
};

template<std::integral BackingInt>
struct BasicInt {
private:
    static constexpr auto int_max = std::numeric_limits<BackingInt>::max();
    static constexpr auto int_min = std::numeric_limits<BackingInt>::min();
    static constexpr auto is_unsigned = std::is_unsigned_v<BackingInt>;
    static constexpr auto is_signed = !is_unsigned;
public:
    using value_type = BackingInt;
    BackingInt m_data;

    template<std::integral I>
    [[nodiscard]]
    constexpr I as() const noexcept {
        return cast<I>(m_data);
    }

    [[nodiscard]]
    constexpr BackingInt get() const noexcept {
        return m_data;
    }

    static constexpr BasicInt from(std::integral auto val) noexcept {
        return BasicInt(cast<BackingInt>(val));
    }

    template<typename OtherBackingInt>
    requires(!std::is_same_v<BackingInt, OtherBackingInt>)
    static constexpr BasicInt from(BasicInt<OtherBackingInt> val) noexcept {
        return BasicInt(cast<BackingInt>(val.get()));
    }

    [[nodiscard]]
    constexpr bool operator==(BasicInt const &) const noexcept = default;
    [[nodiscard]]
    constexpr std::strong_ordering operator<=>(BasicInt const &) const noexcept = default;

    [[nodiscard]]
    constexpr bool operator==(std::integral auto val) const noexcept {
        return std::cmp_equal(m_data, val);
    }

    [[nodiscard]]
    constexpr std::strong_ordering operator<=>(std::integral auto val) const noexcept {
        if (std::cmp_equal(m_data, val))
            return std::strong_ordering::equivalent;
        else if (std::cmp_less(m_data, val))
            return std::strong_ordering::less;
        else
            return std::strong_ordering::greater;
    }

    // clang-format off
    constexpr BasicInt &operator+=(BasicInt const &rhs) noexcept { m_data = BasicInt(safeAdd(rhs.m_data)); return *this; }
    constexpr BasicInt &operator-=(BasicInt const &rhs) noexcept { m_data = BasicInt(safeSub(rhs.m_data)); return *this; }
    constexpr BasicInt &operator*=(BasicInt const &rhs) noexcept { m_data = BasicInt(safeMul(rhs.m_data)); return *this; }
    constexpr BasicInt &operator/=(BasicInt const &rhs) noexcept { m_data = BasicInt(safeDiv(rhs.m_data)); return *this; }
    constexpr BasicInt &operator%=(BasicInt const &rhs) noexcept { m_data = BasicInt(safeMod(rhs.m_data)); return *this; }

    constexpr BasicInt operator+() const noexcept { return *this; }
    constexpr BasicInt operator-() const noexcept requires(is_signed) { return BasicInt(safeNeg()); }
    constexpr BasicInt operator-() const noexcept requires(is_unsigned) = delete;
    constexpr bool operator!() const noexcept { return m_data == 0; }
    constexpr operator bool() const noexcept { return m_data != 0;} // NOLINT(google-explicit-constructor)
    template<std::integral  I>
    constexpr operator I() const noexcept = delete;

    constexpr BasicInt &operator++() noexcept { m_data = safeAdd(1); return *this; }
    [[nodiscard]]
    constexpr BasicInt operator++(int) noexcept { BasicInt tmp(*this); m_data = safeAdd(1); return tmp; }
    constexpr BasicInt &operator--() noexcept { m_data = safeSub(1); return *this; }
    [[nodiscard]]
    constexpr BasicInt operator--(int) noexcept { BasicInt tmp(*this); m_data = safeSub(1); return tmp; }

    [[nodiscard]]
    constexpr BasicInt operator+(BasicInt val) const noexcept { return BasicInt(safeAdd(val.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator-(BasicInt val) const noexcept { return BasicInt(safeSub(val.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator*(BasicInt val) const noexcept { return BasicInt(safeMul(val.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator/(BasicInt val) const noexcept { return BasicInt(safeDiv(val.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator%(BasicInt val) const noexcept { return BasicInt(safeMod(val.m_data)); }

    [[nodiscard]]
    constexpr BasicInt operator+(std::integral auto val) const noexcept { return BasicInt(safeAdd(cast<BackingInt>(val))); }
    [[nodiscard]]
    constexpr BasicInt operator-(std::integral auto val) const noexcept { return BasicInt(safeSub(cast<BackingInt>(val))); }
    [[nodiscard]]
    constexpr BasicInt operator*(std::integral auto val) const noexcept { return BasicInt(safeMul(cast<BackingInt>(val))); }
    [[nodiscard]]
    constexpr BasicInt operator/(std::integral auto val) const noexcept { return BasicInt(safeDiv(cast<BackingInt>(val))); }
    [[nodiscard]]
    constexpr BasicInt operator%(std::integral auto val) const noexcept { return BasicInt(safeMod(cast<BackingInt>(val))); }

    [[nodiscard]]
    friend constexpr BasicInt operator+(std::integral auto lhs, BasicInt rhs) noexcept { return BasicInt(BasicInt::from(lhs).safeAdd(rhs.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator-(std::integral auto lhs, BasicInt rhs) noexcept { return BasicInt(BasicInt::from(lhs).safeSub(rhs.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator*(std::integral auto lhs, BasicInt rhs) noexcept { return BasicInt(BasicInt::from(lhs).safeMul(rhs.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator/(std::integral auto lhs, BasicInt rhs) noexcept { return BasicInt(BasicInt::from(lhs).safeDiv(rhs.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator%(std::integral auto lhs, BasicInt rhs) noexcept { return BasicInt(BasicInt::from(lhs).safeMod(rhs.m_data)); }

    friend std::ostream &operator<<(std::ostream &stream, BasicInt const &x) { return stream << x.m_data; }
    friend std::istream &operator>>(std::istream &in_stream, BasicInt &x) { return in_stream >> x.m_data; }

    // clang-format on

    template<std::integral T, std::integral I>
    [[nodiscard]]
    static constexpr T cast(I val) noexcept {
        if constexpr (std::is_same_v<T, I>) {
            return val;
        } else {
            if (std::in_range<T>(val)) [[likely]]
                return static_cast<T>(val);
            else [[unlikely]] {
                std::string_view err;
                T out = 0;
                if (std::cmp_greater(val, std::numeric_limits<T>::max())) {
                    err = "overflow";
                    out = std::numeric_limits<T>::max();
                } else {
                    err = "underflow";
                    out = std::numeric_limits<T>::min();
                }
                if (!std::is_constant_evaluated())
                    lg::error("({}){} would cause {} for {}", ut::typeName<I>(), val, err, ut::typeName<T>());
                return out;
            }
        }
    }
private:
    [[nodiscard]]
    constexpr BackingInt safeAdd(BackingInt val) const {
        if (m_data > int_max - val) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Adding {} to {} would overflow, saturating", val, m_data);
            return int_max;
        }
        return m_data + val;
    }

    [[nodiscard]]
    constexpr BackingInt safeSub(BackingInt val) const {
        if (m_data < int_min + val) [[unlikely]] {
            if (!std::is_constant_evaluated())
                lg::error("Subtracting {} from {} would underflow, saturating", val, m_data);
            return int_min;
        }
        return m_data - val;
    }

    [[nodiscard]]
    constexpr BackingInt safeMul(BackingInt val) const {
        if constexpr (is_unsigned) {
            if (val == 0) return 0;
            if (m_data > int_max / val) [[unlikely]] {
                if (!std::is_constant_evaluated())
                    lg::error("Multiplying {} by {} would overflow, saturating", m_data, val);
                return int_max;
            }
            return m_data * val;
        } else {
            if (m_data == 0 || val == 0) return 0;

            if (m_data > 0) {
                if (val > 0) {
                    if (m_data > int_max / val) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would overflow, saturating", m_data, val);
                        return int_max;
                    }
                } else {
                    if (val < int_min / m_data) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would underflow, saturating", m_data, val);
                        return int_min;
                    }
                }
            } else {
                if (val > 0) {
                    if (m_data < int_min / val) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would underflow, saturating", m_data, val);
                        return int_min;
                    }
                } else {
                    if (m_data < int_max / val) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would overflow, saturating", m_data, val);
                        return int_max;
                    }
                }
            }
            return m_data * val;
        }
    }

    [[nodiscard]]
    constexpr BackingInt safeDiv(BackingInt val) const {
        if (val == 0) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Trying to divide {} by 0, using {}", m_data, int_max);
            return int_max;
        }
        if constexpr (is_unsigned)
            return m_data / val;
        else {
            if (m_data == int_min && val == -1) [[unlikely]] {
                if (!std::is_constant_evaluated())
                    lg::error("Dividing {} by {} would overflow, saturating", m_data, val);
                return int_max;
            }
            return m_data / val;
        }
    }

    [[nodiscard]]
    constexpr BackingInt safeMod(BackingInt val) const {
        if (val == 0) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Trying to perform {} modulo 0, using {}", m_data, int_max);
            return int_max;
        }
        if constexpr (is_unsigned)
            return m_data % val;
        else {
            if (m_data == int_min && val == -1) [[unlikely]] {
                if (!std::is_constant_evaluated()) lg::error("{} modulo {} would overflow, saturating", m_data, val);
                return int_max;
            }
            return m_data % val;
        }
    }

    [[nodiscard]]
    constexpr BackingInt safeNeg() const {
        if (m_data == int_min) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Negating {} would overflow, saturating", m_data);
            return int_max;
        }
        return -m_data;
    }
};

using Int = BasicInt<std::int64_t>;
using UInt = BasicInt<std::uint64_t>;
// NOLINTBEGIN(readability-magic-numbers)
static_assert([]<typename... I>() {
    (
        [] {
            static_assert(I(3) == I(3));
            static_assert(I(3) != I(4));
            static_assert(I(3) < I(4));
            static_assert(I(3) <= I(3));
            static_assert(I(3) <= I(4));
            static_assert(I(4) > I(3));
            static_assert(I(4) >= I(3));
            static_assert(I(4) >= I(4));

            static_assert(I(3) == 3);
            static_assert(I(3) != 4);
            static_assert(I(3) < 4);
            static_assert(I(3) <= 3);
            static_assert(I(3) <= 4);
            static_assert(I(4) > 3);
            static_assert(I(4) >= 3);
            static_assert(I(4) >= 4);

            static_assert(3 == I(3));
            static_assert(3 != I(4));
            static_assert(3 < I(4));
            static_assert(3 <= I(3));
            static_assert(3 <= I(4));
            static_assert(4 > I(3));
            static_assert(4 >= I(3));
            static_assert(4 >= I(4));

            static_assert(I(3) == 3ull);
            static_assert(I(3) != 4ull);
            static_assert(I(3) < 4ull);
            static_assert(I(3) <= 3ull);
            static_assert(I(3) <= 4ull);
            static_assert(I(4) > 3ull);
            static_assert(I(4) >= 3ull);
            static_assert(I(4) >= 4ull);

            static_assert(+I(1) == +1);
            static_assert(I(1));
            static_assert(!I(0));

            static_assert(I(10) + 1 == 11);
            static_assert(I(10) - 1 == 9);
            static_assert(I(10) * 2 == 20);
            static_assert(I(10) / 2 == 5);
            static_assert(I(10) % 2 == 0);

            static_assert(I(std::numeric_limits<typename I::value_type>::max()) + 1
                          == std::numeric_limits<typename I::value_type>::max());
            static_assert(I(std::numeric_limits<typename I::value_type>::min()) - 1
                          == std::numeric_limits<typename I::value_type>::min());
            static_assert(I(std::numeric_limits<typename I::value_type>::max()) * 2
                          == std::numeric_limits<typename I::value_type>::max());
            static_assert(I(std::numeric_limits<typename I::value_type>::min()) / -1
                          == std::numeric_limits<typename I::value_type>::max());
            static_assert(I(std::numeric_limits<typename I::value_type>::min()) % -1
                          == std::numeric_limits<typename I::value_type>::max());

            static_assert(10 + I(1) == 11);
            static_assert(10 - I(1) == 9);
            static_assert(10 * I(2) == 20);
            static_assert(10 / I(2) == 5);
            static_assert(10 % I(2) == 0);

            static_assert(std::numeric_limits<typename I::value_type>::max() + I(1)
                          == std::numeric_limits<typename I::value_type>::max());
            static_assert(std::numeric_limits<typename I::value_type>::min() - I(1)
                          == std::numeric_limits<typename I::value_type>::min());
            static_assert(std::numeric_limits<typename I::value_type>::max() * I(2)
                          == std::numeric_limits<typename I::value_type>::max());
        }(),
        ...);
    return true;
}.operator()<Int, UInt>());


static_assert(-Int(1) == -1);
static_assert(std::numeric_limits<Int::value_type>::min() / Int(-1) == std::numeric_limits<Int::value_type>::max());
static_assert(std::numeric_limits<Int::value_type>::min() % Int(-1) == std::numeric_limits<Int::value_type>::max());
static_assert(Int::from(UInt(0xffffffffffffffff)) == 0x7fffffffffffffff);

// NOLINTEND(readability-magic-numbers)
#endif  // DWM_INT_HPP
