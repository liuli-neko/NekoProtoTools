#pragma once

#define CS_PROTO_BEGIN_NAMESPACE namespace cs_ccproto {
#define CS_PROTO_END_NAMESPACE }

#define CS_PROTO_USE_NAMESPACE using namespace cs_ccproto;
#define CS_PROTO_ASSERT(cond, fmt, ...) if (!(cond)) {                                  \
    printf("[%s:%d][%s] Assertion failed: " fmt "\n", __FILE__, __LINE__,               \
           __FUNCTION__, ##__VA_ARGS__);                                                \
    abort();                                                                            \
  }

#  ifdef _WIN32
#    define CS_PROTO_DECL_EXPORT     __declspec(dllexport)
#    define CS_PROTO_DECL_IMPORT     __declspec(dllimport)
#    define CS_PROTO_DECL_LOCAL
#  elif defined(__GNUC__) && (__GNUC__ >= 4)
#    define CS_PROTO_DECL_EXPORT     __attribute__((visibility("default")))
#    define CS_PROTO_DECL_IMPORT     __attribute__((visibility("default")))
#    define CS_PROTO_DECL_LOCAL     __attribute__((visibility("hidden")))
#  else
#    define CS_PROTO_DECL_EXPORT
#    define CS_PROTO_DECL_IMPORT
#    define CS_PROTO_DECL_LOCAL
#  endif

#ifndef CS_PROTO_STATIC
# ifdef CS_PROTO_LIBRARY
#   define CS_PROTO_API CS_PROTO_DECL_EXPORT
# else
#   define CS_PROTO_API CS_PROTO_DECL_IMPORT
# endif
#else
# define CS_PROTO_API
#endif

#ifdef __GNUC__
#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)                                                   \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...)                                        \
  macro(a1) __VA_OPT__(FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

#define ENUM_CASE(name)                                                        \
  case name:                                                                   \
    return #name;

#define MAKE_ENUM(type, ...)                                                   \
  enum class type : uint32_t { __VA_ARGS__ };                                  \
  constexpr const char *to_cstring(type _e) {                                  \
    using enum type;                                                           \
    switch (_e) {                                                              \
      FOR_EACH(ENUM_CASE, __VA_ARGS__)                                         \
    default:                                                                   \
      return "unknown";                                                        \
    }                                                                          \
  }
#endif

#if !defined(NDEBUG) && !defined(CS_PROTO_NDEBUG)
#define CS_PROTO_DEBUG
#endif

#if !defined(CS_PROTO_NLOG)
#define CS_PROTO_LOG
#endif

#ifdef CS_PROTO_DEBUG
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

#ifdef CS_PROTO_LOG
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