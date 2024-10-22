/**
 * @file unordered_set.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <unordered_set>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::unordered_set<T>& values) {
    auto ret = sa.startArray(values.size());
    for (const auto& value : values) {
        ret = sa(value) && ret;
    }
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::unordered_set<T>& values) {
    std::size_t size = 0;
    auto ret         = sa(make_size_tag(size));
    if (!ret) {
        return false;
    }
    values.clear();
    T value;
    for (std::size_t i = 0; i < size; ++i) {
        ret = sa(value) && ret;
        values.insert(std::move(value));
    }
    return ret;
}

NEKO_END_NAMESPACE