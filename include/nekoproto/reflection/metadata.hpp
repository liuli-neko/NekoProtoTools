#pragma once
#include "nekoproto/global/macros.hpp"

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/reflection/tags.hpp"
#include "nekoproto/global/traits.hpp"
#include <array>
#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace nekoproto {
NEKO_BEGIN_EXPORT
template <typename T, class Enable = void>
struct Meta;

namespace detail {
template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr auto initValuesTuple(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) -> decltype(auto) {
    // Construct the target tuple using the value elements (odd indices)
    return std::tuple(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple))...);
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr auto initFieldValuesTuple(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) -> decltype(auto) {
    return std::tuple(fieldAccessor(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple)))...);
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr auto initFieldTagsTuple(std::index_sequence<Is...> /*unused*/, ArgsTuple&& /*argsTuple*/) -> decltype(auto) {
    using tuple = std::remove_reference_t<ArgsTuple>;
    return std::tuple(field_tags_v<std::tuple_element_t<(Is * Diameter) + Offset, tuple>>...);
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr auto initNamesArray(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) -> decltype(auto) {
    static_assert(
        ((std::is_convertible_v<std::tuple_element_t<(Is * Diameter) + Offset, std::remove_reference_t<ArgsTuple>>,
                                std::string_view>) &&
         ...), // Fold expression for check
        "All name arguments must be convertible to std::string_view");
    // Construct the target tuple using the value elements (odd indices)
    return std::array{std::string_view(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple)))...};
}

template <std::size_t Diameter, std::size_t Offset, typename... ValueTs>
consteval auto allNamesAt() -> bool {
    using tuple = std::tuple<ValueTs...>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (std::is_convertible_v<std::tuple_element_t<(Is * Diameter) + Offset, tuple>, std::string_view> && ...);
    }(std::make_index_sequence<sizeof...(ValueTs) / Diameter>{});
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr auto initValuesArray(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) -> decltype(auto) {
    // Construct the target tuple using the value elements (odd indices)
    return std::array{std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple))...};
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr auto initFieldValuesArray(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) -> decltype(auto) {
    return std::array{fieldAccessor(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple)))...};
}

template <std::size_t... Is>
constexpr auto initNoTagsTuple(std::index_sequence<Is...> /*unused*/) {
    return std::tuple{((void)Is, NoTags{})...};
}

template <std::size_t N>
using NoTagsTupleT = decltype(initNoTagsTuple(std::make_index_sequence<N>{}));

template <std::size_t N>
inline constexpr auto no_tags_tuple_v = initNoTagsTuple(std::make_index_sequence<N>{});

template <typename T>
using FieldValueDecayT = std::remove_cvref_t<FieldAccessorT<T>>;

template <typename T>
inline constexpr bool is_enum_field_value_v = std::is_enum_v<FieldValueDecayT<T>>;

template <std::size_t Diameter, std::size_t Offset, typename... ValueTs>
consteval auto allEnumFieldValuesAt() -> bool {
    using tuple = std::tuple<ValueTs...>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_enum_field_value_v<std::tuple_element_t<(Is * Diameter) + Offset, tuple>> && ...);
    }(std::make_index_sequence<sizeof...(ValueTs) / Diameter>{});
}

template <std::size_t Diameter, std::size_t Offset, typename... ValueTs>
consteval auto enumFieldValuesSameAt() -> bool {
    using tuple = std::tuple<ValueTs...>;
    using first = FieldValueDecayT<std::tuple_element_t<Offset, tuple>>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (std::is_same_v<first, FieldValueDecayT<std::tuple_element_t<(Is * Diameter) + Offset, tuple>>> &&
                ...);
    }(std::make_index_sequence<sizeof...(ValueTs) / Diameter>{});
}

template <typename... ValueTs>
using DoubleTypeTuple = decltype(initValuesTuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                           std::declval<std::tuple<traits::RefType<ValueTs>...>>()));

template <typename... ValueTs>
using DoubleFieldValuesTuple = decltype(initFieldValuesTuple<2, 1>(
    std::make_index_sequence<sizeof...(ValueTs) / 2>{}, std::declval<std::tuple<traits::RefType<ValueTs>...>>()));

template <typename... ValueTs>
using DoubleFieldTagsTuple = decltype(initFieldTagsTuple<2, 1>(
    std::make_index_sequence<sizeof...(ValueTs) / 2>{}, std::declval<std::tuple<traits::RefType<ValueTs>...>>()));

template <typename... ValueTs>
using DoubleEnumTagsTuple = decltype(initFieldTagsTuple<2, 1>(
    std::make_index_sequence<sizeof...(ValueTs) / 2>{}, std::declval<std::tuple<traits::RefType<ValueTs>...>>()));

template <typename... ValueTs>
using FieldValuesTuple = decltype(initFieldValuesTuple<1, 0>(
    std::make_index_sequence<sizeof...(ValueTs)>{}, std::declval<std::tuple<traits::RefType<ValueTs>...>>()));

template <typename... ValueTs>
using FieldTagsTuple = decltype(initFieldTagsTuple<1, 0>(
    std::make_index_sequence<sizeof...(ValueTs)>{}, std::declval<std::tuple<traits::RefType<ValueTs>...>>()));

template <typename... ValueTs>
using EnumTagsTuple = decltype(initFieldTagsTuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                             std::declval<std::tuple<traits::RefType<ValueTs>...>>()));

template <typename T, class Obj>
concept IsMemberRefFunction = requires(T val, Obj& obj) { std::is_lvalue_reference<decltype(val(obj))>::value; };

template <typename T>
struct IsStdTupleImp : std::false_type {};
template <typename... Ts>
struct IsStdTupleImp<std::tuple<Ts...>> : std::true_type {};

template <typename... Ts>
constexpr auto isStdTuple() -> bool {
    if constexpr (sizeof...(Ts) == 1) {
        return IsStdTupleImp<std::tuple_element_t<0, std::tuple<Ts...>>>::value;
    } else {
        return false;
    }
}

template <typename... Ts>
inline constexpr bool is_std_tuple_v = isStdTuple<Ts...>();

template <typename Tuple, typename Context>
struct TupleUnwrap;

template <typename... Args, typename Context>
struct TupleUnwrap<std::tuple<Args...>, Context> {
    using type = std::tuple<ResolveMemberTypeT<FieldAccessorT<Args>, Context>...>;
};

template <typename Tuple, typename Context>
struct TupleTagsUnwrap;

template <typename... Args, typename Context>
struct TupleTagsUnwrap<std::tuple<Args...>, Context> {
    constexpr static auto value = std::make_tuple(field_tags_v<Args>...);
};

template <typename Tuple, typename Context>
using TupleUnwrapT = typename TupleUnwrap<Tuple, Context>::type;

template <typename Tuple, typename Context>
inline constexpr auto tuple_tags_unwrap_v = TupleTagsUnwrap<Tuple, Context>::value;

template <typename F, typename... Ts>
inline constexpr auto removeVoidToMonostate(F&& func, Ts&&... ts) -> decltype(auto) {
    if constexpr (std::is_void_v<decltype(func(std::forward<Ts>(ts)...))>) {
        func(std::forward<Ts>(ts)...);
        return std::monostate{};
    } else {
        return func(std::forward<Ts>(ts)...);
    }
}
} // namespace detail

template <typename T, std::size_t N, typename TagsT = detail::NoTagsTupleT<N>>
struct Enumerate {
    static_assert(std::is_enum_v<T>, "T must be a enum type");
    std::array<std::string_view, N> names;
    std::array<T, N> values;
    TagsT tags;

    Enumerate()                 = default;
    Enumerate(const Enumerate&) = default;
    Enumerate(Enumerate&&)      = default;

    template <typename... ValueTs>
        requires(sizeof...(ValueTs) > 0U) && (sizeof...(ValueTs) % 2 == 0U) &&
                (detail::allNamesAt<2, 0, ValueTs...>()) && (detail::allEnumFieldValuesAt<2, 1, ValueTs...>()) &&
                (detail::enumFieldValuesSameAt<2, 1, ValueTs...>())
    constexpr Enumerate(ValueTs&&... values) noexcept {
        names               = detail::initNamesArray<2, 0>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                             std::forward_as_tuple(values...));
        const auto& avalues = detail::initFieldValuesArray<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                                    std::forward_as_tuple(values...));
        const auto& atags   = detail::initFieldTagsTuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                                  std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = std::get<Is>(avalues)), ...);
        }(std::make_index_sequence<N>{});
        [this, &atags]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->tags) = std::get<Is>(atags)), ...);
        }(std::make_index_sequence<N>{});
    }
    template <typename... ValueTs>
        requires(sizeof...(ValueTs) > 0U) && (detail::allEnumFieldValuesAt<1, 0, ValueTs...>()) &&
                (detail::enumFieldValuesSameAt<1, 0, ValueTs...>())
    constexpr Enumerate(ValueTs&&... values) noexcept {
        auto enum_to_string = [](const T& value) constexpr -> std::string_view {
            constexpr auto& k_enum_arr = detail::EnumReflectionTable<T>::entries;
            for (std::size_t idx = 0; idx < k_enum_arr.size(); ++idx) {
                if (k_enum_arr[idx].first == value) {
                    return k_enum_arr[idx].second;
                }
            }
            return {};
        };
        const auto& avalues = detail::initFieldValuesArray<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                                    std::forward_as_tuple(values...));
        const auto& atags   = detail::initFieldTagsTuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                                  std::forward_as_tuple(values...));
        [this, &avalues, &enum_to_string]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = std::get<Is>(avalues),
              std::get<Is>(this->names)  = enum_to_string(std::get<Is>(avalues))),
             ...);
        }(std::make_index_sequence<N>{});
        [this, &atags]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->tags) = std::get<Is>(atags)), ...);
        }(std::make_index_sequence<N>{});
    }
};

template <typename T, typename TagsT>
struct Object {
    std::array<std::string_view, std::tuple_size_v<T>> names;
    T values;
    TagsT tags;

    constexpr Object()    = default;
    Object(const Object&) = default;
    Object(Object&&)      = default;

    template <typename... ValueTs>
        requires(sizeof...(ValueTs) % 2 == 0U)
    constexpr Object(ValueTs&&... values) noexcept {
        names               = detail::initNamesArray<2, 0>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                             std::forward_as_tuple(values...));
        const auto& avalues = detail::initValuesTuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                              std::forward_as_tuple(values...));
        const auto& atags   = detail::initFieldTagsTuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                                  std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = fieldAccessor(std::get<Is>(avalues))), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
        [this, &atags]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->tags) = std::get<Is>(atags)), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
    }
};

template <typename T, typename TagsT>
struct Array {
    T values;
    TagsT tags;

    Array()             = default;
    Array(const Array&) = default;
    Array(Array&&)      = default;

    template <typename... ValueTs>
    constexpr Array(ValueTs&&... values) noexcept {
        const auto& avalues = detail::initValuesTuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                              std::forward_as_tuple(values...));
        const auto& atags   = detail::initFieldTagsTuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                                  std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = fieldAccessor(std::get<Is>(avalues))), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
        [this, &atags]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->tags) = std::get<Is>(atags)), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
    }
};

template <typename... ValueTs>
    requires(sizeof...(ValueTs) > 0U) && (sizeof...(ValueTs) % 2 == 0U) && (detail::allNamesAt<2, 0, ValueTs...>()) &&
            (detail::allEnumFieldValuesAt<2, 1, ValueTs...>()) &&
            (detail::enumFieldValuesSameAt<2, 1, ValueTs...>())
Enumerate(ValueTs&&...) -> Enumerate<detail::FieldValueDecayT<std::tuple_element_t<1, std::tuple<ValueTs...>>>,
                                     sizeof...(ValueTs) / 2, detail::DoubleEnumTagsTuple<ValueTs...>>;

template <typename... ValueTs>
    requires(sizeof...(ValueTs) > 0U) && (detail::allEnumFieldValuesAt<1, 0, ValueTs...>()) &&
            (detail::enumFieldValuesSameAt<1, 0, ValueTs...>())
Enumerate(ValueTs&&...) -> Enumerate<detail::FieldValueDecayT<std::tuple_element_t<0, std::tuple<ValueTs...>>>,
                                     sizeof...(ValueTs), detail::EnumTagsTuple<ValueTs...>>;

template <typename... ValueTs>
Object(ValueTs&&...)
    -> Object<detail::DoubleFieldValuesTuple<ValueTs...>, detail::DoubleFieldTagsTuple<ValueTs...>>;

template <typename... ValueTs>
Array(ValueTs&&...) -> Array<detail::FieldValuesTuple<ValueTs...>, detail::FieldTagsTuple<ValueTs...>>;

NEKO_END_EXPORT
} // namespace nekoproto
