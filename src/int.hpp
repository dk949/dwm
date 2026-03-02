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

template<std::integral BackingInt>
struct BasicInt {
private:
    static constexpr auto int_max = std::numeric_limits<BackingInt>::max();
    static constexpr auto int_min = std::numeric_limits<BackingInt>::min();
public:
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

    template<std::integral I>
    static constexpr BasicInt from(I i) noexcept {
        return BasicInt(cast<BackingInt>(i));
    }

    [[nodiscard]]
    constexpr bool operator==(BasicInt const &) const noexcept = default;
    [[nodiscard]]
    constexpr std::strong_ordering operator<=>(BasicInt const &) const noexcept = default;

    [[nodiscard]]
    constexpr bool operator==(std::integral auto i) const noexcept {
        return std::cmp_equal(m_data, i);
    }

    [[nodiscard]]
    constexpr std::strong_ordering operator<=>(std::integral auto i) const noexcept {
        if (std::cmp_equal(m_data, i))
            return std::strong_ordering::equivalent;
        else if (std::cmp_less(m_data, i))
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
    constexpr BasicInt operator-() const noexcept { return BasicInt(safeNeg()); }
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
    constexpr BasicInt operator+(BasicInt i) const noexcept { return BasicInt(safeAdd(i.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator-(BasicInt i) const noexcept { return BasicInt(safeSub(i.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator*(BasicInt i) const noexcept { return BasicInt(safeMul(i.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator/(BasicInt i) const noexcept { return BasicInt(safeDiv(i.m_data)); }
    [[nodiscard]]
    constexpr BasicInt operator%(BasicInt i) const noexcept { return BasicInt(safeMod(i.m_data)); }

    [[nodiscard]]
    constexpr BasicInt operator+(std::integral auto i) const noexcept { return BasicInt(safeAdd(cast<BackingInt>(i))); }
    [[nodiscard]]
    constexpr BasicInt operator-(std::integral auto i) const noexcept { return BasicInt(safeSub(cast<BackingInt>(i))); }
    [[nodiscard]]
    constexpr BasicInt operator*(std::integral auto i) const noexcept { return BasicInt(safeMul(cast<BackingInt>(i))); }
    [[nodiscard]]
    constexpr BasicInt operator/(std::integral auto i) const noexcept { return BasicInt(safeDiv(cast<BackingInt>(i))); }
    [[nodiscard]]
    constexpr BasicInt operator%(std::integral auto i) const noexcept { return BasicInt(safeMod(cast<BackingInt>(i))); }

    [[nodiscard]]
    friend constexpr BasicInt operator+(std::integral auto a, BasicInt b) noexcept { return BasicInt(BasicInt::from(a).safeAdd(b.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator-(std::integral auto a, BasicInt b) noexcept { return BasicInt(BasicInt::from(a).safeSub(b.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator*(std::integral auto a, BasicInt b) noexcept { return BasicInt(BasicInt::from(a).safeMul(b.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator/(std::integral auto a, BasicInt b) noexcept { return BasicInt(BasicInt::from(a).safeDiv(b.m_data)); }
    [[nodiscard]]
    friend constexpr BasicInt operator%(std::integral auto a, BasicInt b) noexcept { return BasicInt(BasicInt::from(a).safeMod(b.m_data)); }

    friend std::ostream &operator<<(std::ostream &os, BasicInt const &x) { return os << x.m_data; }
    friend std::istream &operator>>(std::istream &is, BasicInt &x) { return is >> x.m_data; }

    // clang-format on

    template<std::integral T, std::integral I>
    [[nodiscard]]
    static constexpr T cast(I i) noexcept {
        if constexpr (std::is_same_v<T, I>) {
            return i;
        } else {
            if (std::in_range<T>(i)) [[likely]]
                return i;
            else [[unlikely]] {
                std::string_view err;
                T out = 0;
                if (std::cmp_greater(i, std::numeric_limits<T>::max())) {
                    err = "overflow";
                    out = std::numeric_limits<T>::max();
                } else {
                    err = "underflow";
                    out = std::numeric_limits<T>::min();
                }
                if (!std::is_constant_evaluated())
                    lg::error("({}){} would cause {} for {}", ut::typeName<I>(), i, err, ut::typeName<T>());
                return out;
            }
        }
    }
private:
    [[nodiscard]]
    constexpr BackingInt safeAdd(BackingInt i) const {
        if (m_data > int_max - i) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Adding {} to {} would overflow, saturating", i, m_data);
            return int_max;
        }
        return m_data + i;
    }

    [[nodiscard]]
    constexpr BackingInt safeSub(BackingInt i) const {
        if (m_data < int_min + i) [[unlikely]] {
            if (!std::is_constant_evaluated())
                lg::error("Subtracting {} from {} would underflow, saturating", i, m_data);
            return int_min;
        }
        return m_data - i;
    }

    [[nodiscard]]
    constexpr BackingInt safeMul(BackingInt i) const {
        if constexpr (std::is_unsigned_v<BackingInt>) {
            if (i == 0) return 0;
            if (m_data > int_max / i) [[unlikely]] {
                if (!std::is_constant_evaluated())
                    lg::error("Multiplying {} by {} would overflow, saturating", m_data, i);
                return int_max;
            }
        } else {
            if (m_data == 0 || i == 0) return 0;

            if (m_data > 0) {
                if (i > 0) {
                    if (m_data > int_max / i) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would overflow, saturating", m_data, i);
                        return int_max;
                    }
                } else {
                    if (i < int_min / m_data) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would underflow, saturating", m_data, i);
                        return int_min;
                    }
                }
            } else {
                if (i > 0) {
                    if (m_data < int_min / i) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would underflow, saturating", m_data, i);
                        return int_min;
                    }
                } else {
                    if (m_data < int_max / i) [[unlikely]] {
                        if (!std::is_constant_evaluated())
                            lg::error("Multiplying {} by {} would overflow, saturating", m_data, i);
                        return int_max;
                    }
                }
            }
            return m_data * i;
        }
    }

    [[nodiscard]]
    constexpr BackingInt safeDiv(BackingInt i) const {
        if (i == 0) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Trying to divide {} by 0, using {}", m_data, int_max);
            return int_max;
        }
        if constexpr (std::is_unsigned_v<BackingInt>) return m_data / i;
        if (m_data == int_min && i == -1) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Dividing {} by {} would overflow, saturating", m_data, i);
            return int_max;
        }
        return m_data / i;
    }

    [[nodiscard]]
    constexpr BackingInt safeMod(BackingInt i) const {
        if (i == 0) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("Trying to perform {} modulo 0, using {}", m_data, int_max);
            return int_max;
        }
        if constexpr (std::is_unsigned_v<BackingInt>) return m_data % i;
        if (m_data == int_min && i == -1) [[unlikely]] {
            if (!std::is_constant_evaluated()) lg::error("{} modulo {} would overflow, saturating", m_data, i);
            return int_max;
        }
        return m_data % i;
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
// NOLINTBEGIN(readability-magic-numbers)
static_assert(Int(3) == Int(3));
static_assert(Int(3) != Int(4));
static_assert(Int(3) < Int(4));
static_assert(Int(3) <= Int(3));
static_assert(Int(3) <= Int(4));
static_assert(Int(4) > Int(3));
static_assert(Int(4) >= Int(3));
static_assert(Int(4) >= Int(4));

static_assert(Int(3) == 3);
static_assert(Int(3) != 4);
static_assert(Int(3) < 4);
static_assert(Int(3) <= 3);
static_assert(Int(3) <= 4);
static_assert(Int(4) > 3);
static_assert(Int(4) >= 3);
static_assert(Int(4) >= 4);

static_assert(3 == Int(3));
static_assert(3 != Int(4));
static_assert(3 < Int(4));
static_assert(3 <= Int(3));
static_assert(3 <= Int(4));
static_assert(4 > Int(3));
static_assert(4 >= Int(3));
static_assert(4 >= Int(4));

static_assert(Int(3) == 3ull);
static_assert(Int(3) != 4ull);
static_assert(Int(3) < 4ull);
static_assert(Int(3) <= 3ull);
static_assert(Int(3) <= 4ull);
static_assert(Int(4) > 3ull);
static_assert(Int(4) >= 3ull);
static_assert(Int(4) >= 4ull);

static_assert(+Int(1) == +1);
static_assert(-Int(1) == -1);
static_assert(Int(1));
static_assert(!Int(0));

static_assert(Int(10) + 1 == 11);
static_assert(Int(10) - 1 == 9);
static_assert(Int(10) * 2 == 20);
static_assert(Int(10) / 2 == 5);
static_assert(Int(10) % 2 == 0);

static_assert(Int(std::numeric_limits<int64_t>::max()) + 1 == std::numeric_limits<int64_t>::max());
static_assert(Int(std::numeric_limits<int64_t>::min()) - 1 == std::numeric_limits<int64_t>::min());
static_assert(Int(std::numeric_limits<int64_t>::max()) * 2 == std::numeric_limits<int64_t>::max());
static_assert(Int(std::numeric_limits<int64_t>::min()) / -1 == std::numeric_limits<int64_t>::max());
static_assert(Int(std::numeric_limits<int64_t>::min()) % -1 == std::numeric_limits<int64_t>::max());

static_assert(10 + Int(1) == 11);
static_assert(10 - Int(1) == 9);
static_assert(10 * Int(2) == 20);
static_assert(10 / Int(2) == 5);
static_assert(10 % Int(2) == 0);

static_assert(std::numeric_limits<int64_t>::max() + Int(1) == std::numeric_limits<int64_t>::max());
static_assert(std::numeric_limits<int64_t>::min() - Int(1) == std::numeric_limits<int64_t>::min());
static_assert(std::numeric_limits<int64_t>::max() * Int(2) == std::numeric_limits<int64_t>::max());
static_assert(std::numeric_limits<int64_t>::min() / Int(-1) == std::numeric_limits<int64_t>::max());
static_assert(std::numeric_limits<int64_t>::min() % Int(-1) == std::numeric_limits<int64_t>::max());

// NOLINTEND(readability-magic-numbers)
#endif  // DWM_INT_HPP
