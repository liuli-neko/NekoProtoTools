/**
 * @file pair.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <type_traits>
#include <utility>

#include "../private/helpers.hpp"
#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::pair<K, V>& value) {
    bool ret = true;
    ret      = sa.startObject(2) && ret;
    ret      = sa(make_name_value_pair("first", 5, value.first)) && ret;
    ret      = sa(make_name_value_pair("second", 6, value.second)) && ret;
    ret      = sa.endObject() && ret;
    return ret;
}

template <typename Serializer, typename K, typename V>
inline bool load(Serializer& sa, std::pair<K, V>& value) {
    K key;
    V val;
    bool ret;
    std::size_t size;
    ret = sa(make_size_tag(size));
    if (ret) {
        if (sa(make_name_value_pair("first", 5, key)) && sa(make_name_value_pair("second", 6, val))) {
            value = std::make_pair(std::move(key), std::move(val));
        }
    }
    return ret && (size == 2);
}

NEKO_END_NAMESPACE