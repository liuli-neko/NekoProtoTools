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

#include <ilias/detail/expected.hpp>
#include <ilias/ilias.hpp>
#include <ilias/io/dyn_traits.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <set>

#include "detail/thread_await.hpp"
#include "nekoproto/proto/binary_serializer.hpp"
#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/proto/serializer_base.hpp"
#include "nekoproto/proto/types/byte.hpp"
#include "nekoproto/proto/types/map.hpp"
#include "nekoproto/proto/types/vector.hpp"

NEKO_BEGIN_NAMESPACE

enum MessageType {
    Complete,            // all message data will be received once, the length field is the total length of the message
    Slice,               // the slice of the message
    SliceHeader,         // the slice header of the message.
    Cancel,              // cancel the message
    VersionVerification, // verification of protocol version
};

/**
 * @brief message header
 *
 * @par Header format
 * | length (4 bytes) | data (4 bytes) | message type (2 bytes) |
 * the length field is the length of the message, not the length of the header, and in deferent message type, the field
 * will have different meanings.
 * 1. Complete:
 *      the length field is the total length of the message, 0 means no data.
 *      the data field is the proto type of message.
 * 2. Slice:
 *      the length field is the length of this slice.
 *      the data field is data offset.
 * 3. SliceHeader:
 *      the length field is the total sizeof the message.
 *      the data field is the proto type .
 * 4. Cancel:
 *      the length field is 0.
 *      the data field is 0 or the proto type of message.
 * 5. VersionVerification:
 *      the length field is 0.
 *      the data field is the version of the protocol.
 * @note the length field is the length of the message, not the length of the header
 *
 */
class NEKO_PROTO_API MessageHeader {
public:
    inline MessageHeader(uint32_t length = 0, int32_t data = 0, uint16_t messageType = 0)
        : length(length), data(data), messageType(messageType) {}
    inline static int size() { return 10; }
    uint32_t length = 0; // 4 : the length of the message, no't contain the header
    int32_t data    = 0; // 4 : the proto type of this message in Complete message or the slice index in Slice message
    uint16_t messageType = 0; // 2 : the type of this message

    template <typename SerializerT>
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {
        return serializer(make_fixed_length_field(length), make_fixed_length_field(data),
                          make_fixed_length_field(messageType));
    }
    template <typename SerializerT>
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {
        return serializer(make_fixed_length_field(length), make_fixed_length_field(data),
                          make_fixed_length_field(messageType));
    }

    NEKO_DECLARE_PROTOCOL(MessageHeader, BinarySerializer)
private:
    inline static int specifyType() { return 1; } // NOLINT(readability-identifier-naming)
    friend class detail::proto_method_access;
};

struct NEKO_PROTO_API ProtocolTable {
    uint32_t protocolFactoryVersion            = 0;
    std::map<uint32_t, std::string> protoTable = {};

    NEKO_SERIALIZER(protocolFactoryVersion, protoTable)
    NEKO_DECLARE_PROTOCOL(ProtocolTable, BinarySerializer)
private:
    inline static int specifyType() { return 2; } // NOLINT(readability-identifier-naming)
    friend class detail::proto_method_access;
};

class NEKO_PROTO_API RawDataMessage {
public:
    inline RawDataMessage(uint32_t length = 0, int32_t type = 0, const std::string& name = "unknown")
        : length(length), type(type), name(name) {}
    uint32_t length = 0;
    int32_t type    = 0;
    std::string name;
    std::vector<std::byte> data;

    NEKO_SERIALIZER(length, type, name, data)
    NEKO_DECLARE_PROTOCOL(RawDataMessage, BinarySerializer)
    inline static int specifyType() { return 3; } // NOLINT(readability-identifier-naming)
private:
    friend class detail::proto_method_access;
};

// enum | code | message | system code mapping
#define NEKO_CHANNEL_ERROR_CODE_TABLE                                                                                  \
    NEKO_CHANNEL_ERROR(Ok, 0, "ok", 0)                                                                                 \
    NEKO_CHANNEL_ERROR(InvalidMessageHeader, 1, "receive an unrecognized message header", -1)                          \
    NEKO_CHANNEL_ERROR(InvalidProtoType, 2, "receive a message, but proto type is not registed", -1)                   \
    NEKO_CHANNEL_ERROR(InvalidProtoData, 3, "receive a message, but proto parser is error", -1)                        \
    NEKO_CHANNEL_ERROR(ProtoVersionUnsupported, 4, "proto version is not supported", -1)                               \
    NEKO_CHANNEL_ERROR(UnrecognizedMessage, 5, "receive a error message in connection state", -1)                      \
    NEKO_CHANNEL_ERROR(Timeout, 6, "the operator is timeout", -1)                                                      \
    NEKO_CHANNEL_ERROR(NoData, 7, "serializer maybe failed, return no data.", -1)                                      \
    NEKO_CHANNEL_ERROR(SerializationError, 10, "serialization failed, return error.", -1)                              \
    NEKO_CHANNEL_ERROR(UnsupportOperator, 11, "unsupported operator", -1)

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

inline auto ErrorCategory::message(int64_t value) const -> std::string {
    switch (value) {
#define NEKO_CHANNEL_ERROR(name, code, message, _)                                                                     \
    case code:                                                                                                         \
        return message;
        NEKO_CHANNEL_ERROR_CODE_TABLE
#undef NEKO_CHANNEL_ERROR
    default:
        return "unknown error";
    }
}

#undef NEKO_CHANNEL_ERROR_CODE_TABLE

enum class StreamFlag {
    None                = 0,
    SerializerInThread  = (1 << 0),
    SliceData           = (1 << 1), // if want to support cancelled send, this must be set. otherwise, it is not needed.
    VersionVerification = (1 << 2), // check version of peer protocolFactory
    RecvUnknownTypeData =
        (1 << 3), // recv unknown type protocol, if type is not registered, it will recv as RawDataMessage.
};

inline StreamFlag operator|(StreamFlag flag1, StreamFlag flag2) {
    return static_cast<StreamFlag>(static_cast<uint32_t>(flag1) | static_cast<uint32_t>(flag2));
}
inline StreamFlag operator&(StreamFlag flag1, StreamFlag flag2) {
    return static_cast<StreamFlag>(static_cast<uint32_t>(flag1) & static_cast<uint32_t>(flag2));
}
inline StreamFlag operator^(StreamFlag flag1, StreamFlag flag2) {
    return static_cast<StreamFlag>(static_cast<uint32_t>(flag1) ^ static_cast<uint32_t>(flag2));
}
inline bool operator!(StreamFlag flag1) { return static_cast<uint32_t>(flag1) == 0; }

template <ILIAS_NAMESPACE::StreamClient T = ILIAS_NAMESPACE::IStreamClient>
class ProtoStreamClient {
    using ClientType = T;
    using IoContext  = ILIAS_NAMESPACE::IoContext;
    template <typename U>
    using IoTask = ILIAS_NAMESPACE::IoTask<U>;
    template <typename U>
    using Result = ILIAS_NAMESPACE::Result<U>;
    template <typename U>
    using Unexpected  = ILIAS_NAMESPACE::Unexpected<U>;
    using Error       = ILIAS_NAMESPACE::Error;
    using SystemError = ILIAS_NAMESPACE::SystemError;

public:
    ProtoStreamClient() = default;
    ProtoStreamClient(ProtoFactory& factory, T&& streamClient);
    ProtoStreamClient(const ProtoStreamClient&) = delete;
    ProtoStreamClient(ProtoStreamClient&& /*othre*/);
    ~ProtoStreamClient() noexcept;
    auto setStreamClient(ClientType&& streamClient, bool reconnect = false) -> void;
    auto send(const IProto& message, StreamFlag flag = StreamFlag::None) -> IoTask<void>;
    auto recv(StreamFlag flag = StreamFlag::None) -> IoTask<IProto>;
    auto close() -> IoTask<void>;
    auto setProtoTable(uint32_t version, const std::map<uint32_t, std::string>& protoTable) -> void;
    auto getProtoTable() const -> const ProtocolTable&;

private:
    auto _sendRaw(std::span<std::byte> data) -> IoTask<void>;
    auto _recvRaw(std::span<std::byte> buf) -> IoTask<void>;
    auto _sendVersion() -> IoTask<void>;
    auto _recvVersion(const MessageHeader& header) -> IoTask<void>;
    auto _sendSlice(std::span<std::byte> data, uint32_t offset) -> IoTask<void>;
    auto _sendCancel(uint32_t data = 0) -> IoTask<void>;
    auto _createProto(uint32_t type) -> IProto;

private:
    ProtoFactory* mFactory               = nullptr;
    ClientType mStreamClient             = {};
    std::vector<std::byte> mBuffer       = {};
    MessageHeader mHeader                = {};
    IProto mMessage                      = {};
    uint32_t mSliceSizeCount             = 0;
    ProtocolTable mProtocolTable         = {};
    static constexpr uint32_t gSliceSize = 1200;
};

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoFactory& factory, T&& streamClient)
    : mFactory(&factory), mStreamClient(std::move(streamClient)) {}

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoStreamClient&& othre)
    : mFactory(othre.mFactory), mStreamClient(std::move(othre.mStreamClient)), mBuffer(std::move(othre.mBuffer)),
      mHeader(std::move(othre.mHeader)), mMessage(std::move(othre.mMessage)), mSliceSizeCount(othre.mSliceSizeCount) {}

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::~ProtoStreamClient() noexcept {}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::setStreamClient(T&& streamClient, bool reconnect) -> void {
    mStreamClient = std::move(streamClient);
    if (!reconnect) {
        mHeader         = {};
        mMessage        = {};
        mSliceSizeCount = 0;
        mBuffer.clear();
    }
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::send(const IProto& message, StreamFlag flag) -> IoTask<void> {
    if (!mStreamClient) {
        NEKO_LOG_ERROR("Communication", "no stream client");
        co_return Unexpected<Error>(ErrorCode::UnsupportOperator);
    }
    bool isVerify = static_cast<int>(flag & StreamFlag::VersionVerification) != 0;
    bool isThread = static_cast<int>(flag & StreamFlag::SerializerInThread) != 0;
    bool isSlice  = static_cast<int>(flag & StreamFlag::SliceData) != 0;

    // verify proto factory
    if (isVerify) {
        auto ret = co_await (_sendVersion() | ILIAS_NAMESPACE::ignoreCancellation);
        if (!ret) {
            NEKO_LOG_WARN("Communication", "send to verification version failed!");
            co_return Unexpected<Error>(ret.error());
        }
    }
    // proto to data
    std::vector<char> messageData;
    if (!isSlice) {
        messageData.resize(MessageHeader::size());
    }
    if (isThread) {
        co_await detail::ThreadAwaiter<bool>{std::bind(
            static_cast<bool (IProto::*)(std::vector<char>&) const>(&IProto::toData), &message, std::ref(messageData))};
    } else {
        message.toData(messageData);
    }
    if (messageData.empty() || (!isSlice && messageData.size() == MessageHeader::size())) {
        co_return Unexpected<Error>(Error(ErrorCode::NoData));
    }
    if (messageData.size() > std::numeric_limits<uint32_t>::max()) {
        co_return Unexpected<Error>(Error::MessageTooLarge);
    }
    if (isSlice) {
        // slice data
        uint32_t offset = 0;
        auto header     = MessageHeader((uint32_t)messageData.size(), message.type(), MessageType::SliceHeader);
        std::vector<char> headerData;
        if (!header.makeProto().toData(headerData)) {
            co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
        }
        auto ret = co_await (_sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()}) |
                             ILIAS_NAMESPACE::ignoreCancellation);
        NEKO_LOG_INFO("Communication", "Sending slice header, protocol: {}, size: {}", message.type(),
                      messageData.size());
        while (true) {
            auto sliceSize =
                std::min(static_cast<uint32_t>(messageData.size()) - offset, gSliceSize - MessageHeader::size());
            NEKO_ASSERT(sliceSize > 0, "Communication", "Slice size is 0");
            if (!ret) {
                if (ret.error() == Error::Canceled) {
                    co_await (_sendCancel(message.type()) | ILIAS_NAMESPACE::ignoreCancellation);
                }
                co_return Unexpected<Error>(ret.error());
            }
            ret = co_await (_sendSlice({reinterpret_cast<std::byte*>(messageData.data() + offset), sliceSize}, offset) |
                            ILIAS_NAMESPACE::ignoreCancellation);
            offset += sliceSize;
            if (offset >= messageData.size()) {
                break;
            }
        }
        co_return Result<void>();
    } else {
        // complete data
        auto header = MessageHeader((uint32_t)(messageData.size() - MessageHeader::size()), message.type(),
                                    MessageType::Complete);
        std::vector<char> headerData;
        if (!header.makeProto().toData(headerData)) {
            co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
        }
        NEKO_ASSERT(headerData.size() == MessageHeader::size(), "Communication", "Header size is not correct");
        memcpy(messageData.data(), headerData.data(), headerData.size());
        NEKO_LOG_INFO("Communication", "Send header: message type: Complete proto type: {} length: {}", header.data,
                      header.length);
        co_return co_await (_sendRaw({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()}) |
                            ILIAS_NAMESPACE::ignoreCancellation);
    }
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::recv(StreamFlag flag) -> IoTask<IProto> {
    if (!mStreamClient) {
        NEKO_LOG_ERROR("Communication", "no stream client");
        co_return Unexpected<Error>(ErrorCode::UnsupportOperator);
    }
    if (static_cast<int>(flag & StreamFlag::SliceData) != 0) {
        NEKO_LOG_WARN("Communication", "recv function can't set StreamFlag::SliceData, ignore it, if you want to recv "
                                       "slice data, please set it in send function.");
    }
    if (static_cast<int>(flag & StreamFlag::VersionVerification) != 0) {
        NEKO_LOG_WARN("Communication",
                      "recv function can't set StreamFlag::VersionVerification, ignore it, if you want to "
                      "verification protofactory version, please set it in send function.");
    }
    bool isComplete = false;
    while (!isComplete) {
        // receive header
        mHeader = MessageHeader();
        std::vector<std::byte> messageHeader(MessageHeader::size());
        auto ret = co_await (_recvRaw(messageHeader) | ILIAS_NAMESPACE::ignoreCancellation);
        if (!ret) {
            co_return Unexpected<Error>(ret.error());
        }
        if (!mHeader.makeProto().fromData(reinterpret_cast<char*>(messageHeader.data()), messageHeader.size())) {
            co_return Unexpected<Error>(Error(ErrorCode::InvalidMessageHeader));
        }
        // process header
        switch (mHeader.messageType) {
        case MessageType::Cancel: {
            NEKO_LOG_INFO("Communication", "recv header: message type: Cancel");
            mBuffer.clear();
            mMessage        = {};
            mSliceSizeCount = 0;
            co_return Unexpected<Error>(Error::Canceled);
        }
        case MessageType::VersionVerification: {
            NEKO_LOG_INFO("Communication", "recv header: message type: VersionVerification, lenght: {}",
                          mHeader.length);
            auto ret2 = co_await (_recvVersion(mHeader) | ILIAS_NAMESPACE::ignoreCancellation);
            if (!ret2) {
                co_return Unexpected<Error>(ret2.error());
            }
            break;
        }
        case MessageType::Complete: {
            NEKO_LOG_INFO("Communication", "recv header: message type: Complete proto type: {} lenght: {}",
                          mHeader.data, mHeader.length);
            mMessage = _createProto(mHeader.data);
            mBuffer.resize(mHeader.length);
            auto ret2 = co_await (_recvRaw(mBuffer) | ILIAS_NAMESPACE::ignoreCancellation);
            if (!ret2) {
                co_return Unexpected<Error>(ret2.error());
            }
            isComplete = true;
            break;
        }
        case MessageType::SliceHeader: {
            NEKO_LOG_INFO("Communication", "recv header: message type: SliceHeader, proto type: {}, size: {}",
                          mHeader.data, mHeader.length);
            mMessage = _createProto(mHeader.data);
            mBuffer.resize(mHeader.length);
            if (mMessage == nullptr) {
                co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoType));
            }
            break;
        }
        case MessageType::Slice: {
            NEKO_ASSERT(mBuffer.size() >= mHeader.data + mHeader.length, "Communication",
                        "buffer size({}) is too small, slice header offset({}) length({}), please check SliceHeader "
                        "tell enough length",
                        mBuffer.size(), mHeader.data, mHeader.length);
            NEKO_LOG_INFO("Communication", "recv message slice: message type: Slice, offset: {}, length: {}",
                          mHeader.data, mHeader.length);
            auto ret2 = co_await _recvRaw({mBuffer.data() + mHeader.data, mHeader.length});
            if (!ret2) {
                co_return Unexpected<Error>(ret2.error());
            }
            mSliceSizeCount += mHeader.length;
            if (mSliceSizeCount == mBuffer.size()) {
                NEKO_LOG_INFO("Communication", "Received complete message, size({})", mSliceSizeCount);
                mSliceSizeCount = 0;
                isComplete      = true;
            }
            break;
        }
        default:
            NEKO_LOG_ERROR("Communication", "Recv unsupported message type: {}.",
                           static_cast<int>(mHeader.messageType));
            co_return Unexpected<Error>(Error(ErrorCode::InvalidMessageHeader));
        }
    }
    if (mMessage == nullptr) {
        if (static_cast<int>(flag & StreamFlag::RecvUnknownTypeData) != 0) {
            mMessage      = mFactory->create(RawDataMessage::specifyType());
            auto* proto   = mMessage.cast<RawDataMessage>();
            proto->type   = mHeader.data;
            proto->length = mHeader.length;
            proto->name   = mProtocolTable.protoTable[mHeader.data];
            proto->data   = std::move(mBuffer);
            co_return std::move(mMessage);
        }
        co_return Unexpected<Error>(Error(ErrorCode::UnrecognizedMessage));
    }
    if (static_cast<int>(flag & StreamFlag::SerializerInThread) != 0) {
        auto ret = co_await detail::ThreadAwaiter<bool>(
            std::bind(&IProto::fromData, &mMessage, reinterpret_cast<char*>(mBuffer.data()), mBuffer.size()));
        if (!ret && !ret.value()) {
            co_return Unexpected<Error>(ret.error_or(Error(ErrorCode::InvalidProtoData)));
        }
    } else {
        if (!mMessage.fromData(reinterpret_cast<char*>(mBuffer.data()), mBuffer.size())) {
            co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoData));
        }
    }
    co_return std::move(mMessage);
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::_recvRaw(std::span<std::byte> buf) -> ILIAS_NAMESPACE::IoTask<void> {
    int readsize = 0;
    while (readsize < buf.size()) {
        Result<size_t> ret = co_await mStreamClient.read({buf.data() + readsize, buf.size() - readsize});
        if (ret) {
            if (ret.value() == 0) {
                co_return Unexpected<Error>(Error::ConnectionReset);
            }
            readsize += (uint32_t)ret.value();
#ifdef _WIN32
        } else if (ret.error() == SystemError(WSAEWOULDBLOCK) || ret.error() == SystemError(WSAEINPROGRESS) ||
                   ret.error() == SystemError(WSAENOBUFS)) {
#else
        } else if (ret.error() == SystemError(EAGAIN) || ret.error() == SystemError(EWOULDBLOCK) ||
                   ret.error() == SystemError(ENOBUFS)) {
#endif
            co_await ILIAS_NAMESPACE::sleep(std::chrono::milliseconds(10));
            continue;
        } else if (ret.error() == SystemError(EINTR)) {
            continue;
        } else {
            co_return Unexpected<Error>(ret.error());
        }
    }
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::_sendVersion() -> IoTask<void> {
    ProtocolTable protocolTable          = {};
    protocolTable.protocolFactoryVersion = mFactory->version();
    for (const auto& [name, type] : ProtoFactory::protoTypeMap()) {
        if (type > NEKO_RESERVED_PROTO_TYPE_SIZE) {
            protocolTable.protoTable[type] = name;
        }
    }
    std::vector<char> data;
    data.resize(MessageHeader::size());
    if (!protocolTable.makeProto().toData(data)) {
        co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
    }
    auto header = MessageHeader((uint32_t)(data.size() - MessageHeader::size()), protocolTable.makeProto().type(),
                                MessageType::VersionVerification);
    std::vector<char> headerData;
    if (!header.makeProto().toData(headerData)) {
        co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
    }
    memcpy(data.data(), headerData.data(), headerData.size());
    NEKO_ASSERT(headerData.size() == MessageHeader::size(), "Coummunication", "header size({}) mismatch",
                headerData.size());
    NEKO_ASSERT(data.size() == headerData.size() + header.length, "Coummunication",
                "data size({}) mismatch. header size({}) + data lenght({})", data.size(), headerData.size(),
                header.length);
    auto ret = co_await _sendRaw({reinterpret_cast<std::byte*>(data.data()), data.size()});
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send version verification message");
        co_return Unexpected<Error>(ret.error());
    }
    NEKO_LOG_INFO("Communication", "Sent version verification message, version: {}", mFactory->version());
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::_recvVersion(const MessageHeader& header) -> IoTask<void> {
    mMessage = IProto{new ProtocolTable::ProtoType{}};
    if (header.data != mMessage.type()) {
        co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoType));
    }
    mBuffer.resize(mHeader.length);
    auto ret = co_await _recvRaw(mBuffer);
    if (!ret) {
        co_return Unexpected<Error>(ret.error());
    }
    if (!mMessage.fromData(reinterpret_cast<char*>(mBuffer.data()), mBuffer.size())) {
        co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoData));
    }
    auto* protoTable = mMessage.cast<ProtocolTable>();
    if (protoTable->protocolFactoryVersion != mFactory->version()) {
        NEKO_LOG_WARN("Communication", "ProtoFactory version mismatch: {} != {}", mHeader.data, mFactory->version());
    }
    mProtocolTable = *protoTable;
    NEKO_LOG_INFO("Communication", "sync message proto table, version: {}. size: {}",
                  mProtocolTable.protocolFactoryVersion, mProtocolTable.protoTable.size());
    for (const auto& [type, name] : mProtocolTable.protoTable) {
        NEKO_LOG_INFO("Communication", "Proto table: {} -> {}", name, type);
    }
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::_sendSlice(std::span<std::byte> data, const uint32_t offset) -> IoTask<void> {
    auto header = MessageHeader((uint32_t)data.size(), offset, MessageType::Slice);
    std::vector<char> headerData;
    if (!header.makeProto().toData(headerData)) {
        co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
    }
    auto ret = co_await _sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    NEKO_LOG_INFO("Communication", "Sending slice, offset: {}, length: {}", offset, data.size());
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send message");
        co_return Unexpected<Error>(ret.error());
    }
    ret = co_await _sendRaw(data);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send slice");
        co_return Unexpected<Error>(ret.error());
    }
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::_sendCancel(const uint32_t data) -> IoTask<void> {
    auto header = MessageHeader(0, data, MessageType::Cancel);
    std::vector<char> headerData;
    if (!header.makeProto().toData(headerData)) {
        co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
    }
    auto ret = co_await _sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::_createProto(const uint32_t type) -> IProto {
    if (type <= NEKO_RESERVED_PROTO_TYPE_SIZE || mProtocolTable.protoTable.empty()) {
        return mFactory->create(type);
    }
    auto it = mProtocolTable.protoTable.find(type);
    if (it != mProtocolTable.protoTable.end()) {
        return mFactory->create(it->second.c_str());
    }
    return {};
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::close() -> IoTask<void> {
    if (!mStreamClient) {
        co_return {};
    }
    co_return co_await mStreamClient.shutdown();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::setProtoTable(const uint32_t version,
                                                const std::map<uint32_t, std::string>& protoTable) -> void {
    mProtocolTable = ProtocolTable{version, protoTable};
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::getProtoTable() const -> const ProtocolTable& {
    return mProtocolTable;
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::_sendRaw(std::span<std::byte> data) -> IoTask<void> {
    int sended = 0;
    while (sended < data.size()) {
        Result<size_t> ret = co_await mStreamClient.write({data.data() + sended, data.size() - sended});
        if (ret) {
            if (ret.value() == 0) {
                co_return Unexpected<Error>(Error::ConnectionReset);
            }
            sended += (uint32_t)ret.value();
#ifdef _WIN32
        } else if (ret.error() == SystemError(WSAEWOULDBLOCK) || ret.error() == SystemError(WSAEINPROGRESS) ||
                   ret.error() == SystemError(WSAENOBUFS)) {
#else
        } else if (ret.error() == SystemError(EAGAIN) || ret.error() == SystemError(EWOULDBLOCK) ||
                   ret.error() == SystemError(ENOBUFS)) {
#endif
            co_await ILIAS_NAMESPACE::sleep(std::chrono::milliseconds(10));
            continue;
        } else if (ret.error() == SystemError(EINTR)) {
            continue;
        } else {
            co_return Unexpected<Error>(ret.error());
        }
    }
    co_return Result<void>();
}

template <typename T = ILIAS_NAMESPACE::UdpClient>
class ProtoDatagramClient {
    using ClientType = T;
    using IoContext  = ILIAS_NAMESPACE::IoContext;
    template <typename U>
    using IoTask = ILIAS_NAMESPACE::IoTask<U>;
    template <typename U>
    using Result     = ILIAS_NAMESPACE::Result<U>;
    using IPEndpoint = ILIAS_NAMESPACE::IPEndpoint;
    template <typename U>
    using Unexpected  = ILIAS_NAMESPACE::Unexpected<U>;
    using Error       = ILIAS_NAMESPACE::Error;
    using SystemError = ILIAS_NAMESPACE::SystemError;

public:
    ProtoDatagramClient() = default;
    ProtoDatagramClient(ProtoFactory& factory, ClientType&& streamClient);
    ProtoDatagramClient(const ProtoDatagramClient&) = delete;
    ProtoDatagramClient(ProtoDatagramClient&& /*other*/);
    ~ProtoDatagramClient() noexcept;
    auto setStreamClient(ClientType&& streamClient, bool reset = false) -> void;
    auto send(const IProto& message, const IPEndpoint& endpoint, StreamFlag flag = StreamFlag::None) -> IoTask<void>;
    auto recv(StreamFlag flag = StreamFlag::None) -> IoTask<std::pair<IProto, IPEndpoint>>;
    auto close() -> IoTask<void>;
    auto setProtoTable(uint32_t version, const std::map<uint32_t, std::string>& protoTable) -> void;
    auto getProtoTable() const -> const ProtocolTable&;

private:
    auto _sendVersion(const IPEndpoint& endpoint) -> IoTask<void>;
    auto _recvVersion(const MessageHeader& header) -> IoTask<void>;
    auto _createProto(uint32_t type) -> IProto;

private:
    ProtoFactory* mFactory                = nullptr;
    ClientType mDatagramClient            = {};
    std::vector<std::byte> mBuffer        = {};
    MessageHeader mHeader                 = {};
    IProto mMessage                       = {};
    std::set<uint32_t> mSliceIdCount      = {};
    uint32_t mSliceSizeCount              = 0;
    ProtocolTable mProtocolTable          = {};
    static constexpr uint32_t gSliceSize  = 1200;
    static constexpr uint32_t gUdpMaxSize = 65535;
};

template <typename T>
inline ProtoDatagramClient<T>::ProtoDatagramClient(ProtoFactory& factory, ClientType&& client)
    : mFactory(&factory), mDatagramClient(std::move(client)) {
    mBuffer.reserve(gUdpMaxSize);
}

template <typename T>
inline ProtoDatagramClient<T>::ProtoDatagramClient(ProtoDatagramClient<T>&& other) {
    mFactory        = other.mFactory;
    mDatagramClient = std::move(other.mDatagramClient);
    mBuffer         = std::move(other.mBuffer);
    mHeader         = std::move(other.mHeader);
    mMessage        = std::move(other.mMessage);
    mSliceIdCount   = std::move(other.mSliceIdCount);
}

template <typename T>
inline ProtoDatagramClient<T>::~ProtoDatagramClient() noexcept {}

template <typename T>
inline auto ProtoDatagramClient<T>::setStreamClient(ClientType&& streamClient, const bool reset) -> void {
    mDatagramClient = std::move(streamClient);
    if (reset) {
        mBuffer.clear();
        mHeader  = {};
        mMessage = IProto{};
        mSliceIdCount.clear();
    }
}

template <typename T>
inline auto ProtoDatagramClient<T>::send(const IProto& message, const IPEndpoint& endpoint, StreamFlag flag)
    -> IoTask<void> {
    if (!mDatagramClient) {
        NEKO_LOG_ERROR("Communication", "no datagram client");
        co_return Unexpected<Error>(ErrorCode::UnsupportOperator);
    }
    bool isVerify = static_cast<int>(flag & StreamFlag::VersionVerification) != 0;
    bool isThread = static_cast<int>(flag & StreamFlag::SerializerInThread) != 0;
    bool isSlice  = static_cast<int>(flag & StreamFlag::SliceData) != 0;

    if (isSlice) {
        NEKO_LOG_ERROR("Communication", "slice data not support in datagram client!");
        co_return Unexpected<Error>(Error(ErrorCode::UnsupportOperator));
    }
    // verify proto factory
    if (isVerify) {
        auto ret = co_await (_sendVersion(endpoint) | ILIAS_NAMESPACE::ignoreCancellation);
        if (!ret) {
            NEKO_LOG_WARN("Communication", "send to verification version failed! error: {}", ret.error().toString());
        }
    }
    // proto to data
    std::vector<char> messageData;
    messageData.resize(MessageHeader::size(), 0);
    if (isThread) {
        co_await detail::ThreadAwaiter<bool>{std::bind(
            static_cast<bool (IProto::*)(std::vector<char>&) const>(&IProto::toData), &message, std::ref(messageData))};
    } else {
        message.toData(messageData);
    }
    if (messageData.empty() || messageData.size() == MessageHeader::size()) {
        co_return Unexpected<Error>(Error(ErrorCode::NoData));
    }
    if (messageData.size() > 65527) {
        co_return Unexpected<Error>(Error::MessageTooLarge);
    }
    // complete data
    auto header =
        MessageHeader((uint32_t)(messageData.size() - MessageHeader::size()), message.type(), MessageType::Complete);
    std::vector<char> headerData;
    if (!header.makeProto().toData(headerData)) {
        co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
    }
    NEKO_ASSERT(headerData.size() == MessageHeader::size(), "Communication", "Header size error");
    memcpy(messageData.data(), headerData.data(), headerData.size());
    NEKO_LOG_INFO("Communication", "Send header: message type: Complete proto type: {} size: {}", header.data,
                  header.length);
    auto ret = co_await (
        mDatagramClient.sendto({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()}, endpoint) |
        ILIAS_NAMESPACE::ignoreCancellation);
    if (!ret) {
        co_return Unexpected<Error>(ret.error());
    }
    if (ret.value() != messageData.size()) {
        NEKO_LOG_ERROR("Communication", "Send data error, expect: {} -actual: {}", messageData.size(), ret.value());
        co_return Unexpected<Error>(Error::MessageTooLarge);
    }
    co_return Result<void>();
}

template <typename T>
inline auto ProtoDatagramClient<T>::recv(StreamFlag flag) -> IoTask<std::pair<IProto, IPEndpoint>> {
    if (!mDatagramClient) {
        NEKO_LOG_ERROR("Communication", "no datagram client");
        co_return Unexpected<Error>(ErrorCode::UnsupportOperator);
    }
    if (static_cast<int>(flag & StreamFlag::SliceData) != 0) {
        NEKO_LOG_WARN("Communication", "recv function can't set StreamFlag::SliceData, ignore it, if you want to recv "
                                       "slice data, please set it in send function.");
    }
    if (static_cast<int>(flag & StreamFlag::VersionVerification) != 0) {
        NEKO_LOG_WARN("Communication",
                      "recv function can't set StreamFlag::VersionVerification, ignore it, if you want to "
                      "verification protofactory version, please set it in send function.");
    }
    IPEndpoint endpoint;
    bool isComplete   = false;
    uint32_t recvSize = 0;
    while (!isComplete) {
        mBuffer.resize(gUdpMaxSize, std::byte(0));
        auto ret = co_await (mDatagramClient.recvfrom(mBuffer, endpoint) | ILIAS_NAMESPACE::ignoreCancellation);
        // receive header
        if (!ret) {
            NEKO_LOG_ERROR("Communication", "Recv message header error: {}", ret.error().toString());
            co_return Unexpected<Error>(ret.error());
        }
        recvSize = (uint32_t)ret.value();
        if (recvSize < (uint32_t)MessageHeader::size()) {
            NEKO_LOG_ERROR("Communication", "Recv message header error: recv size({}) < MessageHeader::size({})",
                           recvSize, MessageHeader::size());
            co_return Unexpected<Error>(Error(ErrorCode::InvalidMessageHeader));
        }
        mHeader = MessageHeader();
        if (!mHeader.makeProto().fromData(reinterpret_cast<char*>(mBuffer.data()), MessageHeader::size())) {
            NEKO_LOG_ERROR("Communication", "Recv message header error: deserialize error");
            co_return Unexpected<Error>(Error(ErrorCode::InvalidMessageHeader));
        }
        // process header
        switch (mHeader.messageType) {
        case MessageType::VersionVerification: {
            NEKO_LOG_INFO("Communication", "recv header: message type: VersionVerification, lenght: {}",
                          mHeader.length);
            auto ret2 = co_await (_recvVersion(mHeader) | ILIAS_NAMESPACE::ignoreCancellation);
            if (!ret2) {
                co_return Unexpected<Error>(ret2.error());
            }
            break;
        }
        case MessageType::Complete: {
            NEKO_LOG_INFO("Communication", "Recv header: message type: Complete proto type: {} size: {}", mHeader.data,
                          mHeader.length);
            mMessage = _createProto(mHeader.data);
            if (!ret) {
                NEKO_LOG_ERROR("Communication", "Recv message data error: {}", ret.error().toString());
                co_return Unexpected<Error>(ret.error());
            }
            if (ret.value() - MessageHeader::size() != mHeader.length) {
                NEKO_LOG_ERROR("Communication", "Recv message length mismatch: {} != {}",
                               ret.value() - MessageHeader::size(), mHeader.length);
                co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoData));
            }
            isComplete = true;
            break;
        }
        case MessageType::Cancel:
        case MessageType::SliceHeader:
        case MessageType::Slice:
        default:
            NEKO_LOG_ERROR("Communication", "Recv unsupported message type: {}.",
                           static_cast<int>(mHeader.messageType));
            co_return Unexpected<Error>(Error(ErrorCode::InvalidMessageHeader));
        }
    }
    if (mMessage == nullptr) {
        if (static_cast<int>(flag & StreamFlag::RecvUnknownTypeData) != 0) {
            mMessage      = mFactory->create(RawDataMessage::specifyType());
            auto* proto   = mMessage.cast<RawDataMessage>();
            proto->type   = mHeader.data;
            proto->length = mHeader.length;
            proto->name   = mProtocolTable.protoTable[mHeader.data];
            proto->data   = std::move(mBuffer);
            co_return std::make_pair(std::move(mMessage), endpoint);
        }
        co_return Unexpected<Error>(Error(ErrorCode::UnrecognizedMessage));
    }
    if (static_cast<int>(flag & StreamFlag::SerializerInThread) != 0) {
        auto ret = co_await detail::ThreadAwaiter<bool>(
            std::bind(&IProto::fromData, &mMessage, reinterpret_cast<char*>(mBuffer.data() + MessageHeader::size()),
                      recvSize - MessageHeader::size()));
        if (!ret && !ret.value()) {
            co_return Unexpected<Error>(ret.error_or(Error(ErrorCode::InvalidProtoData)));
        }
    } else {
        if (!mMessage.fromData(reinterpret_cast<char*>(mBuffer.data() + MessageHeader::size()),
                               recvSize - MessageHeader::size())) {
            co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoData));
        }
    }
    co_return std::make_pair(std::move(mMessage), endpoint);
}

template <typename T>
inline auto ProtoDatagramClient<T>::close() -> IoTask<void> {
    if (!mDatagramClient) {
        co_return {};
    }
    mDatagramClient.close();
    co_return Result<void>{};
}

template <typename T>
inline auto ProtoDatagramClient<T>::setProtoTable(const uint32_t version,
                                                  const std::map<uint32_t, std::string>& protoTable) -> void {
    mProtocolTable = ProtocolTable{version, protoTable};
}

template <typename T>
inline auto ProtoDatagramClient<T>::getProtoTable() const -> const ProtocolTable& {
    return mProtocolTable;
}

template <typename T>
inline auto ProtoDatagramClient<T>::_sendVersion(const IPEndpoint& endpoint) -> IoTask<void> {
    ProtocolTable protocolTable          = {};
    protocolTable.protocolFactoryVersion = mFactory->version();
    for (const auto& [name, type] : ProtoFactory::protoTypeMap()) {
        if (type > NEKO_RESERVED_PROTO_TYPE_SIZE) {
            protocolTable.protoTable[type] = name;
        }
    }
    std::vector<char> data;
    data.resize(MessageHeader::size());
    if (!protocolTable.makeProto().toData(data)) {
        co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
    }
    auto header = MessageHeader((uint32_t)(data.size() - MessageHeader::size()), protocolTable.makeProto().type(),
                                MessageType::VersionVerification);
    std::vector<char> headerData;
    if (!header.makeProto().toData(headerData)) {
        co_return Unexpected<Error>(Error(ErrorCode::SerializationError));
    }
    memcpy(data.data(), headerData.data(), headerData.size());
    NEKO_ASSERT(headerData.size() == MessageHeader::size(), "Coummunication", "header size({}) mismatch",
                headerData.size());
    NEKO_ASSERT(data.size() == headerData.size() + header.length, "Coummunication",
                "data size({}) mismatch. header size({}) + data lenght({})", data.size(), headerData.size(),
                header.length);
    auto ret = co_await mDatagramClient.sendto({reinterpret_cast<std::byte*>(data.data()), data.size()}, endpoint);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send version verification message");
        co_return Unexpected<Error>(ret.error());
    }
    NEKO_LOG_INFO("Communication", "Sent version verification message, version: {}", mFactory->version());
    co_return Result<void>();
}

template <typename T>
inline auto ProtoDatagramClient<T>::_recvVersion(const MessageHeader& header) -> IoTask<void> {
    mMessage = IProto{new ProtocolTable::ProtoType{}};
    if (header.data != mMessage.type()) {
        co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoType));
    }
    if (!mMessage.fromData(reinterpret_cast<char*>(mBuffer.data() + MessageHeader::size()),
                           mBuffer.size() - MessageHeader::size())) {
        co_return Unexpected<Error>(Error(ErrorCode::InvalidProtoData));
    }
    auto* protoTable = mMessage.cast<ProtocolTable>();
    if (protoTable->protocolFactoryVersion != mFactory->version()) {
        NEKO_LOG_WARN("Communication", "ProtoFactory version mismatch: {} != {}", mHeader.data, mFactory->version());
    }
    mProtocolTable = *protoTable;
    NEKO_LOG_INFO("Communication", "sync message proto table, version: {}. size: {}",
                  mProtocolTable.protocolFactoryVersion, mProtocolTable.protoTable.size());
    for (const auto& [type, name] : mProtocolTable.protoTable) {
        NEKO_LOG_INFO("Communication", "Proto table: {} -> {}", name, type);
    }
    co_return Result<void>();
}

template <typename T>
inline auto ProtoDatagramClient<T>::_createProto(const uint32_t type) -> IProto {
    if (type <= NEKO_RESERVED_PROTO_TYPE_SIZE || mProtocolTable.protoTable.empty()) {
        NEKO_LOG_INFO("Communication", "create proto by factory: {}", type);
        return mFactory->create(type);
    }
    auto it = mProtocolTable.protoTable.find(type);
    if (it != mProtocolTable.protoTable.end()) {
        NEKO_LOG_INFO("Communication", "create proto by table: {}: {}", it->second, type);
        return mFactory->create(it->second.c_str());
    }
    return {};
}

NEKO_END_NAMESPACE
