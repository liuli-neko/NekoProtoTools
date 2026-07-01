#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "nekoproto/rpc/tags.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

struct RpcPropertyPatch {
    std::optional<std::string_view> name;
    std::optional<std::string_view> prefix;
    std::optional<bool> noPrefix;
    std::optional<std::string_view> description;
    std::optional<std::string_view> version;
    std::vector<std::string_view> argNames;
    bool hasArgNames = false;
    std::optional<bool> notification;
};

template <typename Tags>
auto collect_rpc_properties(const Tags& tags) -> RpcPropertyPatch {
    RpcPropertyPatch properties;

    if (tag_query::has<tag_property::rpc_name>(tags)) {
        properties.name = tag_query::get<tag_property::rpc_name>(tags);
    }
    if (tag_query::has<tag_property::rpc_no_prefix>(tags)) {
        properties.noPrefix = tag_query::get<tag_property::rpc_no_prefix>(tags);
    }
    if (tag_query::has<tag_property::rpc_prefix>(tags)) {
        properties.prefix = tag_query::get<tag_property::rpc_prefix>(tags);
    }
    if (tag_query::has<tag_property::rpc_desc>(tags)) {
        properties.description = tag_query::get<tag_property::rpc_desc>(tags);
    }
    if (tag_query::has<tag_property::rpc_version>(tags)) {
        properties.version = tag_query::get<tag_property::rpc_version>(tags);
    }
    if (tag_query::has<tag_property::rpc_args>(tags)) {
        properties.argNames    = tag_query::get<tag_property::rpc_args>(tags);
        properties.hasArgNames = true;
    }
    if (tag_query::has<tag_property::rpc_notification_flag>(tags)) {
        properties.notification = tag_query::get<tag_property::rpc_notification_flag>(tags);
    }

    return properties;
}

} // namespace detail
NEKO_END_NAMESPACE
