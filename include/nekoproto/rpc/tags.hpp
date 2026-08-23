#pragma once

#include <array>
#include <string_view>
#include <type_traits>
#include <vector>

#include "nekoproto/global/reflection_tags.hpp"
#include "nekoproto/global/string_literal.hpp"

namespace nekoproto {
namespace tag_detail {
template <ConstexprString Prefix>
struct RpcPrefixImpl {
    constexpr static auto prefix = Prefix.view();
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

struct RpcNoPrefixImpl {
    constexpr static bool no_prefix = true;
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString Name>
struct RpcNameImpl {
    constexpr static auto method_name = Name.view(); // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString Description>
struct RpcDescImpl {
    constexpr static auto description = Description.view(); // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString Version>
struct RpcVersionImpl {
    constexpr static auto version = Version.view(); // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString... Names>
struct RpcArgsImpl {
    constexpr static std::array<std::string_view, sizeof...(Names)> arg_names = {Names.view()...}; // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        if constexpr (requires { T::NumParams; }) {
            return static_cast<std::size_t>(T::NumParams) == sizeof...(Names);
        } else {
            return true;
        }
    }
};

struct RpcNotificationImpl {
    constexpr static bool notification = true; // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};
} // namespace tag_detail

template <ConstexprString Prefix>
inline constexpr auto rpc_prefix = tag_detail::RpcPrefixImpl<Prefix>{};

inline constexpr auto rpc_no_prefix = tag_detail::RpcNoPrefixImpl{};

template <ConstexprString Name>
inline constexpr auto rpc_name = tag_detail::RpcNameImpl<Name>{};

template <ConstexprString Description>
inline constexpr auto rpc_desc = tag_detail::RpcDescImpl<Description>{};

template <ConstexprString Version>
inline constexpr auto rpc_version = tag_detail::RpcVersionImpl<Version>{};

template <ConstexprString... Names>
inline constexpr auto rpc_args = tag_detail::RpcArgsImpl<Names...>{};

inline constexpr auto rpc_notification = tag_detail::RpcNotificationImpl{};

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, prefix, RpcPrefix);
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, no_prefix, RpcNoPrefix);
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, method_name, RpcName);
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, description, RpcDesc);
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, version, RpcVersion);
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, arg_names, RpcArgs);
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, notification, RpcNotificationFlag);
} // namespace tag_property

} // namespace nekoproto
