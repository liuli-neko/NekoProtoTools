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
#include <type_traits>
#include <unordered_map>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::unordered_multimap<K, V>& value) {
    bool ret = sa.startArray(value.size());
    for (const auto& v : value) {
        ret = sa.startObject(1) && ret;
        ret = sa(makeNameValuePair("key", 3, v.first)) && ret;
        ret = sa(makeNameValuePair("value", 5, v.second)) && ret;
        ret = sa.endObject() && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename K, typename V>
inline bool load(Serializer& sa, std::unordered_multimap<K, V>& value) {
    K k;
    V v;
    bool ret;
    std::size_t s;
    ret = sa(makeSizeTag(s));
    value.clear();
    while (ret) {
        if (sa(makeNameValuePair("key", 3, k)) && sa(makeNameValuePair("value", 5, v))) {
            value.emplace(std::move(k), std::move(v));
        } else {
            break;
        }
    }
    return ret && (s == value.size());
}

NEKO_END_NAMESPACE