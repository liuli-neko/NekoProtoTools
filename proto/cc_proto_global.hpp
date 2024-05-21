#pragma once

#include "../private/cc_log.hpp"

#define CS_PROTO_BEGIN_NAMESPACE namespace cs_ccproto {
#define CS_PROTO_END_NAMESPACE }
#define CS_PROTO_USE_NAMESPACE using namespace cs_ccproto;
#define CS_PROTO_NAMESPACE ::cs_ccproto

#if __cplusplus >= 202002L || _MSVC_LANG >= 202002L
#define CS_CPP_PLUS 20
#elif __cplusplus >= 201703L || _MSVC_LANG >= 201703L
#define CS_CPP_PLUS 17
#elif __cplusplus >= 201402L || _MSVC_LANG >= 201402L
#define CS_CPP_PLUS 14
#elif __cplusplus >= 201103L || _MSVC_LANG >= 201103L
#define CS_CPP_PLUS 11
#endif

#if defined(__GNUC__) || defined(__MINGW32__)
#define CS_USED [[gnu::used]]
#elif defined(_WIN32)
#define CS_USED 
#endif
#if CS_CPP_PLUS >= 17
#define CONSTEXPR_FUNC constexpr
#define CONSTEXPR_VAR constexpr
#else
#define CONSTEXPR_FUNC
#define CONSTEXPR_VAR constexpr
#endif

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

#if defined(__GNUC__) || defined(__MINGW__)
#include <cxxabi.h>
#endif
CS_PROTO_BEGIN_NAMESPACE
#if defined(__GNUC__) || defined(__MINGW__)
namespace {
    template <class T>
    std::string _cs_class_name() {
        int status = -4; // some arbitrary value to eliminate the compiler warning
        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status),
            std::free
        };
        return (status==0) ? res.get() : typeid(T).name();
    }
}
#elif defined(_WIN32)
namespace {
    template <class T>
    std::string _cs_class_name() {
        std::string string(__FUNCSIG__);
        size_t start = string.find_last_of(' ');
        auto sstring = string.substr(start);
        size_t d =  sstring.find_last_of(':');
        if (d != std::string::npos)
            start = d;
        else
            start = 0;
        while (start < sstring.size() && (sstring[start] == ' ' || sstring[start] == '>' || sstring[start] == ':')) {
            ++start;
        }
        size_t end = sstring.find_last_of('>');
        return sstring.substr(start, end - start);
    }
}
#endif
CS_PROTO_END_NAMESPACE