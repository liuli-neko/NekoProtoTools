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
        ret = sa(NameValuePair("key", 4, v.first)) && ret;
        ret = sa(NameValuePair("value", 6, v.second)) && ret;
        ret = sa.endObject() && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename V>
inline bool save(Serializer& sa, const std::map<std::string, V>& value) {
    bool ret = true;
    ret      = sa.startObject() && ret;
    for (const auto& v : value) {
        ret = sa(NameValuePair(v.first.c_str(), v.first.size(), v.second)) && ret;
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
    SizeTag s;
    ret = sa(s);
    while (ret) {
        if (sa(NameValuePair("key", 4, k)) && sa(NameValuePair("value", 6, v))) {
            value.insert(std::make_pair(k, v));
        } else {
            break;
        }
    }
    return ret && (s.size == value.size());
}

template <typename Serializer, typename V>
inline bool load(Serializer& sa, std::map<std::string, V>& value) {
    bool ret;
    V v;
    while (ret) {
        const auto& name = sa.name();

        if (name == "") {
            break;
        }
        if (sa(NameValuePair(name.data(), name.size(), v))) {
            value.insert(std::make_pair(name, v));
        } else {
            break;
        }
    }
    return ret;
}

NEKO_END_NAMESPACE