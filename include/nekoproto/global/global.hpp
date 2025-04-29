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

#ifndef NEKO_NAMESPACE
#define NEKO_NAMESPACE NekoProto
#endif

#define NEKO_BEGIN_NAMESPACE namespace NEKO_NAMESPACE {
#define NEKO_END_NAMESPACE   }
#define NEKO_USE_NAMESPACE   using namespace NEKO_NAMESPACE;

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
