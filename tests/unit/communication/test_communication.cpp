#include <gtest/gtest.h>
#include <iostream>
#include <random>
#include <sstream>

#include "nekoproto/communication/communication_base.hpp"
#include "nekoproto/proto/json_serializer.hpp"
#include "nekoproto/proto/types/vector.hpp"

#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <ilias/task/when_all.hpp>
NEKO_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;

class Message {
public:
    uint64_t timestamp;
    std::string msg;
    std::vector<int> numbers;

    NEKO_SERIALIZER(timestamp, msg, numbers);
    NEKO_DECLARE_PROTOCOL(Message, JsonSerializer)
};

std::string generate_random_string(size_t length) {
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";
    std::string s;
    s.reserve(length);

    std::random_device rd;        // 获取随机数种子
    std::mt19937 generator(rd()); // 采用梅森旋转算法
    std::uniform_int_distribution<> distribution(0, sizeof(alphanum) - 2);
    for (size_t i = 0; i < length; ++i) {
        s += alphanum[distribution(generator)];
    }
    return s;
}

// contains to string
template <typename T>
std::string to_string(const T& value) {
    std::ostringstream ss;
    for (auto& v : value) {
        ss << v << " ";
    }
    return ss.str();
}

Task<void> ClientLoop(IoContext& ioContext, ProtoFactory& protoFactory, StreamFlag sendFlag, StreamFlag recvFlag) {
    TcpClient tcpClient(ioContext, AF_INET);
    NEKO_LOG_INFO("unit test", "Client connect to service");
    auto ret = co_await tcpClient.connect(IPEndpoint("127.0.0.1", 12345));
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    ProtoStreamClient<TcpClient> client(protoFactory, ioContext, std::move(tcpClient));
    NEKO_LOG_INFO("unit test", "Client loop started");
    const size_t desiredSize = 10 * 1024; // 1MB
    int count                = 10;
    while (count-- > 0) {
        NEKO_LOG_INFO("unit test", "send message to {}", count);
        Message msg;
        msg.msg       = generate_random_string(desiredSize);
        msg.timestamp = time(NULL);
        msg.numbers   = (std::vector<int>{1, 2, 3, 5});
        auto ret1     = co_await client.send(msg.makeProto(), sendFlag);
        if (!ret1) {
            NEKO_LOG_ERROR("unit test", "send failed: {}", ret1.error().message());
            co_return Unexpected(ret1.error());
        }
        NEKO_LOG_INFO("unit test", "send message: timestamp: {}, msg: {} numbers: {}", msg.timestamp,
                      msg.msg.substr(0, 20) + "...", to_string(msg.numbers));
        auto ret = co_await client.recv(recvFlag);
        if (!ret) {
            NEKO_LOG_ERROR("unit test", "recv failed: {}", ret.error().message());
            co_return Unexpected(ret.error());
        }
        auto retMsg = std::move(ret.value());
        auto msg1   = retMsg->cast<Message>();
        if (msg1) {
            NEKO_LOG_INFO("unit test", "recv success: timestamp: {} msg: {} numbers: {}", msg1->timestamp,
                          msg1->msg.substr(0, 20) + "...", to_string(msg1->numbers));
        } else {
            EXPECT_TRUE(false);
        }
    }
    co_return co_await client.close();
}

Task<void> HandleLoop(ProtoStreamClient<TcpClient>&& _client, StreamFlag sendFlag, StreamFlag recvFlag) {
    ProtoStreamClient<TcpClient> client(std::move(_client));
    while (true) {
        NEKO_LOG_INFO("unit test", "HandleLoop");
        auto ret = co_await client.recv(recvFlag);
        if (!ret) {
            NEKO_LOG_ERROR("unit test", "recv failed: {}", ret.error().message());
            co_await client.close();
            co_return Unexpected(ret.error());
        }
        auto retMsg = std::shared_ptr<IProto>(ret.value().release());
        auto msg    = retMsg->cast<Message>();
        if (msg) {
            NEKO_LOG_INFO("unit test", "recv success: timestamp: {}  msg: {}  numbers: {}", msg->timestamp,
                          msg->msg.substr(0, 20) + "...", to_string(msg->numbers));
        } else {
            EXPECT_TRUE(false);
        }
        Message msg1;
        msg1.msg       = ("this is a message from server");
        msg1.timestamp = (time(NULL));
        msg1.numbers   = (std::vector<int>{1, 2, 3});
        auto ret1      = co_await client.send(msg->makeProto(), sendFlag);
        if (!ret1) {
            co_await client.close();
            co_return Unexpected(ret1.error());
        }
        NEKO_LOG_INFO("unit test", "send message: timestamp: {}, msg: {} numbers: {}", msg1.timestamp,
                      msg1.msg.substr(0, 20) + "...", to_string(msg1.numbers));
    }
    co_return co_await client.close();
}
Task<void> serverLoop(IoContext& ioContext, ProtoFactory& protoFactor, StreamFlag sendFlag, StreamFlag recvFlag) {
    TcpListener listener(ioContext, AF_INET);
    NEKO_LOG_INFO("unit test", "serverLoop");
    listener.bind(IPEndpoint("127.0.0.1", 12345));
    auto ret = co_await listener.accept();
    if (!ret) {
        NEKO_LOG_ERROR("unit test", "accept failed: {}", ret.error().message());
        co_return Unexpected(ret.error());
    }
    NEKO_LOG_INFO("unit test", "accept successed");
    auto ret1 = co_await HandleLoop(
        ProtoStreamClient<ilias::TcpClient>(protoFactor, ioContext, std::move(ret.value().first)), sendFlag, recvFlag);
    if (!ret1 && ret1.error() != Error::ConnectionReset) {
        co_return Unexpected(ret1.error());
    }
    co_return Result<>();
}

Task<void> test(IoContext& ioContext, ProtoFactory& protoFactory, StreamFlag sendFlag, StreamFlag recvFlag) {
    auto [ret1, ret2] = co_await whenAll(serverLoop(ioContext, protoFactory, sendFlag, recvFlag),
                                         ClientLoop(ioContext, protoFactory, sendFlag, recvFlag));
    if ((!ret1 && ret1.error() != Error::ConnectionReset) || (!ret2)) {
        exit(-1);
    }
    co_return Result<>();
}

class CoummicationTest : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
    PlatformContext ioContext;
    ProtoFactory protoFactory;
};

TEST_F(CoummicationTest, VerifyAndThreadAndSlice) {
    ilias_wait test(ioContext, protoFactory,
                    StreamFlag::VersionVerification | StreamFlag::SerializerInThread | StreamFlag::SliceData,
                    StreamFlag::SerializerInThread);
}

TEST_F(CoummicationTest, Verify) {
    ilias_wait test(ioContext, protoFactory, StreamFlag::VersionVerification, StreamFlag::None);
}

TEST_F(CoummicationTest, Thread) {
    ilias_wait test(ioContext, protoFactory, StreamFlag::SerializerInThread, StreamFlag::None);
}

TEST_F(CoummicationTest, Slice) { ilias_wait test(ioContext, protoFactory, StreamFlag::SliceData, StreamFlag::None); }

TEST_F(CoummicationTest, None) { ilias_wait test(ioContext, protoFactory, StreamFlag::None, StreamFlag::None); }

int main(int argc, char** argv) {
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}