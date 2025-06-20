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

class DatagramBase {
public:
    enum class Type { Server, Client };
    /**
     * @brief check if the url is valid
     * if return true, whill use this DatagramClient to connect
     * @param url url like "tcp://127.0.0.1:8080", Can be customized, but the protocol must be put first to prevent
     * overlap
     * @return true
     * @return false
     */
    virtual auto checkProtocol(Type type, std::string_view url) -> bool = 0;
    /**
     * @brief start the client or server
     *
     * @return ILIAS_NAMESPACE::IoTask<void>
     */
    virtual auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> = 0;
    /**
     * @brief close this connect
     *
     */
    virtual auto close() -> void  = 0;
    virtual auto cancel() -> void = 0;
};

class DatagramClientBase {
public:
    virtual ~DatagramClientBase() = default;
    /**
     * @brief recv data from remote
     * must a valid jsonrpc request or response
     * @return ILIAS_NAMESPACE::IoTask<std::span<std::byte>>
     */
    virtual auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> = 0;
    /**
     * @brief send jsonrpc request or response data to remote
     * must send all or this connect will be closed
     * @param buffer
     * @return ILIAS_NAMESPACE::IoTask<void>
     */
    virtual auto send(std::span<const std::byte> buffer) -> ILIAS_NAMESPACE::IoTask<void> = 0;
    /**
     * @brief check if the connect is connected
     *
     * @return true
     * @return false
     */
    virtual auto isConnected() -> bool = 0;
};

class DatagramServerBase {
public:
    virtual ~DatagramServerBase() = default;
    /**
     * @brief listen loop
     * you must make sure it return when the server is closed.
     * @return ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void(DatagramClientBase*)>>
     */
    virtual auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> = 0;
    virtual auto isListening() -> bool                                                                 = 0;
};

template <typename T, class enable = void>
class DatagramClient;
/**
 * @brief 数据包接收，负责拆包，默认为最大长度1500的UDP包
 *
 * @tparam T
 * @tparam N
 */
template <>
class DatagramClient<ILIAS_NAMESPACE::UdpClient, void>
    : public DatagramBase, public DatagramClientBase, public DatagramServerBase {
public:
    DatagramClient() { memset(mBuffer.data(), 0, 1500); }

    auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> override {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        auto ret = co_await (mClient.recvfrom({mBuffer.data(), mBuffer.size()}, mEndpoint) |
                             ILIAS_NAMESPACE::ignoreCancellation);
        if (ret) {
            co_return std::span<std::byte>{mBuffer.data(), ret.value()};
        } else {
            co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ret.error());
        }
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<void> override {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
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

    auto close() -> void override {
        if (mClient) {
            mClient.close();
        }
        mIsCancel = false;
    }

    auto cancel() -> void override {
        mIsCancel = true;
        if (mClient) {
            mClient.cancel();
        }
    }

    // like udp://127.0.0.1:12345-127.0.0.1:12346
    auto checkProtocol([[maybe_unused]] Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "udp://");
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        mIsCancel         = false;
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
        if (auto ret = co_await ILIAS_NAMESPACE::UdpClient::make(bindIpendpoint->family()); ret) {
            mClient = std::move(ret.value());
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        mClient.setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
        if (auto ret = mClient.bind(bindIpendpoint.value()); !ret) {
            mClient.close();
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        mEndpoint = remoteIpendpoint.value();
        co_return {};
    }

    auto isConnected() -> bool override { return (bool)mClient; }
    auto isListening() -> bool override { return (bool)mClient; }

    auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> override {
        co_return std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>(this, [](DatagramClientBase*) {});
    }

private:
    ILIAS_NAMESPACE::UdpClient mClient;
    ILIAS_NAMESPACE::IPEndpoint mEndpoint;
    std::array<std::byte, 1500> mBuffer;
    bool mIsCancel = false;
};

/**
 * @brief 数据包接收，负责拆包
 *
 * @tparam T
 * @tparam N
 */
template <>
class DatagramClient<ILIAS_NAMESPACE::TcpClient, void> : public DatagramBase, public DatagramClientBase {
public:
    DatagramClient() = default;
    DatagramClient(ILIAS_NAMESPACE::TcpClient&& client) : mClient(std::move(client)) {}

    auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> override {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
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
        if (mBuffer.size() < size) {
            mBuffer.resize(size);
        }
        auto ret = co_await (mClient.readAll({mBuffer.data(), size}) | ILIAS_NAMESPACE::ignoreCancellation);
        if (ret && ret.value() == size) {
            co_return std::span<std::byte>{reinterpret_cast<std::byte*>(mBuffer.data()), size};
        } else {
            mClient.close();
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error::Unknown));
        }
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<void> override {
        if (!mClient) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
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
    auto close() -> void override {
        if (mClient) {
            mClient.close();
        }
        mIsCancel = false;
    }

    auto cancel() -> void override {
        mIsCancel = true;
        if (mClient) {
            mClient.cancel();
        }
    }

    auto checkProtocol([[maybe_unused]] Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "tcp://") && type == Type::Client;
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        mIsCancel       = false;
        auto ipendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(url.substr(6));
        if (!ipendpoint) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        if (auto ret = co_await ILIAS_NAMESPACE::TcpClient::make(ipendpoint->family()); ret) {
            if (auto ret1 = co_await ret.value().connect(ipendpoint.value()); ret1) {
                mClient = std::move(ret.value());
                co_return {};
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

    auto isConnected() -> bool override { return (bool)mClient; }

private:
    ILIAS_NAMESPACE::TcpClient mClient;
    std::vector<std::byte> mBuffer;
    bool mIsCancel = false;
};

template <>
class DatagramClient<ILIAS_NAMESPACE::TcpListener, void> : public DatagramBase, public DatagramServerBase {
public:
    DatagramClient() {}

    auto close() -> void override {
        if (mListener) {
            mListener.close();
        }
        mIsCancel = false;
    }

    auto cancel() -> void override {
        mIsCancel = true;
        if (mListener) {
            mListener.cancel();
        }
    }

    auto isListening() -> bool override { return (bool)mListener; }

    auto checkProtocol(Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "tcp://") && type == Type::Server;
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        mIsCancel       = false;
        auto ipendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(url.substr(6));
        if (!ipendpoint) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        if (auto ret = co_await ILIAS_NAMESPACE::TcpListener::make(ipendpoint->family()); ret) {
            ret.value().setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
            if (auto ret1 = ret.value().bind(ipendpoint.value()); ret1) {
                mListener = std::move(ret.value());
                co_return {};
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

    auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> override {
        if (!mListener) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        if (auto ret = co_await mListener.accept(); ret) {
            co_return std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>(
                new DatagramClient<ILIAS_NAMESPACE::TcpClient>(std::move(ret.value().first)),
                +[](DatagramClientBase* ptr) { delete ptr; });
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

private:
    ILIAS_NAMESPACE::TcpListener mListener;
    std::vector<std::byte> mBuffer;
    bool mIsCancel = false;
};

NEKO_END_NAMESPACE