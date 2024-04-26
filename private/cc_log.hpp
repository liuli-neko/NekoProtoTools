#pragma once

#if !defined(NDEBUG) && !defined(CS_CCPROTO_NDEBUG)
#define CS_CCPROTO_DEBUG
#endif

#if !defined(CS_CCPROTO_NLOG)
#define CS_CCPROTO_LOG
#endif

#ifdef CS_CCPROTO_DEBUG
#   include <spdlog/spdlog.h>
#   define CS_DEBUG(...)                                                         \
        spdlog::debug(__VA_ARGS__)
#   define CS_ASSERT(cond, ...)                                                  \
        if (!(cond)) {                                                           \
          spdlog::error(__VA_ARGS__);                                            \
          abort();                                                               \
        }
#else
#   define CS_ASSERT(cond, fmt, ...) void
#   define CS_DEBUG(...) void
#endif

#ifdef CS_CCPROTO_LOG
#   include <spdlog/spdlog.h>
#   define CS_LOG_INFO(...)                                                         \
        spdlog::info(__VA_ARGS__)
#   define CS_LOG_WARN(...)                                                         \
        spdlog::warn(__VA_ARGS__)
#   define CS_LOG_ERROR(...)                                                        \
        spdlog::error(__VA_ARGS__)
#   define CS_LOG_FATAL(...)                                                        \
        spdlog::critical(__VA_ARGS__)
#else
#   define CS_LOG_INFO(...) void
#   define CS_LOG_WARN(...) void
#   define CS_LOG_ERROR(...) void
#   define CS_LOG_FATAL(...) void
#endif