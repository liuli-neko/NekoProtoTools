#include "nekoproto/jsonrpc/jsonrpc.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include <string>

namespace NEKO_NAMESPACE::detail {

NEKO_PROTO_API std::string to_string(JsonRpcIdType id) {
    switch (id.index()) {
    case 0:
        return "null";
    case 1:
        return std::to_string(std::get<1>(id));
    case 2:
        return std::get<2>(id);
    default:
        return "unknown";
    }
}

NEKO_PROTO_API
auto make_tcp_stream_client(IPEndpoint ipendpoint) -> IoTask<IliasLengthPrefixedMessageEndpoint> {
    if (auto ret1 = co_await ilias::TcpStream::connect(ipendpoint); ret1) {
        co_return IliasLengthPrefixedMessageEndpoint(std::move(ret1.value()), JsonRpcError::InvalidRequest);
    } else {
        co_return Err(ret1.error());
    }
}

// tcp://127.0.0.1:8080
// 127.0.0.1:8080
NEKO_PROTO_API
auto make_tcp_stream_client(std::string_view url) -> IoTask<IliasLengthPrefixedMessageEndpoint> {
    std::string_view ipstr;
    if (url.substr(0, 6) == "tcp://") {
        ipstr = url.substr(6);
    } else {
        ipstr = url;
    }
    auto ipendpoint = IPEndpoint::fromString(ipstr);
    if (!ipendpoint) {
        co_return Err(ilias::IoError::InvalidArgument);
    }
    co_return co_await make_tcp_stream_client(ipendpoint.value());
}
NEKO_PROTO_API
auto make_tcp_stream_client(const char* url) -> IoTask<IliasLengthPrefixedMessageEndpoint> {
    return make_tcp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_tcp_stream_client(const std::string& url) -> IoTask<IliasLengthPrefixedMessageEndpoint> {
    return make_tcp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_udp_stream_client(IPEndpoint bindIpendpoint, IPEndpoint remoteIpendpoint)
    -> IoTask<IliasChunkedDatagramMessageEndpoint> {
    if (auto ret = ilias::Socket::make(bindIpendpoint.family(), SOCK_DGRAM, 0); ret) {
        auto socket = std::move(ret.value());
        socket.setOption(ilias::sockopt::ReuseAddress(1));
        if (auto ret1 = socket.bind(bindIpendpoint); !ret1) {
            socket.close();
            co_return Err(ret1.error());
        }
        auto udpclient = IliasUdpSocket::from(std::move(socket));
        co_return IliasChunkedDatagramMessageEndpoint(std::move(udpclient.value()), remoteIpendpoint,
                                                      JsonRpcError::MessageToolLarge);
    } else {
        co_return Err(ret.error());
    }
}

// like udp://127.0.0.1:12345-127.0.0.1:12346
// 127.0.0.1:12345-127.0.0.1:12346
NEKO_PROTO_API
auto make_udp_stream_client(std::string_view url) -> IoTask<IliasChunkedDatagramMessageEndpoint> {
    std::string_view bindRemoteIp;
    if (url.substr(0, 6) == "udp://") {
        bindRemoteIp = url.substr(6);
    } else {
        bindRemoteIp = url;
    }
    auto pos = bindRemoteIp.find('-');
    if (pos == std::string_view::npos) {
        co_return Err(ilias::IoError::InvalidArgument);
    }
    auto bindIpendpoint   = IPEndpoint::fromString(bindRemoteIp.substr(0, pos));
    auto remoteIpendpoint = IPEndpoint::fromString(bindRemoteIp.substr(pos + 1));
    if (!bindIpendpoint || !remoteIpendpoint) {
        co_return Err(ilias::IoError::InvalidArgument);
    }
    co_return co_await make_udp_stream_client(bindIpendpoint.value(), remoteIpendpoint.value());
}
NEKO_PROTO_API
auto make_udp_stream_client(const char* url) -> IoTask<IliasChunkedDatagramMessageEndpoint> {
    return make_udp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_udp_stream_client(const std::string& url) -> IoTask<IliasChunkedDatagramMessageEndpoint> {
    co_return co_await make_udp_stream_client(std::string_view(url));
}
} // namespace NEKO_NAMESPACE::detail

namespace NEKO_NAMESPACE {

JsonRpcErrorCategory& JsonRpcErrorCategory::instance() {
    static JsonRpcErrorCategory kInstance;
    return kInstance;
}

auto JsonRpcErrorCategory::message(int value) const noexcept -> std::string {
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

auto make_error_code(JsonRpcError value) noexcept -> std::error_code {
    return std::error_code(static_cast<int>(value), JsonRpcErrorCategory::instance());
}

} // namespace NEKO_NAMESPACE
