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
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#endif

#include <cstring>
#include <string>
#ifdef __GNUC__
#include <cxxabi.h>
#include <list>
#include <map>
#include <vector>
#endif

#include "../private/global.hpp"
#include "../private/helpers.hpp"
#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
/// ====================== enum string =========================
#ifndef NEKO_ENUM_SEARCH_DEPTH
#define NEKO_ENUM_SEARCH_DEPTH 60
#endif

#if defined(__GNUC__) || defined(__MINGW__) || defined(__clang__)
#define NEKO_FUNCTION              __PRETTY_FUNCTION__

namespace detail {
template <typename T, T Value>
constexpr auto _Neko_GetEnumName() noexcept {
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value = MyValues]
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value =
    // (MyEnum)114514]"
    std::string_view name(__PRETTY_FUNCTION__);
    std::size_t eqBegin   = name.find_last_of(' ');
    std::size_t end       = name.find_last_of(']');
    std::string_view body = name.substr(eqBegin + 1, end - eqBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
#elif defined(_MSC_VER)
#define NEKO_ENUM_TO_NAME(enumType)
#define NEKO_FUNCTION __FUNCTION__
namespace detail {
template <typename T, T Value>
constexpr auto _Neko_GetEnumName() noexcept {
    // auto __cdecl _Neko_GetEnumName<enum main::MyEnum,(enum
    // main::MyEnum)0x2>(void) auto __cdecl _Neko_GetEnumName<enum
    // main::MyEnum,main::MyEnum::Wtf>(void)
    std::string_view name(__FUNCSIG__);
    std::size_t dotBegin  = name.find_first_of(',');
    std::size_t end       = name.find_last_of('>');
    std::string_view body = name.substr(dotBegin + 1, end - dotBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
#else
namespace detail {
template <typename T, T Value>
constexpr auto _Neko_GetEnumName() noexcept {
    // Unsupported
    return std::string_view();
}
#endif
template <typename T, T Value>
constexpr bool _Neko_IsValidEnum() noexcept {
    return !_Neko_GetEnumName<T, Value>().empty();
}
template <typename T, std::size_t... N>
constexpr std::size_t _Neko_GetValidEnumCount(std::index_sequence<N...> /*unused*/) noexcept {
    return (... + _Neko_IsValidEnum<T, T(N)>());
}
template <typename T, std::size_t... N>
constexpr auto _Neko_GetValidEnumNames(std::index_sequence<N...> seq) noexcept {
    constexpr auto validCount = _Neko_GetValidEnumCount<T>(seq);

    std::array<std::pair<T, std::string_view>, validCount> arr;
    std::string_view vstr[sizeof...(N)]{_Neko_GetEnumName<T, T(N)>()...};

    std::size_t n    = 0;
    std::size_t left = validCount;
    auto iter        = arr.begin();

    for (auto i : vstr) {
        if (!i.empty()) {
            // Valid name
            iter->first  = T(n);
            iter->second = i;
            ++iter;
        }
        if (left == 0) {
            break;
        }

        n += 1;
    }

    return arr;
}
template <typename T>
std::string enum_to_string(const T& value) {
    constexpr static auto KEnumArr = _Neko_GetValidEnumNames<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
    std::string ret;
    for (int i = 0; i < KEnumArr.size(); ++i) {
        if (KEnumArr[i].first == value) {
            ret = std::string(KEnumArr[i].second);
        }
    }
    return ret;
}
template <typename T>
std::string make_enum_string(const std::string& fmt) {
    constexpr static auto KEnumArr = _Neko_GetValidEnumNames<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
    std::string ret;
    for (int i = 0; i < KEnumArr.size(); ++i) {
        if (KEnumArr[i].second.size() > 0) {
            std::string tfmt = fmt;
            tfmt.replace(tfmt.find("{enum}"), 6, KEnumArr[i].second);
            tfmt.replace(tfmt.find("{num}"), 5, std::to_string(i));
            ret += tfmt;
        }
    }
    return ret;
}
} // namespace detail
/// ====================== end enum string =====================

template <typename SerializerT, typename T,
          traits::enable_if_t<std::is_enum<T>::value> = traits::default_value_for_enable>
inline bool save(SerializerT& sa, const T& value) {
    std::string ret = detail::enum_to_string(value);
    ret += "(" + std::to_string(static_cast<int32_t>(value)) + ")";
    return sa(ret);
}
#else
template <typename SerializerT, typename T,
          traits::enable_if_t<std::is_enum<T>::value> = traits::default_value_for_enable>
inline bool save(SerializerT& sa, const T& value) {
    return sa(static_cast<int>(value));
}
#endif
template <typename SerializerT, typename T,
          traits::enable_if_t<std::is_enum<T>::value> = traits::default_value_for_enable>
inline bool load(SerializerT& sa, T& value) {
    std::string enum_str;
    int enum_int = 0;
    if (sa(enum_str)) {
        std::size_t left  = enum_str.find_last_of(')');
        std::size_t right = enum_str.find_last_of('(');
        int32_t v         = std::stoi(enum_str.substr(right + 1, left - right - 1));
        value             = static_cast<T>(v);
    } else if (sa(enum_int)) {
        value = static_cast<T>(enum_int);
    } else {
        return false;
    }
    return true;
}

NEKO_END_NAMESPACE
#ifdef __clang__
#pragma clang diagnostic pop
#endif