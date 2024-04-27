
#pragma once

#include "cc_rpc_global.hpp"
#include "../proto/cc_proto_base.hpp"
#include "../proto/cc_proto_binary_serializer.hpp"

#include "ilias.hpp"
#include "ilias_co.hpp"
#include "ilias_task.hpp"
#include "ilias_await.hpp"
#include "ilias_async.hpp"

#include <queue>


CS_PROTO_SET_TYPE_START(1)

CS_RPC_BEGIN_NAMESPACE

class CS_RPC_API RPCContext;
class CS_RPC_API NodeBase;
class CS_RPC_API RPCClient;
class CS_RPC_API RPCServer;
class CS_RPC_API RPCChannel;

// 定义RPCContext
// 定义NodeBase
// 定义RPCClient
// 定义RPCServer

class  CCMessageHeader : public CS_PROTO_NAMESPACE::ProtoBase<CCMessageHeader, CS_PROTO_NAMESPACE::BinarySerializer> {
public:
    inline static int size() { return 20; }
    CS_PROPERTY_FIELD(uint32_t, length)         = 0; // 4
    CS_PROPERTY_FIELD(int32_t, protoType)       = 0; // 4
    CS_PROPERTY_FIELD(uint32_t, factoryVersion) = 0; // 4
    CS_PROPERTY_FIELD(int32_t, transType)       = 0; // 4
    CS_PROPERTY_FIELD(uint32_t, reserved)       = 0; // 4
}; // 20

class  ChannelHeader : public CS_PROTO_NAMESPACE::ProtoBase<CCMessageHeader, CS_PROTO_NAMESPACE::BinarySerializer> {
public:
    enum MessageType {
        UserMessage = 0,
        SystemMessage = 1
    };
    CS_PROPERTY_FIELD(uint8_t, messageType)         = 0; // 1
    CS_PROPERTY_FIELD(uint16_t, channelId)          = 0; // 2
}; // 3


class ChannelBase {
public:
    ChannelBase(const ChannelBase&) = delete;
    virtual ~ChannelBase() {}
    virtual ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message) = 0;
    virtual ILIAS_NAMESPACE::Task<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> recv() = 0;
    uint16_t channelId();
    virtual void close() = 0;
protected:
    ChannelBase(RPCContext *ctxt, uint16_t channelId) : mChannelId(channelId) {}
    std::queue<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> mRecvQueue;
    const uint16_t mChannelId;
    RPCContext *mRPCContext = nullptr;
};

inline uint16_t ChannelBase::channelId() {
    return mChannelId;
}

template <typename SenderType, typename ReceiverType>
class Channel;

class RPCContext {
public:
    enum State {
        Uninitialized,
        Listening,
        Closed,
    };
    enum TransType : uint16_t {
        Channel = 1,
    };
    RPCContext(std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> factory);
    ~RPCContext() {}
    void listen(std::string_view hostname);
    ILIAS_NAMESPACE::Task<ChannelBase> connect(std::string_view hostname);
    ILIAS_NAMESPACE::Task<ChannelBase> accept();
    void close();
    std::weak_ptr<ChannelBase> makeChannel(uint16_t channelId);
    std::weak_ptr<ChannelBase> getChannel(uint16_t channelId);
    std::vector<uint16_t> getChannels();
    const CS_PROTO_NAMESPACE::ProtoFactory &getProtoFactory();

protected:
    std::vector<std::shared_ptr<ChannelBase>> mChannels;
    ILIAS_NAMESPACE::IoContext &mIoContext;
    ILIAS_NAMESPACE::IStreamListener streamListener;
    std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> factory;
};

template <>
class Channel<ILIAS_NAMESPACE::IStreamClient, ILIAS_NAMESPACE::IStreamClient> : public ChannelBase {
public:
    Channel(RPCContext *ctxt,ILIAS_NAMESPACE::IStreamClient &client, uint16_t channelId) : ChannelBase(ctxt, channelId), mClient(std::move(client)) {}
    ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message) override {
        auto msg = message->serialize();
        CCMessageHeader msgHeader;
        msgHeader.set_length(msg.size());
        msgHeader.set_protoType(message->type());
        msgHeader.set_factoryVersion(mRPCContext->getProtoFactory().version());
        msgHeader.set_transType(RPCContext::Channel);
        msgHeader.set_reserved(0);
        std::vector<char> headerMsg = msgHeader.serialize();
        std::vector<char> sendMsg;
        sendMsg.resize(headerMsg.size() + msg.size());
        memcpy(sendMsg.data(), headerMsg.data(), headerMsg.size());
        memcpy(sendMsg.data() + headerMsg.size(), msg.data(), msg.size());
        headerMsg.clear();
        msg.clear();
        auto ret = co_await mClient.send(sendMsg.data(), sendMsg.size());
        if (!ret) {
            co_return ret.error();
        }
        co_return ILIAS_NAMESPACE::Result<void>();
    }
    ILIAS_NAMESPACE::Task<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> recv() override {
        std::vector<char> headerData;
        headerData.resize(CCMessageHeader::size(), 0);
        auto ret = co_await recvUntilFill(headerData);
        if (!ret) {
            co_return ret.error();
        }
        CCMessageHeader msgHeader;
        msgHeader.deserialize(std::move(headerData));
        std::vector<char> data(msgHeader.length(), 0);
        ret = co_await recvUntilFill(data);
        if (!ret) {
            co_return ret.error();
        }
        std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message(mRPCContext->getProtoFactory().create(msgHeader.protoType()));
        message->deserialize(std::move(data));
        data.clear();
        headerData.clear();
        co_return std::move(message);
    }
    void close() override {
        ilias_wait mClient.send(nullptr, 0);
        return;
    }

private:
    ILIAS_NAMESPACE::Task<void> recvUntilFill(std::vector<char> &buf) {
        size_t retSize = 0;
        while (buf.size() > retSize) {
            auto ret = co_await mClient.recv(buf.data() + retSize, buf.size() - retSize);
            if (!ret) {
                co_return ret.error();
            }
            retSize += ret.value();
        }
        co_return ILIAS_NAMESPACE::Result<void>();
    }

    ILIAS_NAMESPACE::IStreamClient mClient;
};

CS_RPC_END_NAMESPACE

CS_DECLARE_PROTO(CS_RPC_NAMESPACE::CCMessageHeader, RPCHeader);
CS_DECLARE_PROTO(CS_RPC_NAMESPACE::ChannelHeader, ChannelHeader);
