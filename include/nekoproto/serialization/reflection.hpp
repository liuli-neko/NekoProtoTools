/**
 * @file reflection.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-27
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/global/reflection_tags.hpp"

#include <any>
#include <array>
#include <functional>
#include <map>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility> // For std::index_sequence

#include "private/tags.hpp"
#include "private/traits.hpp"

NEKO_BEGIN_NAMESPACE
template <typename T, class Enable = void>
struct Meta;

namespace detail {
template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_values_tuple(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) {
    // Construct the target tuple using the value elements (odd indices)
    return std::tuple(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple))...);
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_field_values_tuple(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) {
    return std::tuple(field_accessor(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple)))...);
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_field_tags_tuple(std::index_sequence<Is...> /*unused*/, ArgsTuple&& /*argsTuple*/) {
    using tuple = std::remove_reference_t<ArgsTuple>;
    return std::tuple(field_tags_v<std::tuple_element_t<(Is * Diameter) + Offset, tuple>>...);
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_names_array(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) {
    static_assert(
        ((std::is_convertible_v<std::tuple_element_t<(Is * Diameter) + Offset, std::remove_reference_t<ArgsTuple>>,
                                std::string_view>) &&
         ...), // Fold expression for check
        "All name arguments must be convertible to std::string_view");
    // Construct the target tuple using the value elements (odd indices)
    return std::array{std::string_view(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple)))...};
}

template <std::size_t Diameter, std::size_t Offset, typename... ValueTs>
consteval bool all_names_at() {
    using tuple = std::tuple<ValueTs...>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (std::is_convertible_v<std::tuple_element_t<(Is * Diameter) + Offset, tuple>, std::string_view> && ...);
    }(std::make_index_sequence<sizeof...(ValueTs) / Diameter>{});
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_values_array(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) {
    // Construct the target tuple using the value elements (odd indices)
    return std::array{std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple))...};
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_field_values_array(std::index_sequence<Is...> /*unused*/, ArgsTuple&& args_tuple) {
    return std::array{field_accessor(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(args_tuple)))...};
}

template <std::size_t... Is>
constexpr auto init_no_tags_tuple(std::index_sequence<Is...> /*unused*/) {
    return std::tuple{((void)Is, NoTags{})...};
}

template <std::size_t N>
using no_tags_tuple_t = decltype(init_no_tags_tuple(std::make_index_sequence<N>{}));

template <std::size_t N>
inline constexpr auto no_tags_tuple_v = init_no_tags_tuple(std::make_index_sequence<N>{}); // NOLINT

template <typename T>
using field_value_decay_t = std::remove_cvref_t<field_accessor_t<T>>;

template <typename T>
inline constexpr bool is_enum_field_value_v = std::is_enum_v<field_value_decay_t<T>>; // NOLINT

template <std::size_t Diameter, std::size_t Offset, typename... ValueTs>
consteval bool all_enum_field_values_at() {
    using tuple = std::tuple<ValueTs...>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (is_enum_field_value_v<std::tuple_element_t<(Is * Diameter) + Offset, tuple>> && ...);
    }(std::make_index_sequence<sizeof...(ValueTs) / Diameter>{});
}

template <std::size_t Diameter, std::size_t Offset, typename... ValueTs>
consteval bool enum_field_values_same_at() {
    using tuple = std::tuple<ValueTs...>;
    using first = field_value_decay_t<std::tuple_element_t<Offset, tuple>>;
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (std::is_same_v<first, field_value_decay_t<std::tuple_element_t<(Is * Diameter) + Offset, tuple>>> &&
                ...);
    }(std::make_index_sequence<sizeof...(ValueTs) / Diameter>{});
}

template <typename... ValueTs>
using double_type_tuple = decltype(init_values_tuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                           std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename... ValueTs>
using double_field_values_tuple = decltype(init_field_values_tuple<2, 1>(
    std::make_index_sequence<sizeof...(ValueTs) / 2>{}, std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename... ValueTs>
using double_field_tags_tuple = decltype(init_field_tags_tuple<2, 1>(
    std::make_index_sequence<sizeof...(ValueTs) / 2>{}, std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename... ValueTs>
using double_enum_tags_tuple = decltype(init_field_tags_tuple<2, 1>(
    std::make_index_sequence<sizeof...(ValueTs) / 2>{}, std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename... ValueTs>
using field_values_tuple = decltype(init_field_values_tuple<1, 0>(
    std::make_index_sequence<sizeof...(ValueTs)>{}, std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename... ValueTs>
using field_tags_tuple = decltype(init_field_tags_tuple<1, 0>(
    std::make_index_sequence<sizeof...(ValueTs)>{}, std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename... ValueTs>
using enum_tags_tuple = decltype(init_field_tags_tuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                             std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename T, class Obj>
concept is_member_ref_function = requires(T val, Obj& obj) { std::is_lvalue_reference<decltype(val(obj))>::value; };

template <typename T>
struct is_std_tuple_imp : std::false_type {}; // NOLINT
template <typename... Ts>
struct is_std_tuple_imp<std::tuple<Ts...>> : std::true_type {};

template <typename... Ts>
constexpr bool is_std_tuple() {
    if constexpr (sizeof...(Ts) == 1) {
        return is_std_tuple_imp<std::tuple_element_t<0, std::tuple<Ts...>>>::value;
    } else {
        return false;
    }
}

template <typename... Ts>
inline constexpr bool is_std_tuple_v = is_std_tuple<Ts...>(); // NOLINT

template <typename Tuple, typename Context>
struct tuple_unwrap;

template <typename... Args, typename Context>
struct tuple_unwrap<std::tuple<Args...>, Context> {
    using type = std::tuple<resolve_member_type_t<field_accessor_t<Args>, Context>...>;
};

template <typename Tuple, typename Context>
struct tuple_tags_unwrap;

template <typename... Args, typename Context>
struct tuple_tags_unwrap<std::tuple<Args...>, Context> {
    constexpr static auto value = std::make_tuple(field_tags_v<Args>...); // NOLINT
};

template <typename Tuple, typename Context>
using tuple_unwrap_t = typename tuple_unwrap<Tuple, Context>::type;

template <typename Tuple, typename Context>
static constexpr auto tuple_tags_unwrap_v = tuple_tags_unwrap<Tuple, Context>::value; // NOLINT
} // namespace detail

template <typename T, std::size_t N, typename TagsT = detail::no_tags_tuple_t<N>>
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
                (detail::all_names_at<2, 0, ValueTs...>()) && (detail::all_enum_field_values_at<2, 1, ValueTs...>()) &&
                (detail::enum_field_values_same_at<2, 1, ValueTs...>())
    constexpr Enumerate(ValueTs&&... values) noexcept {
        names               = detail::init_names_array<2, 0>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                             std::forward_as_tuple(values...));
        const auto& avalues = detail::init_field_values_array<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                                    std::forward_as_tuple(values...));
        const auto& atags   = detail::init_field_tags_tuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                                  std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = std::get<Is>(avalues)), ...);
        }(std::make_index_sequence<N>{});
        [this, &atags]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->tags) = std::get<Is>(atags)), ...);
        }(std::make_index_sequence<N>{});
    }
    template <typename... ValueTs>
        requires(sizeof...(ValueTs) > 0U) && (detail::all_enum_field_values_at<1, 0, ValueTs...>()) &&
                (detail::enum_field_values_same_at<1, 0, ValueTs...>())
    constexpr Enumerate(ValueTs&&... values) noexcept {
        auto enum_to_string = [](const T& value) constexpr -> std::string_view {
            constexpr auto k_enum_arr =
                detail::neko_get_valid_enum_names<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
            for (std::size_t idx = 0; idx < k_enum_arr.size(); ++idx) {
                if (k_enum_arr[idx].first == value) {
                    return k_enum_arr[idx].second;
                }
            }
            return {};
        };
        const auto& avalues = detail::init_field_values_array<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                                    std::forward_as_tuple(values...));
        const auto& atags   = detail::init_field_tags_tuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
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
        names               = detail::init_names_array<2, 0>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                             std::forward_as_tuple(values...));
        const auto& avalues = detail::init_values_tuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                              std::forward_as_tuple(values...));
        const auto& atags   = detail::init_field_tags_tuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                                  std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = field_accessor(std::get<Is>(avalues))), ...);
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
        const auto& avalues = detail::init_values_tuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                              std::forward_as_tuple(values...));
        const auto& atags   = detail::init_field_tags_tuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                                  std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = field_accessor(std::get<Is>(avalues))), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
        [this, &atags]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->tags) = std::get<Is>(atags)), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
    }
};

template <typename... ValueTs>
    requires(sizeof...(ValueTs) > 0U) && (sizeof...(ValueTs) % 2 == 0U) && (detail::all_names_at<2, 0, ValueTs...>()) &&
            (detail::all_enum_field_values_at<2, 1, ValueTs...>()) &&
            (detail::enum_field_values_same_at<2, 1, ValueTs...>())
Enumerate(ValueTs&&...) -> Enumerate<detail::field_value_decay_t<std::tuple_element_t<1, std::tuple<ValueTs...>>>,
                                     sizeof...(ValueTs) / 2, detail::double_enum_tags_tuple<ValueTs...>>;

template <typename... ValueTs>
    requires(sizeof...(ValueTs) > 0U) && (detail::all_enum_field_values_at<1, 0, ValueTs...>()) &&
            (detail::enum_field_values_same_at<1, 0, ValueTs...>())
Enumerate(ValueTs&&...) -> Enumerate<detail::field_value_decay_t<std::tuple_element_t<0, std::tuple<ValueTs...>>>,
                                     sizeof...(ValueTs), detail::enum_tags_tuple<ValueTs...>>;

template <typename... ValueTs>
Object(ValueTs&&...)
    -> Object<detail::double_field_values_tuple<ValueTs...>, detail::double_field_tags_tuple<ValueTs...>>;

template <typename... ValueTs>
Array(ValueTs&&...) -> Array<detail::field_values_tuple<ValueTs...>, detail::field_tags_tuple<ValueTs...>>;

namespace detail {
template <typename T, class = void>
struct is_ref_object : std::false_type {}; // NOLINT
template <typename T, typename TagsT>
struct is_ref_object<Object<T, TagsT>, void> : std::true_type {};
template <typename T, class = void>
struct is_ref_array : std::false_type {}; // NOLINT
template <typename T, typename TagsT>
struct is_ref_array<Array<T, TagsT>, void> : std::true_type {};

template <typename T, class = void>
struct is_ref_enumerate : std::false_type {}; // NOLINT

template <typename T, std::size_t N, typename TagsT>
struct is_ref_enumerate<Enumerate<T, N, TagsT>, void> : std::true_type {};

template <typename T, typename U, class = void>
struct MemberMetadata {
    using raw_type                          = field_accessor_t<T>;
    using parent_type                       = U;
    constexpr static bool is_ref            = std::is_reference_v<raw_type>;
    constexpr static bool is_l_ref          = std::is_lvalue_reference_v<raw_type>;
    constexpr static bool is_lambda         = is_member_ref_function<raw_type, U>;
    constexpr static bool is_member_pointer = std::is_member_object_pointer_v<raw_type>;
    constexpr static bool is_ok             = is_ref || is_lambda || is_member_pointer;

    constexpr static auto tags = field_tags_v<T>; // NOLINT
    static constexpr decltype(auto) get(auto&& accessor, U& obj) noexcept {
        using AccessorType = std::decay_t<decltype(accessor)>;
        if constexpr (std::is_member_object_pointer_v<AccessorType>) {
            return obj.*accessor;
        } else if constexpr (is_member_ref_function<AccessorType, U>) {
            return accessor(obj); // lambda 或函数
        } else if constexpr (is_tagged_field_v<AccessorType>) {
            return get(accessor.accessor, obj);
        } else {
            return accessor; // 直接引用
        }
    }
};

template <typename T>
concept is_local_ref_value = requires {
    { T::Neko::value };
    requires MemberMetadata<decltype(T::Neko::value), T>::is_ok;
};

template <typename T>
concept is_meta_ref_value = requires {
    { Meta<T>::value };
    requires MemberMetadata<decltype(Meta<T>::value), T>::is_ok;
};

template <typename A, typename T, class = void>
struct is_all_meta_ref_value : std::false_type {}; // NOLINT

template <typename A, typename... Args>
struct is_all_meta_ref_value<A, std::tuple<Args...>, void> {
    constexpr static bool value = (MemberMetadata<Args, A>::is_ok && ...); // NOLINT
};

template <typename T>
concept is_local_names = requires {
    { T::Neko::names };
};

template <typename T>
concept is_meta_names = requires {
    { Meta<T>::names };
};

template <typename T>
concept is_global_ref_values = requires(T& test) {
    { unwrap_struct(test) };
};

template <typename T>
concept is_meta_ref_values = requires {
    { Meta<T>::values };
    requires is_std_tuple_v<std::decay_t<decltype(Meta<T>::values)>>;
    requires is_all_meta_ref_value<T, std::decay_t<decltype(Meta<T>::values)>>::value;
};

template <typename T>
concept is_local_ref_values = requires {
    { T::Neko::values };
    requires is_std_tuple_v<std::decay_t<decltype(T::Neko::values)>>;
    requires is_all_meta_ref_value<T, std::decay_t<decltype(T::Neko::values)>>::value;
};

template <typename T>
concept is_local_ref_object = requires {
    { T::Neko::value };
    requires is_ref_object<std::decay_t<decltype(T::Neko::value)>>::value;
};

template <typename T>
concept is_meta_ref_object = requires {
    { Meta<T>::value };
    requires is_ref_object<std::decay_t<decltype(Meta<T>::value)>>::value;
};

template <typename T>
concept is_local_ref_array = requires {
    { T::Neko::value };
    requires is_ref_array<std::decay_t<decltype(T::Neko::value)>>::value;
};

template <typename T>
concept is_meta_ref_array = requires {
    { Meta<T>::value };
    requires is_ref_array<std::decay_t<decltype(Meta<T>::value)>>::value;
};

template <typename T>
concept is_meta_enumerate = std::is_enum_v<T> && requires {
    { Meta<T>::value };
    requires is_ref_enumerate<std::decay_t<decltype(Meta<T>::value)>>::value;
};

template <typename T, typename ObjT>
constexpr decltype(auto) value_ref(T&& accessor, ObjT& obj) noexcept {
    return MemberMetadata<std::decay_t<T>, ObjT>::get(std::forward<T>(accessor), obj);
}

template <typename T>
constexpr static bool is_ref_by_local_v = //  NOLINT
    (is_local_ref_values<T> && is_local_names<T>) || is_local_ref_object<T> || is_local_ref_array<T> ||
    is_local_ref_value<T>;

template <typename T>
constexpr static bool is_ref_by_meta_v = //  NOLINT
    (is_meta_ref_values<T> && is_meta_names<T>) || is_meta_ref_object<T> || is_meta_ref_array<T> ||
    is_meta_ref_value<T>;

enum class MetaKind {
    ErrorKind,        // error
    AutoUnwrap,       // can_unwrap_v<T> && !is_ref_by_local_v<T> && !is_ref_by_meta_v<T>
    LocalValuesNames, // is_local_ref_values<T> && is_local_names<T>
    LocalObject,      // is_local_ref_object<T>
    LocalArray,       // is_local_ref_array<T>
    LocalValue,       // is_local_ref_value<T> && !is_local_names<T>
    LocalValueNames,  // is_local_names<T> && is_local_ref_value<T>
    MetaValuesNames,  // !is_ref_by_local_v<T> && is_meta_ref_values<T> && is_meta_names<T>
    MetaObject,       // !is_ref_by_local_v<T> && is_meta_ref_object<T>
    MetaArray,        // !is_ref_by_local_v<T> && is_meta_ref_array<T>
    MetaValue,        // !is_ref_by_local_v<T> && is_meta_ref_value<T> && !is_meta_names<T>
    MetaValueNames    // !is_ref_by_local_v<T> && is_meta_names<T> && is_meta_ref_value<T>
};

// High-level backend selection.
//
// MetaKind still describes the exact legacy metadata shape needed by
// MetaPrivateBase. ReflectProviderKind describes which backend family owns the
// type, so a future native reflection provider can replace only automatic
// discovery without disturbing explicit T::Neko or Meta<T> metadata.
enum class ReflectProviderKind { Error, ExplicitMetadata, NativeReflection, LegacyAutoUnwrap };

template <typename T, class Enable = void>
struct NativeReflectionProvider {
    static constexpr bool available = false; // NOLINT
};

template <typename T>
inline constexpr bool native_reflection_provider_available_v = NativeReflectionProvider<T>::available; // NOLINT

// Native reflection annotation bridge.
//
// Future C++26 annotations should be translated here into the same tag objects
// used by make_tags today. The default field hook returns NoTags so enabling the
// placeholder provider cannot silently change serializer or argparser behavior.
template <auto... Annotations>
struct NativeAnnotationTags {
    static constexpr auto value = normalize_tags_v<Annotations...>; // NOLINT
};

template <auto... Annotations>
inline constexpr auto native_annotation_tags_v = NativeAnnotationTags<Annotations...>::value; // NOLINT

template <typename T, std::size_t I, class Enable = void>
struct NativeReflectionFieldTags {
    static constexpr auto value = NoTags{}; // NOLINT
};

template <typename T, std::size_t I>
inline constexpr auto native_reflection_field_tags_v = NativeReflectionFieldTags<T, I>::value; // NOLINT

template <typename T>
constexpr ReflectProviderKind get_reflect_provider_kind() noexcept {
    if constexpr (is_ref_by_local_v<T> || is_ref_by_meta_v<T>) {
        return ReflectProviderKind::ExplicitMetadata;
    } else if constexpr (native_reflection_provider_available_v<T>) {
        return ReflectProviderKind::NativeReflection;
    } else if constexpr (can_unwrap_v<T>) {
        return ReflectProviderKind::LegacyAutoUnwrap;
    } else {
        return ReflectProviderKind::Error;
    }
}

template <typename T>
constexpr static auto reflect_provider_kind_v = get_reflect_provider_kind<T>(); // NOLINT

// 辅助函数，确定类型使用哪种 tag
template <typename T>
constexpr MetaKind get_meta_kind() noexcept {
    if constexpr (is_local_ref_values<T> && is_local_names<T>) {
        return MetaKind::LocalValuesNames;
    } else if constexpr (is_local_ref_object<T>) {
        return MetaKind::LocalObject;
    } else if constexpr (is_local_ref_array<T>) {
        return MetaKind::LocalArray;
    } else if constexpr (is_local_ref_value<T> && is_local_names<T>) {
        return MetaKind::LocalValueNames;
    } else if constexpr (is_local_ref_value<T>) {
        return MetaKind::LocalValue;
    } else if constexpr (!is_ref_by_local_v<T> && is_meta_ref_values<T> && is_meta_names<T>) {
        return MetaKind::MetaValuesNames;
    } else if constexpr (!is_ref_by_local_v<T> && is_meta_ref_object<T>) {
        return MetaKind::MetaObject;
    } else if constexpr (!is_ref_by_local_v<T> && is_meta_ref_array<T>) {
        return MetaKind::MetaArray;
    } else if constexpr (!is_ref_by_local_v<T> && is_meta_ref_value<T> && is_meta_names<T>) {
        return MetaKind::MetaValueNames;
    } else if constexpr (!is_ref_by_local_v<T> && is_meta_ref_value<T>) {
        return MetaKind::MetaValue;
    } else if constexpr (can_unwrap_v<T>) {
        return MetaKind::AutoUnwrap;
    } else {
        return MetaKind::ErrorKind;
    }
}

template <typename T>
constexpr static auto meta_kind_v = get_meta_kind<T>(); // NOLINT

template <typename T, MetaKind Kind>
struct MetaPrivateBase;

// 基础实现 - AutoUnwrap
template <typename T>
struct MetaPrivateBase<T, MetaKind::AutoUnwrap> {
    static constexpr auto names() noexcept {
        return member_names_impl<T>(std::make_index_sequence<member_count_v<T>>{});
    }
    template <typename U>
        requires std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>
    static constexpr decltype(auto) value(U&& obj) {
        return unwrap_struct(std::forward<U>(obj));
    }
};

// 基础实现 - LocalValuesNames
template <typename T>
struct MetaPrivateBase<T, MetaKind::LocalValuesNames> {
    static constexpr auto names() noexcept { return T::Neko::names; }
    static constexpr auto value() noexcept { return T::Neko::values; }
};

// 基础实现 - LocalObject
template <typename T>
struct MetaPrivateBase<T, MetaKind::LocalObject> {
    static constexpr auto& names() noexcept { return T::Neko::value.names; }
    static constexpr auto& value() noexcept { return T::Neko::value.values; }
};

// 基础实现 - LocalArray
template <typename T>
struct MetaPrivateBase<T, MetaKind::LocalArray> {
    static constexpr auto& value() noexcept { return T::Neko::value.values; }
};

// 基础实现 - LocalValue
template <typename T>
struct MetaPrivateBase<T, MetaKind::LocalValue> {
    static constexpr auto value() noexcept { return T::Neko::value; }
};

// 基础实现 - LocalValueNames
template <typename T>
struct MetaPrivateBase<T, MetaKind::LocalValueNames> {
    static constexpr auto names() noexcept { return T::Neko::names; }
    static constexpr auto value() noexcept { return T::Neko::value; }
};

// Meta 变体 - 类似上面的实现
template <typename T>
struct MetaPrivateBase<T, MetaKind::MetaValuesNames> {
    static constexpr auto names() noexcept { return Meta<T>::names; }
    static constexpr auto value() noexcept { return Meta<T>::values; }
};

template <typename T>
struct MetaPrivateBase<T, MetaKind::MetaObject> {
    static constexpr auto& names() noexcept { return Meta<T>::value.names; }
    static constexpr auto& value() noexcept { return Meta<T>::value.values; }
};

template <typename T>
struct MetaPrivateBase<T, MetaKind::MetaArray> {
    static constexpr auto& value() noexcept { return Meta<T>::value.values; }
};

template <typename T>
struct MetaPrivateBase<T, MetaKind::MetaValue> {
    static constexpr auto value() noexcept { return Meta<T>::value; }
};

template <typename T>
struct MetaPrivateBase<T, MetaKind::MetaValueNames> {
    static constexpr auto names() noexcept { return Meta<T>::names; }
    static constexpr auto value() noexcept { return Meta<T>::value; }
};

template <typename T, class = void>
struct MetaPrivate : MetaPrivateBase<T, meta_kind_v<T>> {};

template <typename T>
concept has_names_meta = meta_kind_v<T> != MetaKind::ErrorKind && meta_kind_v<T> != MetaKind::MetaValue &&
                         meta_kind_v<T> != MetaKind::LocalValue && meta_kind_v<T> != MetaKind::MetaArray &&
                         meta_kind_v<T> != MetaKind::LocalArray;

template <typename T>
concept has_values_meta = meta_kind_v<T> != MetaKind::ErrorKind;

template <typename T>
concept has_value_function_one = requires(T& obj) {
    { MetaPrivate<T>::value(obj) };
};
template <typename T>
concept has_value_function = requires {
    { MetaPrivate<T>::value() };
};
template <typename T>
concept has_name_function = requires {
    { MetaPrivate<T>::names() };
};

template <typename Values, bool IsTuple = is_std_tuple_v<Values>>
struct ReflectAccessorCount {
    static constexpr std::size_t value = 1;
};

template <typename Values>
struct ReflectAccessorCount<Values, true> {
    static constexpr std::size_t value = std::tuple_size_v<Values>;
};

template <typename Values>
inline constexpr auto reflect_accessor_count_v = ReflectAccessorCount<Values>::value; // NOLINT

// Bridges the legacy "tuple of accessors or one accessor" representation into
// the index-based provider API. Reflect<T> should not need to know this storage
// detail anymore.
template <std::size_t I, typename Values, bool IsTuple = is_std_tuple_v<Values>>
struct ReflectAccessorAt {
    static_assert(I == 0, "single accessor index out of range");
    using type = Values;

    template <typename U>
    static constexpr decltype(auto) get(U&& values) {
        static_assert(I == 0, "single accessor index out of range");
        return std::forward<U>(values);
    }
};

template <std::size_t I, typename Values>
struct ReflectAccessorAt<I, Values, true> {
    using type = std::tuple_element_t<I, Values>;

    template <typename U>
    static constexpr decltype(auto) get(U&& values) {
        return std::get<I>(std::forward<U>(values));
    }
};

// Transitional metadata adapter.
//
// This is the single place that should know how current metadata is spelled:
// T::Neko, Meta<T>, Object/Array/value forms, macro-generated field_tags, and
// aggregate AutoUnwrap. Reflect<T> consumes this normalized provider surface so
// future native reflection can replace only the automatic discovery branch.
template <typename T>
struct ReflectProvider {
    using context_type = std::decay_t<T>;

    static constexpr ReflectProviderKind provider_kind = reflect_provider_kind_v<T>;                  // NOLINT
    static constexpr MetaKind legacy_kind              = meta_kind_v<T>;                              // NOLINT
    static constexpr MetaKind kind                     = legacy_kind;                                 // NOLINT
    static constexpr bool has_names                    = has_names_meta<T>;                           // NOLINT
    static constexpr bool has_values                   = provider_kind != ReflectProviderKind::Error; // NOLINT

    static constexpr auto names() {
        if constexpr (provider_kind == ReflectProviderKind::NativeReflection) {
            return NativeReflectionProvider<T>::names();
        } else if constexpr (has_name_function<T>) {
            return MetaPrivate<T>::names();
        } else {
            return std::array<std::string_view, 0>{};
        }
    }

    template <typename U>
    static constexpr decltype(auto) accessors(U&& obj) {
        if constexpr (provider_kind == ReflectProviderKind::NativeReflection) {
            return NativeReflectionProvider<T>::accessors(std::forward<U>(obj));
        } else if constexpr (has_value_function_one<T>) {
            return MetaPrivate<T>::value(std::forward<U>(obj));
        } else {
            return accessors();
        }
    }

    static constexpr decltype(auto) accessors() {
        if constexpr (provider_kind == ReflectProviderKind::NativeReflection) {
            return NativeReflectionProvider<T>::accessors();
        } else if constexpr (has_value_function<T>) {
            return MetaPrivate<T>::value();
        } else {
            return std::forward_as_tuple();
        }
    }

    using accessors_type = std::decay_t<decltype(accessors(std::declval<context_type&>()))>;

    static constexpr std::size_t value_count = has_values ? reflect_accessor_count_v<accessors_type> : 0; // NOLINT

    template <std::size_t I>
    using accessor_type = typename ReflectAccessorAt<I, accessors_type>::type;

    template <std::size_t I>
    using field_type = resolve_member_type_t<field_accessor_t<accessor_type<I>>, context_type>;

    template <std::size_t I>
    static constexpr auto name() {
        static_assert(has_names, "provider has no reflected names");
        constexpr auto field_names = names();
        static_assert(I < field_names.size(), "provider name index out of range");
        return field_names[I];
    }

    template <std::size_t I>
    static constexpr auto tag() {
        return std::get<I>(tags());
    }

    template <std::size_t I, typename U>
    static constexpr decltype(auto) get(U&& obj) {
        decltype(auto) values = accessors(std::forward<U>(obj));
        return value_ref(ReflectAccessorAt<I, std::decay_t<decltype(values)>>::get(values), obj);
    }

    static constexpr auto tags() {
        if constexpr (provider_kind == ReflectProviderKind::NativeReflection) {
            return NativeReflectionProvider<T>::tags();
        } else if constexpr (has_values) {
            using values_type = std::decay_t<decltype(accessors(std::declval<context_type&>()))>;
            if constexpr (is_local_ref_object<T> || is_local_ref_array<T>) {
                return T::Neko::value.tags;
            } else if constexpr (is_meta_ref_object<T> || is_meta_ref_array<T>) {
                return Meta<T>::value.tags;
            } else if constexpr (requires { T::Neko::field_tags; }) {
                return T::Neko::field_tags;
            } else if constexpr (requires { Meta<T>::field_tags; }) {
                return Meta<T>::field_tags;
            } else if constexpr (is_std_tuple_v<values_type>) {
                return tuple_tags_unwrap_v<values_type, context_type>;
            } else {
                return std::make_tuple(field_tags_v<values_type>);
            }
        } else {
            return std::make_tuple();
        }
    }
};

// Compile-time field model derived from a provider.
//
// The model resolves accessor declarations into concrete field types, runs tag
// validation after the host type is known, and exposes the public type/tag/count
// constants used by Reflect<T>. It deliberately does not call user callbacks or
// perform runtime lookup.
template <typename T>
struct ReflectModel {
private:
    using Provider    = ReflectProvider<T>;
    using ContextType = typename Provider::context_type;

    static constexpr auto _types() {
        if constexpr (Provider::has_values) {
            using values_type = std::decay_t<decltype(Provider::accessors(std::declval<ContextType&>()))>;
            if constexpr (is_std_tuple_v<values_type>) {
                static_assert(perform_all_checks<values_type, ContextType>(), "tags checks failed");
                return (tuple_unwrap_t<values_type, ContextType>*)nullptr;
            } else {
                static_assert(perform_check<resolve_member_type_t<field_accessor_t<values_type>, ContextType>,
                                            field_tags_v<values_type>>(),
                              "tags checks failed");
                return (std::tuple<resolve_member_type_t<field_accessor_t<values_type>, ContextType>>*)nullptr;
            }
        } else {
            return (std::tuple<>*)nullptr;
        }
    }

public:
    using provider    = Provider;
    using value_types = std::remove_pointer_t<decltype(_types())>;

    static constexpr auto field_tags = Provider::tags();               // NOLINT
    static constexpr int value_count = std::tuple_size_v<value_types>; // NOLINT
};

struct RefAny {
    RefAny()              = default;
    RefAny(const RefAny&) = default;
    RefAny(RefAny&&)      = default;
    template <typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, RefAny> && std::is_lvalue_reference_v<T &&>)
    RefAny(T&& obj) : mAny(std::ref(obj)) {}
    template <typename T>
    decltype(auto) as() {
        return std::any_cast<std::reference_wrapper<T>>(mAny).get();
    }
    RefAny& operator=(const RefAny&) = default;
    RefAny& operator=(RefAny&&)      = default;

    template <typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, RefAny>)
    RefAny& operator=(T&& obj) {
        as<std::remove_cvref_t<T>>() = std::forward<T>(obj);
        return *this;
    }

    const auto& type() const { return mAny.type(); }
    bool valid() const { return mAny.has_value(); }

    template <typename T>
    operator T() {
        return as<T>();
    }
    template <typename T>
    operator std::reference_wrapper<T>() {
        return std::any_cast<std::reference_wrapper<T>>(mAny);
    }
    template <typename T>
    bool operator==(const T& obj) const {
        return std::any_cast<std::reference_wrapper<T>>(mAny).get() == obj;
    }

private:
    std::any mAny;
};

} // namespace detail
template <class... Ts>
struct Overloads : public Ts... {
    using Ts::operator()...;
    Overloads(Ts... args) : Ts(args)... {}
};

template <typename T, class enable = void>
struct Reflect {
private:
    using Provider = detail::ReflectProvider<T>;
    using Model    = detail::ReflectModel<T>;

public:
    // Public reflection algorithm. It consumes only the provider/model surface:
    // no direct T::Neko or Meta<T> probing belongs below this line.
    template <typename U, typename CallAbleT>
    static constexpr void forEach(U&& obj, CallAbleT&& func) {
        if constexpr (!Provider::has_values) {
            static_assert(Provider::has_values, "type has no values meta");
        }
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                auto&& val  = Provider::template get<I>(obj);
                auto&& tags = std::get<I>(field_tags);
                // 编译期根据回调函数的签名选择调用方式
                if constexpr (std::is_invocable_v<CallAbleT, decltype(val), std::string_view, decltype(tags)>) {
                    static_assert(Provider::has_names, "type has no names meta or names size mismatch");
                    func(val, Provider::template name<I>(), tags);
                } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val), decltype(tags)>) {
                    // func(val, tags)
                    func(val, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val), std::string_view>) {
                    // func(val, name)
                    static_assert(Provider::has_names, "type has no names meta or names size mismatch");
                    func(val, Provider::template name<I>());
                } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val)>) {
                    // func(val)
                    func(val);
                } else {
                    static_assert(!Provider::has_values, "Callback function signature not supported. "
                                                         "Supported: (val, name, tags), (val, tags), (val, "
                                                         "name), (val)");
                }
            };
            ((invoke(std::integral_constant<std::size_t, Is>{})), ...);
        }(std::make_index_sequence<Provider::value_count>{});
    }

    static constexpr auto size() noexcept {
        if constexpr (Provider::has_names) {
            return Provider::names().size();
        } else if constexpr (Provider::has_values) {
            return Provider::value_count;
        } else {
            return 0;
        }
    }

    static constexpr decltype(auto) names() noexcept {
        if constexpr (Provider::has_names) {
            return Provider::names();
        } else {
            return std::array<std::string_view, 0>{};
        }
    }

    static constexpr auto name(int index) noexcept {
        if constexpr (Provider::has_names) {
            auto names = Provider::names();
            if (index < 0 || index >= static_cast<int>(names.size())) {
                NEKO_LOG_ERROR("reflection", "index out of range");
                return std::string_view{};
            }
            return names[index];
        } else {
            return std::string_view{};
        }
    }

    static constexpr decltype(auto) className() noexcept { return detail::class_nameof<T>; }

    template <std::size_t N, typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>>
    static decltype(auto) value(U&& obj) noexcept {
        if constexpr (Provider::has_values) {
            static_assert(Provider::value_count > N, "out of range");
            return Provider::template get<N>(std::forward<U>(obj));
        } else {
            static_assert(Provider::has_values, "no values meta");
        }
    }

    template <typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>> && std::is_lvalue_reference_v<U&&>
    static detail::RefAny value(U&& obj, int idx) {
        if constexpr (Provider::has_values) {
            detail::RefAny ret;
            [&obj, &ret, idx]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                ((idx == Is ? (ret = detail::RefAny(Provider::template get<Is>(obj))) : detail::RefAny{}), ...);
            }(std::make_index_sequence<Provider::value_count>{});
            return ret;
        } else {
            static_assert(Provider::has_values, "no values meta");
        }
    }

    template <typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>> && std::is_lvalue_reference_v<U&&>
    static detail::RefAny value(U&& obj, std::string_view name) {
        if constexpr (Provider::has_values && Provider::has_names) {
            detail::RefAny ret;
            [&obj, &ret, name]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                ((name == Provider::template name<Is>() ? (ret = detail::RefAny(Provider::template get<Is>(obj)))
                                                        : detail::RefAny{}),
                 ...);
            }(std::make_index_sequence<Provider::value_count>{});
            return ret;
        } else {
            static_assert(Provider::has_values && Provider::has_names, "no values or names meta");
        }
    }

    using value_types                = typename Model::value_types;
    static constexpr auto field_tags = Model::field_tags;  // NOLINT
    static constexpr int value_count = Model::value_count; // NOLINT

    template <typename CallAbleT>
    static constexpr void forEachMeta(CallAbleT&& func) {
        if constexpr (!Provider::has_values) {
            static_assert(Provider::has_values, "type has no values meta");
        }
        constexpr auto fieldNames = names();
        using names_type          = std::decay_t<decltype(fieldNames)>;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                using Field    = std::tuple_element_t<I, value_types>;
                using FieldTag = std::type_identity<Field>;
                auto&& tags    = std::get<I>(field_tags);
                if constexpr (std::is_invocable_v<CallAbleT&, FieldTag, std::string_view, decltype(tags)>) {
                    static_assert(detail::is_std_array<names_type>::size == value_count,
                                  "type has no names meta or names size mismatch");
                    func(FieldTag{}, fieldNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, std::string_view, decltype(tags)>) {
                    static_assert(detail::is_std_array<names_type>::size == value_count,
                                  "type has no names meta or names size mismatch");
                    func(fieldNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, FieldTag, decltype(tags)>) {
                    func(FieldTag{}, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, decltype(tags)>) {
                    func(tags);
                } else {
                    static_assert(!Provider::has_values,
                                  "Callback function signature not supported. Supported: (type, name, tags), "
                                  "(name, tags), (type, tags), (tags)");
                }
            };
            ((invoke(std::integral_constant<std::size_t, Is>{})), ...);
        }(std::make_index_sequence<value_count>{});
    }
};

template <typename T>
struct Reflect<T, std::enable_if_t<std::is_enum_v<T>>> {
private:
    static constexpr auto _tags() noexcept {
        if constexpr (detail::is_meta_enumerate<T>) {
            return Meta<T>::value.tags;
        } else {
            return detail::no_tags_tuple_v<size()>;
        }
    }

public:
    static constexpr auto names() noexcept {
        if constexpr (detail::is_meta_enumerate<T>) {
            return Meta<T>::value.names;
        } else {
            constexpr auto KEnumArr =
                detail::neko_get_valid_enum_names<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
            constexpr auto KEnumArrSize = KEnumArr.size();
            std::array<std::string_view, KEnumArrSize> names{};
            for (int i = 0; i < static_cast<int>(KEnumArrSize); ++i) {
                names[i] = KEnumArr[i].second;
            }
            return names;
        }
    }
    static constexpr auto values() noexcept {
        if constexpr (detail::is_meta_enumerate<T>) {
            return Meta<T>::value.values;
        } else {
            constexpr auto KEnumArr =
                detail::neko_get_valid_enum_names<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
            constexpr auto KEnumArrSize = KEnumArr.size();
            std::array<T, KEnumArrSize> values{};
            for (int i = 0; i < static_cast<int>(KEnumArrSize); ++i) {
                values[i] = KEnumArr[i].first;
            }
            return values;
        }
    }
    static const auto& nameMap() {
        static std::map<std::string_view, T> kNameMap = []() {
            auto map = std::map<std::string_view, T>{};
            auto ns  = names();
            auto vs  = values();
            for (int i = 0; i < ns.size(); ++i) {
                map[ns[i]] = vs[i];
            }
            return map;
        }();
        return kNameMap;
    }
    static const auto& valueMap() {
        static std::map<T, std::string_view> kValueMap = []() {
            auto map = std::map<T, std::string_view>{};
            auto ns  = names();
            auto vs  = values();
            for (int i = 0; i < static_cast<int>(ns.size()); ++i) {
                map[vs[i]] = ns[i];
            }
            return map;
        }();
        return kValueMap;
    }
    static constexpr auto className() noexcept { return detail::class_nameof<T>; }
    static constexpr auto size() noexcept { return names().size(); }
    static constexpr auto field_tags = _tags();                  // NOLINT
    static constexpr int value_count = static_cast<int>(size()); // NOLINT

    static constexpr auto value(std::string_view name) {
        auto kEnums = values();
        auto kNames = names();
        for (int i = 0; i < (int)size(); ++i) {
            if (kNames[i] == name) {
                return kEnums[i];
            }
        }
        NEKO_LOG_ERROR("reflection", "name not found");
        return T{}; // FIXME: this is not a good way to handle this
    }

    static constexpr auto name(T value) {
        const auto& k_names = names();
        const auto& k_enums = values();
        for (int i = 0; i < (int)size(); ++i) {
            if (k_enums[i] == value) {
                return k_names[i];
            }
        }
        NEKO_LOG_ERROR("reflection", "value not found");
        return std::string_view{}; // FIXME: this is not a good way to handle this
    }

    template <typename U>
        requires(std::is_same_v<T, U> || std::is_integral_v<U>)
    static constexpr auto flagsToString(U value, std::string delimiter = "|") {
        const auto& k_names = names();
        const auto& k_enums = values();
        std::string result;
        for (int i = 0; i < (int)size(); ++i) {
            if (static_cast<std::underlying_type_t<T>>(k_enums[i]) & static_cast<std::underlying_type_t<T>>(value)) {
                result += std::string(k_names[i]) + delimiter;
            }
        }
        if (result.size() > delimiter.size()) {
            result.erase(result.size() - delimiter.size());
        }
        return result;
    }

    template <typename CallAbleT>
    static constexpr void forEachMeta(CallAbleT&& func) {
        constexpr auto enumNames  = names();
        constexpr auto enumValues = values();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                constexpr auto value = enumValues[I];
                auto&& tags          = std::get<I>(field_tags);
                if constexpr (std::is_invocable_v<CallAbleT&, std::integral_constant<T, value>, std::string_view,
                                                  decltype(tags)>) {
                    func(std::integral_constant<T, value>{}, enumNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, T, std::string_view, decltype(tags)>) {
                    func(value, enumNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, std::string_view, decltype(tags)>) {
                    func(enumNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, std::integral_constant<T, value>,
                                                         decltype(tags)>) {
                    func(std::integral_constant<T, value>{}, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, T, decltype(tags)>) {
                    func(value, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, decltype(tags)>) {
                    func(tags);
                } else {
                    static_assert(!std::is_enum_v<T>,
                                  "Callback function signature not supported. Supported: (value, name, tags), "
                                  "(name, tags), (value, tags), (tags)");
                }
            };
            ((invoke(std::integral_constant<std::size_t, Is>{})), ...);
        }(std::make_index_sequence<size()>{});
    }
};

NEKO_END_NAMESPACE
