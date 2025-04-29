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
#include "../private/helpers.hpp"
#include "../reflection.hpp"
#include "../serializer_base.hpp"
#include "nekoproto/global/global.hpp"

#if NEKO_CPP_PLUS >= 17
NEKO_BEGIN_NAMESPACE
namespace detail {
template <typename SerializerT, typename... Args>
static bool unfold_unwrap(SerializerT& sa, std::tuple<Args...>&& tp) {
    return std::apply([&](Args&&... args) { return sa(args...); }, std::forward<std::tuple<Args...>>(tp));
}
} // namespace detail

template <typename SerializerT, typename T>
    requires detail::can_unwrap_v<T> && (!traits::has_method_const_serialize<T, SerializerT>) &&
             (!traits::has_method_serialize<T, SerializerT>)
inline bool save(SerializerT& sa, const T& value) {
    sa.startArray(std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value);
    auto ret = detail::unfold_unwrap(sa, detail::unwrap_struct(value));
    sa.endArray();
    return ret;
}

template <typename SerializerT, typename T>
    requires detail::can_unwrap_v<T> && (!traits::has_method_const_serialize<T, SerializerT>) &&
             (!traits::has_method_serialize<T, SerializerT>)
inline bool load(SerializerT& sa, T& value) {
    uint32_t size = 0;
    sa(make_size_tag(size));
    if (size != std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value) {
        NEKO_LOG_ERROR("proto", "struct size mismatch: json object size {} != struct size {}", size,
                       std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value);
        return false;
    }
    return detail::unfold_unwrap(sa, detail::unwrap_struct(value));
}

NEKO_END_NAMESPACE
/// ====================== end =================================
#endif