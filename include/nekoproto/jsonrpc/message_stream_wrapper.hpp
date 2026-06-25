/**
 * @file datagram_wapper.hpp
 * @author llhsdmd(llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ilias/io/dyn_traits.hpp>
#include <ilias/io/error.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/net/sockopt.hpp>
#include <ilias/net/system.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/net/udp.hpp>
#include <ilias/sync/event.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/task.hpp>
#include <ilias/task/scope.hpp>
#include <system_error>

#include "ilias/defines.hpp"
#include "ilias/task/utils.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/rpc/endpoint.hpp"

NEKO_BEGIN_NAMESPACE

template <typename T>
concept MessageStreamClient = requires(T client, std::vector<std::byte>& buffer) {
    {
        client.recv(buffer) // must return a valid and complete jsonrpc request or response
    } -> std::same_as<ilias::IoTask<void>>;
    {
        client.send(buffer) // while provide a valid and complete jsonrpc response
    } -> std::same_as<ilias::IoTask<void>>;
    {
        client.close() // close the connection
    } -> std::same_as<void>;
    {
        client.cancel() // cancel current operation
    } -> std::same_as<void>;
};

namespace detail {
using ilias::IoTask;
using ilias::IPEndpoint;
using ilias::Task;
using IliasDynStream   = ilias::DynStream;
using IliasUdpSocket   = ilias::UdpSocket;
using ilias::Err;

/**
 * @brief 数据包接收，负责拆包，默认为最大长度1500的UDP包
 *
 * @tparam T
 * @tparam N
 */
template <>
class NEKO_PROTO_API RpcMessageEndpointWrapper<IliasUdpSocket, void> : public IRpcMessageEndpoint {
public:
    RpcMessageEndpointWrapper(IliasUdpSocket&& client, IPEndpoint endpoint)
        : mClient(std::move(client)), mEndpoint(endpoint) {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<void> override;
    auto send(std::span<const std::byte> data) -> IoTask<void> override;
    auto close() -> void override;
    auto cancel() -> void override;

private:
    IliasUdpSocket mClient;
    IPEndpoint mEndpoint;
};

/**
 * @brief 数据包接收，负责拆包
 *
 * @tparam T
 * @tparam N
 */
template <>
class NEKO_PROTO_API RpcMessageEndpointWrapper<IliasDynStream, void> : public IRpcMessageEndpoint {
public:
    RpcMessageEndpointWrapper() = default;
    RpcMessageEndpointWrapper(IliasDynStream&& client) : mClient(std::move(client)) {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<void> override;
    auto send(std::span<const std::byte> data) -> IoTask<void> override;
    auto close() -> void override;
    auto cancel() -> void override;

private:
    IliasDynStream mClient;
};

NEKO_PROTO_API
auto make_tcp_stream_client(IPEndpoint ipendpoint) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>>;
// tcp://127.0.0.1:8080
// 127.0.0.1:8080
NEKO_PROTO_API
auto make_tcp_stream_client(std::string_view url) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>>;
NEKO_PROTO_API
auto make_tcp_stream_client(const char* url) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>>;
NEKO_PROTO_API
auto make_tcp_stream_client(const std::string& url) -> IoTask<RpcMessageEndpointWrapper<IliasDynStream>>;
NEKO_PROTO_API
auto make_udp_stream_client(IPEndpoint bindIpendpoint, IPEndpoint remoteIpendpoint)
    -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>>;
// like udp://127.0.0.1:12345-127.0.0.1:12346
// 127.0.0.1:12345-127.0.0.1:12346
NEKO_PROTO_API
auto make_udp_stream_client(std::string_view url) -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>>;
NEKO_PROTO_API
auto make_udp_stream_client(const char* url) -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>>;
NEKO_PROTO_API
auto make_udp_stream_client(const std::string& url) -> IoTask<RpcMessageEndpointWrapper<IliasUdpSocket>>;
} // namespace detail

NEKO_END_NAMESPACE