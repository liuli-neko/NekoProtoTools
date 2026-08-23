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

namespace nekoproto {
struct LogContext {
    const char* module;
    const char* file;
    const char* func;
    int line;
    std::chrono::system_clock::time_point time;
    const char* color;
};
namespace logdetail {
inline auto
loggerFunc(std::function<void(const char* level, const char* message, const LogContext& context)> logger = nullptr)
    -> std::function<void(const char* level, const char* message, const LogContext& context)>& {
    static std::function<void(const char* level, const char* message, const LogContext& context)> kLoggerFunc = logger;
    if (logger) {
        kLoggerFunc = logger;
    }
    return kLoggerFunc;
}
} // namespace logdetail
inline void
installLogger(std::function<void(const char* level, const char* message, const LogContext& context)> logger) {
    logdetail::loggerFunc(logger);
}
namespace logdetail {
inline auto toLower(const std::string& str) -> std::string {
    std::string result = str; // 创建字符串副本
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return std::tolower(ch); });
    return result;
}
#ifdef NEKO_LOG_NO_COLOR
inline auto ansiColorCode(const char*) -> const char* { return ""; }
#else
inline auto ansiColorCode(const char* name) -> const char* {
    const auto color = toLower(name);
    if (color == "reset") return "\033[0m";
    if (color == "black") return "\033[30m";
    if (color == "red") return "\033[31m";
    if (color == "green") return "\033[32m";
    if (color == "yellow") return "\033[33m";
    if (color == "blue") return "\033[34m";
    if (color == "magenta") return "\033[35m";
    if (color == "cyan") return "\033[36m";
    if (color == "lightgray") return "\033[37m";
    if (color == "darkgray") return "\033[90m";
    if (color == "lightred") return "\033[91m";
    if (color == "lightgreen") return "\033[92m";
    if (color == "lightyellow") return "\033[93m";
    if (color == "lightblue") return "\033[94m";
    if (color == "lightmagenta") return "\033[95m";
    if (color == "lightcyan") return "\033[96m";
    if (color == "white") return "\033[97m";
    return "";
}
#endif

enum LogFilterMode {
    FilterInclude,
    FilterExclude,
};
inline auto logLevelFilter(const std::string& level, int op) -> bool {
    static std::vector<std::string> kLevels = {NEKO_LOG_LEVEL_FATAL, NEKO_LOG_LEVEL_ERROR, NEKO_LOG_LEVEL_WARN,
                                               NEKO_LOG_LEVEL_INFO,  NEKO_LOG_LEVEL_DEBUG, NEKO_LOG_LEVEL_TRACE};
    static std::set<std::string> kFilter    = {NEKO_LOG_LEVEL_FATAL, NEKO_LOG_LEVEL_ERROR, NEKO_LOG_LEVEL_WARN};
    if (op == 0) {
        kFilter.clear();
        for (const auto& mlevel : kLevels) {
            if (mlevel != level) {
                kFilter.insert(mlevel);
            } else {
                kFilter.insert(mlevel);
                break;
            }
        }
        return true;
    }
    if (op == 1) {
        return kFilter.find(level) != kFilter.end();
    }
    return kFilter.find(level) == kFilter.end();
}
inline auto logFilter(int opt, int flag = FilterInclude, const std::string& module = "")
    -> std::tuple<int, std::set<std::string>>& {
    static std::tuple<int, std::set<std::string>> kNekoLogFilter = {FilterExclude, {}};
    if (opt == 0) {
        if (flag == std::get<0>(kNekoLogFilter)) {
            std::get<1>(kNekoLogFilter).insert(toLower(module));
        } else {
            std::get<0>(kNekoLogFilter) = flag;
            std::get<1>(kNekoLogFilter).clear();
            std::get<1>(kNekoLogFilter).insert(toLower(module));
        }
    } else if (opt == 2) {
        std::get<1>(kNekoLogFilter).clear();
        std::get<0>(kNekoLogFilter) = FilterExclude;
    }
    return kNekoLogFilter;
}
template <typename... Args>
inline void addLogFilter(const int flag, Args&&... args) {
    (logFilter(0, flag, std::forward<Args>(args)), ...);
}

inline void nekoProtoPrivateLogOut(const char* level, const char* message, const LogContext& context) {
    if (loggerFunc() != nullptr) {
        loggerFunc()(level, message, context);
        return;
    }
    if (!logLevelFilter(level, 1)) {
        return;
    }
    auto& [flag, filters] = logFilter(1);
    if (flag == FilterExclude && !filters.empty() && filters.find(toLower(context.module)) != filters.end()) {
        return;
    }
    if (flag == FilterInclude && filters.empty()) {
        return;
    }
    if (flag == FilterInclude && filters.find(toLower(context.module)) == filters.end()) {
        return;
    }
    time_t time  = std::chrono::system_clock::to_time_t(context.time);
    char buf[64] = {0};
#ifdef _MSVC_LANG
#pragma warning(push)
#pragma warning(disable : 4996) // disable deprecated warning
#endif
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
#ifdef _MSVC_LANG
#pragma warning(pop)
#endif
    uint64_t disMillseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(context.time.time_since_epoch()).count() -
        (std::chrono::duration_cast<std::chrono::seconds>(context.time.time_since_epoch()).count() * 1000);
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
    fprintf(stderr, "%s[%s.%03d] %s - [%s:%d][%s] [%s]%s %s\n", ansiColorCode(context.color), buf,
            static_cast<int>(disMillseconds), level, fileStr.c_str(), context.line, funcStr.c_str(), context.module,
            ansiColorCode("reset"), message);
#else
    fprintf(stderr, "[%s.%03d] %s - [%s] %s\n", buf, static_cast<int>(disMillseconds), level, context.module, message);
#endif
}
} // namespace logdetail
} // namespace nekoproto

#define NEKO_LOG_INCLUDE(module, ...)                                                                                  \
    nekoproto::logdetail::addLogFilter(nekoproto::logdetail::FilterInclude, module, ##__VA_ARGS__)
#define NEKO_LOG_EXCLUDE(module, ...)                                                                                  \
    nekoproto::logdetail::addLogFilter(nekoproto::logdetail::FilterExclude, module, ##__VA_ARGS__)
#define NEKO_LOG_SET_LEVEL(level) nekoproto::logdetail::logLevelFilter(level, 0)
#if defined(NEKO_PROTO_USE_FMT)
#include <fmt/format.h>
#define NEKO_PRIVATE_LOG(level, color, module, fmtstr, ...)                                                            \
    nekoproto::logdetail::nekoProtoPrivateLogOut(                                                                      \
        #level, fmt::format(fmtstr, ##__VA_ARGS__).c_str(),                                                            \
        {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now(), #color})
#elif defined(NEKO_PROTO_USE_SPDLOG)
#include <spdlog/spdlog.h>
#define NEKO_PRIVATE_LOG(level, color, module, fmt, ...)                                                               \
    spdlog::level("[{}][{}:{}][{}] " fmt, #module, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#elif defined(NEKO_PROTO_USE_STD_FORMAT) && NEKO_CPP_PLUS >= 20
#include <format>
#define NEKO_PRIVATE_LOG(level, color, module, fmt, ...)                                                               \
    nekoproto::logdetail::nekoProtoPrivateLogOut(                                                                      \
        #level, std::format(fmt, ##__VA_ARGS__).c_str(),                                                               \
        {module, __FILE__, __FUNCTION__, __LINE__, std::chrono::system_clock::now(), #color})
#else
#define NEKO_PRIVATE_LOG(...)
#endif
#else
#define NEKO_PRIVATE_LOG(...)
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
#define NEKO_LOG_DEBUG(module, ...) NEKO_PRIVATE_LOG(debug, blue, module, __VA_ARGS__)
#define NEKO_LOG_TRACE(module, ...) NEKO_PRIVATE_LOG(trace, darkGray, module, __VA_ARGS__)
#define NEKO_ASSERT(cond, module, ...)                                                                                 \
    if (!(cond)) {                                                                                                     \
        NEKO_PRIVATE_LOG(fatal, lightRed, module, __VA_ARGS__);                                                        \
        abort();                                                                                                       \
    }
#else
#define NEKO_ASSERT(cond, fmt, ...)
#define NEKO_LOG_DEBUG(...)
#define NEKO_LOG_TRACE(...)
#endif

#if defined(NEKO_PROTO_LOG)
#define NEKO_LOG_INFO(module, ...)  NEKO_PRIVATE_LOG(info, lightgray, module, __VA_ARGS__)
#define NEKO_LOG_WARN(module, ...)  NEKO_PRIVATE_LOG(warn, lightyellow, module, __VA_ARGS__)
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
