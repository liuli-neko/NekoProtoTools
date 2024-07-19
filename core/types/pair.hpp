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

#include "../private/global.hpp"
#include "../private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::pair<K, V>& value) {
    bool ret = true;
    ret      = sa.startObject(2) && ret;
    ret      = sa(makeNameValuePair("first", 5, value.first)) && ret;
    ret      = sa(makeNameValuePair("second", 6, value.second)) && ret;
    ret      = sa.endObject() && ret;
    return ret;
}

template <typename Serializer, typename K, typename V>
inline bool load(Serializer& sa, std::pair<K, V>& value) {
    K k;
    V v;
    bool ret;
    std::size_t s;
    ret = sa(makeSizeTag(s));
    if (ret) {
        if (sa(makeNameValuePair("first", 5, k)) && sa(makeNameValuePair("second", 6, v))) {
            value = std::make_pair(std::move(k), std::move(v));
        }
    }
    return ret && (s == 2);
}

NEKO_END_NAMESPACE