/**
 * @file unordered_multimapmap.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <unordered_map>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::unordered_multimap<K, V>& values) {
    bool ret = sa.startArray(values.size());
    for (const auto& value : values) {
        ret = sa.startObject(1) && ret;
        ret = sa(make_name_value_pair("key", 3, value.first)) && ret;
        ret = sa(make_name_value_pair("value", 5, value.second)) && ret;
        ret = sa.endObject() && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename K, typename V>
inline bool load(Serializer& sa, std::unordered_multimap<K, V>& values) {
    K key;
    V value;
    bool ret;
    std::size_t size;
    ret = sa(make_size_tag(size));
    values.clear();
    while (ret && size--) {
        ret = sa.startNode() && ret;
        if (sa(make_name_value_pair("key", 3, key)) && sa(make_name_value_pair("value", 5, value))) {
            values.emplace(std::move(key), std::move(value));
        } else {
            break;
        }
        ret = sa.finishNode() && ret;
    }
    return ret;
}

NEKO_END_NAMESPACE