#include <gtest/gtest.h>
#include <random>
#include <sstream>

#include "ilias/defines.hpp"
#include "ilias/net/sockfd.hpp"
#include "ilias/net/udp.hpp"
#include "nekoproto/communication/communication_base.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/types/vector.hpp" // IWYU pragma: export

#include <ilias/io/error.hpp>
#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <ilias/task/when_all.hpp>
#include <system_error>

NEKO_USE_NAMESPACE

using ILIAS_NAMESPACE::IoContext;
using ILIAS_NAMESPACE::IPEndpoint;
using ILIAS_NAMESPACE::TcpClient;
using ILIAS_NAMESPACE::TcpListener;
using ILIAS_NAMESPACE::UdpClient;
using ILIAS_NAMESPACE::Unexpected;
using ILIAS_NAMESPACE::sockopt::TcpNoDelay;

class Message {
public:
    uint64_t timestamp;
    std::string msg;
    std::vector<int> numbers;

    NEKO_SERIALIZER(timestamp, msg, numbers);
    NEKO_DECLARE_PROTOCOL(Message, JsonSerializer)
};

std::string generate_random_string(std::size_t length) {
    static const char kAlphanum[] = "0123456789"
                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz";
    std::string str;
    str.reserve(length);

    std::random_device rd;        // 获取随机数种子
    std::mt19937 generator(rd()); // 采用梅森旋转算法
    std::uniform_int_distribution<> distribution(0, sizeof(kAlphanum) - 2);
    for (std::size_t i = 0; i < length; ++i) {
        str += kAlphanum[distribution(generator)];
    }
    return str;
}

// contains to string
template <typename T>
std::string to_string(const T& values) {
    std::ostringstream ss;
    for (auto& value : values) {
        ss << value << " ";
    }
    return ss.str();
}

ILIAS_NAMESPACE::IoTask<void>
client_loop([[maybe_unused]] ILIAS_NAMESPACE::IoContext& ioContext,
            ProtoFactory& protoFactory, // NOLINT(readability-function-cognitive-complexity)
            StreamFlag sendFlag, StreamFlag recvFlag) {
    NEKO_LOG_DEBUG("unit test", "Client connect to service");
    auto ret = co_await TcpClient::connect(IPEndpoint("127.0.0.1", 10345));
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    ret.value().setOption(TcpNoDelay(1));
    ProtoStreamClient<TcpClient> client(protoFactory, std::move(ret.value()));
    NEKO_LOG_DEBUG("unit test", "Client loop started");
    const std::size_t desiredSize = 10 * 1024; // 1MB
    int count                     = 10;
    while (count-- > 0) {
        NEKO_LOG_DEBUG("unit test", "start {}th send test", count);
        Message msg;
        msg.msg       = generate_random_string(desiredSize);
        msg.timestamp = time(NULL);
        msg.numbers   = (std::vector<int>{1, 2, 3, 5});
        auto ret1     = co_await client.send(msg.makeProto(), sendFlag);
        if (!ret1) {
            NEKO_LOG_DEBUG("unit test", "send failed: {}", ret1.error().message());
            co_return Unexpected(ret1.error());
        }
        NEKO_LOG_DEBUG("unit test", "send message: timestamp: {}, msg: {} numbers: {}", msg.timestamp,
                       msg.msg.substr(0, 20) + "...", to_string(msg.numbers));
        auto ret2 = co_await client.recv(recvFlag);
        if (!ret2) {
            NEKO_LOG_DEBUG("unit test", "recv failed: {}", ret2.error().message());
            co_return Unexpected(ret2.error());
        }
        IProto retMsg(std::move(ret2.value()));
        auto* msg1 = retMsg.cast<Message>();
        if (msg1 != nullptr) {
            NEKO_LOG_DEBUG("unit test", "recv success: timestamp: {} msg: {} numbers: {}", msg1->timestamp,
                           msg1->msg.substr(0, 20) + "...", to_string(msg1->numbers));
        } else {
            EXPECT_TRUE(false);
        }
    }
    if (static_cast<int>(sendFlag & StreamFlag::VersionVerification) != 0) {
        const auto& protoTable = client.getProtoTable();
        EXPECT_EQ(protoTable.protocolFactoryVersion, 1);
        EXPECT_EQ(protoTable.protoTable.size(), 1);
        EXPECT_STREQ(protoTable.protoTable.begin()->second.c_str(), "Message");
    }
    co_return co_await client.close();
}

ILIAS_NAMESPACE::IoTask<void> handle_loop(ProtoStreamClient<TcpClient>&& pClient,
                                          StreamFlag sendFlag, // NOLINT(readability-function-cognitive-complexity)
                                          StreamFlag recvFlag) {
    ProtoStreamClient<TcpClient> client(std::move(pClient));
    const std::size_t desiredSize = 10 * 1024; // 1MB
    while (true) {
        NEKO_LOG_DEBUG("unit test", "HandleLoop");
        auto ret = co_await client.recv(recvFlag);
        if (!ret) {
            NEKO_LOG_DEBUG("unit test", "recv failed: {}", ret.error().message());
            co_await client.close();
            co_return Unexpected(ret.error());
        }
        auto* msg = ret->cast<Message>();
        if (msg != nullptr) {
            NEKO_LOG_DEBUG("unit test", "recv success: timestamp: {}  msg: {}  numbers: {}", msg->timestamp,
                           msg->msg.substr(0, 20) + "...", to_string(msg->numbers));
        } else {
            EXPECT_TRUE(false);
        }
        Message msg1;
        msg1.msg       = generate_random_string(desiredSize);
        msg1.timestamp = (time(NULL));
        msg1.numbers   = (std::vector<int>{1, 2, 3});
        auto ret1      = co_await client.send(msg->makeProto(), sendFlag);
        if (!ret1) {
            co_await client.close();
            co_return Unexpected(ret1.error());
        }
        NEKO_LOG_DEBUG("unit test", "send message: timestamp: {}, msg: {} numbers: {}", msg1.timestamp,
                       msg1.msg.substr(0, 20) + "...", to_string(msg1.numbers));
    }
    if (static_cast<int>(sendFlag & StreamFlag::VersionVerification) != 0) {
        const auto& protoTable = client.getProtoTable();
        EXPECT_EQ(protoTable.protocolFactoryVersion, 1);
        EXPECT_EQ(protoTable.protoTable.size(), 1);
        EXPECT_STREQ(protoTable.protoTable.begin()->second.c_str(), "Message");
    }
    co_return co_await client.close();
}

ILIAS_NAMESPACE::IoTask<void> server_loop([[maybe_unused]] IoContext& ioContext, ProtoFactory& protoFactor,
                                          StreamFlag sendFlag, StreamFlag recvFlag) {
    NEKO_LOG_DEBUG("unit test", "serverLoop");
    auto retl = co_await TcpListener::bind(IPEndpoint("127.0.0.1", 10345));
    if (!retl) {
        NEKO_LOG_DEBUG("unit test", "bind failed: {}", retl.error().message());
        co_return Unexpected(retl.error());
    }
    auto ret = co_await retl.value().accept();
    if (!ret) {
        NEKO_LOG_DEBUG("unit test", "accept failed: {}", ret.error().message());
        co_return Unexpected(ret.error());
    }
    NEKO_LOG_DEBUG("unit test", "accept successed");
    ret.value().first.setOption(TcpNoDelay(1));
    auto ret1 = co_await handle_loop(ProtoStreamClient<ilias::TcpClient>(protoFactor, std::move(ret.value().first)),
                                     sendFlag, recvFlag);
    if (!ret1 && ret1.error() != ILIAS_NAMESPACE::IoError::ConnectionReset) {
        co_return Unexpected(ret1.error());
    }
    co_return {};
}

ILIAS_NAMESPACE::IoTask<void> test(IoContext& ioContext, ProtoFactory& protoFactory, StreamFlag sendFlag,
                                   StreamFlag recvFlag) {
    auto [ret1, ret2] = co_await whenAll(server_loop(ioContext, protoFactory, sendFlag, recvFlag),
                                         client_loop(ioContext, protoFactory, sendFlag, recvFlag));
    EXPECT_FALSE((!ret1 && ret1.error() != ILIAS_NAMESPACE::IoError::ConnectionReset) || (!ret2));
    co_return {};
}

ILIAS_NAMESPACE::IoTask<void> udp_client([[maybe_unused]] IoContext& ioContext, ProtoFactory& protoFactory,
                                         StreamFlag sendFlags, StreamFlag recvFlags, const IPEndpoint& bindPoint,
                                         const IPEndpoint& endpoint) {
    NEKO_LOG_DEBUG("unit test", "testing udp client ...");
    auto udpclient = ILIAS_NAMESPACE::Socket::make(AF_INET, SOCK_DGRAM, 0).value();
#ifdef _WIN32
    auto fd = udpclient.get();
    // 关闭SIO_UDP_CONNRESET
    DWORD optionValue = FALSE; // 禁用连接重置
    int optionLength  = sizeof(optionValue);
    DWORD returnValue;
    int returnLength = sizeof(returnValue);
    DWORD lpcbBytesReturned;
    auto wsaRet = WSAIoctl(fd, SIO_UDP_CONNRESET, &optionValue, optionLength, &returnValue, returnLength,
                           &lpcbBytesReturned, nullptr, nullptr);
    if (wsaRet == SOCKET_ERROR) {
        NEKO_LOG_DEBUG("unit test", "WSAIoctl fd {} failed: {}", fd,
                       ILIAS_NAMESPACE::SystemError::fromErrno().toString());
    }
#endif
    auto ret = udpclient.bind(bindPoint);
    if (!ret) {
        NEKO_LOG_DEBUG("unit test", "udp bind failed: {}", ret.error().message());
        co_return Unexpected(ret.error());
    }
    ProtoDatagramClient<UdpClient> client(protoFactory, UdpClient::from(std::move(udpclient)).value());
    int count = 10;
    while ((count--) != 0) {
        NEKO_LOG_DEBUG("unit test", "{}th testing...", count);
        Message msg1;
        msg1.msg       = ("this is a message from server " + std::to_string(count));
        msg1.timestamp = (time(NULL));
        msg1.numbers   = (std::vector<int>{1, 2, 3});
        auto ret2      = co_await client.send(msg1.makeProto(), endpoint, sendFlags);

        if (!ret2) {
            NEKO_LOG_DEBUG("unit test", "send failed: {}", ret2.error().message());
            co_return Unexpected(ret2.error());
        }

        auto ret1 = co_await client.recv(recvFlags);
        if (!ret1) {
            NEKO_LOG_DEBUG("unit test", "recv failed: {}", ret1.error().message());
            co_return Unexpected(ret1.error());
        }
        IProto msg(std::move(ret1.value().first));
        auto* proto = msg.cast<Message>();
        NEKO_LOG_DEBUG("unit test", "recv successed: {}, from: {}, count: {}", proto->msg,
                       ret1.value().second.toString(), count);
    }
    if (static_cast<int>(sendFlags & StreamFlag::VersionVerification) != 0) {
        const auto& protoTable = client.getProtoTable();
        EXPECT_EQ(protoTable.protocolFactoryVersion, 1);
        EXPECT_EQ(protoTable.protoTable.size(), 1);
        EXPECT_STREQ(protoTable.protoTable.begin()->second.c_str(), "Message");
    }
    co_await client.close();
    NEKO_LOG_DEBUG("unit test", "udp test finished");
    co_return {};
}

ILIAS_NAMESPACE::IoTask<void> udp_client_peer([[maybe_unused]] IoContext& ioContext, ProtoFactory& protoFactory,
                                              StreamFlag sendFlags, StreamFlag recvFlags, const IPEndpoint& bindPoint,
                                              const IPEndpoint& endpoint) {
    NEKO_LOG_DEBUG("unit test", "testing udp client ...");
    auto udpclient = ILIAS_NAMESPACE::Socket::make(AF_INET, SOCK_DGRAM, 0).value();
#ifdef _WIN32
    auto fd = udpclient.get();
    // 关闭SIO_UDP_CONNRESET
    DWORD optionValue = FALSE; // 禁用连接重置
    int optionLength  = sizeof(optionValue);
    DWORD returnValue;
    int returnLength = sizeof(returnValue);
    DWORD lpcbBytesReturned;
    auto wsaRet = WSAIoctl(fd, SIO_UDP_CONNRESET, &optionValue, optionLength, &returnValue, returnLength,
                           &lpcbBytesReturned, nullptr, nullptr);
    if (wsaRet == SOCKET_ERROR) {
        NEKO_LOG_DEBUG("unit test", "WSAIoctl fd {} failed: {}", fd,
                       ILIAS_NAMESPACE::SystemError::fromErrno().toString());
    }
#endif
    auto ret = udpclient.bind(bindPoint);
    if (!ret) {
        NEKO_LOG_DEBUG("unit test", "udp bind failed: {}", ret.error().message());
        co_return Unexpected(ret.error());
    }
    ProtoDatagramClient<UdpClient> client(protoFactory, UdpClient::from(std::move(udpclient)).value());
    int count = 10;
    while ((count--) != 0) {
        NEKO_LOG_DEBUG("unit test", "{}th testing...", count);
        auto ret1 = co_await client.recv(recvFlags);
        if (!ret1) {
            NEKO_LOG_DEBUG("unit test", "recv failed: {}", ret1.error().message());
            co_return Unexpected(ret1.error());
        }
        IProto msg(std::move(ret1.value().first));
        auto* proto = msg.cast<Message>();
        NEKO_LOG_DEBUG("unit test", "recv successed: {}, from: {}, count: {}", proto->msg,
                       ret1.value().second.toString(), count);
        Message msg1;
        msg1.msg       = ("this is a message from server " + std::to_string(count));
        msg1.timestamp = (time(NULL));
        msg1.numbers   = (std::vector<int>{1, 2, 3});
        auto ret2      = co_await client.send(msg1.makeProto(), endpoint, sendFlags);

        if (!ret2) {
            NEKO_LOG_DEBUG("unit test", "send failed: {}", ret2.error().message());
            co_return Unexpected(ret2.error());
        }
    }
    if (static_cast<int>(sendFlags & StreamFlag::VersionVerification) != 0) {
        const auto& protoTable = client.getProtoTable();
        EXPECT_EQ(protoTable.protocolFactoryVersion, 1);
        EXPECT_EQ(protoTable.protoTable.size(), 1);
        EXPECT_STREQ(protoTable.protoTable.begin()->second.c_str(), "Message");
    }
    co_await client.close();
    NEKO_LOG_DEBUG("unit test", "udp test finished");
    co_return {};
}

ILIAS_NAMESPACE::IoTask<void> udp_test(IoContext& ioContext, ProtoFactory& protoFactory, StreamFlag sendFlags,
                                       StreamFlag recvFlags) {
    uint16_t port1 = (rand() % 1000) + 10000;
    uint16_t port2 = (rand() % 1000) + 10000;
    auto [ret1, ret2] =
        co_await whenAll(udp_client_peer(ioContext, protoFactory, sendFlags, recvFlags, IPEndpoint("127.0.0.1", port1),
                                         IPEndpoint("127.0.0.1", port2)),
                         udp_client(ioContext, protoFactory, sendFlags, recvFlags, IPEndpoint("127.0.0.1", port2),
                                    IPEndpoint("127.0.0.1", port1)));
    if (!ret1) {
        NEKO_LOG_DEBUG("unit test", "udp test failed: {}", ret1.error().message());
    }
    if (!ret2) {
        NEKO_LOG_DEBUG("unit test", "udp test failed: {}", ret2.error().message());
    }
    co_return {};
}

class Communication : public ::testing::Test {
public:
    void SetUp() override { ioContext.install(); }
    void TearDown() override {}
    ILIAS_NAMESPACE::PlatformContext ioContext;
    ProtoFactory protoFactory;
};

TEST_F(Communication, VerifyAndThreadAndSlice) {
    test(ioContext, protoFactory,
         StreamFlag::VersionVerification | StreamFlag::SerializerInThread | StreamFlag::SliceData,
         StreamFlag::SerializerInThread)
        .wait();
}

TEST_F(Communication, Verify) {
    test(ioContext, protoFactory, StreamFlag::VersionVerification, StreamFlag::None).wait();
}

TEST_F(Communication, Thread) {
    test(ioContext, protoFactory, StreamFlag::SerializerInThread, StreamFlag::None).wait();
}

TEST_F(Communication, Slice) { test(ioContext, protoFactory, StreamFlag::SliceData, StreamFlag::None).wait(); }

TEST_F(Communication, None) { test(ioContext, protoFactory, StreamFlag::None, StreamFlag::None).wait(); }

TEST_F(Communication, UdpThreadAndVerify) {
    udp_test(ioContext, protoFactory, StreamFlag::SerializerInThread | StreamFlag::VersionVerification,
             StreamFlag::SerializerInThread)
        .wait();
}

TEST_F(Communication, UdpVerify) {
    udp_test(ioContext, protoFactory, StreamFlag::VersionVerification, StreamFlag::None).wait();
}

TEST_F(Communication, UdpThread) {
    udp_test(ioContext, protoFactory, StreamFlag::SerializerInThread, StreamFlag::None).wait();
}

TEST_F(Communication, UdpNone) { udp_test(ioContext, protoFactory, StreamFlag::None, StreamFlag::None).wait(); }

#define CUSTOM_MAIN                                                                                                    \
    NEKO_LOG_EXCLUDE("BinarySerializer", "Communication");                                                             \
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);                                                                           \
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);

#include "../common/common_main.cpp.in" // IWYU pragma: export
