#pragma once

// Macro bundle shared by header consumers and module interfaces; no API declarations.
#ifndef NEKO_EXPORT
#define NEKO_EXPORT
#endif
#ifndef NEKO_BEGIN_EXPORT
#define NEKO_BEGIN_EXPORT
#endif
#ifndef NEKO_END_EXPORT
#define NEKO_END_EXPORT
#endif

// NEKO_BUILDING_MODULE identifies the interface being compiled, not enabled features.
#define NEKO_MODULE_REFLECTION 1
#define NEKO_MODULE_SERIALIZER 2
#define NEKO_MODULE_ARGPARSER 3
#define NEKO_MODULE_PROTOCOL 4
#define NEKO_MODULE_TRANSPORT 5
#define NEKO_MODULE_RPC 6
#define NEKO_MODULE_JSONRPC 7
#define NEKO_MODULE_COMMUNICATION 8

// Configuration and visibility macros shared by headers and module interfaces.
#include <nekoproto/global/config.h>

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

#if !defined(NDEBUG) && !defined(NEKO_PROTO_NDEBUG)
#define NEKO_PROTO_DEBUG
#endif

#if !defined(NEKO_PROTO_NLOG) && !defined(NDEBUG)
#define NEKO_PROTO_LOG
#endif

#if defined(NEKO_PROTO_LOG)
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <functional>
#include <set>
#include <stdio.h>
#include <string>
#include <tuple>

#define NEKO_LOG_LEVEL_TRACE "trace"
#define NEKO_LOG_LEVEL_DEBUG "debug"
#define NEKO_LOG_LEVEL_INFO  "info"
#define NEKO_LOG_LEVEL_WARN  "warn"
#define NEKO_LOG_LEVEL_ERROR "error"
#define NEKO_LOG_LEVEL_FATAL "fatal"

#define NEKO_LOG_INCLUDE(module, ...)                                                                                  \
    nekoproto::logdetail::addLogFilter(nekoproto::logdetail::FilterInclude, module, ##__VA_ARGS__)
#define NEKO_LOG_EXCLUDE(module, ...)                                                                                  \
    nekoproto::logdetail::addLogFilter(nekoproto::logdetail::FilterExclude, module, ##__VA_ARGS__)
#define NEKO_LOG_SET_LEVEL(level) nekoproto::logdetail::logLevelFilter(level, 0)
#if defined(NEKO_PROTO_USE_FMT)
#include <fmt/format.h>
#define NEKO_DETAIL_LOG(level, color, module, fmtstr, ...)                                                             \
    nekoproto::logdetail::nekoProtoPrivateLogOut(                                                                      \
        #level, fmt::format(fmtstr, ##__VA_ARGS__).c_str(),                                                            \
        {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now(), #color})
#elif defined(NEKO_PROTO_USE_SPDLOG)
#include <spdlog/spdlog.h>
#define NEKO_DETAIL_LOG(level, color, module, fmt, ...)                                                                \
    spdlog::level("[{}][{}:{}][{}] " fmt, #module, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#elif defined(NEKO_PROTO_USE_STD_FORMAT) && NEKO_CPP_PLUS >= 20
#include <format>
#define NEKO_DETAIL_LOG(level, color, module, fmt, ...)                                                                \
    nekoproto::logdetail::nekoProtoPrivateLogOut(                                                                      \
        #level, std::format(fmt, ##__VA_ARGS__).c_str(),                                                               \
        {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now(), #color})
#else
#define NEKO_DETAIL_LOG(...)
#endif
#else
#define NEKO_DETAIL_LOG(...)
#define NEKO_LOG_LEVEL_DEBUG
#define NEKO_LOG_LEVEL_TRACE
#define NEKO_LOG_LEVEL_INFO
#define NEKO_LOG_LEVEL_WARN
#define NEKO_LOG_LEVEL_ERROR
#define NEKO_LOG_LEVEL_FATAL
#define NEKO_LOG_INCLUDE(...)
#define NEKO_LOG_EXCLUDE(...)
#define NEKO_LOG_SET_LEVEL(...)
#endif

#if defined(NEKO_PROTO_DEBUG)
#define NEKO_LOG_DEBUG(module, ...) NEKO_DETAIL_LOG(debug, blue, module, __VA_ARGS__)
#define NEKO_LOG_TRACE(module, ...) NEKO_DETAIL_LOG(trace, darkGray, module, __VA_ARGS__)
#define NEKO_ASSERT(cond, module, ...)                                                                                 \
    if (!(cond)) {                                                                                                     \
        NEKO_DETAIL_LOG(fatal, lightRed, module, __VA_ARGS__);                                                         \
        abort();                                                                                                       \
    }
#else
#define NEKO_ASSERT(cond, fmt, ...)
#define NEKO_LOG_DEBUG(...)
#define NEKO_LOG_TRACE(...)
#endif

#if defined(NEKO_PROTO_LOG)
#define NEKO_LOG_INFO(module, ...)  NEKO_DETAIL_LOG(info, lightgray, module, __VA_ARGS__)
#define NEKO_LOG_WARN(module, ...)  NEKO_DETAIL_LOG(warn, lightyellow, module, __VA_ARGS__)
#define NEKO_LOG_ERROR(module, ...) NEKO_DETAIL_LOG(error, lightRed, module, __VA_ARGS__)
#define NEKO_LOG_FATAL(module, ...)                                                                                    \
    NEKO_DETAIL_LOG(fatal, red, module, __VA_ARGS__);                                                                  \
    abort()
#else
#define NEKO_LOG_INFO(...)
#define NEKO_LOG_WARN(...)
#define NEKO_LOG_ERROR(...)
#define NEKO_LOG_FATAL(...)
#endif
