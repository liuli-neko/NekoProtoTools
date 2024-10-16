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
#include <type_traits>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::multimap<K, V>& value) {
    bool ret = sa.startArray(value.size());
    for (const auto& v : value) {
        ret = sa.startObject(1) && ret;
        ret = sa(make_name_value_pair("key", 3, v.first)) && ret;
        ret = sa(make_name_value_pair("value", 5, v.second)) && ret;
        ret = sa.endObject() && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename K, typename V>
inline bool load(Serializer& sa, std::multimap<K, V>& value) {
    K k;
    V v;
    bool ret;
    std::size_t s;
    ret = sa(make_size_tag(s));
    value.clear();
    while (ret && s--) {
        ret = sa.startNode() && ret;
        if (sa(make_name_value_pair("key", 3, k)) && sa(make_name_value_pair("value", 5, v))) {
            value.emplace(std::move(k), std::move(v));
        } else {
            break;
        }
        ret = sa.finishNode() && ret;
    }
    return ret;
}

NEKO_END_NAMESPACE