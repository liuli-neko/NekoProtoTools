#include <iostream>
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

std::string to_hex(const std::vector<char>& data) {
    std::stringstream ss;
    for (auto c : data) {
        ss << "\\" << std::hex << (uint32_t(c) & 0xFF);
    }
    return ss.str();
}

Task<void> ClientLoop(IoContext& ioContext, NEKO_NAMESPACE::ProtoFactory& protoFactory) {
    TcpClient tcpClient(ioContext, AF_INET);
    NEKO_LOG_INFO("unit test", "Client connect to service");
    auto ret = co_await tcpClient.connect(IPEndpoint("127.0.0.1", 12345));
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    ProtoStreamClient<TcpClient> client(protoFactory, ioContext, std::move(tcpClient));
    NEKO_LOG_INFO("unit test", "Client loop started");
    int count = 10;
    while (count-- > 0) {
        NEKO_LOG_INFO("unit test", "send message to {}", count);
        Message msg;
        msg.msg       = "this is a message from client";
        msg.timestamp = time(NULL);
        msg.numbers   = (std::vector<int>{1, 2, 3, 5});
        auto ret1 = co_await client.send(msg.makeProto(), NEKO_NAMESPACE::ProtoStreamClient<TcpClient>::SerializerInThread);
        if (!ret1) {
            NEKO_LOG_ERROR("unit test", "send failed: {}", ret1.error().message());
            co_return Unexpected(ret1.error());
        }
        auto ret = co_await client.recv();
        if (!ret) {
            NEKO_LOG_ERROR("unit test", "recv failed: {}", ret.error().message());
            co_return Unexpected(ret.error());
        }
        auto retMsg = std::move(ret.value());
        auto msg1   = retMsg->cast<Message>();
        if (msg1) {
            std::cout << "timestamp: " << msg1->timestamp << std::endl;
            std::cout << "msg: " << msg1->msg << std::endl;
            std::cout << "numbers: ";
            for (auto n : msg1->numbers) {
                std::cout << n << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "recv: " << to_hex(retMsg->toData()) << std::endl;
        }
    }
    co_return co_await client.close();
}

Task<void> HandleLoop(ProtoStreamClient<TcpClient>&& _client) {
    ProtoStreamClient<TcpClient> client(std::move(_client));
    while (true) {
        NEKO_LOG_INFO("unit test", "HandleLoop");
        auto ret = co_await client.recv();
        if (!ret) {
            NEKO_LOG_ERROR("unit test", "recv failed: {}", ret.error().message());
            co_await client.close();
            co_return Unexpected(ret.error());
        }
        auto retMsg = std::shared_ptr<IProto>(ret.value().release());
        auto msg    = retMsg->cast<Message>();
        if (msg) {
            std::cout << "timestamp: " << msg->timestamp << std::endl;
            std::cout << "msg: " << msg->msg << std::endl;
            std::cout << "numbers: ";
            for (auto n : msg->numbers) {
                std::cout << n << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "recv: " << to_hex(retMsg->toData()) << std::endl;
        }
        Message msg1;
        msg1.msg       = ("this is a message from server");
        msg1.timestamp = (time(NULL));
        msg1.numbers   = (std::vector<int>{1, 2, 3});
        auto ret1      = co_await client.send(msg->makeProto());
        if (!ret1) {
            co_await client.close();
            co_return Unexpected(ret1.error());
        }
    }
    co_return co_await client.close();
}
Task<void> serverLoop(IoContext& ioContext, NEKO_NAMESPACE::ProtoFactory& protoFactor) {
    TcpListener listener(ioContext, AF_INET);
    NEKO_LOG_INFO("unit test", "serverLoop");
    listener.bind(IPEndpoint("127.0.0.1", 12345));
    auto ret = co_await listener.accept();
    if (!ret) {
        NEKO_LOG_ERROR("unit test", "accept failed: {}", ret.error().message());
        co_return Unexpected(ret.error());
    }
    NEKO_LOG_INFO("unit test", "accept successed");
    auto ret1 =
        co_await HandleLoop(ProtoStreamClient<ilias::TcpClient>(protoFactor, ioContext, std::move(ret.value().first)));
    if (!ret1 && ret1.error() != Error::ConnectionReset) {
        co_return Unexpected(ret1.error());
    }
    co_return Result<>();
}

Task<void> test(IoContext& ioContext, ProtoFactory& protoFactory) {
    auto [ret1, ret2] = co_await whenAll(serverLoop(ioContext, protoFactory), ClientLoop(ioContext, protoFactory));
    if ((!ret1 && ret1.error() != Error::ConnectionReset) || (!ret2)) {
        exit(-1);
    }
    co_return Result<>();
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    PlatformContext ioContext;
    NEKO_NAMESPACE::ProtoFactory protoFactory;
    TcpClient client;
    ilias_wait test(ioContext, protoFactory);
    // cc_rpc_base::rpc_server server;
    // server.start();
    return 0;
}