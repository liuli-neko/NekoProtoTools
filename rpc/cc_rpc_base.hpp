
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

class CS_RPC_API ChannelFactory;

class  CCMessageHeader : public CS_PROTO_NAMESPACE::ProtoBase<CCMessageHeader, CS_PROTO_NAMESPACE::BinarySerializer> {
public:
    inline CCMessageHeader(uint32_t length = 0, int32_t protoType = 0, uint32_t factoryVersion = 0, uint16_t transType = 0, uint32_t reserved = 0) :
        m_length(length), m_protoType(protoType), m_factoryVersion(factoryVersion), m_transType(transType), m_reserved(reserved) {}
    inline static int size() { return 20; }
    CS_PROPERTY_FIELD(uint32_t, length)         = 0; // 4 : the length of the message, no't contain the header
    CS_PROPERTY_FIELD(int32_t, protoType)       = 0; // 4 : the type of the message in proto factory
    CS_PROPERTY_FIELD(uint32_t, factoryVersion) = 0; // 4 : the version of the proto factory
    CS_PROPERTY_FIELD(uint16_t, transType)      = 0; // 2 : the type of this transaction
    CS_PROPERTY_FIELD(uint32_t, reserved)       = 0; // 4 : the reserved field
};

class  ChannelHeader : public CS_PROTO_NAMESPACE::ProtoBase<CCMessageHeader, CS_PROTO_NAMESPACE::BinarySerializer> {
public:
    enum MessageType {
        UserMessage = 1,
        ConnectMessage = 2,
        HeartbeatMessage = 4,
        CloseMessage = 5,

        OkMessage = 127,
        ErrorMessage = 128,
    };
    CS_PROPERTY_FIELD(uint8_t, messageType)         = 0; // 1
    CS_PROPERTY_FIELD(uint16_t, channelId)          = 0; // 2
}; // 3


class CS_RPC_API ChannelBase {
public:
    ChannelBase(const ChannelBase&) = delete;
    virtual ~ChannelBase() {}
    virtual ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message) = 0;
    virtual ILIAS_NAMESPACE::Task<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> recv() = 0;
    uint16_t channelId();
    virtual void close() = 0;
protected:
    ChannelBase(ChannelFactory *ctxt, uint16_t channelId) : mChannelId(channelId) {}
    std::queue<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> mRecvQueue;
    const uint16_t mChannelId;
    ChannelFactory *mChannelFactory = nullptr;
};

inline uint16_t ChannelBase::channelId() {
    return mChannelId;
}

template <typename SenderType, typename ReceiverType>
class Channel;

class CS_RPC_API ChannelFactory {
public:
    enum class State {
        Uninitialized,
        Listening,
        Connected,
        Closed,
    };
    enum class TransType : uint16_t {
        ShakeHand = 0,
        Channel = 1,
        SubChannel = 2,
    };
    ChannelFactory(ILIAS_NAMESPACE::IoContext &ioContext, std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> factory);
    ~ChannelFactory() {}
    void listen(std::string_view hostname);
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> connect(std::string_view hostname,const uint16_t channelId);
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> accept();
    void close();
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> makeChannel(ILIAS_NAMESPACE::IStreamClient &&client,const uint16_t channelId);
    /**
     * @brief make a subchannel from the main channel
     * 
     * @param mainChannel 
     * @param channelId 
     * @return ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> 
     */
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> makeChannel(std::weak_ptr<ChannelBase> mainChannel ,const uint16_t channelId);
    std::weak_ptr<ChannelBase> getChannel(uint16_t channelId);
    std::vector<uint16_t> getChannels();
    const CS_PROTO_NAMESPACE::ProtoFactory &getProtoFactory();

protected:
    std::map<uint16_t, std::shared_ptr<ChannelBase>> mChannels;
    ILIAS_NAMESPACE::IoContext &mIoContext;
    ILIAS_NAMESPACE::IStreamListener mListener;
    std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> mFactory;
};

template <>
class Channel<ILIAS_NAMESPACE::ByteStream<>, ILIAS_NAMESPACE::ByteStream<>> : public ChannelBase {
public:
    Channel(ChannelFactory *ctxt,ILIAS_NAMESPACE::ByteStream<> &&client, uint16_t channelId) : ChannelBase(ctxt, channelId), mClient(std::move(client)) {}
    ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message) override {
        auto msg = message->serialize();
        CCMessageHeader msgHeader(msg.size(), 
            message->type(), 
            mChannelFactory->getProtoFactory().version(), 
            static_cast<uint16_t>(ChannelFactory::TransType::Channel));
        std::vector<char> sendMsg = msgHeader.serialize();
        sendMsg.resize(sendMsg.size() + msg.size());
        memcpy(sendMsg.data() + CCMessageHeader::size(), msg.data(), msg.size());
        msg.clear();
        auto ret = co_await mClient.send(sendMsg.data(), sendMsg.size());
        sendMsg.clear();
        if (!ret) {
            co_return ret.error();
        }
        co_return ILIAS_NAMESPACE::Result<void>();
    }
    ILIAS_NAMESPACE::Task<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> recv() override {
        std::vector<char> headerData;
        headerData.resize(CCMessageHeader::size(), 0);
        auto ret = co_await mClient.recvAll(headerData.data(), headerData.size());
        if (!ret) {
            co_return ret.error();
        }
        CCMessageHeader msgHeader;
        msgHeader.deserialize(std::move(headerData));
        std::vector<char> data(msgHeader.length(), 0);
        ret = co_await mClient.recvAll(data.data(), data.size());
        if (!ret) {
            co_return ret.error();
        }
        std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message(mChannelFactory->getProtoFactory().create(msgHeader.protoType()));
        message->deserialize(std::move(data));
        co_return std::move(message);
    }
    void close() override {
        ilias_wait mClient.send(nullptr, 0);
        return;
    }

private:
    ILIAS_NAMESPACE::ByteStream<> mClient;
};

CS_RPC_END_NAMESPACE

CS_DECLARE_PROTO(CS_RPC_NAMESPACE::CCMessageHeader, RPCHeader);
CS_DECLARE_PROTO(CS_RPC_NAMESPACE::ChannelHeader, ChannelHeader);
