/**
 * @file global.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief NekoProtoTools
 *
 * @version 0.1
 * @date 2024-05-25
 *
 * @par license
 *  MIT License
 *
 * @copyright Copyright (c) 2024 by llhsdmd
 *
 */
#pragma once

#include <nekoproto/global/config.h>

#include <string>
#include <string_view>

namespace nekoproto {
template <typename>
inline constexpr bool always_false_v = false;
} // namespace nekoproto

#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
    #define NEKO_DETAIL_CPP_RAW_VER _MSVC_LANG
#else
    #define NEKO_DETAIL_CPP_RAW_VER __cplusplus
#endif
#define NEKO_DETAIL_CPP_RAW_VER_YEAR ((NEKO_DETAIL_CPP_RAW_VER / 100) % 100)
#if NEKO_DETAIL_CPP_RAW_VER >= 201103L
    #define NEKO_CPP_PLUS (NEKO_DETAIL_CPP_RAW_VER_YEAR - (NEKO_DETAIL_CPP_RAW_VER_YEAR % 3) + 2)
#else
    #define NEKO_CPP_PLUS 1
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
