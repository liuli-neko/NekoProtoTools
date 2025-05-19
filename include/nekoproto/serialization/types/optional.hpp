/**
 * @file optional.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-09-29
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <optional>
#include <type_traits>

#include "../private/helpers.hpp"
#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::optional<T>& value) {
    if (value.has_value()) {
        return sa(*value);
    }
    return sa(nullptr);
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::optional<T>& value) {
    if (sa(nullptr)) {
        value.reset();
        return true;
    }
    value = std::make_optional<T>();
    return sa(*value);
}

template <typename T>
struct is_minimal_serializable<std::optional<T>, void> : is_minimal_serializable<T> {};

NEKO_END_NAMESPACE