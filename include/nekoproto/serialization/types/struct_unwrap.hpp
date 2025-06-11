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
#include "nekoproto/serialization/reflection.hpp"

NEKO_BEGIN_NAMESPACE

template <typename SerializerT, typename T>
    requires detail::has_values_meta<std::decay_t<T>>
inline bool save(SerializerT& sa, const T& value) {
    using ValueType = std::decay_t<T>;
    bool ret        = true;
    if constexpr (detail::has_values_meta<ValueType> && detail::has_names_meta<ValueType>) {
        sa.startObject(detail::member_count_v<ValueType>);
        Reflect<ValueType>::forEachWithName(value, [&sa, &ret](auto&& value, std::string_view name) {
            ret = sa(make_name_value_pair(name, value)) && ret;
        });
        sa.endObject();
    } else if constexpr (detail::has_values_meta<ValueType>) {
        sa.startArray(detail::member_count_v<ValueType>);
        Reflect<ValueType>::forEachWithoutName(value, [&sa, &ret](auto&& value) { ret = sa(value) && ret; });
        sa.endArray();
    }
    return ret;
}

template <typename SerializerT, typename T>
    requires detail::has_values_meta<std::decay_t<T>>
inline bool load(SerializerT& sa, T& value) {
    using ValueType = std::decay_t<T>;
    uint32_t size   = 0;
    bool ret        = true;
    ret             = sa.startNode();
    ret             = ret && sa(make_size_tag(size));
    if (!ret) {
        sa.finishNode();
        return ret;
    }
    if constexpr (detail::has_values_meta<ValueType> && detail::has_names_meta<ValueType>) {
        Reflect<ValueType>::forEachWithName(value, [&sa, &ret](auto&& value, std::string_view name) {
            ret = sa(make_name_value_pair(name, value)) && ret;
        });
    } else if constexpr (detail::has_values_meta<ValueType>) {
        if (size != detail::member_count_v<ValueType>) {
            NEKO_LOG_ERROR(
                "proto",
                "array struct size mismatch: json object size {} != struct size {}, array can not has optional member.",
                size, detail::member_count_v<ValueType>);
            sa.finishNode();
            return false;
        }
        Reflect<ValueType>::forEachWithoutName(value, [&sa, &ret](auto&& value) { ret = sa(value) && ret; });
    }
    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE
/// ====================== end =================================
