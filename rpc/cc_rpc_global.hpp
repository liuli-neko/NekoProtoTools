#pragma once

#include "../private/cc_log.hpp"

#define CS_RPC_BEGIN_NAMESPACE namespace cs_ccproto {
#define CS_RPC_END_NAMESPACE }
#define CS_RPC_USE_NAMESPACE using namespace cs_ccproto;
#define CS_RPC_NAMESPACE ::cs_ccproto

#define CS_RPC_ASSERT(cond, fmt, ...) if (!(cond)) {                                    \
    printf("[%s:%d][%s] Assertion failed: " fmt "\n", __FILE__, __LINE__,               \
           __FUNCTION__, ##__VA_ARGS__);                                                \
    abort();                                                                            \
  }

#  ifdef _WIN32
#    define CS_RPC_DECL_EXPORT     __declspec(dllexport)
#    define CS_RPC_DECL_IMPORT     __declspec(dllimport)
#    define CS_RPC_DECL_LOCAL
#  elif defined(__GNUC__) && (__GNUC__ >= 4)
#    define CS_RPC_DECL_EXPORT     __attribute__((visibility("default")))
#    define CS_RPC_DECL_IMPORT     __attribute__((visibility("default")))
#    define CS_RPC_DECL_LOCAL     __attribute__((visibility("hidden")))
#  else
#    define CS_RPC_DECL_EXPORT
#    define CS_RPC_DECL_IMPORT
#    define CS_RPC_DECL_LOCAL
#  endif

#ifndef CS_RPC_STATIC
# ifdef CS_RPC_LIBRARY
#   define CS_RPC_API CS_RPC_DECL_EXPORT
# else
#   define CS_RPC_API CS_RPC_DECL_IMPORT
# endif
#else
# define CS_RPC_API
#endif

