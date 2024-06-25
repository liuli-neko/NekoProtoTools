#pragma once

#if !defined(NDEBUG) && !defined(NEKO_PROTO_NDEBUG)
#define NEKO_PROTO_DEBUG
#endif

#if !defined(NEKO_PROTO_NLOG)
#define NEKO_PROTO_LOG
#endif

#if defined(NEKO_PROTO_DEBUG) && defined(NEKO_PROTO_LOG_CONTEXT)
#include <spdlog/spdlog.h>
#define NEKO_DEBUG(fmt, ...) spdlog::debug("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define NEKO_ASSERT(cond, fmt, ...)                                                                                    \
    if (!(cond)) {                                                                                                     \
        spdlog::error("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__);                                              \
        abort();                                                                                                       \
    }
#elif defined(NEKO_PROTO_DEBUG)
#include <spdlog/spdlog.h>
#define NEKO_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define NEKO_ASSERT(cond, ...)                                                                                         \
    if (!(cond)) {                                                                                                     \
        spdlog::error(__VA_ARGS__);                                                                                    \
        abort();                                                                                                       \
    }
#else
#define NEKO_ASSERT(cond, fmt, ...)
#define NEKO_DEBUG(...)
#endif

#if defined(NEKO_PROTO_LOG) && defined(NEKO_PROTO_LOG_CONTEXT)
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#define NEKO_LOG_INFO(fmt, ...)  spdlog::info("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define NEKO_LOG_WARN(fmt, ...)  spdlog::warn("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define NEKO_LOG_ERROR(fmt, ...) spdlog::error("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define NEKO_LOG_FATAL(fmt, ...) spdlog::critical("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#elif defined(NEKO_PROTO_LOG)
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#define NEKO_LOG_INFO(...)  spdlog::info(__VA_ARGS__)
#define NEKO_LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#define NEKO_LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define NEKO_LOG_FATAL(...) spdlog::critical(__VA_ARGS__)
#else
#define NEKO_LOG_INFO(...)
#define NEKO_LOG_WARN(...)
#define NEKO_LOG_ERROR(...)
#define NEKO_LOG_FATAL(...)
#endif
