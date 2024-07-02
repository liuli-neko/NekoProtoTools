/**
 * @file json_serializer_struct.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <cstring>

#include "json_serializer.hpp"
#include "private/global.hpp"
#include "serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
/// ====================== struct to array =====================
namespace {
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

template <typename T>
constexpr bool can_serialize_v = can_serialize<T>::value;

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
} // namespace

template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, T, std::enable_if_t<can_unwrap_v<T> && !can_serialize_v<T>>> {
    template <typename U>
    static void serializeTupleImpl(bool& ret, WriterT& writer, U& value) {
        using Type = std::remove_reference_t<std::remove_cv_t<U>>;
        ret        = JsonConvert<WriterT, ValueT, Type>::toJsonValue(writer, value) && ret;
    }
    template <typename... Args>
    static bool serializeTupleTo(WriterT& writer, const std::tuple<Args...>& tp) {
        bool ret = true;
        std::apply([&](Args&&... args) { ((serializeTupleImpl(ret, writer, args)), ...); }, tp);
        return ret;
    }
    static bool toJsonValue(WriterT& writer, const T& value) {
        auto ret = writer.StartArray();
        ret      = serializeTupleTo(writer, unwrap_struct(value)) && ret;
        ret      = writer.EndArray() && ret;
        return ret;
    }

    template <typename U>
    static void deserializeTupleImpl(const ValueT& value, U& tp) {
        using Type = std::remove_reference_t<std::remove_cv_t<U>>;
        JsonConvert<WriterT, ValueT, Type>::fromJsonValue(&tp, value);
    }
    template <typename... Args>
    static void deserializeTupleFrom(const ValueT& value, const std::tuple<Args...>& tp) {
        int index = 0;
        std::apply([&](Args&&... args) { ((deserializeTupleImpl(value[index++], args)), ...); }, tp);
    }
    static bool fromJsonValue(T* dst, const ValueT& value) {
        NEKO_ASSERT(dst != nullptr, "dst is nullptr");
        if (!value.IsArray()) {
            return false;
        }
        if (value.Size() != tuple_size_v<T>) {
            return false;
        }
        deserializeTupleFrom(value, unwrap_struct(*dst));
        return true;
    }
};
#if NEKO_SERIALIZABLE_TO_STRING_ENABLE == 1
template <typename T>
struct FormatStringCovert<T, std::enable_if_t<!can_serialize_v<T> && can_unwrap_v<T>>> {
    template <typename U>
    static void serializeTupleImpl(std::string& ret, U& value) {
        using Type = std::remove_reference_t<std::remove_cv_t<U>>;
        ret += FormatStringCovert<Type>::toString(nullptr, 0, value) + ", ";
    }
    template <typename... Args>
    static std::string serializeTupleTo(const std::tuple<Args...>& tp) {
        std::string ret;
        std::apply([&](Args&&... args) { ((serializeTupleImpl(ret, args)), ...); }, tp);
        return ret;
    }
    static std::string toString(const char* name, const size_t len, const T& value) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ") + std::string(_class_name<T>());
        ret += std::string("{") + serializeTupleTo(unwrap_struct(value));
        ret.pop_back();
        ret.back() = '}';
        return ret;
    }
};
#endif
/// ====================== end =================================
#else
#endif

NEKO_END_NAMESPACE