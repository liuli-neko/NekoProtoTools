/**
 * @file vector.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <vector>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::vector<T>& value) {
    auto ret = sa.startArray(value.size());
    for (const auto& v : value)
        ret = sa(v) && ret;
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::vector<T>& value) {
    std::size_t s = 0;
    auto ret      = sa(make_size_tag(s));
    if (!ret) {
        return false;
    }
    value.resize(s);
    for (size_t i = 0; i < s; ++i) {
        ret = sa(value[i]) && ret;
    }
    return ret;
}

NEKO_END_NAMESPACE