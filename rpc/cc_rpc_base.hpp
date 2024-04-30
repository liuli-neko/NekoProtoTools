
#pragma once

#include <queue>

#include "../proto/cc_proto_base.hpp"
#include "../proto/cc_proto_binary_serializer.hpp"
#include "cc_rpc_global.hpp"
#include "ilias.hpp"
#include "ilias_async.hpp"
#include "ilias_await.hpp"
#include "ilias_co.hpp"
#include "ilias_task.hpp"

CS_RPC_BEGIN_NAMESPACE
class ChannelFactory;

class CS_RPC_API CCMessageHeader : public CS_PROTO_NAMESPACE::ProtoBase<CCMessageHeader, CS_PROTO_NAMESPACE::BinarySerializer> {
public:
    inline CCMessageHeader(uint32_t length = 0, int32_t protoType = 0, uint16_t transType = 0, uint32_t reserved = 0)
        : m_length(length), m_protoType(protoType), m_transType(transType), m_reserved(reserved) {}
    inline static int size() { return 14; }
    CS_PROPERTY_FIELD(uint32_t, length) = 0;     // 4 : the length of the message, no't contain the header
    CS_PROPERTY_FIELD(int32_t, protoType) = 0;   // 4 : the type of the message in proto factory
    CS_PROPERTY_FIELD(uint16_t, transType) = 0;  // 2 : the type of this transaction
    CS_PROPERTY_FIELD(uint32_t, reserved) = 0;   // 4 : the reserved field
};

class CS_RPC_API ChannelHeader : public CS_PROTO_NAMESPACE::ProtoBase<ChannelHeader, CS_PROTO_NAMESPACE::BinarySerializer> {
public:
    enum MessageType {
        UserMessage = 1,
        ConnectMessage = 2,
        HeartbeatMessage = 4,
        CloseMessage = 5,

        OkMessage = 127,
        ErrorMessage = 128,
    };
    CS_PROPERTY_FIELD(uint32_t, factoryVersion) = 0;  // 4 : the version of the proto factory
    CS_PROPERTY_FIELD(uint8_t, messageType) = 0;      // 1
    CS_PROPERTY_FIELD(uint16_t, channelId) = 0;       // 2
};

class CS_RPC_API ChannelBase {
public:
    enum ChannelState {
        Uninitialized,
        Connected,
        Closed,
    };
    ChannelBase(const ChannelBase&) = delete;
    virtual ~ChannelBase() {}
    virtual ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message) = 0;
    virtual ILIAS_NAMESPACE::Task<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> recv() = 0;
    ChannelState state();
    uint16_t channelId();
    virtual void close() = 0;
    virtual void destroy() = 0;

protected:
    ChannelBase(ChannelFactory* ctxt, uint16_t channelId);
    const uint16_t mChannelId;
    ChannelFactory * const mChannelFactory = nullptr;
    ChannelState mState;
};

class CS_RPC_API ChannelFactory {
public:
    enum class State {
        Uninitialized,
        Listening,
        Closed,
    };
    enum class TransType : uint16_t {
        ShakeHand = 0,
        Channel = 1,
        SubChannel = 2,
    };
    ChannelFactory(ILIAS_NAMESPACE::IoContext& ioContext, std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> factory);
    ~ChannelFactory();
    void listen(std::string_view hostname);
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> connect(std::string_view hostname, const uint16_t channelId);
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> accept();
    void close();
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> makeChannel(ILIAS_NAMESPACE::IStreamClient&& client,
                                                                  const uint16_t channelId);
    std::weak_ptr<ChannelBase> getChannel(uint16_t channelId);
    std::vector<uint16_t> getChannels();
    void destroyChannel(uint16_t channelId);
    const CS_PROTO_NAMESPACE::ProtoFactory& getProtoFactory();

protected:
    std::map<uint16_t, std::shared_ptr<ChannelBase>> mChannels;
    ILIAS_NAMESPACE::IoContext& mIoContext;
    ILIAS_NAMESPACE::IStreamListener mListener;
    std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> mFactory;
};

class CS_RPC_API ByteStreamChannel : public ChannelBase {
public:
    ByteStreamChannel(ChannelFactory* ctxt, ILIAS_NAMESPACE::ByteStream<>&& client, uint16_t channelId);
    ~ByteStreamChannel() = default;
    ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message) override;
    ILIAS_NAMESPACE::Task<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> recv() override;
    void close() override;
    void destroy() override;

private:
    ILIAS_NAMESPACE::ByteStream<> mClient;
};

inline ChannelBase::ChannelBase(ChannelFactory* ctxt, uint16_t channelId)
    : mChannelId(channelId), mChannelFactory(ctxt) {}

inline uint16_t ChannelBase::channelId() { return mChannelId; }

inline ChannelBase::ChannelState ChannelBase::state() { return mState; }

CS_RPC_END_NAMESPACE
