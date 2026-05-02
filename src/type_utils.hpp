#ifndef DWM_TYPE_UTILS_HPP
#define DWM_TYPE_UTILS_HPP


#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template<typename Variant, typename T>
struct IsInVariant : std::false_type { };

template<typename T, typename... Members>
struct IsInVariant<std::variant<Members...>, T> : std::disjunction<std::is_same<Members, T>...> { };

template<typename Variant, typename T>
inline constexpr auto is_in_variant_v = IsInVariant<Variant, T>::value;

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
struct TupleToVariant;

template<typename... Types>
struct TupleToVariant<std::tuple<Types...>> {
    using type = std::variant<Types...>;
};

template<typename T>
using TupleToVariantT = TupleToVariant<T>::type;

template<typename T>
struct VariantToTuple;

template<typename... Types>
struct VariantToTuple<std::variant<Types...>> {
    using type = std::tuple<Types...>;
};

template<typename T>
using VariantToTupleT = VariantToTuple<T>::type;

static_assert(std::is_same_v<TupleToVariantT<std::tuple<>>, std::variant<>>);
static_assert(std::is_same_v<TupleToVariantT<std::tuple<int, float>>, std::variant<int, float>>);
static_assert(std::is_same_v<VariantToTupleT<std::variant<>>, std::tuple<>>);
static_assert(std::is_same_v<VariantToTupleT<std::variant<int, float>>, std::tuple<int, float>>);
static_assert(std::is_same_v<TupleToVariantT<VariantToTupleT<std::variant<int, float>>>, std::variant<int, float>>);

template<typename Tuple, template<typename> typename Template>
struct MapTupleTypes;

template<template<typename> typename Template, typename... Types>
struct MapTupleTypes<std::tuple<Types...>, Template> {
    using type = std::tuple<Template<Types>...>;
};

template<typename Tuple, template<typename> typename Template>
using MapTupleTypesT = MapTupleTypes<Tuple, Template>::type;

static_assert(
    std::is_same_v<MapTupleTypesT<std::tuple<int, float>, std::vector>, std::tuple<std::vector<int>, std::vector<float>>>);

template<typename Str>
struct IsStdStringLike : std::disjunction<std::is_same<Str, std::string>, std::is_same<Str, std::string_view>> { };

template<typename Str>
inline constexpr auto is_std_string_like_v = IsStdStringLike<Str>::value;

template<typename Str>
concept StdStringLike = is_std_string_like_v<Str>;

template<typename Str>
struct IsStringViewLike
        : std::disjunction<
              std::disjunction<std::conjunction<std::is_array<std::remove_reference_t<Str>>,
                                   std::is_same<std::remove_const_t<std::remove_extent_t<std::remove_reference_t<Str>>>, char>>,
                  std::conjunction<std::is_pointer<std::remove_reference_t<Str>>,
                      std::is_same<char, std::remove_const_t<std::remove_pointer_t<std::remove_reference_t<Str>>>>>>,
              std::is_same<std::remove_reference_t<Str>, std::string_view>> { };

template<typename Str>
inline constexpr auto is_string_view_like_v = IsStringViewLike<Str>::value;

template<typename Str>
concept StringViewLike = is_string_view_like_v<Str>;

template<typename Str>
struct IsStringLike : std::disjunction<IsStringViewLike<Str>, IsStdStringLike<Str>> { };

template<typename Str>
inline constexpr auto is_string_like_v = IsStringLike<Str>::value;

template<typename Str>
concept StringLike = is_string_like_v<Str>;

// NOLINTBEGIN
static_assert(is_std_string_like_v<std::string_view>);
static_assert(is_std_string_like_v<std::string>);

static_assert(is_string_view_like_v<std::string_view>);
static_assert(is_string_view_like_v<char *>);
static_assert(is_string_view_like_v<char const *>);
static_assert(is_string_view_like_v<char[]>);
static_assert(is_string_view_like_v<char[10]>);
static_assert(is_string_view_like_v<char const[]>);
static_assert(is_string_view_like_v<char const[10]>);

static_assert(is_string_like_v<std::string_view>);
static_assert(is_string_like_v<std::string>);
static_assert(is_string_like_v<std::string_view>);
static_assert(is_string_like_v<char *>);
static_assert(is_string_like_v<char const *>);
static_assert(is_string_like_v<char[]>);
static_assert(is_string_like_v<char[10]>);
static_assert(is_string_like_v<char const[]>);
static_assert(is_string_like_v<char const[10]>);
// NOLINTEND
#endif  // DWM_TYPE_UTILS_HPP
