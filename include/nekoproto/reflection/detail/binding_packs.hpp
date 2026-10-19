#pragma once
#include "nekoproto/global/macros.hpp"
#include <tuple>
#include <type_traits>
#include <utility>

NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
template <class T>
constexpr auto bindingCount(T& object) {
    auto& [...fields] = object;
    return std::integral_constant<std::size_t, sizeof...(fields)>{};
}
template <class T>
constexpr auto bindAggregate(T& object) noexcept {
    auto& [...fields] = object;
    return std::tie(fields...);
}
}
NEKO_END_EXPORT
