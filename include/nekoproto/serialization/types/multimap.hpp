/**
 * @file multimapmap.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <map>

#include "../private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::multimap<K, V>& value) {
    bool ret = sa.startArray(value.size());
    for (const auto& val : value) {
        ret = sa.startObject(1) && ret;
        ret = sa(make_name_value_pair("key", 3, val.first)) && ret;
        ret = sa(make_name_value_pair("value", 5, val.second)) && ret;
        ret = sa.endObject() && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename K, typename V>
inline bool load(Serializer& sa, std::multimap<K, V>& value) {
    K key;
    V val;
    bool ret         = true;
    std::size_t size = 0;
    ret              = sa.startNode();
    ret              = ret && sa(make_size_tag(size));
    value.clear();
    while (ret && size--) {
        ret = sa.startNode() && ret;
        if (sa(make_name_value_pair("key", 3, key)) && sa(make_name_value_pair("value", 5, val))) {
            value.emplace(std::move(key), std::move(val));
        } else {
            break;
        }
        ret = sa.finishNode() && ret;
    }
    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE