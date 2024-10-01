/**
 * @file communication_base.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <errno.h>

#include <queue>
#include <ilias.hpp>
#include <inet.hpp>
#include <net.hpp>

#include "proto/binary_serializer.hpp"
#include "proto/proto_base.hpp"
#include "proto/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
class ChannelFactory;

// enum | code | message | system code mapping
#define NEKO_CHANNEL_ERROR_CODE_TABLE                                                                                  \
    NEKO_CHANNEL_ERROR(Ok, 0, "ok", 0)                                                                                 \
    NEKO_CHANNEL_ERROR(InvalidMessageHeader, 1, "receive an unrecognized message header", -1)                          \
    NEKO_CHANNEL_ERROR(InvalidChannelHeader, 2, "receive an unrecognized channel header", -1)                          \
    NEKO_CHANNEL_ERROR(InvalidProtoType, 3, "receive a message, but proto type is not registed", -1)                   \
    NEKO_CHANNEL_ERROR(InvalidUrl, 4, "unrecognized or error url", -1)                                                 \
    NEKO_CHANNEL_ERROR(ConnectFailed, 5, "can't connect to the url", -1)                                               \
    NEKO_CHANNEL_ERROR(ProtoVersionUnsupported, 6, "proto version is not supported", -1)                               \
    NEKO_CHANNEL_ERROR(ChannelIdInconsistent, 7, "unable to negotiate a consistent channel id", -1)                    \
    NEKO_CHANNEL_ERROR(ConnectionMessageTypeError, 8, "receive a error message in connection state", -1)               \
    NEKO_CHANNEL_ERROR(ChannelBroken, 9, "the channel is broken", -1)                                                  \
    NEKO_CHANNEL_ERROR(ChannelClosed, 10, "the channel is closed", -1)                                                 \
    NEKO_CHANNEL_ERROR(ChannelClosedByPeer, 11, "the channel is closed by peer", -1)                                   \
    NEKO_CHANNEL_ERROR(Timeout, 12, "the operator is timeout", -1)

class NEKO_PROTO_API MessageHeader {
public:
    inline MessageHeader(uint32_t length = 0, int32_t protoType = 0, uint16_t transType = 0, uint32_t reserved = 0)
        : length(length), protoType(protoType), transType(transType), reserved(reserved) {}
    inline static int size() { return 14; }
    uint32_t length    = 0; // 4 : the length of the message, no't contain the header
    int32_t protoType  = 0; // 4 : the type of the message in proto factory
    uint16_t transType = 0; // 2 : the type of this transaction
    uint32_t reserved  = 0; // 4 : the reserved field

    template <typename SerializerT>
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {
        return serializer(makeFixedLengthField(length), makeFixedLengthField(protoType),
                          makeFixedLengthField(transType), makeFixedLengthField(reserved));
    }
    template <typename SerializerT>
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {
        return serializer(makeFixedLengthField(length), makeFixedLengthField(protoType),
                          makeFixedLengthField(transType), makeFixedLengthField(reserved));
    }
};

class NEKO_PROTO_API ChannelHeader {
public:
    enum MessageType {
        UserMessage      = 1,
        ConnectMessage   = 2,
        HeartbeatMessage = 4,
        CloseMessage     = 5,

        OkMessage    = 127,
        ErrorMessage = 128,
    };
    uint32_t factoryVersion = 0; // 4 : the version of the proto factory
    uint8_t messageType     = 0; // 1
    uint16_t channelId      = 0; // 2
    inline static int size() { return 7; }

    template <typename SerializerT>
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {
        return serializer(makeFixedLengthField(factoryVersion), makeFixedLengthField(messageType),
                          makeFixedLengthField(channelId));
    }
    template <typename SerializerT>
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {
        return serializer(makeFixedLengthField(factoryVersion), makeFixedLengthField(messageType),
                          makeFixedLengthField(channelId));
    }
};

#define NEKO_CHANNEL_ERROR(name, code, message, _) name = code,
enum class ErrorCode { NEKO_CHANNEL_ERROR_CODE_TABLE };
#undef NEKO_CHANNEL_ERROR

class NEKO_PROTO_API ErrorCategory : public ILIAS_NAMESPACE::ErrorCategory {
public:
    static auto instance() -> const ErrorCategory&;
    auto message(int64_t value) const -> std::string override;
    auto name() const -> std::string_view override;
    auto equivalent(int64_t self, const ILIAS_NAMESPACE::Error& other) const -> bool override;
};
ILIAS_DECLARE_ERROR(ErrorCode, ErrorCategory);

class NEKO_PROTO_API ChannelBase {
public:
    enum ChannelState {
        Uninitialized,
        Connected,
        Closed,
    };
    ChannelBase(const ChannelBase&) = delete;
    virtual ~ChannelBase() noexcept {}
    virtual ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<NEKO_NAMESPACE::IProto> message) = 0;
    virtual ILIAS_NAMESPACE::Task<std::unique_ptr<NEKO_NAMESPACE::IProto>> recv()             = 0;
    ChannelState state();
    uint16_t channelId();
    virtual void close()   = 0;
    virtual void destroy() = 0;

protected:
    ChannelBase(ChannelFactory* ctxt, uint16_t channelId);
    const uint16_t mChannelId;
    ChannelFactory* const mChannelFactory = nullptr;
    ChannelState mState;
};

class NEKO_PROTO_API ChannelFactory {
public:
    enum class State {
        Uninitialized,
        Listening,
        Closed,
    };
    enum class TransType : uint16_t {
        ShakeHand  = 0,
        Channel    = 1,
        SubChannel = 2,
    };
    ChannelFactory(ILIAS_NAMESPACE::IoContext& ioContext, std::shared_ptr<NEKO_NAMESPACE::ProtoFactory> factory);
    ~ChannelFactory();
    ILIAS_NAMESPACE::Expected<void, ILIAS_NAMESPACE::Error> listen(std::string_view hostname);
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> connect(std::string_view hostname, const uint16_t channelId);
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> accept();
    void close();
    ILIAS_NAMESPACE::Task<std::weak_ptr<ChannelBase>> makeChannel(ILIAS_NAMESPACE::IStreamClient&& client,
                                                                  const uint16_t channelId);
    std::weak_ptr<ChannelBase> getChannel(uint16_t channelId);
    std::vector<uint16_t> getChannelIds();
    void destroyChannel(uint16_t channelId);
    const NEKO_NAMESPACE::ProtoFactory& getProtoFactory();

protected:
    std::map<uint16_t, std::shared_ptr<ChannelBase>> mChannels;
    ILIAS_NAMESPACE::IoContext& mIoContext;
    ILIAS_NAMESPACE::IStreamListener mListener;
    std::shared_ptr<NEKO_NAMESPACE::ProtoFactory> mFactory;
};

class NEKO_PROTO_API ByteStreamChannel : public ChannelBase {
public:
    ByteStreamChannel(ChannelFactory* ctxt, ILIAS_NAMESPACE::IStreamClient&& client, uint16_t channelId);
    ~ByteStreamChannel() noexcept = default;
    ILIAS_NAMESPACE::Task<void> send(std::unique_ptr<NEKO_NAMESPACE::IProto> message) override;
    ILIAS_NAMESPACE::Task<std::unique_ptr<NEKO_NAMESPACE::IProto>> recv() override;
    void close() override;
    void destroy() override;

private:
    ILIAS_NAMESPACE::IStreamClient mClient;
};

inline ChannelBase::ChannelBase(ChannelFactory* ctxt, uint16_t channelId)
    : mChannelId(channelId), mChannelFactory(ctxt) {}

inline uint16_t ChannelBase::channelId() { return mChannelId; }

inline ChannelBase::ChannelState ChannelBase::state() { return mState; }

NEKO_END_NAMESPACE
