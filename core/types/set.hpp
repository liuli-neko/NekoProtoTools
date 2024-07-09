#pragma once
#include <set>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::set<T>& value) {
    auto ret = sa.startArray(value.size());
    for (const auto& v : value)
        ret = sa(v) && ret;
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::set<T>& value) {
    std::size_t s = 0;
    auto ret      = sa(makeSizeTag(s));
    if (!ret) {
        return false;
    }
    value.clear();
    T v;
    for (std::size_t i = 0; i < s; ++i) {
        ret = sa(v) && ret;
        value.insert(std::move(v));
    }
    return ret;
}

NEKO_END_NAMESPACE