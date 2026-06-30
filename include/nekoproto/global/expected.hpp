#pragma once

#include "nekoproto/global/global.hpp"

#include <version>

#ifndef NEKO_PROTO_USE_STD_EXPECTED
    #if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
        #define NEKO_PROTO_USE_STD_EXPECTED 1
    #else
        #define NEKO_PROTO_USE_STD_EXPECTED 0
    #endif
#endif

#if NEKO_PROTO_USE_STD_EXPECTED
    #include <expected>
#else
    #include <zeus/expected.hpp>
#endif

NEKO_BEGIN_NAMESPACE
namespace expected {

#if NEKO_PROTO_USE_STD_EXPECTED

template <typename T, typename E>
using expected = std::expected<T, E>;

template <typename E>
using unexpected = std::unexpected<E>;

using unexpect_t = std::unexpect_t;
inline constexpr auto unexpect = std::unexpect;

template <typename E>
using bad_expected_access = std::bad_expected_access<E>;

#else

template <typename T, typename E>
using expected = zeus::expected<T, E>;

template <typename E>
using unexpected = zeus::unexpected<E>;

using unexpect_t = zeus::unexpect_t;
inline constexpr auto unexpect = zeus::unexpect;

template <typename E>
using bad_expected_access = zeus::bad_expected_access<E>;

#endif

} // namespace expected
NEKO_END_NAMESPACE
