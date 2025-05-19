/**
 * @file unordered_map.hpp
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

template <typename Serializer, typename K, typename V,
          typename std::enable_if<!std::is_same<K, std::string>::value, char>::type = 0>
inline bool save(Serializer& sa, const std::unordered_map<K, V>& value) {
    bool ret = sa.startArray(value.size());
    for (const auto& val : value) {
        ret = sa.startObject(1) && ret;
        ret = sa(make_name_value_pair("key", 3, val.first)) && ret;
        ret = sa(make_name_value_pair("value", 5, val.second)) && ret;
        ret = sa.endObject() && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename V>
inline bool save(Serializer& sa, const std::unordered_map<std::string, V>& value) {
    bool ret = true;
    ret      = sa.startObject(value.size()) && ret;
    for (const auto& val : value) {
        ret = sa(make_name_value_pair(val.first.c_str(), val.first.size(), val.second)) && ret;
    }
    ret = sa.endObject() && ret;
    return ret;
}

template <typename Serializer, typename K, typename V,
          typename std::enable_if<!std::is_same<K, std::string>::value, char>::type = 0>
inline bool load(Serializer& sa, std::unordered_map<K, V>& value) {
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

template <typename Serializer, typename V>
inline bool load(Serializer& sa, std::unordered_map<std::string, V>& value) {
    bool ret = true;
    V val;
    std::size_t size = 0;
    ret              = sa.startNode();
    ret              = ret && sa(make_size_tag(size));
    value.clear();
    while (ret) {
        const auto& name = sa.name();
        if (name == "") {
            break;
        }
        if (sa(val)) {
            value.emplace(std::move(name), std::move(val));
        } else {
            ret = false;
            break;
        }
    }
    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE