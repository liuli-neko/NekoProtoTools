#include <iostream>
#include <sstream>

#include "../communication/communication_base.hpp"
#include "../core/json_serializer.hpp"
#include "../core/types/vector.hpp"

#include "ilias_networking.hpp"
#ifdef _WIN32
#include "ilias_iocp.cpp"
#endif
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

Task<void> ClientLoop(IoContext& ioContext, ChannelFactory& channelFactor) {
    auto ret = co_await channelFactor.connect("tcp://127.0.0.1:1234", 0);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    std::cout << "connect successed " << std::endl;
    auto channel = ret.value();
    int count    = 10;
    while (count-- > 0) {
        if (auto cl = channel.lock(); cl != nullptr) {
            NEKO_LOG_INFO("send message to channel {}", cl->channelId());
            auto msg          = NEKO_MAKE_UNIQUE(Message::ProtoType, Message{});
            (*msg)->msg       = "this is a message from client";
            (*msg)->timestamp = time(NULL);
            (*msg)->numbers   = (std::vector<int>{1, 2, 3, 5});
            auto ret1         = co_await cl->send(std::move(msg));
            if (!ret1) {
                NEKO_LOG_ERROR("send failed: {}", ret1.error().message());
                co_return Unexpected(ret1.error());
            }
        } else {
            NEKO_LOG_ERROR("channel expired");
            co_return Result<>();
        }
        if (auto cl = channel.lock(); cl != nullptr) {
            auto ret = co_await cl->recv();
            if (!ret) {
                NEKO_LOG_ERROR("recv failed: {}", ret.error().message());
                co_return Unexpected(ret.error());
            }
            auto retMsg = std::shared_ptr<IProto>(ret.value().release());
            std::cout << "a message from channel : " << cl->channelId() << std::endl;
            auto msg = std::dynamic_pointer_cast<Message>(retMsg);
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
        } else {
            NEKO_LOG_ERROR("channel expired");
            co_return Result<>();
        }
    }
    if (auto cl = channel.lock(); cl != nullptr) {
        cl->close();
    }
    co_return Result<>();
}

Task<void> HandleLoop(std::weak_ptr<NEKO_NAMESPACE::ChannelBase> channel) {
    while (true) {
        NEKO_LOG_INFO("HandleLoop");
        if (auto cl = channel.lock(); cl != nullptr) {
            auto ret = co_await cl->recv();
            if (!ret) {
                NEKO_LOG_ERROR("recv failed: {}", ret.error().message());
                cl->destroy();
                co_return Unexpected(ret.error());
            }
            auto retMsg = std::shared_ptr<IProto>(ret.value().release());
            auto msg    = std::dynamic_pointer_cast<Message>(retMsg);
            std::cout << "a message from channel : " << cl->channelId() << std::endl;
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
        } else {
            NEKO_LOG_ERROR("channel expired");
            co_return Result<>();
        }
        auto msg          = NEKO_MAKE_UNIQUE(Message::ProtoType, Message{});
        (*msg)->msg       = ("this is a message from server");
        (*msg)->timestamp = (time(NULL));
        (*msg)->numbers   = (std::vector<int>{1, 2, 3});
        if (auto cl = channel.lock(); cl != nullptr) {
            auto ret1 = co_await cl->send(std::move(msg));
            if (!ret1) {
                cl->destroy();
                co_return Unexpected(ret1.error());
            }
        } else {
            NEKO_LOG_ERROR("channel expired");
            co_return Result<>();
        }
    }
    if (!channel.expired()) {
        channel.lock()->destroy();
    }
    co_return Result<>();
}
Task<void> serverLoop(IoContext& ioContext, ChannelFactory& channelFactor) {
    NEKO_LOG_INFO("serverLoop");
    auto ret = co_await channelFactor.accept();
    if (!ret) {
        NEKO_LOG_ERROR("accept failed: {}", ret.error().message());
        co_return Unexpected(ret.error());
    }
    NEKO_LOG_INFO("accept successed");
    auto ret1 = co_await HandleLoop(ret.value());
    if (!ret1 && ret1.error() != ErrorCode::ChannelClosedByPeer) {
        co_return Unexpected(ret1.error());
    }
    co_return Result<>();
}

Task<void> test(IoContext& ioContext, ChannelFactory& channelFactor, ChannelFactory& channelFactor1) {
    auto [ret1, ret2] = co_await WhenAll(serverLoop(ioContext, channelFactor), ClientLoop(ioContext, channelFactor1));
    if ((!ret1 && ret1.error() != ErrorCode::ChannelClosedByPeer) || (!ret2)) {
        exit(-1);
    }
    co_return Result<>();
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    spdlog::set_level(spdlog::level::debug);
    PlatformIoContext ioContext;
    std::shared_ptr<NEKO_NAMESPACE::ProtoFactory> protoFactory(new NEKO_NAMESPACE::ProtoFactory());
    ChannelFactory channelFactor(ioContext, protoFactory);
    ChannelFactory channelFactor1(ioContext, protoFactory);
    channelFactor.listen("tcp://127.0.0.1:1234");
    ilias_wait test(ioContext, channelFactor, channelFactor1);
    channelFactor.close();
    // cc_rpc_base::rpc_server server;
    // server.start();

    return 0;
}