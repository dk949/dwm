#ifndef DWM_TYPE_UTILS_HPP
#define DWM_TYPE_UTILS_HPP


#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template<typename Variant, typename T>
struct is_in_variant : std::false_type { };

template<typename T, typename... Members>
struct is_in_variant<std::variant<Members...>, T> : std::disjunction<std::is_same<Members, T>...> { };

template<typename Variant, typename T>
inline constexpr auto is_in_variant_v = is_in_variant<Variant, T>::value;

template<typename T, typename Variant>
concept InVariant = is_in_variant_v<Variant, T>;

static_assert(!is_in_variant_v<std::variant<>, int>);
static_assert(!is_in_variant_v<std::variant<float>, int>);
static_assert(!is_in_variant_v<std::variant<float, double>, int>);
static_assert(is_in_variant_v<std::variant<int>, int>);
static_assert(is_in_variant_v<std::variant<int, float, double>, int>);
static_assert(is_in_variant_v<std::variant<int, float, double, int>, int>);

template<typename Variant, typename T>
struct variant_index;

template<typename T, typename... Members>
struct variant_index<std::variant<Members...>, T> {
    static constexpr auto value = []<std::size_t... idx>(std::index_sequence<idx...>) {
        // TODO(dk949): Not having a default value here will cause a compile error if T is not in Members.
        //              This might be better than returning npos if the value will be used to index an array.
        std::size_t out = std::variant_npos;
        void(([&]() {
            if constexpr (std::is_same_v<Members, T>) {
                out = idx;
                return false;
            } else
                return true;
        }() && ...));
        return out;
    }(std::make_index_sequence<sizeof...(Members)>());
};

template<typename Variant, typename T>
inline constexpr std::size_t variant_index_v = variant_index<Variant, T>::value;

static_assert(variant_index_v<std::variant<int>, int> == 0);
static_assert(variant_index_v<std::variant<float, int>, int> == 1);
static_assert(variant_index_v<std::variant<float, int, double>, int> == 1);
static_assert(variant_index_v<std::variant<float, int, double>, char> == std::variant_npos);


template<typename T>
struct tuple_to_variant;

template<typename... Types>
struct tuple_to_variant<std::tuple<Types...>> {
    using type = std::variant<Types...>;
};

template<typename T>
using tuple_to_variant_t = tuple_to_variant<T>::type;

template<typename T>
struct variant_to_tuple;

template<typename... Types>
struct variant_to_tuple<std::variant<Types...>> {
    using type = std::tuple<Types...>;
};

template<typename T>
using variant_to_tuple_t = variant_to_tuple<T>::type;

static_assert(std::is_same_v<tuple_to_variant_t<std::tuple<>>, std::variant<>>);
static_assert(std::is_same_v<tuple_to_variant_t<std::tuple<int, float>>, std::variant<int, float>>);
static_assert(std::is_same_v<variant_to_tuple_t<std::variant<>>, std::tuple<>>);
static_assert(std::is_same_v<variant_to_tuple_t<std::variant<int, float>>, std::tuple<int, float>>);
static_assert(std::is_same_v<tuple_to_variant_t<variant_to_tuple_t<std::variant<int, float>>>, std::variant<int, float>>);

template<typename Tuple, template<typename> typename Template>
struct map_tuple_types;

template<template<typename> typename Template, typename... Types>
struct map_tuple_types<std::tuple<Types...>, Template> {
    using type = std::tuple<Template<Types>...>;
};

template<typename Tuple, template<typename> typename Template>
using map_tuple_types_t = map_tuple_types<Tuple, Template>::type;

static_assert(
    std::is_same_v<map_tuple_types_t<std::tuple<int, float>, std::vector>, std::tuple<std::vector<int>, std::vector<float>>>);

template<typename Str>
struct is_std_string_like : std::disjunction<std::is_same<Str, std::string>, std::is_same<Str, std::string_view>> { };

template<typename Str>
inline constexpr auto is_std_string_like_v = is_std_string_like<Str>::value;

template<typename Str>
concept StdStringLike = is_std_string_like_v<Str>;

template<typename Str>
struct is_string_view_like
        : std::disjunction<
              std::conjunction<std::is_pointer<Str>, std::is_same<char, std::remove_const_t<std::remove_pointer_t<Str>>>>,
              std::is_same<Str, std::string_view>> { };

template<typename Str>
inline constexpr auto is_string_view_like_v = is_string_view_like<Str>::value;

template<typename Str>
concept StringViewLike = is_string_view_like_v<Str>;

template<typename Str>
struct is_string_like : std::disjunction<is_string_view_like<Str>, is_std_string_like<Str>> { };

template<typename Str>
inline constexpr auto is_string_like_v = is_string_like<Str>::value;

template<typename Str>
concept StringLike = is_string_like_v<Str>;

static_assert(is_std_string_like_v<std::string_view>);
static_assert(is_std_string_like_v<std::string>);

static_assert(is_string_view_like_v<std::string_view>);
static_assert(is_string_view_like_v<char *>);
static_assert(is_string_view_like_v<char const *>);

static_assert(is_string_like_v<std::string_view>);
static_assert(is_string_like_v<std::string>);
static_assert(is_string_like_v<std::string_view>);
static_assert(is_string_like_v<char *>);
static_assert(is_string_like_v<char const *>);
#endif  // DWM_TYPE_UTILS_HPP
