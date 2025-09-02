/**
 * @file jsonrpc_error.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ilias/io/error.hpp>
#include <system_error>
#include <type_traits>

#include "nekoproto/global/config.h"
#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

enum class JsonRpcError {
    Ok = 0,
    // Server error Reserved for implementation-defined server-errors.
    ServerErrorStart   = -32000,
    MethodNotBind      = -32000,
    ClientNotInit      = -32001,
    ResponseIdNotMatch = -32002,
    MessageToolLarge   = -32003, // Request too large The JSON sent is too big.
    ServerErrorEnd     = -32099, // Server error end

    ParseError = -32700, // Parse error Invalid JSON was received by the server. An error
    // occurred on the server while parsing the JSON text.
    InvalidRequest = -32600, // Invalid Request The JSON sent is not a valid Request object.
    MethodNotFound = -32601, // Method not found The method does not exist / is not available.
    InvalidParams  = -32602, // Invalid params Invalid method parameter(s).
    InternalError  = -32603, // Internal error Internal JSON-RPC error.
};

class JsonRpcErrorCategory : public std::error_category {
public:
    static JsonRpcErrorCategory& instance();
    auto message(int value) const noexcept -> std::string override;
    auto name() const noexcept -> const char* override { return "jsonrpc"; }
};

inline JsonRpcErrorCategory& JsonRpcErrorCategory::instance() {
    static JsonRpcErrorCategory kInstance;
    return kInstance;
}

inline auto JsonRpcErrorCategory::message(int value) const noexcept -> std::string {
    switch ((JsonRpcError)value) {
    case JsonRpcError::Ok:
        return "Ok";
    case JsonRpcError::MethodNotBind:
        return "Method not Bind";
    case JsonRpcError::ParseError:
        return "Parse Eror";
    case JsonRpcError::InvalidRequest:
        return "InvalidRequest";
    case JsonRpcError::MethodNotFound:
        return "Method ot Found";
    case JsonRpcError::InvalidParams:
        return "InvalidParams";
    case JsonRpcError::InternalError:
        return "Internal Error";
    case JsonRpcError::ClientNotInit:
        return "Client Not Init";
    case JsonRpcError::ResponseIdNotMatch:
        return "Response Id Not Match";
    default:
        return "Unknow Error";
    }
}

ILIAS_DECLARE_ERROR(JsonRpcError, JsonRpcErrorCategory); // NOLINT
inline auto make_error_code(JsonRpcError value) noexcept -> std::error_code {
    return std::error_code(static_cast<int>(value), JsonRpcErrorCategory::instance());
}
NEKO_END_NAMESPACE

template <>
struct std::is_error_code_enum<NEKO_NAMESPACE::JsonRpcError> : std::true_type {};