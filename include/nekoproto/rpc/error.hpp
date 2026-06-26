#pragma once

#include <string>
#include <system_error>
#include <type_traits>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

enum class RpcError {
    Ok                                = 0,
    MethodNotBind                     = 1,
    ClientNotInit                     = 2,
    ResponseIdNotMatch                = 3,
    InvalidRequest                    = 4,
    MethodNotFound                    = 5,
    InvalidParams                     = 6,
    InternalError                     = 7,
    MethodIdNotNegotiated             = 8,
    MethodTableOutdated               = 9,
    MethodIdNotFound                  = 10,
    MethodIdRemoved                   = 11,
    MethodSignatureMismatch           = 12,
    MethodIdRequiredButUnsupported    = 13,
    CompressionRequiredButUnsupported = 14,
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
        case RpcError::MethodIdNotNegotiated:
            return "Method Id Not Negotiated";
        case RpcError::MethodTableOutdated:
            return "Method Table Outdated";
        case RpcError::MethodIdNotFound:
            return "Method Id Not Found";
        case RpcError::MethodIdRemoved:
            return "Method Id Removed";
        case RpcError::MethodSignatureMismatch:
            return "Method Signature Mismatch";
        case RpcError::MethodIdRequiredButUnsupported:
            return "Method Id Required But Unsupported";
        case RpcError::CompressionRequiredButUnsupported:
            return "Compression Required But Unsupported";
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
