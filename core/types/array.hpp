#pragma once
#include <array>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T, std::size_t N>
inline bool save(Serializer& sa, const std::array<T, N>& value) {
    auto ret = sa.startArray(value.size());
    for (const auto& v : value)
        ret = sa(v) && ret;
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T, std::size_t N>
inline bool load(Serializer& sa, std::array<T, N>& value) {
    std::size_t s = 0;
    auto ret      = sa(makeSizeTag(s));
    if (!ret || s != N) {
        NEKO_LOG_WARN("load array error or size mismatch: expected {}, got {}", N, s);
        return false;
    }
    for (auto& v : value)
        ret = sa(v) && ret;
    return ret;
}

NEKO_END_NAMESPACE