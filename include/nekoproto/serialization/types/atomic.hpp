/**
 * @file atomic.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <atomic>
#include <type_traits>

#include "nekoproto/global/global.hpp"
#include "../private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::atomic<T>& value) {
    return sa(value.load());
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, std::atomic<T>& value) {
    T tmp;
    if (sa(tmp)) {
        value.store(tmp);
        return true;
    }
    return false;
}

template <typename T>
struct is_minimal_serializable<std::atomic<T>, void> : std::true_type {};

NEKO_END_NAMESPACE