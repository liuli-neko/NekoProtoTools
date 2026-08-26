#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "nekoproto/rpc/method.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

namespace nekoproto {

// clang-format off
struct RpcBuiltinMethods {
    RpcMethodSpec<std::vector<std::string>(), 
                rpc_name<"get_method_list">,
                rpc_desc<"Get a list of all method that have been declared in the service">,
                rpc_version<"1.0.0">> get_method_list;

    RpcMethodSpec<std::vector<std::string>(), 
                rpc_name<"get_method_info_list">,
                rpc_desc<"Get a list of all method info that have been declared in the service">,
                rpc_version<"1.0.0">> get_method_info_list;

    RpcMethodSpec<std::string(std::string), 
                rpc_name<"get_method_info">, 
                rpc_args<"method_name">,
                rpc_desc<"Get a method info by method name">,
                rpc_version<"1.0.0">> get_method_info;

    RpcMethodSpec<std::vector<std::string>(), 
                rpc_name<"get_bind_method_list">,
                rpc_desc<"Get a list of all method that have been binded in the service">,
                rpc_version<"1.0.0">> get_binded_method_list;

    RpcMethodSpec<std::map<std::string, std::string>(),
                rpc_name<"get_execution_policy">,
                rpc_desc<"Get public per-connection limits, current timeout setting, and deadline semantics">,
                rpc_version<"1.1.0">> get_execution_policy;

    RpcMethodSpec<std::uint64_t(std::uint64_t),
                rpc_name<"set_connection_timeout">,
                rpc_args<"timeout_ns">,
                rpc_desc<"Set the default server-visible timeout for future calls on this connection; zero clears it">,
                rpc_version<"1.1.0">> set_connection_timeout;

    RpcMethodSpec<std::map<std::string, std::uint64_t>(),
                rpc_name<"get_connection_status">,
                rpc_desc<"Get active and queued request counts for only the caller's current connection">,
                rpc_version<"1.1.0">> get_connection_status;

    RpcMethodSpec<std::map<std::string, std::string>(),
                rpc_name<"get_connection_tasks">,
                rpc_desc<"Get only currently queued or running request ids for the caller's current connection">,
                rpc_version<"1.1.0">> get_connection_tasks;

    NEKO_SERIALIZER(get_method_list, get_method_info_list, get_method_info, get_binded_method_list,
                    get_execution_policy, set_connection_timeout, get_connection_status, get_connection_tasks)
};
// clang-format on

} // namespace nekoproto
