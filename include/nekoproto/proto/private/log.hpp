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
#include <cstring>

#if !defined(NDEBUG) && !defined(NEKO_PROTO_NDEBUG)
#define NEKO_PROTO_DEBUG
#endif

#if !defined(NEKO_PROTO_NLOG)
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

NEKO_BEGIN_NAMESPACE
struct LogContext {
    const char* module;
    const char* file;
    const char* func;
    int line;
    std::chrono::system_clock::time_point time;
    const char* color;
};
namespace logdetail {
inline std::function<void(const char* level, const char* message, const LogContext& context)>&
logger_func(std::function<void(const char* level, const char* message, const LogContext& context)> logger = nullptr) {
    static std::function<void(const char* level, const char* message, const LogContext& context)> kLoggerFunc = logger;
    if (logger) {
        kLoggerFunc = logger;
    }
    return kLoggerFunc;
}
} // namespace logdetail
inline void
install_logger(std::function<void(const char* level, const char* message, const LogContext& context)> logger) {
    logdetail::logger_func(logger);
}
namespace logdetail {
inline std::string to_lower(const std::string& str) {
    std::string result = str; // 创建字符串副本
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return std::tolower(ch); });
    return result;
}
#define ANSI_COLOR_TABLE                                                                                               \
    ANSI_COLOR(reset, "\033[0m")                                                                                       \
    ANSI_COLOR(black, "\033[30m")                                                                                      \
    ANSI_COLOR(red, "\033[31m")                                                                                        \
    ANSI_COLOR(green, "\033[32m")                                                                                      \
    ANSI_COLOR(yellow, "\033[33m")                                                                                     \
    ANSI_COLOR(blue, "\033[34m")                                                                                       \
    ANSI_COLOR(magenta, "\033[35m")                                                                                    \
    ANSI_COLOR(cyan, "\033[36m")                                                                                       \
    ANSI_COLOR(lightgray, "\033[37m")                                                                                  \
    ANSI_COLOR(darkgray, "\033[90m")                                                                                   \
    ANSI_COLOR(lightred, "\033[91m")                                                                                   \
    ANSI_COLOR(lightgreen, "\033[92m")                                                                                 \
    ANSI_COLOR(lightyellow, "\033[93m")                                                                                \
    ANSI_COLOR(lightblue, "\033[94m")                                                                                  \
    ANSI_COLOR(lightmagenta, "\033[95m")                                                                               \
    ANSI_COLOR(lightcyan, "\033[96m")                                                                                  \
    ANSI_COLOR(white, "\033[97m")

#ifdef NEKO_LOG_NO_COLOR
iline const char* ansi_color_code(const char*) { return ""; }
#else
inline const char* ansi_color_code(const char* name) {
#define ANSI_COLOR(color, code)                                                                                        \
    if (strcmp(to_lower(name).c_str(), #color) == 0) return code;
    ANSI_COLOR_TABLE
    return "";
}
#undef ANSI_COLOR
#endif
#undef ANSI_COLOR_TABLE

enum LogFliterMode {
    FilterInclude,
    FilterExclude,
};
inline bool log_level_filter(const std::string& level, int op) {
    static std::vector<std::string> kLevels = {"fatal", "error", "warn", "info"};
    static std::set<std::string> kFilter    = {"fatal", "error", "warn"};
    if (op == 0) {
        if (level == "debug") {
            kFilter.insert("debug");
        } else {
            kFilter.clear();
            for (const auto& mlevel : kLevels) {
                if (mlevel != level) {
                    kFilter.insert(mlevel);
                } else {
                    kFilter.insert(mlevel);
                    break;
                }
            }
        }
        return true;
    }
    if (op == 1) {
        return kFilter.find(level) != kFilter.end();
    }
    return kFilter.find(level) == kFilter.end();
}
inline std::tuple<int, std::set<std::string>>& log_filter(int opt, int flag = FilterInclude,
                                                          const std::string& module = "") {
    static std::tuple<int, std::set<std::string>> kNekoLogFilter = {FilterExclude, {}};
    if (opt == 0) {
        if (flag == std::get<0>(kNekoLogFilter)) {
            std::get<1>(kNekoLogFilter).insert(to_lower(module));
        } else {
            std::get<0>(kNekoLogFilter) = flag;
            std::get<1>(kNekoLogFilter).clear();
            std::get<1>(kNekoLogFilter).insert(to_lower(module));
        }
    } else if (opt == 2) {
        std::get<1>(kNekoLogFilter).clear();
        std::get<0>(kNekoLogFilter) = FilterExclude;
    }
    return kNekoLogFilter;
}
template <typename... Args>
inline void add_log_filter(const int flag, Args&&... args) {
    (log_filter(0, flag, std::forward<Args>(args)), ...);
}

inline void neko_proto_private_log_out(const char* level, const char* message, const LogContext& context) {
    if (logger_func() != nullptr) {
        logger_func()(level, message, context);
        return;
    }
    if (!log_level_filter(level, 1)) {
        return;
    }
    auto& [flag, filters] = log_filter(1);
    if (flag == FilterExclude && !filters.empty() && filters.find(to_lower(context.module)) != filters.end()) {
        return;
    }
    if (flag == FilterInclude && filters.empty()) {
        return;
    }
    if (flag == FilterInclude && filters.find(to_lower(context.module)) == filters.end()) {
        return;
    }
    time_t time  = std::chrono::system_clock::to_time_t(context.time);
    char buf[64] = {0};
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&time));
    uint64_t disMillseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(context.time.time_since_epoch()).count() -
        std::chrono::duration_cast<std::chrono::seconds>(context.time.time_since_epoch()).count() * 1000;
#if defined(NEKO_PROTO_LOG_CONTEXT)
    std::string fileStr = context.file;
    if (fileStr.find_last_of('/') != std::string::npos) {
        fileStr = fileStr.substr(fileStr.find_last_of('/') + 1);
    } else if (fileStr.find_last_of('\\') != std::string::npos) {
        fileStr = fileStr.substr(fileStr.find_last_of('\\') + 1);
    }
    std::string funcStr = context.func;
    if (funcStr.find_last_of(':') != std::string::npos) {
        funcStr = funcStr.substr(funcStr.find_last_of(':') + 1);
    }
    fprintf(stderr, "%s[%s.%03d] %s - [%s:%d][%s] [%s]%s %s\n", ansi_color_code(context.color), buf,
            static_cast<int>(disMillseconds), level, fileStr.c_str(), context.line, funcStr.c_str(), context.module,
            ansi_color_code("reset"), message);
#else
    fprintf(stderr, "[%s.%03d] %s - [%s] %s\n", buf, static_cast<int>(disMillseconds), level, context.module, message);
#endif
}
} // namespace logdetail
NEKO_END_NAMESPACE
#define NEKO_LOG_LEVEL_DEBUG "debug"
#define NEKO_LOG_LEVEL_INFO  "info"
#define NEKO_LOG_LEVEL_WARN  "warn"
#define NEKO_LOG_LEVEL_ERROR "error"
#define NEKO_LOG_LEVEL_FATAL "fatal"
#define NEKO_LOG_INCLUDE(module, ...)                                                                                  \
    NEKO_NAMESPACE::logdetail::add_log_filter(NEKO_NAMESPACE::logdetail::FilterInclude, module, ##__VA_ARGS__)
#define NEKO_LOG_EXCLUDE(module, ...)                                                                                  \
    NEKO_NAMESPACE::logdetail::add_log_filter(NEKO_NAMESPACE::logdetail::FilterExclude, module, ##__VA_ARGS__)
#define NEKO_LOG_SET_LEVEL(level) NEKO_NAMESPACE::logdetail::log_level_filter(level, 0)
#if defined(NEKO_PROTO_USE_FMT)
#include <fmt/format.h>
#define NEKO_PRIVATE_LOG(level, color, module, fmtstr, ...)                                                            \
    NEKO_NAMESPACE::logdetail::neko_proto_private_log_out(                                                             \
        #level, fmt::format(fmtstr, ##__VA_ARGS__).c_str(),                                                            \
        {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now(), #color})
#elif defined(NEKO_PROTO_USE_SPDLOG)
#include <spdlog/spdlog.h>
#define NEKO_PRIVATE_LOG(level, color, module, fmt, ...)                                                               \
    spdlog::level("[{}][{}:{}][{}] " fmt, #module, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#elif defined(NEKO_PROTO_USE_STD_FORMAT) && NEKO_CPP_PLUS >= 20
#include <format>
#define NEKO_PRIVATE_LOG(level, color, module, fmt, ...)                                                               \
    NEKO_NAMESPACE::logdetail::neko_proto_private_log_out(                                                             \
        #level, std::format(fmt, ##__VA_ARGS__).c_str(),                                                               \
        {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now(), #color})
#else
#define NEKO_PRIVATE_LOG(...)
#endif
#else
#define NEKO_PRIVATE_LOG(...)
#define NEKO_LOG_LEVEL_DEBUG
#define NEKO_LOG_LEVEL_INFO
#define NEKO_LOG_LEVEL_WARN
#define NEKO_LOG_LEVEL_ERROR
#define NEKO_LOG_LEVEL_FATAL
#define NEKO_LOG_INCLUDE(...)
#define NEKO_LOG_EXCLUDE(...)
#define NEKO_LOG_SET_LEVEL(...)
#endif

#if defined(NEKO_PROTO_DEBUG)
#define NEKO_LOG_DEBUG(module, ...) NEKO_PRIVATE_LOG(debug, blue, module, __VA_ARGS__)
#define NEKO_ASSERT(cond, module, ...)                                                                                 \
    if (!(cond)) {                                                                                                     \
        NEKO_PRIVATE_LOG(fatal, lightRed, module, __VA_ARGS__);                                                        \
        abort();                                                                                                       \
    }
#else
#define NEKO_ASSERT(cond, fmt, ...)
#define NEKO_LOG_DEBUG(...)
#endif

#if defined(NEKO_PROTO_LOG)
#define NEKO_LOG_INFO(module, ...)  NEKO_PRIVATE_LOG(info, darkGray, module, __VA_ARGS__)
#define NEKO_LOG_WARN(module, ...)  NEKO_PRIVATE_LOG(warn, yellow, module, __VA_ARGS__)
#define NEKO_LOG_ERROR(module, ...) NEKO_PRIVATE_LOG(error, lightRed, module, __VA_ARGS__)
#define NEKO_LOG_FATAL(module, ...)                                                                                    \
    NEKO_PRIVATE_LOG(fatal, red, module, __VA_ARGS__);                                                                 \
    abort()
#else
#define NEKO_LOG_INFO(...)
#define NEKO_LOG_WARN(...)
#define NEKO_LOG_ERROR(...)
#define NEKO_LOG_FATAL(...)
#endif
