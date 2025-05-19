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
inline bool save(Serializer& sa, const std::array<T, N>& values) {
    auto ret = sa.startArray(N);
    for (const auto& value : values) {
        ret = sa(value) && ret;
    }
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T, std::size_t N>
inline bool load(Serializer& sa, std::array<T, N>& values) {
    std::size_t size = 0;
    auto ret         = sa.startNode();
    ret              = ret && sa(make_size_tag(size));
    if (!ret || size != N) {
        NEKO_LOG_WARN("proto", "load array error or size mismatch: expected {}, got {}", N, size);
        sa.finishNode();
        return false;
    }
    for (auto& value : values) {
        ret = sa(value) && ret;
    }
    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE