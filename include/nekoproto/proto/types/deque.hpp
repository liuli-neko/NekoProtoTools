/**
 * @file deque.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <deque>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::deque<T>& values) {
    auto ret = sa.startArray(values.size());
    for (const auto& value : values) {
        ret = sa(value) && ret;
    }
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::deque<T>& value) {
    std::size_t size = 0;
    auto ret         = sa(make_size_tag(size));
    if (!ret) {
        return false;
    }
    value.resize(size);
    for (size_t i = 0; i < size; ++i) {
        ret = sa(value[i]) && ret;
    }
    return ret;
}

NEKO_END_NAMESPACE