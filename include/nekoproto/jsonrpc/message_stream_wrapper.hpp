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
    MessageStream(ILIAS_NAMESPACE::UdpClient&& client, ILIAS_NAMESPACE::IPEndpoint endpoint)
        : mClient(std::move(client)), mEndpoint(endpoint) {}

    auto recv(std::vector<std::byte>& buffer) -> ILIAS_NAMESPACE::IoTask<> {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        int current = buffer.size();
        buffer.resize(current + 1500);
        while (true) {
            auto ret = co_await (mClient.recvfrom({buffer.data() + current, buffer.size() - current}, mEndpoint) |
                                 ILIAS_NAMESPACE::ignoreCancellation);
            if (ret && ret.value() + current == buffer.size()) { // UDP单个包理论上不会超过1500
                current = buffer.size();
                buffer.resize(current + 1500);
            } else if (ret && ret.value() + current < buffer.size()) {
                buffer.resize(ret.value() + current);
                co_return {};
            } else {
                co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ret.error());
            }
        }
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<> {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (data.size() >= 1500) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::MessageToolLarge);
        }
        auto ret  = co_await (mClient.sendto(data, mEndpoint) | ILIAS_NAMESPACE::ignoreCancellation);
        auto send = ret.value_or(0);
        while (ret && send < data.size()) {
            ret = co_await (mClient.sendto({data.data() + send, data.size() - send}, mEndpoint) |
                            ILIAS_NAMESPACE::ignoreCancellation);
            if (ret && ret.value() > 0) {
                send += ret.value();
            } else {
                break;
            }
        }
        if (!ret) {
            co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ret.error());
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
    ILIAS_NAMESPACE::UdpClient mClient;
    ILIAS_NAMESPACE::IPEndpoint mEndpoint;
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
    MessageStream(ILIAS_NAMESPACE::TcpClient&& client) : mClient(std::move(client)) {}

    auto recv(std::vector<std::byte>& buffer) -> ILIAS_NAMESPACE::IoTask<> {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        uint32_t size = 0;
        if (auto ret = co_await (mClient.readAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) |
                                 ILIAS_NAMESPACE::ignoreCancellation);
            !ret) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        } else if (ret.value() == sizeof(size)) {
            size = ILIAS_NAMESPACE::networkToHost(size);
        } else {
            mClient.close();
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Unknown);
        }
        if (buffer.size() < size) {
            buffer.resize(size);
        }
        auto ret = co_await (mClient.readAll({buffer.data(), size}) | ILIAS_NAMESPACE::ignoreCancellation);
        if (ret && ret.value() == size) {
            co_return {};
        } else {
            mClient.close();
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error::Unknown));
        }
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<void> {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        auto size = ILIAS_NAMESPACE::hostToNetwork((uint32_t)data.size());
        if (auto ret = co_await (mClient.writeAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) |
                                 ILIAS_NAMESPACE::ignoreCancellation);
            !ret) {
            mClient.close();
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        } else if (ret.value() != sizeof(size)) {
            mClient.close();
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Unknown);
        }
        auto ret = co_await (mClient.writeAll(data) | ILIAS_NAMESPACE::ignoreCancellation);
        if (!ret || ret.value() != data.size()) {
            mClient.close();
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error::Unknown));
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
    ILIAS_NAMESPACE::TcpClient mClient;
};

template <>
class MessageStream<TcpListener, void> {
public:
    MessageStream(ILIAS_NAMESPACE::TcpListener&& listener) : mListener(std::move(listener)) {}

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

    auto accept() -> ILIAS_NAMESPACE::IoTask<MessageStream<TcpStream>> {
        if (!mListener) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (auto ret = co_await mListener.accept(); ret) {
            co_return MessageStream<TcpStream>(std::move(ret.value().first));
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

private:
    ILIAS_NAMESPACE::TcpListener mListener;
};

// tcp://127.0.0.1:8080
auto make_tcp_stream_client(std::string_view url) -> ILIAS_NAMESPACE::IoTask<MessageStream<TcpStream>> {
    auto ipendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(url.substr(6));
    if (!ipendpoint) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
    }
    if (auto ret = co_await ILIAS_NAMESPACE::TcpClient::make(ipendpoint->family()); ret) {
        if (auto ret1 = co_await ret.value().connect(ipendpoint.value()); ret1) {
            co_return MessageStream<TcpStream>(std::move(ret.value()));
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
        }
    } else {
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
}

// like udp://127.0.0.1:12345-127.0.0.1:12346
auto make_udp_stream_client(std::string_view url) -> ILIAS_NAMESPACE::IoTask<MessageStream<UdpStream>> {
    auto bindRemoteIp = url.substr(6);
    auto pos          = bindRemoteIp.find('-');
    if (pos == std::string_view::npos) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
    }
    auto bindIpendpoint   = ILIAS_NAMESPACE::IPEndpoint::fromString(bindRemoteIp.substr(0, pos));
    auto remoteIpendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(bindRemoteIp.substr(pos + 1));
    if (!bindIpendpoint || !remoteIpendpoint) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
    }
    ILIAS_NAMESPACE::UdpClient client;
    if (auto ret = co_await ILIAS_NAMESPACE::UdpClient::make(bindIpendpoint->family()); ret) {
        client = std::move(ret.value());
    } else {
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    client.setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
    if (auto ret = client.bind(bindIpendpoint.value()); !ret) {
        client.close();
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    co_return MessageStream<UdpStream>(std::move(client), remoteIpendpoint.value());
}

auto make_tcp_stream_server(std::string_view url) -> ILIAS_NAMESPACE::IoTask<MessageStream<TcpListener>> {
    auto ipendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(url.substr(6));
    if (!ipendpoint) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
    }
    if (auto ret = co_await ILIAS_NAMESPACE::TcpListener::make(ipendpoint->family()); ret) {
        ret.value().setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
        if (auto ret1 = ret.value().bind(ipendpoint.value()); ret1) {
            co_return MessageStream<TcpListener>(std::move(ret.value()));
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
        }
    } else {
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
}

class IMessageStream {
public:
    IMessageStream()          = default;
    virtual ~IMessageStream() = default;

    virtual auto recv(std::vector<std::byte>& buffer) -> ILIAS_NAMESPACE::IoTask<>    = 0;
    virtual auto send(std::span<const std::byte> buffer) -> ILIAS_NAMESPACE::IoTask<> = 0;
    virtual auto close() -> void                                                      = 0;
    virtual auto cancel() -> void                                                     = 0;
};

template <MessageStreamClient T>
class MessageStreamWrapper : public IMessageStream {
public:
    MessageStreamWrapper(T&& client) : mClient(std::move(client)) {}
    auto recv(std::vector<std::byte>& buffer) -> ILIAS_NAMESPACE::IoTask<> override { return mClient.recv(buffer); }
    auto send(std::span<const std::byte> buffer) -> ILIAS_NAMESPACE::IoTask<> override { return mClient.send(buffer); }
    auto close() -> void override { return mClient.close(); }
    auto cancel() -> void override { return mClient.cancel(); }

private:
    T mClient;
};

class IMessageListener {
public:
    IMessageListener()          = default;
    virtual ~IMessageListener() = default;

    virtual auto accept() -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<IMessageStream>> = 0;
    virtual auto close() -> void                                                      = 0;
    virtual auto cancel() -> void                                                     = 0;
};

template <MessageStreamListener T>
class MessageListenerWrapper : public IMessageListener {
public:
    MessageListenerWrapper(T&& listener) : mListener(std::move(listener)) {}
    auto accept() -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<IMessageStream>> override {
        if (auto ret = co_await mListener.accept(); ret) {
            using ClientT = std::decay_t<decltype(*ret)>;
            if constexpr (std::is_same_v<ClientT, std::unique_ptr<IMessageStream>>) {
                co_return std::move(*ret);
            } else {
                co_return std::make_unique<MessageStreamWrapper<ClientT>>(std::move(*ret));
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }
    auto close() -> void override { return mListener.close(); }
    auto cancel() -> void override { return mListener.cancel(); }

private:
    T mListener;
};
} // namespace detail

NEKO_END_NAMESPACE