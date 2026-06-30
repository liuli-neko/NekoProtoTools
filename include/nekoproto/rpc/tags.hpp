#pragma once

#include <string_view>
#include <type_traits>

#include "nekoproto/global/reflection_tags.hpp"
#include "nekoproto/global/string_literal.hpp"

NEKO_BEGIN_NAMESPACE

template <ConstexprString Prefix>
struct RpcPrefixTag {
    constexpr static auto prefix = Prefix;
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

struct RpcNoPrefixTag {
    constexpr static bool no_prefix = true;
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString Prefix>
inline constexpr auto rpc_prefix_tag = RpcPrefixTag<Prefix>{}; // NOLINT
inline constexpr auto rpc_no_prefix_tag = RpcNoPrefixTag{}; // NOLINT

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, prefix, rpc_prefix); // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, no_prefix, rpc_no_prefix);       // NOLINT
} // namespace tag_property

NEKO_END_NAMESPACE
