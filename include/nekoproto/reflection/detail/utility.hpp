#pragma once
#include "nekoproto/global/macros.hpp"

#include <array>
#include <type_traits>

NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
template <typename T>
struct IsStdArray : std::false_type {};

template <typename T, std::size_t N>
struct IsStdArray<std::array<T, N>> : std::true_type {
    using value_type             = T;
    constexpr static std::size_t size = N;
};

} // namespace nekoproto::detail
NEKO_END_EXPORT
