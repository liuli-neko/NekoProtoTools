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

#include <ilias/error.hpp>
#include <ilias/net/sockopt.hpp>
#include <ilias/net/system.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/net/udp.hpp>
#include <ilias/sync/event.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/sync/scope.hpp>
#include <ilias/task.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"

NEKO_BEGIN_NAMESPACE

template <typename T>
concept MessageStreamClient = requires(T client, std::vector<std::byte>& buffer) {
    {
        client.recv(buffer) // must return a valid and complete jsonrpc request or response
    } -> std::same_as<ILIAS_NAMESPACE::IoTask<>>;
    {
        client.send(buffer) // while provide a valid and complete jsonrpc response
    } -> std::same_as<ILIAS_NAMESPACE::IoTask<>>;
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
using ILIAS_NAMESPACE::Error;
using ILIAS_NAMESPACE::IoTask;
using ILIAS_NAMESPACE::IPEndpoint;
using ILIAS_NAMESPACE::Task;
using IliasTcpClient   = ILIAS_NAMESPACE::TcpClient;
using IliasTcpListener = ILIAS_NAMESPACE::TcpListener;
using IliasUdpClient   = ILIAS_NAMESPACE::UdpClient;
using ILIAS_NAMESPACE::hostToNetwork;
using ILIAS_NAMESPACE::ignoreCancellation;
using ILIAS_NAMESPACE::networkToHost;
using ILIAS_NAMESPACE::Unexpected;

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
class MessageStream<UdpStream, void> {
public:
    MessageStream(IliasUdpClient&& client, IPEndpoint endpoint) : mClient(std::move(client)), mEndpoint(endpoint) {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<> {
        if (!mClient) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        int current = buffer.size();
        buffer.resize(current + 1500);
        while (true) {
            auto ret = co_await (mClient.recvfrom({buffer.data() + current, buffer.size() - current}, mEndpoint) |
                                 ignoreCancellation);
            if (ret && ret.value() + current == buffer.size()) { // UDP单个包理论上不会超过1500
                current = buffer.size();
                buffer.resize(current + 1500);
            } else if (ret && ret.value() + current < buffer.size()) {
                buffer.resize(ret.value() + current);
                co_return {};
            } else {
                co_return Unexpected<Error>(ret.error());
            }
        }
    }

    auto send(std::span<const std::byte> data) -> IoTask<> {
        if (!mClient) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        if (data.size() >= 1500) {
            co_return Unexpected(JsonRpcError::MessageToolLarge);
        }
        auto ret  = co_await (mClient.sendto(data, mEndpoint) | ignoreCancellation);
        auto send = ret.value_or(0);
        while (ret && send < data.size()) {
            ret = co_await (mClient.sendto({data.data() + send, data.size() - send}, mEndpoint) | ignoreCancellation);
            if (ret && ret.value() > 0) {
                send += ret.value();
            } else {
                break;
            }
        }
        if (!ret) {
            co_return Unexpected<Error>(ret.error());
        }
        co_return {};
    }

    auto close() -> void {
        if (mClient) {
            mClient.close();
        }
    }

    auto cancel() -> void {
        if (mClient) {
            mClient.cancel();
        }
    }

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
class MessageStream<TcpStream, void> {
public:
    MessageStream() = default;
    MessageStream(IliasTcpClient&& client) : mClient(std::move(client)) {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<> {
        if (!mClient) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        uint32_t size = 0;
        if (auto ret =
                co_await (mClient.readAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) | ignoreCancellation);
            !ret) {
            co_return Unexpected(ret.error());
        } else if (ret.value() == sizeof(size)) {
            size = networkToHost(size);
        } else {
            mClient.close();
            co_return Unexpected(Error::Unknown);
        }
        if (buffer.size() < size) {
            buffer.resize(size);
        }
        auto ret = co_await (mClient.readAll({buffer.data(), size}) | ignoreCancellation);
        if (ret && ret.value() == size) {
            co_return {};
        } else {
            mClient.close();
            co_return Unexpected(ret.error_or(Error::Unknown));
        }
    }

    auto send(std::span<const std::byte> data) -> IoTask<void> {
        if (!mClient) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        auto size = hostToNetwork((uint32_t)data.size());
        if (auto ret =
                co_await (mClient.writeAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) | ignoreCancellation);
            !ret) {
            mClient.close();
            co_return Unexpected(ret.error());
        } else if (ret.value() != sizeof(size)) {
            mClient.close();
            co_return Unexpected(Error::Unknown);
        }
        auto ret = co_await (mClient.writeAll(data) | ignoreCancellation);
        if (!ret || ret.value() != data.size()) {
            mClient.close();
            co_return Unexpected(ret.error_or(Error::Unknown));
        }
        co_return {};
    }
    auto close() -> void {
        if (mClient) {
            mClient.close();
        }
    }

    auto cancel() -> void {
        if (mClient) {
            mClient.cancel();
        }
    }

private:
    IliasTcpClient mClient;
};

template <>
class MessageStream<TcpListener, void> {
public:
    MessageStream(IliasTcpListener&& listener) : mListener(std::move(listener)) {}

    auto close() -> void {
        if (mListener) {
            mListener.close();
        }
    }

    auto cancel() -> void {
        if (mListener) {
            mListener.cancel();
        }
    }

    auto accept() -> IoTask<MessageStream<TcpStream>> {
        if (!mListener) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        if (auto ret = co_await mListener.accept(); ret) {
            co_return MessageStream<TcpStream>(std::move(ret.value().first));
        } else {
            co_return Unexpected(ret.error());
        }
    }

private:
    IliasTcpListener mListener;
};

auto make_tcp_stream_client(IPEndpoint ipendpoint) -> IoTask<MessageStream<TcpStream>> {
    if (auto ret = co_await IliasTcpClient::make(ipendpoint.family()); ret) {
        if (auto ret1 = co_await ret.value().connect(ipendpoint); ret1) {
            co_return MessageStream<TcpStream>(std::move(ret.value()));
        } else {
            co_return Unexpected(ret1.error());
        }
    } else {
        co_return Unexpected(ret.error());
    }
}

// tcp://127.0.0.1:8080
// 127.0.0.1:8080
auto make_tcp_stream_client(std::string_view url) -> IoTask<MessageStream<TcpStream>> {
    std::string_view ipstr;
    if (url.substr(0, 6) == "tcp://") {
        ipstr = url.substr(6);
    } else {
        ipstr = url;
    }
    auto ipendpoint = IPEndpoint::fromString(ipstr);
    if (!ipendpoint) {
        co_return Unexpected(Error::InvalidArgument);
    }
    co_return co_await make_tcp_stream_client(ipendpoint.value());
}

auto make_tcp_stream_client(const char* url) -> IoTask<MessageStream<TcpStream>> {
    return make_tcp_stream_client(std::string_view(url));
}

auto make_tcp_stream_client(const std::string& url) -> IoTask<MessageStream<TcpStream>> {
    return make_tcp_stream_client(std::string_view(url));
}

auto make_udp_stream_client(IPEndpoint bindIpendpoint, IPEndpoint remoteIpendpoint)
    -> IoTask<MessageStream<UdpStream>> {
    IliasUdpClient client;
    if (auto ret = co_await IliasUdpClient::make(bindIpendpoint.family()); ret) {
        client = std::move(ret.value());
    } else {
        co_return Unexpected(ret.error());
    }
    client.setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
    if (auto ret = client.bind(bindIpendpoint); !ret) {
        client.close();
        co_return Unexpected(ret.error());
    }
    co_return MessageStream<UdpStream>(std::move(client), remoteIpendpoint);
}

// like udp://127.0.0.1:12345-127.0.0.1:12346
// 127.0.0.1:12345-127.0.0.1:12346
auto make_udp_stream_client(std::string_view url) -> IoTask<MessageStream<UdpStream>> {
    std::string_view bindRemoteIp;
    if (url.substr(0, 6) == "udp://") {
        bindRemoteIp = url.substr(6);
    } else {
        bindRemoteIp = url;
    }
    auto pos = bindRemoteIp.find('-');
    if (pos == std::string_view::npos) {
        co_return Unexpected(Error::InvalidArgument);
    }
    auto bindIpendpoint   = IPEndpoint::fromString(bindRemoteIp.substr(0, pos));
    auto remoteIpendpoint = IPEndpoint::fromString(bindRemoteIp.substr(pos + 1));
    if (!bindIpendpoint || !remoteIpendpoint) {
        co_return Unexpected(Error::InvalidArgument);
    }
    co_return co_await make_udp_stream_client(bindIpendpoint.value(), remoteIpendpoint.value());
}

auto make_udp_stream_client(const char* url) -> IoTask<MessageStream<UdpStream>> {
    return make_udp_stream_client(std::string_view(url));
}

auto make_udp_stream_client(const std::string& url) -> IoTask<MessageStream<UdpStream>> {
    co_return co_await make_udp_stream_client(std::string_view(url));
}

auto make_tcp_stream_server(IPEndpoint ipendpoint) -> IoTask<MessageStream<TcpListener>> {
    if (auto ret = co_await IliasTcpListener::make(ipendpoint.family()); ret) {
        ret.value().setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
        if (auto ret1 = ret.value().bind(ipendpoint); ret1) {
            co_return MessageStream<TcpListener>(std::move(ret.value()));
        } else {
            co_return Unexpected(ret1.error());
        }
    } else {
        co_return Unexpected(ret.error());
    }
}

auto make_tcp_stream_server(std::string_view url) -> IoTask<MessageStream<TcpListener>> {
    std::string_view ipstr;
    if (url.substr(0, 6) == "tcp://") {
        ipstr = url.substr(6);
    } else {
        ipstr = url;
    }
    auto ipendpoint = IPEndpoint::fromString(ipstr);
    if (!ipendpoint) {
        co_return Unexpected(Error::InvalidArgument);
    }
    co_return co_await make_tcp_stream_server(ipendpoint.value());
}

auto make_tcp_stream_server(const char* url) -> IoTask<MessageStream<TcpListener>> {
    return make_tcp_stream_server(std::string_view(url));
}
auto make_tcp_stream_server(const std::string& url) -> IoTask<MessageStream<TcpListener>> {
    return make_tcp_stream_server(std::string_view(url));
}
class IMessageStream {
public:
    IMessageStream()          = default;
    virtual ~IMessageStream() = default;

    virtual auto recv(std::vector<std::byte>& buffer) -> IoTask<>    = 0;
    virtual auto send(std::span<const std::byte> buffer) -> IoTask<> = 0;
    virtual auto close() -> void                                     = 0;
    virtual auto cancel() -> void                                    = 0;
};

template <MessageStreamClient T>
class MessageStreamWrapper : public IMessageStream {
public:
    MessageStreamWrapper(T&& client) : mClient(std::move(client)) {}
    auto recv(std::vector<std::byte>& buffer) -> IoTask<> override { return mClient.recv(buffer); }
    auto send(std::span<const std::byte> buffer) -> IoTask<> override { return mClient.send(buffer); }
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
} // namespace detail

NEKO_END_NAMESPACE