#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include "nekoproto/rpc/method.hpp"
#include "nekoproto/rpc/tags.hpp"
#include "nekoproto/serialization/reflection.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

inline std::string rpc_join_name(std::string_view prefix, std::string_view name) {
    if (prefix.empty()) {
        return std::string(name);
    }
    if (name.empty()) {
        return std::string(prefix);
    }
    std::string result;
    result.reserve(prefix.size() + 1 + name.size());
    result.append(prefix);
    result.push_back('.');
    result.append(name);
    return result;
}

template <typename T>
concept RpcReflectable = has_values_meta<std::remove_cvref_t<T>>;

template <typename Field, typename Fn, typename Tags>
void rpc_visit_field(Field& field, std::string_view fieldName, const Tags& tags, bool hasFieldName,
                     std::string_view prefix, Fn&& fn);

template <typename T, typename Fn>
void forEachRpcMethod(T& protocol, Fn&& fn, std::string_view prefix = {}) {
    if constexpr (RpcMethodObject<T>) {
        const auto fullName = rpc_join_name(prefix, protocol.declaredName());
        protocol.setRemoteName(fullName);
        fn(protocol);
    } else if constexpr (RpcReflectable<T>) {
        auto withName = [&]<typename Field, typename Tags>(Field& field, std::string_view fieldName,
                                                           const Tags& tags) {
            rpc_visit_field(field, fieldName, tags, true, prefix, fn);
        };
        auto withoutName = [&]<typename Field, typename Tags>(Field& field, const Tags& tags) {
            rpc_visit_field(field, std::string_view{}, tags, false, prefix, fn);
        };
        Reflect<std::remove_cvref_t<T>>::forEach(protocol, Overloads{withName, withoutName});
    } else {
        static_assert(RpcMethodObject<T> || RpcReflectable<T>, "RPC protocol type must be reflectable");
    }
}

template <typename Field, typename Fn, typename Tags>
void rpc_visit_field(Field& field, std::string_view fieldName, const Tags& tags, bool hasFieldName,
                     std::string_view prefix, Fn&& fn) {
    using FieldT = std::remove_cvref_t<Field>;
    if constexpr (RpcMethodObject<FieldT>) {
        const auto fullName = rpc_join_name(prefix, field.declaredName());
        field.setRemoteName(fullName);
        fn(field);
    } else if constexpr (RpcReflectable<FieldT>) {
        std::string nextPrefix;
        if constexpr (rpc_no_prefix_tag_v<Tags>()) {
            nextPrefix = std::string(prefix);
        } else if constexpr (rpc_prefix_tag_value<Tags>().size() > 0) {
            nextPrefix = rpc_join_name(prefix, rpc_prefix_tag_value<Tags>());
        } else if (hasFieldName) {
            nextPrefix = rpc_join_name(prefix, fieldName);
        } else {
            nextPrefix = std::string(prefix);
        }
        forEachRpcMethod(field, fn, nextPrefix);
    }
}

} // namespace detail

NEKO_END_NAMESPACE
