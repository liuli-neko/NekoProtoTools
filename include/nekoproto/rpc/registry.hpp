#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include "nekoproto/rpc/method.hpp"
#include "nekoproto/rpc/properties.hpp"
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

template <typename Method, typename Fn>
void rpc_visit_method(Method& method, std::string_view fieldName, bool hasFieldName,
                      const RpcPropertyPatch& fieldProperties, std::string_view prefix, Fn&& fn) {
    method.applyRpcProperties(fieldProperties);
    // Name priority: reflected field tag > method-declared name/spec tag > reflected field name.
    if (method.declaredName().empty() && hasFieldName) {
        method.setDeclaredName(fieldName);
    }

    std::string methodPrefix;
    if (method.rpcNoPrefix()) {
        methodPrefix = {};
    } else if (!method.rpcPrefix().empty()) {
        methodPrefix = rpc_join_name(prefix, method.rpcPrefix());
    } else {
        methodPrefix = std::string(prefix);
    }

    const auto fullName = rpc_join_name(methodPrefix, method.declaredName());
    method.setRemoteName(fullName);
    fn(method);
}

template <typename T, typename Fn>
void for_each_rpc_method(T& protocol, Fn&& fn, std::string_view prefix = {}) {
    if constexpr (RpcMethodObject<T>) {
        rpc_visit_method(protocol, std::string_view{}, false, RpcPropertyPatch{}, prefix, fn);
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
    const auto fieldProperties = collect_rpc_properties(tags);
    if constexpr (RpcMethodObject<FieldT>) {
        rpc_visit_method(field, fieldName, hasFieldName, fieldProperties, prefix, fn);
    } else if constexpr (RpcReflectable<FieldT>) {
        std::string nextPrefix;
        if (fieldProperties.noPrefix.value_or(false)) {
            nextPrefix = std::string(prefix);
        } else if (fieldProperties.prefix) {
            nextPrefix = rpc_join_name(prefix, *fieldProperties.prefix);
        } else if (hasFieldName) {
            nextPrefix = rpc_join_name(prefix, fieldName);
        } else {
            nextPrefix = std::string(prefix);
        }
        for_each_rpc_method(field, fn, nextPrefix);
    }
}

} // namespace detail

NEKO_END_NAMESPACE
