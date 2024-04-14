#pragma once

#define CS_PROTO_BEGIN_NAMESPACE namespace cs_ccproto {
#define CS_PROTO_END_NAMESPACE }

#define CS_PROTO_USE_NAMESPACE using namespace cs_ccproto;
#define CS_PROTO_ASSERT(cond, fmt, ...) if (!(cond)) {                                  \
    printf("[%s:%d][%s] Assertion failed: " fmt "\n", __FILE__, __LINE__,               \
           __FUNCTION__, ##__VA_ARGS__);                                                \
    abort();                                                                            \
  }


#ifdef __GNU__
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

#ifndef NO_LOG
#define LOG(fmt, ...)                                                          \
  printf("[%s:%d][%s] " fmt "\n", __FILE__, __LINE__, __FUNCTION__,            \
         ##__VA_ARGS__);
#else
#define LOG(fmt, ...)
#endif
