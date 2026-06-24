#pragma once

#include <system_error>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

enum class RpcError {
    Ok                 = 0,
    MethodNotBind      = 1,
    ClientNotInit      = 2,
    ResponseIdNotMatch = 3,
    InvalidRequest     = 4,
    MethodNotFound     = 5,
    InvalidParams      = 6,
    InternalError      = 7,
};

class NEKO_PROTO_API RpcErrorCategory : public std::error_category {
public:
    static RpcErrorCategory& instance();

    auto message(int value) const noexcept -> std::string override;

    auto name() const noexcept -> const char* override;
};

NEKO_PROTO_API auto make_error_code(RpcError value) noexcept -> std::error_code;

NEKO_END_NAMESPACE

template <>
struct std::is_error_code_enum<NEKO_NAMESPACE::RpcError> : std::true_type {};
