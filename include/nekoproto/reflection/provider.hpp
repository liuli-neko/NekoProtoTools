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

// Capability queries must not instantiate the provider's accessor tuple: doing
// so can form void&, instantiate an unsupported structured binding, or diagnose
// invalid metadata while merely testing a constraint. Actual reflection still
// uses the diagnostic provider selection above.
template <typename T>
struct ReflectionCapabilities {
    using context_type = std::remove_cvref_t<T>;
    static constexpr auto kind = getReflectProviderKind<context_type, false>();
    static constexpr bool has_values = [] {
        if constexpr (kind == ReflectProviderKind::NativeReflection) {
            return NativeReflectionProvider<context_type>::has_values;
        } else {
            return kind != ReflectProviderKind::Error;
        }
    }();
    static constexpr bool has_names = [] {
        if constexpr (kind == ReflectProviderKind::NativeReflection) {
            return NativeReflectionProvider<context_type>::has_names;
        } else if constexpr (has_values) {
            return HasLegacyNamesMeta<context_type>;
        } else {
            return false;
        }
    }();
};

template <typename T> concept HasNamesMeta = ReflectionCapabilities<T>::has_names;
template <typename T> concept HasValuesMeta = ReflectionCapabilities<T>::has_values;
}
NEKO_END_EXPORT
