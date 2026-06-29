#pragma once

#include <string_view>
#include <type_traits>

#include "nekoproto/global/reflection_tags.hpp"
#include "nekoproto/global/string_literal.hpp"

NEKO_BEGIN_NAMESPACE

template <ConstexprString Prefix, auto BaseTags = NoTags{}>
struct RpcPrefixTag {
    constexpr static auto prefix = Prefix;
    constexpr static auto base   = BaseTags;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}>
struct RpcNoPrefixTag {
    constexpr static bool noPrefix = true;
    constexpr static auto base     = BaseTags;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Prefix, auto BaseTags = NoTags{}>
inline constexpr auto rpc_prefix_tag = RpcPrefixTag<Prefix, BaseTags>{};

inline constexpr auto rpc_no_prefix_tag = RpcNoPrefixTag<>{};

namespace detail {

template <typename Tags>
constexpr bool rpc_no_prefix_tag_v() {
    using CleanTags = std::remove_cvref_t<Tags>;
    if constexpr (requires { CleanTags::noPrefix; }) {
        return CleanTags::noPrefix;
    } else if constexpr (requires { CleanTags::base; }) {
        return rpc_no_prefix_tag_v<decltype(CleanTags::base)>();
    } else {
        return false;
    }
}

template <typename Tags>
constexpr std::string_view rpc_prefix_tag_value() {
    using CleanTags = std::remove_cvref_t<Tags>;
    if constexpr (requires { CleanTags::prefix; }) {
        return CleanTags::prefix.view();
    } else if constexpr (requires { CleanTags::base; }) {
        return rpc_prefix_tag_value<decltype(CleanTags::base)>();
    } else {
        return {};
    }
}

} // namespace detail

NEKO_END_NAMESPACE
