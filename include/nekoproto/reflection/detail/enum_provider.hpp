#pragma once
#include "nekoproto/global/macros.hpp"
#include "legacy_metadata.hpp"
#include "native_annotations.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
#if NEKO_USE_NATIVE_REFLECTION
template <typename T>
struct NativeEnumReflectionTable {
    static constexpr std::size_t size = std::meta::enumerators_of(^^T).size();
    template <std::size_t I>
    static consteval auto item() { return std::meta::enumerators_of(^^T)[I]; }
    static constexpr auto names = []<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<std::string_view, size>{std::meta::identifier_of(item<I>())...};
    }(std::make_index_sequence<size>{});
    static constexpr auto values = []<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, size>{std::meta::extract<T>(item<I>())...};
    }(std::make_index_sequence<size>{});
    static constexpr auto tags = []<std::size_t... I>(std::index_sequence<I...>) {
        static_assert((performCheck<T, nativeTagsOf<item<I>()>()>() && ...), "enum tags checks failed");
        return std::tuple{nativeTagsOf<item<I>()>()...};
    }(std::make_index_sequence<size>{});
};
#endif

// Automatic enum discovery retains the legacy ordering/alias contract. Native
// enum semantics are deliberately selected only by the explicit native mode.
template <typename T>
using SelectedEnumReflectionTable =
#if NEKO_USE_NATIVE_REFLECTION && NEKO_PROTO_REFLECTION_BACKEND == NEKO_REFLECTION_NATIVE
    NativeEnumReflectionTable<T>;
#else
    EnumReflectionTable<T>;
#endif
}
NEKO_END_EXPORT
