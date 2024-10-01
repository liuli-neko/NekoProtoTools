/**
 * @file log.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "global.hpp"

#if !defined(NDEBUG) && !defined(NEKO_PROTO_NDEBUG)
#define NEKO_PROTO_DEBUG
#endif

#if !defined(NEKO_PROTO_NLOG)
#define NEKO_PROTO_LOG
#endif

#if defined(NEKO_PROTO_LOG)
#include <chrono>
#include <stdio.h>
#include <string>
struct neko_proto_log_context {
    const char* module;
    const char* file;
    const char* func;
    int line;
    std::chrono::system_clock::time_point time;
};
inline void neko_proto_private_log_out(const char* level, const char* message, const neko_proto_log_context& context) {
    time_t t     = std::chrono::system_clock::to_time_t(context.time);
    char buf[64] = {0};
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    uint64_t dis_millseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(context.time.time_since_epoch()).count() -
        std::chrono::duration_cast<std::chrono::seconds>(context.time.time_since_epoch()).count() * 1000;
#if defined(NEKO_PROTO_LOG_CONTEXT)
    std::string file_str = context.file;
    if (file_str.find_last_of('/') != std::string::npos) {
        file_str = file_str.substr(file_str.find_last_of('/') + 1);
    } else if (file_str.find_last_of('\\') != std::string::npos) {
        file_str = file_str.substr(file_str.find_last_of('\\') + 1);
    }
    fprintf(stderr, "[%s.%03lld] %s - [%s:%d][%s] [%s] %s\n", buf, dis_millseconds, level, file_str.c_str(),
            context.line, context.func, context.module, message);
#else
    fprintf(stderr, "[%s.%03lld] %s - [%s] %s\n", buf, dis_millseconds, level, context.module, message);
#endif
}

#if defined(NEKO_PROTO_USE_FMT)
#include <fmt/format.h>
#define NEKO_PRIVATE_LOG(level, module, fmtstr, ...)                                                                   \
    neko_proto_private_log_out(#level, fmt::format(fmtstr, ##__VA_ARGS__).c_str(),                                     \
                               {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now()})
#elif defined(NEKO_PROTO_USE_SPDLOG)
#include <spdlog/spdlog.h>
#if defined(NEKO_PROTO_LOG_CONTEXT)
#define NEKO_PRIVATE_LOG(level, module, fmt, ...)                                                                      \
    spdlog::level("[{}][{}:{}][{}] " fmt, #module, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#elif defined(NEKO_PROTO_USE_STD_FORMAT) && __cpp_lib_format >= 201907L
#include <format>
#define NEKO_PRIVATE_LOG(level, module, fmt, ...)                                                                      \
    neko_proto_private_log_out(#level, std::format(fmt, ##__VA_ARGS__).c_str(),                                        \
                               {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now()})
#else
#define NEKO_PRIVATE_LOG(level, module, fmt, ...) spdlog::level("[{}]" fmt, #module, ##__VA_ARGS__)
#endif
#else
#define NEKO_PRIVATE_LOG(level, module, fmt, ...)
#endif
#endif

#if defined(NEKO_PROTO_DEBUG)
#define NEKO_DEBUG(module, ...) NEKO_PRIVATE_LOG(debug, module, __VA_ARGS__)
#define NEKO_ASSERT(cond, module, ...)                                                                                 \
    if (!(cond)) {                                                                                                     \
        NEKO_PRIVATE_LOG(error, module, __VA_ARGS__);                                                                  \
        abort();                                                                                                       \
    }
#else
#define NEKO_ASSERT(cond, fmt, ...)
#define NEKO_DEBUG(...)
#endif

#if defined(NEKO_PROTO_LOG)
#define NEKO_LOG_INFO(module, ...)  NEKO_PRIVATE_LOG(info, module, __VA_ARGS__)
#define NEKO_LOG_WARN(module, ...)  NEKO_PRIVATE_LOG(warn, module, __VA_ARGS__)
#define NEKO_LOG_ERROR(module, ...) NEKO_PRIVATE_LOG(error, module, __VA_ARGS__)
#define NEKO_LOG_FATAL(module, ...) NEKO_PRIVATE_LOG(critical, module, __VA_ARGS__)
#else
#define NEKO_LOG_INFO(...)
#define NEKO_LOG_WARN(...)
#define NEKO_LOG_ERROR(...)
#define NEKO_LOG_FATAL(...)
#endif
