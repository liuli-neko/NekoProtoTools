#pragma once
#include "nekoproto/global/macros.hpp"
#include "features.hpp"
#include "detail/legacy_provider.hpp"
#include "detail/native_provider.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
template <typename T>
using SelectedReflectionProvider = std::conditional_t<
    reflect_provider_kind_v<T> == ReflectProviderKind::NativeReflection,
    NativeReflectionProvider<T>, LegacyReflectionProvider<T>>;

template <typename T>
struct ReflectProvider : SelectedReflectionProvider<T> {
    static constexpr auto legacy_kind = meta_kind_v<T>;
    static constexpr auto kind = legacy_kind; // compatibility with legacy introspection
};

template <typename T> concept HasNamesMeta = ReflectProvider<T>::has_names;
template <typename T> concept HasValuesMeta = ReflectProvider<T>::has_values;
}
NEKO_END_EXPORT
