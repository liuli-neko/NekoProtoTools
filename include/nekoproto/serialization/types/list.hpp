/**
 * @file list.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <list>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::list<T>& values) {
    auto ret = sa.startArray(values.size());
    for (const auto& value : values) {
        ret = sa(value) && ret;
    }
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::list<T>& values) {
    std::size_t size = 0;
    auto ret         = sa.startNode();
    ret              = ret && sa(make_size_tag(size));
    if (!ret) {
        sa.finishNode();
        return false;
    }
    values.clear();
    values.resize(size);
    for (auto& value : values) {
        ret = sa(value) && ret;
    }

    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE