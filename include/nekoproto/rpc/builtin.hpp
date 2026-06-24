#pragma once

#include <string>
#include <vector>

#include "nekoproto/rpc/method.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

struct RpcBuiltinMethods {
    RpcMethod<std::vector<std::string>(), "get_method_list"> getMethodList;
    RpcMethod<std::vector<std::string>(), "get_method_info_list"> getMethodInfoList;
    RpcMethod<std::string(std::string), "get_method_info", "method_name"> getMethodInfo;
    RpcMethod<std::vector<std::string>(), "get_bind_method_list"> getBindedMethodList;
};

NEKO_END_NAMESPACE
