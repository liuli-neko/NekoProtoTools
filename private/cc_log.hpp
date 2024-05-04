#pragma once

#if !defined(NDEBUG) && !defined(CS_CCPROTO_NDEBUG)
#define CS_CCPROTO_DEBUG
#endif

#if !defined(CS_CCPROTO_NLOG)
#define CS_CCPROTO_LOG
#endif

#if defined(CS_CCPROTO_DEBUG) && defined(CS_CCPROTO_LOG_CONTEXT)
#include <spdlog/spdlog.h>
#define CS_DEBUG(fmt, ...) spdlog::debug("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define CS_ASSERT(cond, fmt, ...)                                        \
    if (!(cond)) {                                                       \
        spdlog::error("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        abort();                                                         \
    }
#elif defined(CS_CCPROTO_DEBUG)
#include <spdlog/spdlog.h>
#define CS_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define CS_ASSERT(cond, ...)        \
    if (!(cond)) {                  \
        spdlog::error(__VA_ARGS__); \
        abort();                    \
    }
#else
#define CS_ASSERT(cond, fmt, ...) void
#define CS_DEBUG(...) void
#endif

#if defined(CS_CCPROTO_LOG) && defined(CS_CCPROTO_LOG_CONTEXT)
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#define CS_LOG_INFO(fmt, ...) spdlog::info("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define CS_LOG_WARN(fmt, ...) spdlog::warn("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define CS_LOG_ERROR(fmt, ...) spdlog::error("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define CS_LOG_FATAL(fmt, ...) spdlog::critical("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#elif defined(CS_CCPROTO_LOG)
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#define CS_LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define CS_LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define CS_LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define CS_LOG_FATAL(...) spdlog::critical(__VA_ARGS__)
#else
#define CS_LOG_INFO(...) void
#define CS_LOG_WARN(...) void
#define CS_LOG_ERROR(...) void
#define CS_LOG_FATAL(...) void
#endif