/**
 * @file shared_ptr.hpp
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

#include "../private/helpers.hpp"
#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::shared_ptr<T>& value) {
    if (value == nullptr) {
        return sa(nullptr);
    }
    return sa(*value);
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::shared_ptr<T>& value) {
    if (sa(nullptr)) {
        value = nullptr;
        return true;
    }
    value = std::make_shared<T>();
    return sa(*value);
}

template <typename T>
struct is_minimal_serializable<std::shared_ptr<T>, void> : is_minimal_serializable<T> {};

NEKO_END_NAMESPACE