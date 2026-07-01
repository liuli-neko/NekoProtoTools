#pragma once

#include <array>
#include <string_view>
#include <type_traits>
#include <vector>

#include "nekoproto/global/reflection_tags.hpp"
#include "nekoproto/global/string_literal.hpp"

NEKO_BEGIN_NAMESPACE
namespace tag_detail {
template <ConstexprString Prefix>
struct rpc_prefix_impl {
    constexpr static auto prefix = Prefix.view();
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

struct rpc_no_prefix_impl {
    constexpr static bool no_prefix = true;
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString Name>
struct rpc_name_impl {
    constexpr static auto method_name = Name.view(); // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString Description>
struct rpc_desc_impl {
    constexpr static auto description = Description.view(); // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString Version>
struct rpc_version_impl {
    constexpr static auto version = Version.view(); // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString... Names>
struct rpc_args_impl {
    constexpr static std::array<std::string_view, sizeof...(Names)> arg_names = {Names.view()...}; // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        if constexpr (requires { T::NumParams; }) {
            return static_cast<std::size_t>(T::NumParams) == sizeof...(Names);
        } else {
            return true;
        }
    }
};

struct rpc_notification_impl {
    constexpr static bool notification = true; // NOLINT
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};
} // namespace tag_detail

template <ConstexprString Prefix>
inline constexpr auto rpc_prefix = tag_detail::rpc_prefix_impl<Prefix>{};

inline constexpr auto rpc_no_prefix = tag_detail::rpc_no_prefix_impl{};

template <ConstexprString Name>
inline constexpr auto rpc_name = tag_detail::rpc_name_impl<Name>{};

template <ConstexprString Description>
inline constexpr auto rpc_desc = tag_detail::rpc_desc_impl<Description>{};

template <ConstexprString Version>
inline constexpr auto rpc_version = tag_detail::rpc_version_impl<Version>{};

template <ConstexprString... Names>
inline constexpr auto rpc_args = tag_detail::rpc_args_impl<Names...>{};

inline constexpr auto rpc_notification = tag_detail::rpc_notification_impl{};

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, prefix, rpc_prefix);               // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, no_prefix, rpc_no_prefix);                     // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, method_name, rpc_name);            // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, description, rpc_desc);            // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, version, rpc_version);             // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, arg_names, rpc_args); // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, notification, rpc_notification_flag);          // NOLINT
} // namespace tag_property

NEKO_END_NAMESPACE
