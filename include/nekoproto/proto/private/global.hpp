/**
 * @file global.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief NekoProtoTools
 *
 * @version 0.1
 * @date 2024-05-25
 *
 * @par license
 *  GPL-2.0 license
 *
 * @copyright Copyright (c) 2024 by llhsdmd
 *
 */
#pragma once

#include "config.h"

#define NEKO_BEGIN_NAMESPACE namespace NekoProto {
#define NEKO_END_NAMESPACE   }
#define NEKO_USE_NAMESPACE   using namespace NekoProto;
#define NEKO_NAMESPACE       ::NekoProto

#if __cplusplus >= 202002L || _MSVC_LANG >= 202002L
#define NEKO_CPP_PLUS 20
#include <version>
#elif __cplusplus >= 201703L || _MSVC_LANG >= 201703L
#define NEKO_CPP_PLUS 17
#elif __cplusplus >= 201402L || _MSVC_LANG >= 201402L
#define NEKO_CPP_PLUS 14
#elif __cplusplus >= 201103L || _MSVC_LANG >= 201103L
#define NEKO_CPP_PLUS 11
#endif

#if defined(__GNUC__) || defined(__MINGW32__)
#define NEKO_USED [[gnu::used]]
#elif defined(_WIN32)
#define NEKO_USED
#endif
#if NEKO_CPP_PLUS >= 20
#define NEKO_IF_LIKELY   [[likely]]
#define NEKO_IF_UNLIKELY [[unlikely]]
#else
#define NEKO_IF_IS_LIKELY
#define NEKO_IF_UNLIKELY
#endif
#if NEKO_CPP_PLUS >= 17
#include <string>
#define NEKO_CONSTEXPR_FUNC         constexpr
#define NEKO_CONSTEXPR_IF           if constexpr
#define NEKO_CONSTEXPR_VAR          constexpr
#define NEKO_STRING_VIEW            std::string_view
#define NEKO_MAKE_UNIQUE(type, ...) std::make_unique<type>(__VA_ARGS__)
#define NEKO_NOEXCEPT               noexcept
#else
#include <string>
#define NEKO_CONSTEXPR_FUNC
#define NEKO_CONSTEXPR_IF           if
#define NEKO_CONSTEXPR_VAR          constexpr
#define NEKO_STRING_VIEW            std::string
#define NEKO_MAKE_UNIQUE(type, ...) std::unique_ptr<type>(new type(__VA_ARGS__))
#define NEKO_NOEXCEPT               noexcept
#endif

#ifdef _WIN32
#define NEKO_DECL_EXPORT __declspec(dllexport)
#define NEKO_DECL_IMPORT __declspec(dllimport)
#define NEKO_DECL_LOCAL
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define NEKO_DECL_EXPORT __attribute__((visibility("default")))
#define NEKO_DECL_IMPORT __attribute__((visibility("default")))
#define NEKO_DECL_LOCAL  __attribute__((visibility("hidden")))
#else
#define NEKO_DECL_EXPORT
#define NEKO_DECL_IMPORT
#define NEKO_DECL_LOCAL
#endif

#ifndef NEKO_PROTO_STATIC
#ifdef NEKO_PROTO_LIBRARY
#define NEKO_PROTO_API NEKO_DECL_EXPORT
#else
#define NEKO_PROTO_API NEKO_DECL_IMPORT
#endif
#else
#define NEKO_PROTO_API
#endif

#ifdef __GNUC__
#define PARENS ()

#define EXPAND(...)  EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)            __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN()                FOR_EACH_HELPER

#define ENUM_CASE(name)                                                                                                \
    case name:                                                                                                         \
        return #name;

#define MAKE_ENUM(type, ...)                                                                                           \
    enum class type : uint32_t { __VA_ARGS__ };                                                                        \
    constexpr const char* to_cstring(type _e) {                                                                        \
        using enum type;                                                                                               \
        switch (_e) {                                                                                                  \
            FOR_EACH(ENUM_CASE, __VA_ARGS__)                                                                           \
        default:                                                                                                       \
            return "unknown";                                                                                          \
        }                                                                                                              \
    }
#endif

NEKO_BEGIN_NAMESPACE
#if defined(__GNUC__) || defined(__MINGW__)
#define NEKO_PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
namespace {
template <class T>
NEKO_CONSTEXPR_FUNC NEKO_STRING_VIEW _class_name() NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
    NEKO_CONSTEXPR_VAR NEKO_STRING_VIEW PrettyFunction = __PRETTY_FUNCTION__;
    std::size_t start                                  = PrettyFunction.find_last_of('[');
    std::size_t end                                    = start;
    while (end < PrettyFunction.size() && (PrettyFunction[end] != ';')) {
        end++;
    }
    auto sstring       = PrettyFunction.substr(start, end - start);
    std::size_t nstart = sstring.find_first_of('<');
    if (nstart != NEKO_STRING_VIEW::npos) {
        sstring = sstring.substr(0, nstart);
    }
    nstart = sstring.find_last_of(':');
    if (nstart != NEKO_STRING_VIEW::npos) {
        start = nstart;
    } else {
        start = 0;
    }
    nstart = sstring.find_last_of(' ');
    if (nstart != NEKO_STRING_VIEW::npos) {
        start = start > nstart ? start : nstart;
    }
    while (start < sstring.size() && (sstring[start] == ' ' || sstring[start] == '>' || sstring[start] == ':')) {
        ++start;
    }
    return sstring.substr(start, end - start);
}
} // namespace
#elif defined(_WIN32)
#define NEKO_PRETTY_FUNCTION_NAME __FUNCSIG__
namespace {
template <class T>
NEKO_CONSTEXPR_FUNC NEKO_STRING_VIEW _class_name() NEKO_NOEXCEPT {
    NEKO_STRING_VIEW string(__FUNCSIG__);
    std::size_t start = string.find("_class_name<") + 15;
    std::size_t d     = string.find_first_of(' ', start);
    if (d != NEKO_STRING_VIEW::npos) start = d + 1;
    std::size_t end = string.find('<', start);
    auto sstring    = string.substr(start, end - start);
    d               = sstring.find_last_of(':');
    if (d != NEKO_STRING_VIEW::npos)
        start = d;
    else
        start = 0;
    while (start < sstring.size() && (sstring[start] == ' ' || sstring[start] == '>' || sstring[start] == ':')) {
        ++start;
    }
    end = end == NEKO_STRING_VIEW::npos ? sstring.find_last_of('>') : end;
    return sstring.substr(start, end - start);
}
} // namespace
#endif
NEKO_END_NAMESPACE