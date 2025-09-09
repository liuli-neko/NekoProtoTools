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

#include "../private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::vector<T>& values) {
    auto ret = sa.startArray(values.size());
    for (const auto& value : values) {
        ret = sa(value) && ret;
    }
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::vector<T>& values) {
    std::size_t size = 0;
    auto ret         = sa.startNode();
    ret              = ret && sa(make_size_tag(size));
    if (!ret) {
        return false;
    }
    values.resize(size);
    for (std::size_t i = 0; i < size; ++i) {
        ret = sa(values[i]) && ret;
    }
    return sa.finishNode() && ret;
}

template <typename Serializer>
inline bool save(Serializer& sa, const std::vector<bool>& values) {
    auto ret = sa.startArray(values.size());
    for (bool value : values) {
        ret = sa(value) && ret;
    }
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer>
inline bool load(Serializer& sa, std::vector<bool>& values) {
    std::size_t size = 0;
    auto ret         = sa.startNode();
    ret              = ret && sa(make_size_tag(size));
    if (!ret) {
        sa.finishNode();
        return false;
    }
    values.resize(size);
    for (std::size_t i = 0; i < size; ++i) {
        bool value;
        ret       = sa(value) && ret;
        values[i] = value;
    }
    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE