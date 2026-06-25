#include "nekoproto/jsonrpc/jsonrpc.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include <string>

namespace NEKO_NAMESPACE::detail {
using ilias::hostToNetwork;
using ilias::networkToHost;
using ilias::unstoppable;

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

auto RpcMessageEndpointWrapper<IliasUdpSocket, void>::recv(std::vector<std::byte>& buffer) -> IoTask<std::size_t> {
    if (!mClient) {
        co_return Err(JsonRpcError::ClientNotInit);
    }
    int current = buffer.size();
    buffer.resize(current + 1500);
    while (true) {
        auto ret = co_await (mClient.recvfrom({buffer.data() + current, buffer.size() - current}) | unstoppable());
        if (ret && ret.value().first + current == buffer.size()) { // UDP单个包理论上不会超过1500
            mEndpoint = ret.value().second;
            current   = buffer.size();
            buffer.resize(current + 1500);
        } else if (ret && ret.value().first + current < buffer.size()) {
            mEndpoint = ret.value().second;
            buffer.resize(ret.value().first + current);
            co_return {};
        } else {
            co_return Err(ret.error());
        }
    }
}

auto RpcMessageEndpointWrapper<IliasUdpSocket, void>::send(std::span<const std::byte> data) -> IoTask<std::size_t> {
    if (!mClient) {
        co_return Err(JsonRpcError::ClientNotInit);
    }
    if (data.size() >= 1500) {
        co_return Err(JsonRpcError::MessageToolLarge);
    }
    auto ret  = co_await (mClient.sendto(data, mEndpoint) | unstoppable());
    auto send = ret.value_or(0);
    while (ret && send < data.size()) {
        ret = co_await (mClient.sendto({data.data() + send, data.size() - send}, mEndpoint) | unstoppable());
        if (ret && ret.value() > 0) {
            send += ret.value();
        } else {
            break;
        }
    }
    if (!ret) {
        co_return Err(ret.error());
    }
    co_return {};
}

auto RpcMessageEndpointWrapper<IliasUdpSocket, void>::close() -> void {
    if (mClient) {
        mClient.close();
    }
}

auto RpcMessageEndpointWrapper<IliasUdpSocket, void>::cancel() -> void {
    if (mClient) {
        mClient.cancel();
    }
}

auto RpcMessageEndpointWrapper<IliasUdpSocket, void>::shutdown() -> ilias::IoTask<void> { co_return {}; }

auto RpcMessageEndpointWrapper<IliasUdpSocket, void>::flush() -> ilias::IoTask<void> { co_return {}; }

auto RpcMessageEndpointWrapper<IliasDynStream, void>::recv(std::vector<std::byte>& buffer) -> IoTask<std::size_t> {
    if (!mClient) {
        co_return Err(JsonRpcError::ClientNotInit);
    }
    uint32_t size = 0;
    if (auto ret = co_await (mClient.readAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) | unstoppable());
        !ret) {
        co_return Err(ret.error());
    } else if (ret.value() == sizeof(size)) {
        size = networkToHost(size);
    } else {
        mClient.close();
        co_return Err(ilias::IoError::Unknown);
    }
    if (buffer.size() < size) {
        buffer.resize(size);
    }
    auto ret = co_await (mClient.readAll({buffer.data(), size}) | unstoppable());
    if (ret && ret.value() == size) {
        co_return ret.value();
    } else {
        mClient.close();
        co_return Err(ret.error_or(ilias::IoError::Unknown));
    }
}

auto RpcMessageEndpointWrapper<IliasDynStream, void>::send(std::span<const std::byte> data) -> IoTask<std::size_t> {
    if (!mClient) {
        co_return Err(JsonRpcError::ClientNotInit);
    }
    auto size = hostToNetwork((uint32_t)data.size());
    if (auto ret = co_await (mClient.writeAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) | unstoppable());
        !ret) {
        mClient.close();
        co_return Err(ret.error());
    } else if (ret.value() != sizeof(size)) {
        mClient.close();
        co_return Err(ilias::IoError::Unknown);
    }
    auto ret = co_await (mClient.writeAll(data) | unstoppable());
    if (!ret || ret.value() != data.size()) {
        mClient.close();
        co_return Err(ret.error_or(ilias::IoError::Unknown));
    }
    co_return ret.value();
}
auto RpcMessageEndpointWrapper<IliasDynStream, void>::close() -> void {
    if (mClient) {
        mClient.close();
    }
}

auto RpcMessageEndpointWrapper<IliasDynStream, void>::cancel() -> void {
    // TODO: Ilias::DynStream has no cancel support
}

auto RpcMessageEndpointWrapper<IliasDynStream, void>::shutdown() -> ilias::IoTask<void> {
    if (mClient) {
        co_return co_await mClient.shutdown();
    }
    co_return {};
}

auto RpcMessageEndpointWrapper<IliasDynStream, void>::flush() -> ilias::IoTask<void> {
    if (mClient) {
        co_return co_await mClient.flush();
    }
    co_return {};
}

NEKO_PROTO_API
auto make_tcp_stream_client(IPEndpoint ipendpoint) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>> {
    if (auto ret1 = co_await ilias::TcpStream::connect(ipendpoint); ret1) {
        co_return RpcMessageEndpointWrapper<IliasDynStream>(std::move(ret1.value()));
    } else {
        co_return Err(ret1.error());
    }
}

// tcp://127.0.0.1:8080
// 127.0.0.1:8080
NEKO_PROTO_API
auto make_tcp_stream_client(std::string_view url) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>> {
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
auto make_tcp_stream_client(const char* url) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>> {
    return make_tcp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_tcp_stream_client(const std::string& url) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>> {
    return make_tcp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_udp_stream_client(IPEndpoint bindIpendpoint, IPEndpoint remoteIpendpoint)
    -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>> {
    if (auto ret = ilias::Socket::make(bindIpendpoint.family(), SOCK_DGRAM, 0); ret) {
        auto socket = std::move(ret.value());
        socket.setOption(ilias::sockopt::ReuseAddress(1));
        if (auto ret1 = socket.bind(bindIpendpoint); !ret1) {
            socket.close();
            co_return Err(ret1.error());
        }
        auto udpclient = IliasUdpSocket::from(std::move(socket));
        co_return RpcMessageEndpointWrapper<IliasUdpSocket>(std::move(udpclient.value()), remoteIpendpoint);
    } else {
        co_return Err(ret.error());
    }
}

// like udp://127.0.0.1:12345-127.0.0.1:12346
// 127.0.0.1:12345-127.0.0.1:12346
NEKO_PROTO_API
auto make_udp_stream_client(std::string_view url) -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>> {
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
auto make_udp_stream_client(const char* url) -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>> {
    return make_udp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_udp_stream_client(const std::string& url) -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>> {
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
