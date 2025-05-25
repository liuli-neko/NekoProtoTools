/**
 * @file enum.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024 llhsdmd BusyStudent
 *
 */
#pragma once

#include <string>

#include "../private/helpers.hpp"
#include "../serializer_base.hpp"
#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename SerializerT, typename T>
    requires std::is_enum_v<T>
inline bool save(SerializerT& sa, const T& value) {
    auto ret = Reflect<T>::name(value);
    if (ret.empty()) {
        return sa(static_cast<int>(value));
    }
    return sa(ret);
}
template <typename SerializerT, typename T>
    requires std::is_enum_v<T>
inline bool load(SerializerT& sa, T& value) {
    std::string enumStr;
    if (sa(enumStr)) {
        if (enumStr.empty()) {
            return false;
        }
        if (auto it = Reflect<T>::nameMap().find(enumStr); it != Reflect<T>::nameMap().end()) {
            value = it->second;
            return true;
        }
        return false;
    }
    int enumInt = 0;
    if (sa(enumInt)) {
        value = static_cast<T>(enumInt);
    } else {
        return false;
    }
    return true;
}

NEKO_END_NAMESPACE