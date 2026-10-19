#pragma once
#include "nekoproto/global/macros.hpp"

#include "nekoproto/reflect.hpp"

#include "nekoproto/global/detail/expected_dependency.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto {
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
} // namespace nekoproto
NEKO_END_EXPORT
