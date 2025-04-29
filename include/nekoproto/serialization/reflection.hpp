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

#include <array>
#include <tuple>
#include <type_traits>
#include <utility> // For std::index_sequence

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

template <typename... ValueTs>
using double_type_tuple = decltype(init_values_tuple<2, 1>(std::make_index_sequence<sizeof...(ValueTs) / 2>{},
                                                           std::declval<std::tuple<traits::ref_type<ValueTs>...>>()));

template <typename T, class Obj>
concept is_member_ref_function = requires(T val, Obj& obj) { std::is_lvalue_reference<decltype(val(obj))>::value; };

template <typename T>
struct is_std_tuple : std::false_type {};

template <typename... Ts>
struct is_std_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_tuple_v = is_std_tuple<std::remove_cvref_t<T>>::value;
} // namespace detail

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
};

template <typename T>
concept is_local_ref_values = requires {
    { T::Neko::values };
    requires is_std_tuple_v<std::decay_t<decltype(T::Neko::values)>>;
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
concept is_local_ref_value = requires {
    { T::Neko::value };
    requires std::is_reference_v<decltype(T::Neko::value)>;
} || requires {
    { T::Neko::value };
    requires is_member_ref_function<decltype(T::Neko::value), T>;
} || requires {
    { T::Neko::value };
    requires std::is_member_object_pointer_v<decltype(T::Neko::value)>;
};

template <typename T>
concept is_meta_ref_value = requires {
    { Meta<T>::value };
    requires std::is_reference_v<decltype(Meta<T>::value)>;
} || requires {
    { Meta<T>::value };
    requires is_member_ref_function<decltype(Meta<T>::value), T>;
} || requires {
    { Meta<T>::value };
    requires std::is_member_object_pointer_v<decltype(Meta<T>::value)>;
};

template <typename T>
    requires std::is_member_object_pointer_v<std::remove_cvref_t<T>>
constexpr decltype(auto) value_ref(T&& dt, auto& obj) noexcept {
    return obj.*dt;
}

template <typename T, typename ObjT>
    requires std::is_lvalue_reference_v<T> && (!std::is_member_object_pointer_v<std::remove_cvref_t<T>>) &&
             (!is_member_ref_function<T, ObjT>)
constexpr decltype(auto) value_ref(T&& dt, ObjT& /*unused*/) noexcept {
    return dt;
}

template <typename T, typename ObjT>
    requires is_member_ref_function<T, ObjT>
constexpr decltype(auto) value_ref(T&& dt, ObjT& obj) noexcept {
    return dt(obj);
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
    static constexpr decltype(auto) value(T& obj) { return unwrap_struct(obj); }
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
} // namespace detail

template <typename T>
struct Reflect {
private:
    static auto _getNames() {
        if constexpr (detail::has_name_function<T>) {
            return detail::MetaPrivate<T>::names();
        } else if constexpr (detail::has_name_member<T>) {
            return detail::MetaPrivate<T>::names;
        } else {
            return std::array<std::string_view, 0>{};
        }
    }

    static decltype(auto) _getValues(T& obj) { return detail::MetaPrivate<T>::value(obj); }
    static decltype(auto) _getValues() {
        if constexpr (detail::has_value_function<T>) {
            return detail::MetaPrivate<T>::value();
        } else if constexpr (detail::has_value_member<T>) {
            return detail::MetaPrivate<T>::value;
        } else {
            return std::forward_as_tuple();
        }
    }

public:
    template <typename CallableT>
        requires std::is_class_v<CallableT> && requires(CallableT call) {
            { call(std::declval<int&>()) };
        }
    static void forEach(T& obj, CallableT&& func) noexcept {
        if constexpr (detail::has_values_meta<T>) {
            if constexpr (detail::has_value_function_one<T>) {
                decltype(auto) values = _getValues(obj);
                if constexpr (detail::is_std_tuple_v<std::decay_t<decltype(values)>>) {
                    [&values, &func, &obj]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                        ((func(detail::value_ref(std::get<Is>(values), obj))), ...);
                    }(std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(values)>>>{});
                } else {
                    func(detail::value_ref(values, obj));
                }
            } else {
                decltype(auto) values = _getValues();
                if constexpr (detail::is_std_tuple_v<std::decay_t<decltype(values)>>) {
                    [&values, &func, &obj]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                        ((func(detail::value_ref(std::get<Is>(values), obj))), ...);
                    }(std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(values)>>>{});
                } else {
                    func(detail::value_ref(values, obj));
                }
            }
        } else {
            NEKO_LOG_ERROR("reflection", "apply unknown");
        }
    }

    template <typename CallableT>
        requires std::is_class_v<CallableT> && requires(CallableT call) {
            { call(std::declval<int&>(), std::declval<std::string_view>()) };
        }
    static void forEach(T& obj, CallableT&& func) noexcept {
        if constexpr (detail::has_values_meta<T> && detail::has_names_meta<T>) {
            if constexpr (detail::has_value_function_one<T>) {
                auto names            = _getNames();
                decltype(auto) values = _getValues(obj);
                if constexpr (detail::is_std_tuple_v<std::decay_t<decltype(values)>>) {
                    static_assert(std::tuple_size_v<std::decay_t<decltype(values)>> ==
                                      std::tuple_size_v<std::decay_t<decltype(names)>>,
                                  "values and names size mismatch");
                    [&values, &names, &func, &obj]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                        ((func(detail::value_ref(std::get<Is>(values), obj), names[Is])), ...);
                    }(std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(values)>>>{});
                } else {
                    static_assert(1 == std::tuple_size_v<std::decay_t<decltype(names)>>,
                                  "values and names size mismatch");
                    func(detail::value_ref(values, obj), names[0]);
                }
            } else {
                auto names            = _getNames();
                decltype(auto) values = _getValues();
                using ValueType       = std::decay_t<decltype(values)>;
                using NameType        = std::decay_t<decltype(names)>;
                if constexpr (detail::is_std_tuple_v<ValueType>) {
                    static_assert(std::tuple_size_v<ValueType> == std::tuple_size_v<NameType>,
                                  "values and names size mismatch");
                    [&values, &names, &func, &obj]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                        ((func(detail::value_ref(std::get<Is>(values), obj), names[Is])), ...);
                    }(std::make_index_sequence<std::tuple_size_v<ValueType>>{});
                } else {
                    static_assert(1 == std::tuple_size_v<NameType>, "values and names size mismatch");
                    func(detail::value_ref(values, obj), names[0]);
                }
            }
        } else {
            NEKO_LOG_ERROR("reflection", "apply unknown");
        }
    }

    static decltype(auto) names() noexcept {
        if constexpr (detail::has_names_meta<T>) {
            return _getNames();
        } else {
            return std::array<std::string_view, 0>{};
        }
    }
};

NEKO_END_NAMESPACE