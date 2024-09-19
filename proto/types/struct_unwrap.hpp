/**
 * @file struct_unwrap.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024 llhsdmd BusyStudent
 *
 */
#pragma once
#include <cstring>

#include "../json_serializer.hpp"
#include "../private/global.hpp"
#include "../private/helpers.hpp"
#include "../serializer_base.hpp"

#if NEKO_CPP_PLUS >= 17
NEKO_BEGIN_NAMESPACE

/// ====================== struct to array =====================
namespace detail {
template <size_t N>
struct size {
    static constexpr auto value = N;
};

// Unwrap here
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<1>) noexcept {
    auto& [a] = data;
    return std::forward_as_tuple(a);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<2>) noexcept {
    auto& [a, b] = data;
    return std::forward_as_tuple(a, b);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<3>) noexcept {
    auto& [a, b, c] = data;
    return std::forward_as_tuple(a, b, c);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<4>) noexcept {
    auto& [a, b, c, d] = data;
    return std::forward_as_tuple(a, b, c, d);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<5>) noexcept {
    auto& [a, b, c, d, e] = data;
    return std::forward_as_tuple(a, b, c, d, e);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<6>) noexcept {
    auto& [a, b, c, d, e, f] = data;
    return std::forward_as_tuple(a, b, c, d, e, f);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<7>) noexcept {
    auto& [a, b, c, d, e, f, g] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<8>) noexcept {
    auto& [a, b, c, d, e, f, g, h] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<9>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<10>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<11>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<12>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<13>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<14>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m, n);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<15>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<16>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<17>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<18>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<19>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s);
}

template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<20>) noexcept {
    auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t] = data;
    return std::forward_as_tuple(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t);
}

struct any_type {
    template <typename T>
    operator T() {}
};
template <typename T, typename _Cond = void, typename... Args>
struct can_aggregate_impl : std::false_type {};
template <typename T, typename... Args>
struct can_aggregate_impl<T, std::void_t<decltype(T{{std::declval<Args>()}...})>, Args...> : std::true_type {};
template <typename T, typename... Args>
struct can_aggregate : can_aggregate_impl<T, void, Args...> {};

/**
 * @brief Get the struct size at compile time
 *
 * @tparam T
 * @return the size of the
 */
template <typename T, typename... Args>
constexpr auto tuple_size(Args&&... args) noexcept {
    if constexpr (!can_aggregate<T, Args...>::value) {
        return sizeof...(args) - 1;
    } else {
        return tuple_size<T>(std::forward<Args>(args)..., any_type{});
    }
}

template <typename T>
constexpr size_t tuple_size_v = tuple_size<T>();

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
constexpr bool can_unwrap_v = std::is_aggregate_v<std::remove_cv_t<T>> && !is_std_array<T>::value;

/**
 * @brief Convert the struct reference to tuple
 *
 * @tparam T
 * @param data
 * @return constexpr auto
 */
template <typename T>
constexpr auto unwrap_struct(T& data) noexcept {
    static_assert(can_unwrap_v<T>, "The struct must be aggregate");
    return unwrap_struct_impl(data, size<tuple_size_v<T>>{});
}

template <typename SerializerT, typename... Args>
static bool unfold_unwrap(SerializerT& sa, std::tuple<Args...>&& tp) {
    return std::apply([&](Args&&... args) { return sa(args...); }, std::forward<std::tuple<Args...>>(tp));
}
} // namespace detail

template <typename SerializerT, typename T,
          traits::enable_if_t<detail::can_unwrap_v<T>, !traits::has_method_const_save<T, SerializerT>::value,
                              !traits::has_method_const_serialize<T, SerializerT>::value,
                              !traits::has_method_serialize<T, SerializerT>::value> = traits::default_value_for_enable>
inline bool save(SerializerT& sa, const T& value) {
    sa.startArray(std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value);
    auto ret = detail::unfold_unwrap(sa, detail::unwrap_struct(value));
    sa.endArray();
    return ret;
}

template <typename SerializerT, typename T,
          traits::enable_if_t<detail::can_unwrap_v<T>, !traits::has_method_load<T, SerializerT>::value,
                              !traits::has_method_deserialize<T, SerializerT>::value,
                              !traits::has_method_serialize<T, SerializerT>::value> = traits::default_value_for_enable>
inline bool load(SerializerT& sa, T& value) {
    uint32_t s;
    sa(makeSizeTag(s));
    if (s != std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value) {
        NEKO_LOG_ERROR("proto", "struct size mismatch: json object size {} != struct size {}", s,
                       std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value);
        return false;
    }
    return detail::unfold_unwrap(sa, detail::unwrap_struct(value));
}

NEKO_END_NAMESPACE
/// ====================== end =================================
#endif