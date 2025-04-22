/**
 * @file byte.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-10-16
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <cstddef>
#include <type_traits>

#include "nekoproto/global/global.hpp"
#include "../private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer>
inline bool save(Serializer& sa, const std::byte& value) {
    return sa(static_cast<uint8_t>(value));
}

template <typename Serializer>
inline bool load(Serializer& sa, std::byte& value) {
    uint8_t tmp;
    if (sa(tmp)) {
        value = static_cast<std::byte>(tmp);
        return true;
    }
    return false;
}

template <>
struct is_minimal_serializable<std::byte, void> : std::true_type {};

NEKO_END_NAMESPACE