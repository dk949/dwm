#ifndef DWM_VARIANT_UTILS_HPP
#define DWM_VARIANT_UTILS_HPP

#include <functional>
#include <string>
#include <type_traits>
#include <variant>

template<typename T>
struct is_variant : std::false_type { };

template<typename... Args>
struct is_variant<std::variant<Args...>> : std::true_type { };

template<typename T>
inline constexpr bool is_variant_v = is_variant<T>::value;

template<typename T>
concept Variant = is_variant_v<T>;

template<Variant T>
struct fn_ptr_variant { };

template<typename... Args>
struct fn_ptr_variant<std::variant<Args...>> {
    template<typename T>
    using get_fn_type = std::conditional_t<std::is_same_v<T, std::monostate>,
        void (*)(),
        std::conditional_t<(sizeof(T) > 2 * sizeof(void *)), void (*)(T const &), void (*)(T)>>;
    using type = std::variant<get_fn_type<Args>...>;
};

template<Variant T>
using fn_ptr_variant_t = fn_ptr_variant<T>::type;

static_assert(std::is_same_v<  //
    std::variant<void (*)(),
        void (*)(int),
        void (*)(float),
        void (*)(std::string const &)>,                                      //
    fn_ptr_variant_t<std::variant<std::monostate, int, float, std::string>>  //
    >);

template<Variant V>
constexpr bool variantInvoke(fn_ptr_variant_t<V> const &fn, V const &arg) {
    if (fn.index() != arg.index()) return false;
    auto const idx = fn.index();
    [&]<std::size_t... i>(std::index_sequence<i...>) {
        ([&]() {
            if (i == idx) {
                if constexpr (std::is_same_v<std::variant_alternative_t<i, V>, std::monostate>)
                    std::invoke(std::get<i>(fn));
                else
                    std::invoke(std::get<i>(fn), std::get<i>(arg));
                return false;
            }
            return true;
        }() && ...);
    }(std::make_index_sequence<std::variant_size_v<V>>());
    return true;
}

#endif  // DWM_VARIANT_UTILS_HPP
