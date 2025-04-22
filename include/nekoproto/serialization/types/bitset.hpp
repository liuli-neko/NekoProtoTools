/**
 * @file bitset.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-17
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <bitset>
#include <type_traits>

#include "nekoproto/global/global.hpp"
#include "../private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, std::size_t N>
inline bool save(Serializer& sa, const std::bitset<N>& value) {
    return sa(value.to_string('0', '1'));
}

template <typename Serializer, std::size_t N>
inline bool load(Serializer& sa, std::bitset<N>& value) {
    std::string str;
    if (!sa(str)) {
        return false;
    }
    if (str.size() != N) {
        return false;
    }
    for (auto ch : str) {
        if (ch != '0' && ch != '1') {
            return false;
        }
    }
    value = std::bitset<N>(str);
    return true;
}

template <std::size_t N>
struct is_minimal_serializable<std::bitset<N>, void> : std::true_type {};

NEKO_END_NAMESPACE