#pragma once

#include <system_error>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

enum class RpcError {
    Ok                 = 0,
    MethodNotBind      = -32000,
    ClientNotInit      = -32001,
    ResponseIdNotMatch = -32002,
    InvalidRequest     = -32600,
    MethodNotFound     = -32601,
    InvalidParams      = -32602,
    InternalError      = -32603,
};

class RpcErrorCategory : public std::error_category {
public:
    static RpcErrorCategory& instance() {
        static RpcErrorCategory kInstance;
        return kInstance;
    }

    auto message(int value) const noexcept -> std::string override {
        switch (static_cast<RpcError>(value)) {
        case RpcError::Ok:
            return "Ok";
        case RpcError::MethodNotBind:
            return "Method not Bind";
        case RpcError::ClientNotInit:
            return "Client Not Init";
        case RpcError::ResponseIdNotMatch:
            return "Response Id Not Match";
        case RpcError::InvalidRequest:
            return "Invalid Request";
        case RpcError::MethodNotFound:
            return "Method Not Found";
        case RpcError::InvalidParams:
            return "Invalid Params";
        case RpcError::InternalError:
            return "Internal Error";
        default:
            return "Unknown RPC Error";
        }
    }

    auto name() const noexcept -> const char* override { return "rpc"; }
};

inline auto make_error_code(RpcError value) noexcept -> std::error_code {
    return std::error_code(static_cast<int>(value), RpcErrorCategory::instance());
}

NEKO_END_NAMESPACE

template <>
struct std::is_error_code_enum<NEKO_NAMESPACE::RpcError> : std::true_type {};
