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
#include "nekoproto/global/macros.hpp"

#include "global.hpp"
#include <cstring>

#if defined(NEKO_PROTO_LOG)
NEKO_BEGIN_EXPORT
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
    static std::function<void(const char* level, const char* message, const LogContext& context)> s_logger_func =
        logger;
    if (logger) {
        s_logger_func = logger;
    }
    return s_logger_func;
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
    if (color == "reset") return "\033[0m";         // NOLINT
    if (color == "black") return "\033[30m";        // NOLINT
    if (color == "red") return "\033[31m";          // NOLINT
    if (color == "green") return "\033[32m";        // NOLINT
    if (color == "yellow") return "\033[33m";       // NOLINT
    if (color == "blue") return "\033[34m";         // NOLINT
    if (color == "magenta") return "\033[35m";      // NOLINT
    if (color == "cyan") return "\033[36m";         // NOLINT
    if (color == "lightgray") return "\033[37m";    // NOLINT
    if (color == "darkgray") return "\033[90m";     // NOLINT
    if (color == "lightred") return "\033[91m";     // NOLINT
    if (color == "lightgreen") return "\033[92m";   // NOLINT
    if (color == "lightyellow") return "\033[93m";  // NOLINT
    if (color == "lightblue") return "\033[94m";    // NOLINT
    if (color == "lightmagenta") return "\033[95m"; // NOLINT
    if (color == "lightcyan") return "\033[96m";    // NOLINT
    if (color == "white") return "\033[97m";        // NOLINT
    return "";
}
#endif

enum LogFilterMode {
    FilterInclude,
    FilterExclude,
};
inline auto logLevelFilter(const std::string& level, int op) -> bool {
    static std::vector<std::string> s_levels = {NEKO_LOG_LEVEL_FATAL, NEKO_LOG_LEVEL_ERROR, NEKO_LOG_LEVEL_WARN,
                                                NEKO_LOG_LEVEL_INFO,  NEKO_LOG_LEVEL_DEBUG, NEKO_LOG_LEVEL_TRACE};
    static std::set<std::string> s_filter    = {NEKO_LOG_LEVEL_FATAL, NEKO_LOG_LEVEL_ERROR, NEKO_LOG_LEVEL_WARN};
    if (op == 0) {
        s_filter.clear();
        for (const auto& mlevel : s_levels) {
            if (mlevel != level) {
                s_filter.insert(mlevel);
            } else {
                s_filter.insert(mlevel);
                break;
            }
        }
        return true;
    }
    if (op == 1) {
        return s_filter.contains(level);
    }
    return !s_filter.contains(level);
}
inline auto logFilter(int opt, int flag = FilterInclude, const std::string& module = "")
    -> std::tuple<int, std::set<std::string>>& {
    static std::tuple<int, std::set<std::string>> s_neko_log_filter = {FilterExclude, {}};
    if (opt == 0) {
        if (flag == std::get<0>(s_neko_log_filter)) {
            std::get<1>(s_neko_log_filter).insert(toLower(module));
        } else {
            std::get<0>(s_neko_log_filter) = flag;
            std::get<1>(s_neko_log_filter).clear();
            std::get<1>(s_neko_log_filter).insert(toLower(module));
        }
    } else if (opt == 2) {
        std::get<1>(s_neko_log_filter).clear();
        std::get<0>(s_neko_log_filter) = FilterExclude;
    }
    return s_neko_log_filter;
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
    if (flag == FilterExclude && !filters.empty() && filters.contains(toLower(context.module))) {
        return;
    }
    if (flag == FilterInclude && filters.empty()) {
        return;
    }
    if (flag == FilterInclude && !filters.contains(toLower(context.module))) {
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
    uint64_t dis_millseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(context.time.time_since_epoch()).count() -
        (std::chrono::duration_cast<std::chrono::seconds>(context.time.time_since_epoch()).count() * 1000);
#if defined(NEKO_PROTO_LOG_CONTEXT)
    std::string file_str = context.file;
    if (file_str.find_last_of('/') != std::string::npos) {
        file_str = file_str.substr(file_str.find_last_of('/') + 1);
    } else if (file_str.find_last_of('\\') != std::string::npos) {
        file_str = file_str.substr(file_str.find_last_of('\\') + 1);
    }
    std::string func_str = context.func;
    if (func_str.find_last_of(':') != std::string::npos) {
        func_str = func_str.substr(func_str.find_last_of(':') + 1);
    }
    fprintf(stderr, "%s[%s.%03d] %s - [%s:%d][%s] [%s]%s %s\n", ansiColorCode(context.color), buf,
            static_cast<int>(dis_millseconds), level, file_str.c_str(), context.line, func_str.c_str(), context.module,
            ansiColorCode("reset"), message);
#else
    fprintf(stderr, "[%s.%03d] %s - [%s] %s\n", buf, static_cast<int>(dis_millseconds), level, context.module, message);
#endif
}
} // namespace logdetail
} // namespace nekoproto

NEKO_END_EXPORT
#endif
