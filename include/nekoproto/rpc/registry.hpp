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
void rpc_visit_field(Field& field, std::string_view field_name, const Tags& tags, std::string_view prefix, Fn&& fn);

template <typename Method, typename Fn>
void rpc_visit_method(Method& method, std::string_view field_name, const RpcPropertyPatch& field_properties,
                      std::string_view prefix, Fn&& fn) {
    method.applyRpcProperties(field_properties);
    // Name priority: reflected field tag > method-declared name/spec tag > reflected field name.
    if (method.declaredName().empty() && !field_name.empty()) {
        method.setDeclaredName(field_name);
    }

    std::string method_prefix;
    if (method.rpcNoPrefix()) {
        method_prefix = {};
    } else if (!method.rpcPrefix().empty()) {
        method_prefix = rpc_join_name(prefix, method.rpcPrefix());
    } else {
        method_prefix = std::string(prefix);
    }

    const auto full_name = rpc_join_name(method_prefix, method.declaredName());
    method.setRemoteName(full_name);
    fn(method);
}

template <typename T, typename Fn>
void for_each_rpc_method(T& protocol, Fn&& fn, std::string_view prefix = {}) {
    if constexpr (RpcMethodObject<T>) {
        rpc_visit_method(protocol, std::string_view{}, RpcPropertyPatch{}, prefix, fn);
    } else if constexpr (RpcReflectable<T>) {
        auto with_name = [&]<typename Field, typename Tags>(Field& field, std::string_view field_name,
                                                            const Tags& tags) {
            rpc_visit_field(field, field_name, tags, prefix, fn);
        };
        auto without_name = [&]<typename Field, typename Tags>(Field& field, const Tags& tags) {
            rpc_visit_field(field, std::string_view{}, tags, prefix, fn);
        };
        Reflect<std::remove_cvref_t<T>>::forEach(protocol, Overloads{with_name, without_name});
    } else {
        static_assert(RpcMethodObject<T> || RpcReflectable<T>, "RPC protocol type must be reflectable");
    }
}

template <typename Field, typename Fn, typename Tags>
void rpc_visit_field(Field& field, std::string_view field_name, const Tags& tags, std::string_view prefix, Fn&& fn) {
    using FieldT                = std::remove_cvref_t<Field>;
    const auto field_properties = collect_rpc_properties(tags);
    if constexpr (RpcMethodObject<FieldT>) {
        rpc_visit_method(field, field_name, field_properties, prefix, fn);
    } else if constexpr (RpcReflectable<FieldT>) {
        std::string next_prefix;
        if (field_properties.noPrefix.value_or(false)) {
            next_prefix = std::string(prefix);
        } else if (field_properties.prefix) {
            next_prefix = rpc_join_name(prefix, *field_properties.prefix);
        } else if (!field_name.empty()) {
            next_prefix = rpc_join_name(prefix, field_name);
        } else {
            next_prefix = std::string(prefix);
        }
        for_each_rpc_method(field, fn, next_prefix);
    }
}

} // namespace detail

NEKO_END_NAMESPACE
