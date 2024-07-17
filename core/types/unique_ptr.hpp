/**
 * @file unique_ptr.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once
#include <memory>
#include <type_traits>

#include "../private/global.hpp"
#include "../private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::unique_ptr<T>& value) {
    if (value == nullptr) {
        return sa(nullptr);
    }
    return sa(*value);
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::unique_ptr<T>& value) {
    if (sa(nullptr)) {
        value = nullptr;
        return true;
    }
    value = NEKO_MAKE_UNIQUE(T);
    return sa(*value);
}

NEKO_END_NAMESPACE