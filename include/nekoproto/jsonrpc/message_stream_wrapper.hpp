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

#include <ilias/io/error.hpp>
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

NEKO_BEGIN_NAMESPACE

template <typename T>
concept MessageStreamClient = requires(T client, std::vector<std::byte>& buffer) {
    {
        client.recv(buffer) // must return a valid and complete jsonrpc request or response
    } -> std::same_as<ILIAS_NAMESPACE::IoTask<void>>;
    {
        client.send(buffer) // while provide a valid and complete jsonrpc response
    } -> std::same_as<ILIAS_NAMESPACE::IoTask<void>>;
    {
        client.close() // close the connection
    } -> std::same_as<void>;
    {
        client.cancel() // cancel current operation
    } -> std::same_as<void>;
};

template <typename T>
concept MessageStreamListener = requires(T server) {
    {
        server.accept() // must return a valid client, if return same client, the server will be closed
    };
    {
        server.close() // close the server
    } -> std::same_as<void>;
    {
        server.cancel() // cancel current operation
    } -> std::same_as<void>;
};

namespace detail {
using Error = std::error_code;
using ILIAS_NAMESPACE::IoTask;
using ILIAS_NAMESPACE::IPEndpoint;
using ILIAS_NAMESPACE::Task;
using IliasTcpClient   = ILIAS_NAMESPACE::TcpClient;
using IliasTcpListener = ILIAS_NAMESPACE::TcpListener;
using IliasUdpClient   = ILIAS_NAMESPACE::UdpClient;
using ILIAS_NAMESPACE::hostToNetwork;
using ILIAS_NAMESPACE::networkToHost;
using ILIAS_NAMESPACE::Unexpected;
using ILIAS_NAMESPACE::unstoppable;

template <typename T, class enable = void>
class MessageStream;

struct UdpStream {};
struct TcpStream {};
struct TcpListener {};
/**
 * @brief 数据包接收，负责拆包，默认为最大长度1500的UDP包
 *
 * @tparam T
 * @tparam N
 */
template <>
class NEKO_PROTO_API MessageStream<UdpStream, void> {
public:
    MessageStream(IliasUdpClient&& client, IPEndpoint endpoint) : mClient(std::move(client)), mEndpoint(endpoint) {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<void>;
    auto send(std::span<const std::byte> data) -> IoTask<void>;
    auto close() -> void;
    auto cancel() -> void;

private:
    IliasUdpClient mClient;
    IPEndpoint mEndpoint;
};

/**
 * @brief 数据包接收，负责拆包
 *
 * @tparam T
 * @tparam N
 */
template <>
class NEKO_PROTO_API MessageStream<TcpStream, void> {
public:
    MessageStream() = default;
    MessageStream(IliasTcpClient&& client) : mClient(std::move(client)) {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<void>;
    auto send(std::span<const std::byte> data) -> IoTask<void>;
    auto close() -> void;
    auto cancel() -> void;

private:
    IliasTcpClient mClient;
};

template <>
class NEKO_PROTO_API MessageStream<TcpListener, void> {
public:
    MessageStream(IliasTcpListener&& listener) : mListener(std::move(listener)) {}

    auto close() -> void;
    auto cancel() -> void;
    auto accept() -> IoTask<MessageStream<TcpStream>>;

private:
    IliasTcpListener mListener;
};

class IMessageStream {
public:
    IMessageStream()          = default;
    virtual ~IMessageStream() = default;

    virtual auto recv(std::vector<std::byte>& buffer) -> IoTask<void>    = 0;
    virtual auto send(std::span<const std::byte> buffer) -> IoTask<void> = 0;
    virtual auto close() -> void                                         = 0;
    virtual auto cancel() -> void                                        = 0;
};

template <MessageStreamClient T>
class MessageStreamWrapper : public IMessageStream {
public:
    MessageStreamWrapper(T&& client) : mClient(std::move(client)) {}
    auto recv(std::vector<std::byte>& buffer) -> IoTask<void> override { return mClient.recv(buffer); }
    auto send(std::span<const std::byte> buffer) -> IoTask<void> override { return mClient.send(buffer); }
    auto close() -> void override { return mClient.close(); }
    auto cancel() -> void override { return mClient.cancel(); }

private:
    T mClient;
};

class IMessageListener {
public:
    IMessageListener()          = default;
    virtual ~IMessageListener() = default;

    virtual auto accept() -> IoTask<std::unique_ptr<IMessageStream>> = 0;
    virtual auto close() -> void                                     = 0;
    virtual auto cancel() -> void                                    = 0;
};

template <MessageStreamListener T>
class MessageListenerWrapper : public IMessageListener {
public:
    MessageListenerWrapper(T&& listener) : mListener(std::move(listener)) {}
    auto accept() -> IoTask<std::unique_ptr<IMessageStream>> override {
        if (auto ret = co_await mListener.accept(); ret) {
            using ClientT = std::decay_t<decltype(*ret)>;
            if constexpr (std::is_same_v<ClientT, std::unique_ptr<IMessageStream>>) {
                co_return std::move(*ret);
            } else {
                co_return std::make_unique<MessageStreamWrapper<ClientT>>(std::move(*ret));
            }
        } else {
            co_return Unexpected(ret.error());
        }
    }
    auto close() -> void override { return mListener.close(); }
    auto cancel() -> void override { return mListener.cancel(); }

private:
    T mListener;
};
NEKO_PROTO_API
auto make_tcp_stream_client(IPEndpoint ipendpoint) -> IoTask<MessageStream<TcpStream>>;
// tcp://127.0.0.1:8080
// 127.0.0.1:8080
NEKO_PROTO_API
auto make_tcp_stream_client(std::string_view url) -> IoTask<MessageStream<TcpStream>>;
NEKO_PROTO_API
auto make_tcp_stream_client(const char* url) -> IoTask<MessageStream<TcpStream>>;
NEKO_PROTO_API
auto make_tcp_stream_client(const std::string& url) -> IoTask<MessageStream<TcpStream>>;
NEKO_PROTO_API
auto make_udp_stream_client(IPEndpoint bindIpendpoint, IPEndpoint remoteIpendpoint) -> IoTask<MessageStream<UdpStream>>;
// like udp://127.0.0.1:12345-127.0.0.1:12346
// 127.0.0.1:12345-127.0.0.1:12346
NEKO_PROTO_API
auto make_udp_stream_client(std::string_view url) -> IoTask<MessageStream<UdpStream>>;
NEKO_PROTO_API
auto make_udp_stream_client(const char* url) -> IoTask<MessageStream<UdpStream>>;
NEKO_PROTO_API
auto make_udp_stream_client(const std::string& url) -> IoTask<MessageStream<UdpStream>>;
NEKO_PROTO_API
auto make_tcp_stream_server(IPEndpoint ipendpoint) -> IoTask<MessageStream<TcpListener>>;
NEKO_PROTO_API
auto make_tcp_stream_server(std::string_view url) -> IoTask<MessageStream<TcpListener>>;
NEKO_PROTO_API
auto make_tcp_stream_server(const char* url) -> IoTask<MessageStream<TcpListener>>;
NEKO_PROTO_API
auto make_tcp_stream_server(const std::string& url) -> IoTask<MessageStream<TcpListener>>;
} // namespace detail

NEKO_END_NAMESPACE