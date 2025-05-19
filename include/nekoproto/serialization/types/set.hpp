/**
 * @file set.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <set>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::set<T>& value) {
    auto ret = sa.startArray(value.size());
    for (const auto& val : value) {
        ret = sa(val) && ret;
    }
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::set<T>& value) {
    std::size_t size = 0;
    auto ret         = sa.startNode();
    ret              = ret && sa(make_size_tag(size));
    if (!ret) {
        return false;
    }
    value.clear();
    T val;
    for (std::size_t i = 0; i < size; ++i) {
        ret = sa(val) && ret;
        value.insert(std::move(val));
    }
    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE