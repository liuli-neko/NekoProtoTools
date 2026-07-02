#pragma once

#include <string>
#include <vector>

#include "nekoproto/rpc/method.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

// clang-format off
struct RpcBuiltinMethods {
    RpcMethodSpec<std::vector<std::string>(), 
                rpc_name<"get_method_list">,
                rpc_desc<"Get a list of all method that have been declared in the service">,
                rpc_version<"1.0.0">> getMethodList;

    RpcMethodSpec<std::vector<std::string>(), 
                rpc_name<"get_method_info_list">,
                rpc_desc<"Get a list of all method info that have been declared in the service">,
                rpc_version<"1.0.0">> getMethodInfoList;

    RpcMethodSpec<std::string(std::string), 
                rpc_name<"get_method_info">, 
                rpc_args<"method_name">,
                rpc_desc<"Get a method info by method name">,
                rpc_version<"1.0.0">> getMethodInfo;

    RpcMethodSpec<std::vector<std::string>(), 
                rpc_name<"get_bind_method_list">,
                rpc_desc<"Get a list of all method that have been binded in the service">,
                rpc_version<"1.0.0">> getBindedMethodList;
};
// clang-format on

NEKO_END_NAMESPACE
