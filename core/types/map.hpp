#pragma once
#include <map>
#include <type_traits>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V,
          typename std::enable_if<!std::is_same<K, std::string>::value, char>::type = 0>
inline bool save(Serializer& sa, const std::map<K, V>& value) {
    bool ret = sa.startArray(value.size());
    for (const auto& v : value) {
        ret = sa.startObject() && ret;
        ret = sa(makeNameValuePair("key", 4, v.first)) && ret;
        ret = sa(makeNameValuePair("value", 6, v.second)) && ret;
        ret = sa.endObject() && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename V>
inline bool save(Serializer& sa, const std::map<std::string, V>& value) {
    bool ret = true;
    sa(makeSizeTag(value.size()));
    ret = sa.startObject() && ret;
    for (const auto& v : value) {
        ret = sa(makeNameValuePair(v.first.c_str(), v.first.size(), v.second)) && ret;
    }
    ret = sa.endObject() && ret;
    return ret;
}

template <typename Serializer, typename K, typename V,
          typename std::enable_if<!std::is_same<K, std::string>::value, char>::type = 0>
inline bool load(Serializer& sa, std::map<K, V>& value) {
    K k;
    V v;
    bool ret;
    std::size_t s;
    ret = sa(makeSizeTag(s));
    while (ret) {
        if (sa(makeNameValuePair("key", 4, k)) && sa(makeNameValuePair("value", 6, v))) {
            value.emplace(std::move(k), std::move(v));
        } else {
            break;
        }
    }
    return ret && (s == value.size());
}

template <typename Serializer, typename V>
inline bool load(Serializer& sa, std::map<std::string, V>& value) {
    bool ret = true;
    V v;
    std::size_t s;
    ret = sa(makeSizeTag(s));
    while (ret) {
        const auto& name = sa.name();
        if (name == "") {
            break;
        }
        if (sa(v)) {
            value.emplace(std::move(name), std::move(v));
        } else {
            ret = false;
            break;
        }
    }
    return ret;
}

NEKO_END_NAMESPACE