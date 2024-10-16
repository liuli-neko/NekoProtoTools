/**
 * @file array.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <array>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T, std::size_t N>
inline bool save(Serializer& sa, const std::array<T, N>& value) {
    auto ret = sa.startArray(N);
    for (const auto& v : value)
        ret = sa(v) && ret;
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T, std::size_t N>
inline bool load(Serializer& sa, std::array<T, N>& value) {
    std::size_t s = 0;
    auto ret      = sa(make_size_tag(s));
    if (!ret || s != N) {
        NEKO_LOG_WARN("proto", "load array error or size mismatch: expected {}, got {}", N, s);
        return false;
    }
    for (auto& v : value)
        ret = sa(v) && ret;
    return ret;
}

NEKO_END_NAMESPACE