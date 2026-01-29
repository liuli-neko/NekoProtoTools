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

#include <any>
#include <array>
#include <map>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility> // For std::index_sequence
#include <variant>

#include "private/tags.hpp"
#include "private/traits.hpp"

NEKO_BEGIN_NAMESPACE
template <typename T, class enable = void>
struct Meta;

namespace detail {
template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_values_tuple(std::index_sequence<Is...> /*unused*/, ArgsTuple&& argsTuple) {
    // Construct the target tuple using the value elements (odd indices)
    return std::tuple(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(argsTuple))...);
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_names_array(std::index_sequence<Is...> /*unused*/, ArgsTuple&& argsTuple) {
    static_assert(
        ((std::is_convertible_v<std::tuple_element_t<(Is * Diameter) + Offset, std::remove_reference_t<ArgsTuple>>,
                                std::string_view>) &&
         ...), // Fold expression for check
        "All name arguments must be convertible to std::string_view");
    // Construct the target tuple using the value elements (odd indices)
    return std::array{std::string_view(std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(argsTuple)))...};
}

template <std::size_t Diameter, std::size_t Offset, std::size_t... Is, typename ArgsTuple>
constexpr decltype(auto) init_values_array(std::index_sequence<Is...> /*unused*/, ArgsTuple&& argsTuple) {
    // Construct the target tuple using the value elements (odd indices)
    return std::array{std::get<(Is * Diameter) + Offset>(std::forward<ArgsTuple>(argsTuple))...};
}

template <typename... ValueTs>
using double_type_tuple = decltype(init_values_tuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
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
    using type = std::tuple<resolve_member_type_t<unwrap_tags_t<Args>, Context>...>;
};

template <typename Tuple, typename Context>
struct tuple_tags_unwrap;

template <typename... Args, typename Context>
struct tuple_tags_unwrap<std::tuple<Args...>, Context> {
    constexpr static auto value = std::make_tuple(unwrap_tags_v<Args>...); // NOLINT
};

template <typename Tuple, typename Context>
using tuple_unwrap_t = typename tuple_unwrap<Tuple, Context>::type;

template <typename Tuple, typename Context>
static constexpr auto tuple_tags_unwrap_v = tuple_tags_unwrap<Tuple, Context>::value; // NOLINT
} // namespace detail

template <typename T, std::size_t N>
struct Enumerate {
    static_assert(std::is_enum_v<T>, "T must be a enum type");
    std::array<std::string_view, N> names;
    std::array<T, N> values;

    Enumerate()                 = default;
    Enumerate(const Enumerate&) = default;
    Enumerate(Enumerate&&)      = default;

    template <typename... ValueTs>
        requires(sizeof...(ValueTs) % 2 == 0U) && ((std::is_enum_v<ValueTs> + ...) == (sizeof...(ValueTs) / 2))
    constexpr Enumerate(ValueTs&&... values) noexcept {
        names               = detail::init_names_array<2, 0>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                             std::forward_as_tuple(values...));
        const auto& avalues = detail::init_values_array<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                              std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = std::get<Is>(avalues)), ...);
        }(std::make_index_sequence<N>{});
    }
    template <typename... ValueTs>
        requires((std::is_enum_v<ValueTs> + ...) == sizeof...(ValueTs))
    constexpr Enumerate(ValueTs&&... values) noexcept {
        auto enumToString = [](const T& value) -> std::string {
            constexpr auto KEnumArr = neko_get_valid_enum_names<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
            std::string ret;
            for (int idx = 0; idx < KEnumArr.size(); ++idx) {
                if (KEnumArr[idx].first == value) {
                    ret = std::string(KEnumArr[idx].second);
                }
            }
            if (ret.empty()) {
                ret = std::to_string(std::underlying_type_t<T>(value));
            }
            return ret;
        };
        this->values = std::array{values...};
        this->names  = std::array{enumToString(values)...};
    }
};

template <typename T>
struct Object {
    std::array<std::string_view, std::tuple_size_v<T>> names;
    T values;

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
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = std::get<Is>(avalues)), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
    }
};

template <typename T>
struct Array {
    T values;

    Array()             = default;
    Array(const Array&) = default;
    Array(Array&&)      = default;

    template <typename... ValueTs>
    constexpr Array(ValueTs&&... values) noexcept {
        const auto& avalues = detail::init_values_tuple<1, 0>(std::make_index_sequence<sizeof...(ValueTs)>{},
                                                              std::forward_as_tuple(values...));
        [this, &avalues]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
            ((std::get<Is>(this->values) = std::get<Is>(avalues)), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>{});
    }
};

template <typename... ValueTs>
    requires(sizeof...(ValueTs) % 2 == 0U) && ((std::is_enum_v<ValueTs> + ...) == (sizeof...(ValueTs) / 2))
Enumerate(ValueTs&&...) -> Enumerate<std::tuple_element_t<1, std::tuple<ValueTs...>>, sizeof...(ValueTs) / 2>;

template <typename... ValueTs>
    requires((std::is_enum_v<ValueTs> + ...) == sizeof...(ValueTs))
Enumerate(ValueTs&&...) -> Enumerate<std::tuple_element_t<0, std::tuple<ValueTs...>>, sizeof...(ValueTs)>;

template <typename... ValueTs>
Object(ValueTs&&...) -> Object<detail::double_type_tuple<ValueTs...>>;

template <typename... ValueTs>
Array(ValueTs&&...) -> Array<std::tuple<traits::ref_type<ValueTs>...>>;

namespace detail {
template <typename T, class enable = void>
struct is_ref_object : std::false_type {}; // NOLINT
template <typename T>
struct is_ref_object<Object<T>, void> : std::true_type {};
template <typename T, class enable = void>
struct is_ref_array : std::false_type {}; // NOLINT
template <typename T>
struct is_ref_array<Array<T>, void> : std::true_type {};

template <typename T, class enable = void>
struct is_ref_enumerate : std::false_type {}; // NOLINT

template <typename T, std::size_t N>
struct is_ref_enumerate<Enumerate<T, N>, void> : std::true_type {};

template <typename T, typename U, class enable = void>
struct MemberMetadata {
    using raw_type                        = unwrap_tags_t<T>;
    using parent_type                     = U;
    constexpr static bool IsRef           = std::is_reference_v<raw_type>;
    constexpr static bool IsLRef          = std::is_lvalue_reference_v<raw_type>;
    constexpr static bool IsLambda        = is_member_ref_function<raw_type, U>;
    constexpr static bool IsMemberPointer = std::is_member_object_pointer_v<raw_type>;
    constexpr static bool IsOk            = IsRef || IsLambda || IsMemberPointer;

    constexpr static auto tags = unwrap_tags_v<T>; // NOLINT
    template <typename V>
    constexpr static std::conditional_t<is_tagged_value_v<V>,
                                        std::conditional_t<!IsLambda && !IsMemberPointer, raw_type&, raw_type>, V>
    value(V&& obj) {
        if constexpr (is_tagged_value_v<V>) {
            return std::forward<V>(obj).value;
        } else {
            return std::forward<V>(obj);
        }
    }
};

template <typename T>
concept is_local_ref_value = requires {
    { T::Neko::value };
    requires MemberMetadata<decltype(T::Neko::value), T>::IsOk;
};

template <typename T>
concept is_meta_ref_value = requires {
    { Meta<T>::value };
    requires MemberMetadata<decltype(Meta<T>::value), T>::IsOk;
};

template <typename A, typename T, class enable = void>
struct is_all_meta_ref_value : std::false_type {}; // NOLINT

template <typename A, typename... Args>
struct is_all_meta_ref_value<A, std::tuple<Args...>, void> {
    constexpr static bool value = (MemberMetadata<Args, A>::IsOk && ...); // NOLINT
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

template <typename T, typename U>
    requires MemberMetadata<std::decay_t<T>, U>::IsMemberPointer
constexpr decltype(auto) value_ref(T&& dt, U& obj) noexcept {
    return obj.*MemberMetadata<std::decay_t<T>, U>::value(dt);
}

template <typename T, typename ObjT>
    requires MemberMetadata<T, ObjT>::IsLRef && (!MemberMetadata<std::decay_t<T>, ObjT>::IsMemberPointer) &&
             (!MemberMetadata<std::decay_t<T>, ObjT>::IsLambda)
constexpr decltype(auto) value_ref(T&& dt, ObjT& /*unused*/) noexcept {
    return MemberMetadata<std::decay_t<T>, ObjT>::value(dt);
}

template <typename T, typename ObjT>
    requires MemberMetadata<std::decay_t<T>, ObjT>::IsLambda
constexpr decltype(auto) value_ref(T&& dt, ObjT& obj) noexcept {
    return MemberMetadata<std::decay_t<T>, ObjT>::value(dt)(obj);
}

template <typename T, typename ObjT>
    requires(!MemberMetadata<std::decay_t<T>, ObjT>::IsRef) && (!MemberMetadata<std::decay_t<T>, ObjT>::IsLRef) &&
            (!MemberMetadata<std::decay_t<T>, ObjT>::IsMemberPointer) &&
            (!MemberMetadata<std::decay_t<T>, ObjT>::IsLambda) && is_tagged_value_v<T>
constexpr decltype(auto) value_ref(T&& dt, ObjT& /*unused*/) noexcept {
    return MemberMetadata<std::decay_t<T>, ObjT>::value(dt);
}

template <typename T>
constexpr static bool is_ref_by_local_v = //  NOLINT
    (is_local_ref_values<T> && is_local_names<T>) || is_local_ref_object<T> || is_local_ref_array<T> ||
    is_local_ref_value<T>;

template <typename T>
constexpr static bool is_ref_by_meta_v = //  NOLINT
    (is_meta_ref_values<T> && is_meta_names<T>) || is_meta_ref_object<T> || is_meta_ref_array<T> ||
    is_meta_ref_value<T>;

template <typename T, class enable = void>
struct MetaPrivate;

template <typename T>
    requires can_unwrap_v<T> && (!is_ref_by_local_v<T>) && (!is_ref_by_meta_v<T>)
struct MetaPrivate<T, void> {
    static constexpr auto names = // NOLINT
        member_names_impl<T>(std::make_index_sequence<member_count_v<T>>{});
    template <typename U>
        requires std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>
    static constexpr decltype(auto) value(U&& obj) {
        return unwrap_struct(std::forward<U>(obj));
    }
};

template <typename T>
    requires is_local_ref_values<T> && is_local_names<T>
struct MetaPrivate<T, void> {
    static constexpr auto names = // NOLINT
        T::Neko::names;
    static constexpr auto value = // NOLINT
        T::Neko::values;
};

template <is_local_ref_object T>
struct MetaPrivate<T, void> {
    static constexpr auto& names() // NOLINT
    {
        return T::Neko::value.names;
    }
    static constexpr auto& value() // NOLINT
    {
        return T::Neko::value.values;
    }
};

template <is_local_ref_array T>
struct MetaPrivate<T, void> {
    static constexpr auto& value() // NOLINT
    {
        return T::Neko::value.values;
    }
};

template <typename T>
    requires is_local_ref_value<T> && (!is_local_names<T>)
struct MetaPrivate<T, void> {
    static constexpr auto value = // NOLINT
        T::Neko::value;
};

template <typename T>
    requires is_local_names<T> && is_local_ref_value<T>
struct MetaPrivate<T, void> {
    static constexpr auto names = // NOLINT
        T::Neko::names;
    static constexpr auto value = // NOLINT
        T::Neko::value;
};

template <typename T>
    requires(!is_ref_by_local_v<T>) && is_meta_ref_values<T> && is_meta_names<T>
struct MetaPrivate<T, void> {
    static constexpr auto names = // NOLINT
        Meta<T>::names;
    static constexpr auto value = // NOLINT
        Meta<T>::values;
};

template <typename T>
    requires(!is_ref_by_local_v<T>) && is_meta_ref_object<T>
struct MetaPrivate<T, void> {
    static constexpr auto& names() // NOLINT
    {
        return Meta<T>::value.names;
    }
    static constexpr auto& value() // NOLINT
    {
        return Meta<T>::value.values;
    }
};

template <typename T>
    requires(!is_ref_by_local_v<T>) && is_meta_ref_array<T>
struct MetaPrivate<T, void> {
    static constexpr auto& value() // NOLINT
    {
        return Meta<T>::value.values;
    }
};

template <typename T>
    requires(!is_ref_by_local_v<T>) && is_meta_ref_value<T> && (!is_meta_names<T>)
struct MetaPrivate<T, void> {
    static constexpr auto value = // NOLINT
        Meta<T>::value;
};

template <typename T>
    requires(!is_ref_by_local_v<T>) && is_meta_names<T> && is_meta_ref_value<T>
struct MetaPrivate<T, void> {
    static constexpr auto names = // NOLINT
        Meta<T>::names;
    static constexpr auto value = // NOLINT
        Meta<T>::value;
};

template <typename T>
concept has_names_meta = requires {
    { MetaPrivate<T>::names };
} || requires {
    { MetaPrivate<T>::names(std::declval<T&>()) };
} || requires {
    { MetaPrivate<T>::names() };
};

template <typename T>
concept has_values_meta = requires {
    { MetaPrivate<T>::value };
} || requires {
    { MetaPrivate<T>::value(std::declval<T&>()) };
} || requires {
    { MetaPrivate<T>::value() };
};

template <typename T>
concept has_value_function_one = requires(T& obj) {
    { MetaPrivate<T>::value(obj) };
};
template <typename T>
concept has_value_function = requires {
    { MetaPrivate<T>::value() };
};
template <typename T>
concept has_value_member = requires {
    { MetaPrivate<T>::value };
};
template <typename T>
concept has_name_function = requires {
    { MetaPrivate<T>::names() };
};

template <typename T>
concept has_name_member = requires {
    { MetaPrivate<T>::names };
};

template <typename T>
struct ReflectHelper {
    static constexpr auto getNames() {
        if constexpr (detail::has_name_function<T>) {
            return detail::MetaPrivate<T>::names();
        } else if constexpr (detail::has_name_member<T>) {
            return detail::MetaPrivate<T>::names;
        } else {
            return std::array<std::string_view, 0>{};
        }
    }
    template <typename U>
    static constexpr decltype(auto) getValues(U&& obj) {
        if constexpr (detail::has_value_function_one<T>) {
            return detail::MetaPrivate<T>::value(std::forward<U>(obj));
        } else {
            return getValues();
        }
    }
    static constexpr decltype(auto) getValues() {
        if constexpr (detail::has_value_function<T>) {
            return detail::MetaPrivate<T>::value();
        } else if constexpr (detail::has_value_member<T>) {
            return detail::MetaPrivate<T>::value;
        } else {
            return std::forward_as_tuple();
        }
    }
};
struct RefAny {
    RefAny()              = default;
    RefAny(const RefAny&) = default;
    RefAny(RefAny&&)      = default;
    template <typename T>
    RefAny(T&& obj) : mAny(std::ref(std::forward<T>(obj))) {}
    template <typename T>
    decltype(auto) as() {
        return std::any_cast<std::reference_wrapper<T>>(mAny).get();
    }
    RefAny& operator=(const RefAny&) = default;
    RefAny& operator=(RefAny&&)      = default;

    template <typename T>
    RefAny& operator=(T&& obj) {
        as<T>() = std::forward<T>(obj);
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
    static constexpr auto _types() {
        if constexpr (detail::has_values_meta<T>) {
            using values_type =
                std::decay_t<decltype(detail::ReflectHelper<T>::getValues(std::declval<std::decay_t<T>&>()))>;
            using ContextType = std::decay_t<T>;
            if constexpr (detail::is_std_tuple_v<values_type>) {
                static_assert(detail::perform_all_checks<values_type, ContextType>(), "tags checks failed");
                return (detail::tuple_unwrap_t<values_type, ContextType>*)nullptr;
            } else {
                static_assert(detail::perform_check<resolve_member_type_t<unwrap_tags_t<values_type>, ContextType>,
                                                    unwrap_tags_v<values_type>>(),
                              "tags checks failed");
                return (std::tuple<resolve_member_type_t<unwrap_tags_t<values_type>, ContextType>>*)nullptr;
            }
        } else {
            return (std::tuple<>*)nullptr;
        }
    }
    static constexpr auto _tags() {
        if constexpr (detail::has_values_meta<T>) {
            using values_type =
                std::decay_t<decltype(detail::ReflectHelper<T>::getValues(std::declval<std::decay_t<T>&>()))>;
            using ContextType = std::decay_t<T>;
            if constexpr (detail::is_std_tuple_v<values_type>) {
                return detail::tuple_tags_unwrap_v<values_type, ContextType>;
            } else {
                return std::make_tuple(unwrap_tags_v<values_type>);
            }
        } else {
            return std::make_tuple();
        }
    }

public:
    template <typename U, typename CallAbleT>
    static constexpr void forEach(U&& obj, CallAbleT&& func) noexcept {
        if constexpr (!detail::has_values_meta<T>) {
            static_assert(detail::has_values_meta<T>, "type has no values meta");
        }
        decltype(auto) values = detail::ReflectHelper<T>::getValues(obj);
        using value_types     = std::decay_t<decltype(values)>;
        auto names            = []() {
            if constexpr (detail::has_names_meta<T>) {
                return detail::ReflectHelper<T>::getNames();
            } else {
                return std::array<std::string_view, 0>{};
            }
        }();

        if constexpr (detail::is_std_tuple_v<value_types>) {
            constexpr auto Size = std::tuple_size_v<value_types>;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                    auto&& val  = detail::value_ref(std::get<I>(values), obj);
                    auto&& tags = std::get<I>(value_tags);
                    // 编译期根据回调函数的签名选择调用方式
                    if constexpr (std::is_invocable_v<CallAbleT, decltype(val), std::string_view, decltype(tags)>) {
                        static_assert(names.size() == Size, "type has no names meta or names size mismatch");
                        func(val, names[I], tags);
                    } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val), decltype(tags)>) {
                        // func(val, tags)
                        func(val, tags);
                    } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val), std::string_view>) {
                        // func(val, name)
                        static_assert(names.size() == Size, "type has no names meta or names size mismatch");
                        func(val, names[I]);
                    } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val)>) {
                        // func(val)
                        func(val);
                    } else {
                        static_assert(!detail::has_values_meta<T>, "Callback function signature not supported. "
                                                                   "Supported: (val, name, tags), (val, tags), (val, "
                                                                   "name), (val)");
                    }
                };

                ((invoke(std::integral_constant<std::size_t, Is>{})), ...);
            }(std::make_index_sequence<Size>{});
        } else {
            auto&& val = detail::value_ref(values, obj);
                    auto&& tags = std::get<0>(value_tags);
            if constexpr (std::is_invocable_v<CallAbleT, decltype(val), std::string_view,
                                              decltype(tags)>) {
                static_assert(names.size() == 1, "type has no names meta or names size mismatch");
                func(val, names[0], tags);
            } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val), decltype(tags)>) {
                func(val, tags);
            } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val), std::string_view>) {
                static_assert(names.size() == 1, "type has no names meta or names size mismatch");
                func(val, names[0]);
            } else if constexpr (std::is_invocable_v<CallAbleT, decltype(val)>) {
                func(val);
            } else {
                static_assert(
                    !detail::has_values_meta<T>,
                    "Callback function signature not supported. Supported: (val, name, tags), (val, tags), (val, "
                    "name), (val)");
            }
        }
    }

    static constexpr auto size() noexcept {
        if constexpr (detail::has_names_meta<T>) {
            return detail::ReflectHelper<T>::getNames().size();
        } else if constexpr (detail::has_values_meta<T>) {
            if constexpr (detail::has_value_function_one<T>) {
                return detail::member_count_v<T>;
            } else {
                decltype(auto) values = detail::ReflectHelper<T>::getValues();
                if constexpr (detail::is_std_tuple_v<std::decay_t<decltype(values)>>) {
                    return std::tuple_size_v<std::decay_t<decltype(values)>>;
                } else {
                    return 1;
                }
            }
        } else {
            return 0;
        }
    }

    static constexpr decltype(auto) names() noexcept {
        if constexpr (detail::has_names_meta<T>) {
            return detail::ReflectHelper<T>::getNames();
        } else {
            return std::array<std::string_view, 0>{};
        }
    }

    static constexpr auto name(int index) noexcept {
        if constexpr (detail::has_names_meta<T>) {
            auto names = detail::ReflectHelper<T>::getNames();
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
        if constexpr (detail::has_values_meta<T>) {
            decltype(auto) values = detail::ReflectHelper<T>::getValues(std::forward<U>(obj));
            if constexpr (detail::is_std_tuple_v<std::decay_t<decltype(values)>>) {
                static_assert(std::tuple_size_v<std::decay_t<decltype(values)>> > N, "out of range");
                return detail::value_ref(std::get<N>(values), obj);
            } else if constexpr (N == 0) {
                return detail::value_ref(values, obj);
            } else {
                static_assert(N < 0, "out of range");
            }
        } else {
            static_assert(detail::has_values_meta<T>, "no values meta");
        }
    }

    template <typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>>
    static detail::RefAny value(U&& obj, int idx) {
        if constexpr (detail::has_values_meta<T>) {
            detail::RefAny ret;
            decltype(auto) values = detail::ReflectHelper<T>::getValues(std::forward<U>(obj));
            if constexpr (detail::is_std_tuple_v<std::decay_t<decltype(values)>>) {
                [&obj, &ret, &values, idx]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                    ((idx == Is ? (ret = detail::RefAny(detail::value_ref(std::get<Is>(values), obj)))
                                : detail::RefAny{}),
                     ...);
                }(std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(values)>>>{});
            } else {
                if (idx == 0) {
                    ret = detail::RefAny(detail::value_ref(values, obj));
                }
            }
            return ret;
        } else {
            static_assert(detail::has_values_meta<T>, "no values meta");
        }
    }

    template <typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>>
    static detail::RefAny value(U&& obj, std::string_view name) {
        if constexpr (detail::has_values_meta<T> && detail::has_names_meta<T>) {
            detail::RefAny ret;
            decltype(auto) values = detail::ReflectHelper<T>::getValues(std::forward<U>(obj));
            decltype(auto) names  = detail::ReflectHelper<T>::getNames();
            if constexpr (detail::is_std_tuple_v<std::decay_t<decltype(values)>>) {
                [&obj, &ret, &values, &names, name]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                    ((name == names[Is] ? (ret = detail::RefAny(detail::value_ref(std::get<Is>(values), obj)))
                                        : detail::RefAny{}),
                     ...);
                }(std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(values)>>>{});
            } else {
                if (names.size() == 1 && name == names[0]) {
                    ret = detail::RefAny(detail::value_ref(values, obj));
                }
            }
            return ret;
        } else {
            static_assert(detail::has_values_meta<T> && detail::has_names_meta<T>, "no values or names meta");
        }
    }

    using value_types                = std::remove_pointer_t<decltype(_types())>;
    static constexpr auto value_tags = _tags();                             // NOLINT
    static constexpr int value_count = std::tuple_size<value_types>::value; // NOLINT
};

template <typename T>
struct Reflect<T, std::enable_if_t<std::is_enum_v<T>>> {
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
    static const auto& nameMap() noexcept {
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
    static const auto& valueMap() noexcept {
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
    static constexpr auto value(std::string_view name) noexcept {
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
    static constexpr auto name(T value) noexcept {
        auto kNames = names();
        auto kEnums = values();
        for (int i = 0; i < (int)size(); ++i) {
            if (kEnums[i] == value) {
                return kNames[i];
            }
        }
        NEKO_LOG_ERROR("reflection", "value not found");
        return std::string_view{}; // FIXME: this is not a good way to handle this
    }
};

NEKO_END_NAMESPACE