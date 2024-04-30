#include <iostream>
#include <sstream>

#include "../rpc/cc_rpc_base.hpp"
#include "../proto/cc_proto_json_serializer.hpp"

#include "ilias_poll.hpp"

CS_RPC_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;

class Message : public CS_PROTO_NAMESPACE::ProtoBase<Message> {
    CS_PROPERTY_FIELD(uint64_t, timestamp);
    CS_PROPERTY_FIELD(std::string, msg);
    CS_PROPERTY_CONTAINER_FIELD(std::vector<int>, numbers);
};
CS_DECLARE_PROTO(Message, Message);

std::string to_hex(const std::vector<char> &data) {
    std::stringstream ss;
    for (auto c : data) {
        ss << "\\" << std::hex << (uint32_t(c) & 0xFF);
    }
    return ss.str();
}

Task<void> ClientLoop(PollContext &ioContext, ChannelFactory &channelFactor) {
    auto ret = co_await channelFactor.connect("tcp://127.0.0.1:1234", 0);
    if (!ret) {
        co_return ret.error();
    }
    std::cout << "connect successed " << std::endl;
    auto channel = ret.value();
    int count = 10;
    while (count -- > 0) {
        if (auto cl = channel.lock(); cl != nullptr) {
            CS_LOG_INFO("send message to channel {}", cl->channelId());
            auto msg = std::make_unique<Message>();
            msg->set_msg("this is a message from client");
            msg->set_timestamp(time(NULL));
            msg->set_numbers({1, 2, 3, 5});
            auto ret1 = co_await cl->send(std::move(msg));
            if (!ret1) {
                CS_LOG_ERROR("send failed: {}", ret1.error().message());
                co_return ret1.error();
            }
        } else {
            CS_LOG_ERROR("channel expired");
            co_return Result<>();
        }
        if (auto cl = channel.lock(); cl != nullptr) {
            auto ret = co_await cl->recv();
            if (!ret) {
                CS_LOG_ERROR("recv failed: {}", ret.error().message());
                co_return ret.error();
            }
            auto retMsg = std::shared_ptr<IProto>(ret.value().release());
            std::cout << "a message from channel : " << cl->channelId() << std::endl;
            auto msg = std::dynamic_pointer_cast<Message>(retMsg);
            if (msg) {
                std::cout << "timestamp: " << msg->timestamp() << std::endl;
                std::cout << "msg: " << msg->msg() << std::endl;
                std::cout << "numbers: ";
                for (auto n : msg->numbers()) {
                    std::cout << n << " ";
                }
                std::cout << std::endl;
            } else {
                std::cout << "recv: " << to_hex(retMsg->serialize()) << std::endl;
            }
        } else {
            CS_LOG_ERROR("channel expired");
            co_return Result<>();
        }
    }
    // if (auto cl = channel.lock(); cl != nullptr) {
        // cl->close();
    // }
    co_return Result<>();
}

Task<void> HandleLoop(std::weak_ptr<cs_ccproto::ChannelBase> channel) {
    while (true) {
        CS_LOG_INFO("HandleLoop");
        if (auto cl = channel.lock(); cl != nullptr) {
            auto ret = co_await cl->recv();
            if (!ret) {
                CS_LOG_ERROR("recv failed: {}", ret.error().message());
                cl->destroy();
                co_return ret.error();
            }
            auto retMsg = std::shared_ptr<IProto>(ret.value().release());
            auto msg = std::dynamic_pointer_cast<Message>(retMsg);
            std::cout << "a message from channel : " << cl->channelId() << std::endl;
            if (msg) {
                std::cout << "timestamp: " << msg->timestamp() << std::endl;
                std::cout << "msg: " << msg->msg() << std::endl;
                std::cout << "numbers: ";
                for (auto n : msg->numbers()) {
                    std::cout << n << " ";
                }
                std::cout << std::endl;
            } else {
                std::cout << "recv: " << to_hex(retMsg->serialize()) << std::endl;
            }
        } else {
            CS_LOG_ERROR("channel expired");
            co_return Result<>();
        }
        auto msg = std::make_unique<Message>();
        msg->set_msg("this is a message from server");
        msg->set_timestamp(time(NULL));
        msg->set_numbers({1, 2, 3});
        if (auto cl = channel.lock(); cl != nullptr) {
            auto ret1 = co_await cl->send(std::move(msg));
            if (!ret1) {
                cl->destroy();
                co_return ret1.error();
            }
        } else {
            CS_LOG_ERROR("channel expired");
            co_return Result<>();
        }
    }
    if (!channel.expired()) {
        channel.lock()->destroy();
    }
    co_return Result<>();
}
Task<void> serverLoop(PollContext &ioContext, ChannelFactory &channelFactor) {
    while (true)
    {
        CS_LOG_INFO("serverLoop");
        auto ret = co_await channelFactor.accept();
        if (!ret) {
            CS_LOG_ERROR("accept failed: {}", ret.error().message());
            co_return ret.error();
        }
        CS_LOG_INFO("accept successed");
        ilias_go HandleLoop(ret.value());
    }
}

int main(int argc, char **argv) {
    spdlog::set_level(spdlog::level::debug);
    PollContext ioContext;
    std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> protoFactory(new CS_PROTO_NAMESPACE::ProtoFactory());
    ChannelFactory channelFactor(ioContext, protoFactory);
    if (argc > 1) {
        if (std::string(argv[1]) == "server") {
            channelFactor.listen("tcp://127.0.0.1:1234");
            ilias_wait serverLoop(ioContext, channelFactor);
        } else {
            ilias_wait ClientLoop(ioContext, channelFactor);
        }
    } else {
        std::cout << "Usage: " << argv[0] << " [server|client]" << std::endl;
    }
    channelFactor.close();
    // cc_rpc_base::rpc_server server;
    // server.start();

    return 0;
}