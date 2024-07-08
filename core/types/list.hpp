#pragma once
#include <list>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::list<T>& value) {
    auto ret = sa.startArray(value.size());
    for (const auto& v : value)
        ret = sa(v) && ret;
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::list<T>& value) {
    std::size_t s = 0;
    auto ret      = sa(makeSizeTag(s));
    if (!ret) {
        return false;
    }
    value.resize(s);
    for (auto& v : value)
        ret = sa(v) && ret;

    return ret;
}

NEKO_END_NAMESPACE