/**
 * @file proto_json_serializer_enum.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <cstring>
#ifdef __GNUC__
#include <cxxabi.h>
#include <list>
#include <map>
#include <vector>
#endif

#include "json_serializer.hpp"
#include "private/global.hpp"
#include "serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
/// ====================== enum string =========================
#ifndef NEKO_ENUM_SEARCH_DEPTH
#define NEKO_ENUM_SEARCH_DEPTH 60
#endif

#ifdef __GNUC__
#define NEKO_STRINGIFY_TYPE_RAW(x) NEKO_STRINGIFY_TYPEINFO(typeid(x))
#define NEKO_FUNCTION              __PRETTY_FUNCTION__

namespace {
template <typename T, T Value>
constexpr auto _Neko_GetEnumName() noexcept {
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value = MyValues]
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value =
    // (MyEnum)114514]"
    std::string_view name(__PRETTY_FUNCTION__);
    size_t eqBegin        = name.find_last_of(' ');
    size_t end            = name.find_last_of(']');
    std::string_view body = name.substr(eqBegin + 1, end - eqBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
inline std::string NEKO_STRINGIFY_TYPEINFO(const std::type_info& info) {
    int status;
    auto str = ::abi::__cxa_demangle(info.name(), nullptr, nullptr, &status);
    if (str) {
        std::string ret(str);
        ::free(str);
        return ret;
    }
    return info.name();
}
#elif defined(_MSC_VER)
#define NEKO_STRINGIFY_TYPE_RAW(type) NEKO_STRINGIFY_TYPEINFO(typeid(type))
#define NEKO_ENUM_TO_NAME(enumType)
#define NEKO_FUNCTION __FUNCTION__
namespace {
inline const char* NEKO_STRINGIFY_TYPEINFO(const std::type_info& info) {
    // Skip struct class prefix
    auto name = info.name();
    if (::strncmp(name, "class ", 6) == 0) {
        return name + 6;
    }
    if (::strncmp(name, "struct ", 7) == 0) {
        return name + 7;
    }
    if (::strncmp(name, "enum ", 5) == 0) {
        return name + 5;
    }
    return name;
}
template <typename T, T Value>
constexpr auto _Neko_GetEnumName() noexcept {
    // auto __cdecl _Neko_GetEnumName<enum main::MyEnum,(enum
    // main::MyEnum)0x2>(void) auto __cdecl _Neko_GetEnumName<enum
    // main::MyEnum,main::MyEnum::Wtf>(void)
    std::string_view name(__FUNCSIG__);
    size_t dotBegin       = name.find_first_of(',');
    size_t end            = name.find_last_of('>');
    std::string_view body = name.substr(dotBegin + 1, end - dotBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
#else
namespace {
#define NEKO_STRINGIFY_TYPE_RAW(type) typeid(type).name()
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
template <typename T, size_t... N>
constexpr size_t _Neko_GetValidEnumCount(std::index_sequence<N...> seq) noexcept {
    return (... + _Neko_IsValidEnum<T, T(N)>());
}
template <typename T, size_t... N>
constexpr auto _Neko_GetValidEnumNames(std::index_sequence<N...> seq) noexcept {
    constexpr auto validCount = _Neko_GetValidEnumCount<T>(seq);

    std::array<std::pair<T, std::string_view>, validCount> arr;
    std::string_view vstr[sizeof...(N)]{_Neko_GetEnumName<T, T(N)>()...};

    size_t n    = 0;
    size_t left = validCount;
    auto iter   = arr.begin();

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
} // namespace

template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, T, typename std::enable_if<std::is_enum<T>::value>::type> {
    constexpr static auto kEnumArr = _Neko_GetValidEnumNames<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
    static bool toJsonValue(WriterT& writer, const T& value) {
        std::string ret;
        for (int i = 0; i < kEnumArr.size(); ++i) {
            if (kEnumArr[i].first == value) {
                ret = std::string(kEnumArr[i].second);
            }
        }
        ret += "(" + std::to_string(static_cast<int32_t>(value)) + ")";
        return writer.String(ret.c_str(), ret.size(), true);
    }
    static bool fromJsonValue(T* dst, const ValueT& value) {
        NEKO_ASSERT(dst != nullptr, "dst is nullptr");
        if (value.IsString()) {
            std::string str(value.GetString(), value.GetStringLength());
            size_t left  = str.find_last_of(')');
            size_t right = str.find_last_of('(');
            int32_t v    = std::stoi(str.substr(right + 1, left - right - 1));
            *dst         = static_cast<T>(v);
            return true;
        }
        if (value.IsInt()) {
            *dst = static_cast<T>(value.GetInt());
            return true;
        }
        return false;
    }
};

template <typename T>
struct FormatStringCovert<T, typename std::enable_if<std::is_enum<T>::value>::type> {
    constexpr static auto kEnumArr = _Neko_GetValidEnumNames<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
    static std::string toString(const char* name, const size_t len, const T& value) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        bool isFind = false;
        for (int i = 0; i < kEnumArr.size(); ++i) {
            if (kEnumArr[i].first == value) {
                isFind = true;
                ret += std::string(kEnumArr[i].second);
            }
        }
        if (!isFind)
            ret += std::string(_class_name<T>()) + "(" + std::to_string(static_cast<int32_t>(value)) + ")";
        return ret;
    }
};
/// ====================== end enum string =====================
#else
template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, T, typename std::enable_if<std::is_enum<T>::value>::type> {
    static bool toJsonValue(WriterT& writer, const T& value) { return writer.Int(static_cast<int32_t>(value)); }
    static bool fromJsonValue(T* dst, const ValueT& value) {
        NEKO_ASSERT(dst != nullptr, "dst is nullptr");
        if (value.IsString()) {
            std::string str(value.GetString(), value.GetStringLength());
            size_t left = str.find_last_of(')');
            ;
            size_t right = str.find_last_of('(');
            int32_t v    = std::stoi(str.substr(right + 1, left - right - 1));
            *dst         = static_cast<T>(v);
            return true;
        }
        if (value.IsInt()) {
            *dst = static_cast<T>(value.GetInt());
            return true;
        }
        return false;
    }
};

template <typename T>
struct FormatStringCovert<T, typename std::enable_if<std::is_enum<T>::value>::type> {
    static std::string toString(const char* name, const size_t len, const T& value) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += std::string(_class_name<T>()) + "(" + std::to_string(static_cast<int32_t>(value)) + ")";
        return ret;
    }
};
#endif

NEKO_END_NAMESPACE