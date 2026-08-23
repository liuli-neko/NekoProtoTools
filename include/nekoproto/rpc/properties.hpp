#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "nekoproto/rpc/tags.hpp"

namespace nekoproto {
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
auto collectRpcProperties(const Tags& tags) -> RpcPropertyPatch {
    RpcPropertyPatch properties;

    if (tag_query::has<tag_property::RpcName>(tags)) {
        properties.name = tag_query::get<tag_property::RpcName>(tags);
    }
    if (tag_query::has<tag_property::RpcNoPrefix>(tags)) {
        properties.noPrefix = tag_query::get<tag_property::RpcNoPrefix>(tags);
    }
    if (tag_query::has<tag_property::RpcPrefix>(tags)) {
        properties.prefix = tag_query::get<tag_property::RpcPrefix>(tags);
    }
    if (tag_query::has<tag_property::RpcDesc>(tags)) {
        properties.description = tag_query::get<tag_property::RpcDesc>(tags);
    }
    if (tag_query::has<tag_property::RpcVersion>(tags)) {
        properties.version = tag_query::get<tag_property::RpcVersion>(tags);
    }
    if (tag_query::has<tag_property::RpcArgs>(tags)) {
        properties.argNames    = tag_query::get<tag_property::RpcArgs>(tags);
        properties.hasArgNames = true;
    }
    if (tag_query::has<tag_property::RpcNotificationFlag>(tags)) {
        properties.notification = tag_query::get<tag_property::RpcNotificationFlag>(tags);
    }

    return properties;
}

} // namespace detail
} // namespace nekoproto
