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

#include <ilias/defines.hpp>
#include <ilias/io/dyn_traits.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <ilias/result.hpp>
#include <ilias/task.hpp>
#include <ilias/task/task.hpp>
#include <ilias/task/utils.hpp>
#include <set>
#include <system_error>

#include "nekoproto/global/global.hpp"
#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/transport/endpoint.hpp"

namespace nekoproto {

using Error = std::error_code;
using ilias::blocking;
using ilias::DynStream;
using ilias::Err;
using ilias::hostToNetwork;
using ilias::IoContext;
using ilias::IoError;
using ilias::IoTask;
using ilias::IPEndpoint;
using ilias::networkToHost;
using ilias::Task;
using ilias::unstoppable;

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
    MessageHeader(uint32_t length = 0, int32_t data = 0, uint16_t message_type = 0)
        : length(length), data(data), message_type(message_type) {}
    static auto size() -> int { return 10; }
    uint32_t length = 0; // 4 : the length of the message, no't contain the header
    int32_t data    = 0; // 4 : the proto type of this message in Complete message or the slice index in Slice message
    uint16_t message_type = 0; // 2 : the type of this message

    // clang-format off
    struct Neko {
        static constexpr auto value = Object(
            "length",      makeTags<BinaryTag{.fixed_length = sizeof(uint32_t)}>(&MessageHeader::length), 
            "data",        makeTags<BinaryTag{.fixed_length = sizeof(int32_t)}>(&MessageHeader::data), 
            "messageType", makeTags<BinaryTag{.fixed_length = sizeof(uint16_t)}>(&MessageHeader::message_type));
    };
    // clang-format on
};

struct ProtocolTable {
    uint32_t protocol_factory_version           = 0;
    std::map<uint32_t, std::string> proto_table = {};

    // clang-format off
    struct Neko {
        static constexpr auto value = Object(
            "protocol_factory_version", &ProtocolTable::protocol_factory_version, 
            "proto_table",              &ProtocolTable::proto_table);
    };
    // clang-format on
    NEKO_DECLARE_PROTOCOL(ProtocolTable, BinarySerializer)
private:
    static auto specifyType() -> int { return 2; }
    friend class detail::ProtoMethodAccess;
};

struct RawDataMessage {
    RawDataMessage(uint32_t length = 0, int32_t type = 0, const std::string& name = "unknown")
        : length(length), type(type), name(name) {}
    uint32_t length = 0;
    int32_t type    = 0;
    std::string name;
    std::vector<std::byte> data;

    // clang-format off
    struct Neko {
        static constexpr auto value = Object(
            "length",      &RawDataMessage::length, 
            "type",        &RawDataMessage::type,
            "name",        &RawDataMessage::name,
            "data",        &RawDataMessage::data);
    };
    // clang-format on
    NEKO_DECLARE_PROTOCOL(RawDataMessage, BinarySerializer)
    static auto specifyType() -> int { return 3; }

private:
    friend class detail::ProtoMethodAccess;
};

enum class ErrorCode {
    Ok                      = 0,
    InvalidMessageHeader    = 1,
    InvalidProtoType        = 2,
    InvalidProtoData        = 3,
    ProtoVersionUnsupported = 4,
    UnrecognizedMessage     = 5,
    Timeout                 = 6,
    NoData                  = 7,
    SerializationError      = 10,
    UnsupportOperator       = 11,
};

class NEKO_PROTO_API ErrorCategory : public std::error_category {
public:
    static auto instance() -> const ErrorCategory&;
    auto message(int value) const -> std::string override;
    auto name() const noexcept -> const char* override;
    auto equivalent(int value, const std::error_condition& other) const noexcept -> bool override;
};

inline auto ErrorCategory::message(int value) const -> std::string {
    switch (static_cast<ErrorCode>(value)) {
    case ErrorCode::Ok:
        return "ok";
    case ErrorCode::InvalidMessageHeader:
        return "receive an unrecognized message header";
    case ErrorCode::InvalidProtoType:
        return "receive a message, but proto type is not registed";
    case ErrorCode::InvalidProtoData:
        return "receive a message, but proto parser is error";
    case ErrorCode::ProtoVersionUnsupported:
        return "proto version is not supported";
    case ErrorCode::UnrecognizedMessage:
        return "receive a error message in connection state";
    case ErrorCode::Timeout:
        return "the operator is timeout";
    case ErrorCode::NoData:
        return "serializer maybe failed, return no data.";
    case ErrorCode::SerializationError:
        return "serialization failed, return error.";
    case ErrorCode::UnsupportOperator:
        return "unsupported operator";
    default:
        return "unknown error";
    }
}

inline auto make_error_code(ErrorCode code) -> std::error_code {
    return std::error_code(static_cast<int>(code), ErrorCategory::instance());
}
} // namespace nekoproto

template <>
struct std::is_error_code_enum<nekoproto::ErrorCode> : std::true_type {};

namespace nekoproto {

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

namespace detail {

class ProtoClientBase {
protected:
    ProtoClientBase() = default;
    explicit ProtoClientBase(ProtoFactory& factory) : mFactory(&factory) {}
    ProtoClientBase(const ProtoClientBase&) = delete;
    ProtoClientBase(ProtoClientBase&&)      = default;

    auto serializeMessageData(const IProto& message, bool runInThread, bool reserveHeader) const
        -> IoTask<std::vector<char>>;
    auto serializeHeader(const MessageHeader& header) const -> std::vector<char>;
    auto serializeVersionPacket() const -> IoTask<std::vector<char>>;
    auto syncProtocolTable(std::span<std::byte> payload, const MessageHeader& header) -> IoTask<void>;
    auto finishMessage(IProto message, const MessageHeader& header, std::vector<std::byte>&& payload, StreamFlag flag)
        -> IoTask<IProto>;
    auto createProto(uint32_t type) const -> IProto;

protected:
    ProtoFactory* mFactory       = nullptr;
    ProtocolTable mProtocolTable = {};
};

inline auto ProtoClientBase::serializeMessageData(const IProto& message, bool runInThread, bool reserveHeader) const
    -> IoTask<std::vector<char>> {
    std::vector<char> data;
    if (reserveHeader) {
        data.resize(MessageHeader::size());
    }
    bool ok = false;
    if (runInThread) {
        ok = co_await blocking([&]() -> bool { return message.toData(data); });
    } else {
        ok = message.toData(data);
    }
    if (!ok) {
        data.clear();
    }
    co_return data;
}

inline auto ProtoClientBase::serializeHeader(const MessageHeader& header) const -> std::vector<char> {
    std::vector<char> headerData;
    BinarySerializer::OutputSerializer serializer(headerData);
    if (!serializer(makeTags<BinaryTag{.raw_fixed_data = true}>(header)) || !serializer.end()) {
        headerData.clear();
    }
    return headerData;
}

inline auto ProtoClientBase::serializeVersionPacket() const -> IoTask<std::vector<char>> {
    ProtocolTable protocolTable = {};
    if (mFactory != nullptr) {
        protocolTable.protocol_factory_version = mFactory->version();
        for (const auto& [name, type] : ProtoFactory::protoTypeMap()) {
            if (type > reserved_proto_type_size) {
                protocolTable.proto_table[type] = name;
            }
        }
    }

    IProto proto = protocolTable.makeProto();
    auto dataRet = co_await serializeMessageData(proto, false, true);
    if (!dataRet) {
        co_return Err(dataRet.error());
    }
    auto data = std::move(dataRet.value());
    if (data.empty() || static_cast<int>(data.size()) == MessageHeader::size()) {
        co_return std::vector<char>{};
    }

    auto headerData = serializeHeader(MessageHeader(static_cast<uint32_t>(data.size() - MessageHeader::size()),
                                                    proto.type(), MessageType::VersionVerification));
    if (headerData.empty()) {
        co_return std::vector<char>{};
    }
    memcpy(data.data(), headerData.data(), headerData.size());
    co_return data;
}

inline auto ProtoClientBase::syncProtocolTable(std::span<std::byte> payload, const MessageHeader& header)
    -> IoTask<void> {
    IProto proto{new ProtocolTable::ProtoType{}};
    if (header.data != proto.type()) {
        co_return Err(Error(ErrorCode::InvalidProtoType));
    }
    if (!proto.fromData(reinterpret_cast<char*>(payload.data()), payload.size())) {
        co_return Err(Error(ErrorCode::InvalidProtoData));
    }

    auto* protoTable = proto.cast<ProtocolTable>();
    if (mFactory != nullptr && protoTable->protocol_factory_version != mFactory->version()) {
        NEKO_LOG_WARN("Communication", "ProtoFactory version mismatch: {} != {}", protoTable->protocol_factory_version,
                      mFactory->version());
    }
    mProtocolTable = *protoTable;
    NEKO_LOG_INFO("Communication", "sync message proto table, version: {}. size: {}",
                  mProtocolTable.protocol_factory_version, mProtocolTable.proto_table.size());
    for (const auto& [type, name] : mProtocolTable.proto_table) {
        NEKO_LOG_INFO("Communication", "Proto table: {} -> {}", name, type);
    }
    co_return {};
}

inline auto ProtoClientBase::finishMessage(IProto message, const MessageHeader& header,
                                           std::vector<std::byte>&& payload, StreamFlag flag) -> IoTask<IProto> {
    if (message == nullptr) {
        if (static_cast<int>(flag & StreamFlag::RecvUnknownTypeData) != 0) {
            auto rawData  = mFactory->create(RawDataMessage::specifyType());
            auto* proto   = rawData.cast<RawDataMessage>();
            proto->type   = header.data;
            proto->length = header.length;
            proto->name   = mProtocolTable.proto_table[header.data];
            proto->data   = std::move(payload);
            co_return rawData;
        }
        co_return Err(Error(ErrorCode::UnrecognizedMessage));
    }

    const auto parsePayload = [&]() -> bool {
        return message.fromData(reinterpret_cast<char*>(payload.data()), payload.size());
    };
    if (static_cast<int>(flag & StreamFlag::SerializerInThread) != 0) {
        auto ret = co_await blocking(parsePayload);
        if (!ret) {
            co_return Err(Error(ErrorCode::InvalidProtoData));
        }
    } else if (!parsePayload()) {
        co_return Err(Error(ErrorCode::InvalidProtoData));
    }
    co_return std::move(message);
}

inline auto ProtoClientBase::createProto(const uint32_t type) const -> IProto {
    if (mFactory == nullptr) {
        return {};
    }
    if (type <= reserved_proto_type_size || mProtocolTable.proto_table.empty()) {
        return mFactory->create(type);
    }
    auto it = mProtocolTable.proto_table.find(type);
    if (it != mProtocolTable.proto_table.end()) {
        return mFactory->create(it->second.c_str());
    }
    return {};
}

} // namespace detail

template <CommunicationStream T = DynStream>
class ProtoStreamClient : private detail::ProtoClientBase {
    using ClientType = T;
    using Base       = detail::ProtoClientBase;

public:
    ProtoStreamClient() = default;
    ProtoStreamClient(ProtoFactory& factory, T&& streamClient);
    ProtoStreamClient(const ProtoStreamClient&) = delete;
    ProtoStreamClient(ProtoStreamClient&& /*other*/);
    ~ProtoStreamClient() noexcept;
    void setStreamClient(ClientType&& streamClient, bool reconnect = false);
    auto send(const IProto& message, StreamFlag flag = StreamFlag::None) -> IoTask<void>;
    auto recv(StreamFlag flag = StreamFlag::None) -> IoTask<IProto>;
    auto close() -> IoTask<void>;
    void setProtoTable(uint32_t version, const std::map<uint32_t, std::string>& protoTable);
    auto getProtoTable() const -> const ProtocolTable&;

private:
    auto sendRaw(std::span<std::byte> data) -> IoTask<void>;
    auto recvRaw(std::span<std::byte> buf) -> IoTask<void>;
    auto sendVersion() -> IoTask<void>;
    auto recvVersion(const MessageHeader& header) -> IoTask<void>;
    auto sendSlice(std::span<std::byte> data, uint32_t offset) -> IoTask<void>;
    auto sendCancel(uint32_t data = 0) -> IoTask<void>;

private:
    ClientType mStreamClient             = {};
    std::vector<std::byte> mBuffer       = {};
    MessageHeader mHeader                = {};
    IProto mMessage                      = {};
    uint32_t mSliceSizeCount             = 0;
    static constexpr uint32_t gSliceSize = 1200;
};

template <CommunicationStream T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoFactory& factory, T&& streamClient)
    : Base(factory), mStreamClient(std::move(streamClient)) {}

template <CommunicationStream T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoStreamClient&& other)
    : Base(std::move(other)), mStreamClient(std::move(other.mStreamClient)), mBuffer(std::move(other.mBuffer)),
      mHeader(std::move(other.mHeader)), mMessage(std::move(other.mMessage)), mSliceSizeCount(other.mSliceSizeCount) {}

template <CommunicationStream T>
inline ProtoStreamClient<T>::~ProtoStreamClient() noexcept {}

template <CommunicationStream T>
inline void ProtoStreamClient<T>::setStreamClient(T&& streamClient, bool reconnect) {
    mStreamClient = std::move(streamClient);
    if (!reconnect) {
        mHeader         = {};
        mMessage        = {};
        mSliceSizeCount = 0;
        mBuffer.clear();
    }
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::send(const IProto& message, StreamFlag flag) -> IoTask<void> {
    if (!mStreamClient) {
        NEKO_LOG_ERROR("Communication", "no stream client");
        co_return Err(ErrorCode::UnsupportOperator);
    }

    const bool isVerify = static_cast<int>(flag & StreamFlag::VersionVerification) != 0;
    const bool isThread = static_cast<int>(flag & StreamFlag::SerializerInThread) != 0;
    const bool isSlice  = static_cast<int>(flag & StreamFlag::SliceData) != 0;

    if (isVerify) {
        auto ret = co_await (sendVersion() | unstoppable);
        if (!ret) {
            NEKO_LOG_WARN("Communication", "send to verification version failed!");
            co_return Err(ret.error());
        }
    }

    auto messageDataRet = co_await Base::serializeMessageData(message, isThread, !isSlice);
    if (!messageDataRet) {
        co_return Err(messageDataRet.error());
    }
    auto messageData = std::move(messageDataRet.value());
    if (messageData.empty() || (!isSlice && static_cast<int>(messageData.size()) == MessageHeader::size())) {
        co_return Err(ErrorCode::NoData);
    }
    if (messageData.size() > std::numeric_limits<uint32_t>::max()) {
        co_return Err(IoError::MessageTooLarge);
    }

    if (isSlice) {
        uint32_t offset = 0;
        auto headerData = Base::serializeHeader(
            MessageHeader(static_cast<uint32_t>(messageData.size()), message.type(), MessageType::SliceHeader));
        if (headerData.empty()) {
            co_return Err(ErrorCode::SerializationError);
        }
        auto ret =
            co_await (sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()}) | unstoppable);
        NEKO_LOG_INFO("Communication", "Sending slice header, protocol: {}, size: {}", message.type(),
                      messageData.size());
        while (true) {
            auto sliceSize =
                std::min(static_cast<uint32_t>(messageData.size()) - offset, gSliceSize - MessageHeader::size());
            NEKO_ASSERT(sliceSize > 0, "Communication", "Slice size is 0");
            if (!ret) {
                if (ret.error() == IoError::Canceled) {
                    co_await (sendCancel(message.type()) | unstoppable);
                }
                co_return Err(ret.error());
            }
            ret = co_await (sendSlice({reinterpret_cast<std::byte*>(messageData.data() + offset), sliceSize}, offset) |
                            unstoppable);
            offset += sliceSize;
            if (offset >= messageData.size()) {
                break;
            }
        }
        co_return {};
    }

    auto headerData = Base::serializeHeader(MessageHeader(
        static_cast<uint32_t>(messageData.size() - MessageHeader::size()), message.type(), MessageType::Complete));
    if (headerData.empty()) {
        co_return Err(Error(ErrorCode::SerializationError));
    }
    NEKO_ASSERT(static_cast<int>(headerData.size()) == MessageHeader::size(), "Communication",
                "Header size is not correct");
    memcpy(messageData.data(), headerData.data(), headerData.size());
    NEKO_LOG_INFO("Communication", "Send header: message type: Complete proto type: {} length: {}", message.type(),
                  messageData.size() - MessageHeader::size());
    co_return co_await (sendRaw({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()}) | unstoppable);
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::recv(StreamFlag flag) -> IoTask<IProto> {
    if (!mStreamClient) {
        NEKO_LOG_ERROR("Communication", "no stream client");
        co_return Err(ErrorCode::UnsupportOperator);
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
        mHeader = MessageHeader();
        std::vector<std::byte> messageHeader(MessageHeader::size());
        auto ret = co_await (recvRaw(messageHeader));
        if (!ret) {
            co_return Err(ret.error());
        }
        BinarySerializer::InputSerializer serializer(reinterpret_cast<char*>(messageHeader.data()),
                                                     messageHeader.size());
        auto taggedHeader = makeTags<BinaryTag{.raw_fixed_data = true}>(mHeader);
        if (!serializer(taggedHeader)) {
            co_return Err(Error(ErrorCode::InvalidMessageHeader));
        }

        switch (mHeader.message_type) {
        case MessageType::Cancel:
            NEKO_LOG_INFO("Communication", "recv header: message type: Cancel");
            mBuffer.clear();
            mMessage        = {};
            mSliceSizeCount = 0;
            co_return Err(IoError::Canceled);
        case MessageType::VersionVerification: {
            NEKO_LOG_INFO("Communication", "recv header: message type: VersionVerification, lenght: {}",
                          mHeader.length);
            auto ret2 = co_await (recvVersion(mHeader));
            if (!ret2) {
                co_return Err(ret2.error());
            }
            break;
        }
        case MessageType::Complete:
            NEKO_LOG_INFO("Communication", "recv header: message type: Complete proto type: {} lenght: {}",
                          mHeader.data, mHeader.length);
            mMessage = Base::createProto(mHeader.data);
            mBuffer.resize(mHeader.length);
            if (auto ret2 = co_await (recvRaw(mBuffer)); !ret2) {
                co_return Err(ret2.error());
            }
            isComplete = true;
            break;
        case MessageType::SliceHeader:
            NEKO_LOG_INFO("Communication", "recv header: message type: SliceHeader, proto type: {}, size: {}",
                          mHeader.data, mHeader.length);
            mMessage = Base::createProto(mHeader.data);
            mBuffer.resize(mHeader.length);
            if (mMessage == nullptr) {
                co_return Err(Error(ErrorCode::InvalidProtoType));
            }
            break;
        case MessageType::Slice:
            NEKO_ASSERT(mBuffer.size() >= mHeader.data + mHeader.length, "Communication",
                        "buffer size({}) is too small, slice header offset({}) length({}), please check SliceHeader "
                        "tell enough length",
                        mBuffer.size(), mHeader.data, mHeader.length);
            NEKO_LOG_INFO("Communication", "recv message slice: message type: Slice, offset: {}, length: {}",
                          mHeader.data, mHeader.length);
            if (auto ret2 = co_await recvRaw({mBuffer.data() + mHeader.data, mHeader.length}); !ret2) {
                co_return Err(ret2.error());
            }
            mSliceSizeCount += mHeader.length;
            if (mSliceSizeCount == mBuffer.size()) {
                NEKO_LOG_INFO("Communication", "Received complete message, size({})", mSliceSizeCount);
                mSliceSizeCount = 0;
                isComplete      = true;
            }
            break;
        default:
            NEKO_LOG_ERROR("Communication", "Recv unsupported message type: {}.",
                           static_cast<int>(mHeader.message_type));
            co_return Err(Error(ErrorCode::InvalidMessageHeader));
        }
    }

    co_return co_await Base::finishMessage(std::move(mMessage), mHeader, std::move(mBuffer), flag);
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::recvRaw(std::span<std::byte> buf) -> IoTask<void> {
    int readsize = 0;
    while (readsize < static_cast<int>(buf.size())) {
        auto ret = co_await mStreamClient.read({buf.data() + readsize, buf.size() - readsize});
        if (!ret) {
            co_return Err(ret.error());
        }
        if (ret.value() == 0) {
            co_return Err(IoError::ConnectionReset);
        }
        readsize += static_cast<uint32_t>(ret.value());
    }
    co_return {};
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::sendVersion() -> IoTask<void> {
    auto dataRet = co_await Base::serializeVersionPacket();
    if (!dataRet) {
        co_return Err(dataRet.error());
    }
    auto data = std::move(dataRet.value());
    if (data.empty()) {
        co_return Err(Error(ErrorCode::SerializationError));
    }
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(data.data()), data.size()});
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send version verification message");
        co_return Err(ret.error());
    }
    NEKO_LOG_INFO("Communication", "Sent version verification message, version: {}", this->mFactory->version());
    co_return {};
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::recvVersion(const MessageHeader& header) -> IoTask<void> {
    mBuffer.resize(mHeader.length);
    auto ret = co_await recvRaw(mBuffer);
    if (!ret) {
        co_return Err(ret.error());
    }
    co_return co_await Base::syncProtocolTable(mBuffer, header);
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::sendSlice(std::span<std::byte> data, const uint32_t offset) -> IoTask<void> {
    auto headerData =
        Base::serializeHeader(MessageHeader(static_cast<uint32_t>(data.size()), offset, MessageType::Slice));
    if (headerData.empty()) {
        co_return Err(Error(ErrorCode::SerializationError));
    }
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    NEKO_LOG_INFO("Communication", "Sending slice, offset: {}, length: {}", offset, data.size());
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send message");
        co_return Err(ret.error());
    }
    ret = co_await sendRaw(data);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send slice");
        co_return Err(ret.error());
    }
    co_return {};
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::sendCancel(const uint32_t data) -> IoTask<void> {
    auto headerData = Base::serializeHeader(MessageHeader(0, data, MessageType::Cancel));
    if (headerData.empty()) {
        co_return Err(Error(ErrorCode::SerializationError));
    }
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    if (!ret) {
        co_return Err(ret.error());
    }
    co_return {};
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::close() -> IoTask<void> {
    if (!mStreamClient) {
        co_return {};
    }
    mStreamClient.close();
    co_return {};
}

template <CommunicationStream T>
inline void ProtoStreamClient<T>::setProtoTable(const uint32_t version,
                                                const std::map<uint32_t, std::string>& protoTable) {
    this->mProtocolTable = ProtocolTable{version, protoTable};
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::getProtoTable() const -> const ProtocolTable& {
    return this->mProtocolTable;
}

template <CommunicationStream T>
inline auto ProtoStreamClient<T>::sendRaw(std::span<std::byte> data) -> IoTask<void> {
    int sended = 0;
    while (sended < static_cast<int>(data.size())) {
        auto ret = co_await mStreamClient.write({data.data() + sended, data.size() - sended});
        if (!ret) {
            co_return Err(ret.error());
        }
        if (ret.value() == 0) {
            co_return Err(IoError::ConnectionReset);
        }
        sended += static_cast<uint32_t>(ret.value());
    }
    co_return {};
}

template <typename T = ilias::UdpSocket>
class ProtoDatagramClient : private detail::ProtoClientBase {
    using ClientType = T;
    using Base       = detail::ProtoClientBase;

public:
    ProtoDatagramClient() = default;
    ProtoDatagramClient(ProtoFactory& factory, ClientType&& datagramClient);
    ProtoDatagramClient(const ProtoDatagramClient&) = delete;
    ProtoDatagramClient(ProtoDatagramClient&& /*other*/);
    ~ProtoDatagramClient() noexcept;
    void setStreamClient(ClientType&& datagramClient, bool reset = false);
    auto send(const IProto& message, const IPEndpoint& endpoint, StreamFlag flag = StreamFlag::None) -> IoTask<void>;
    auto recv(StreamFlag flag = StreamFlag::None) -> IoTask<std::pair<IProto, IPEndpoint>>;
    auto close() -> IoTask<void>;
    void setProtoTable(uint32_t version, const std::map<uint32_t, std::string>& protoTable);
    auto getProtoTable() const -> const ProtocolTable&;

private:
    auto sendVersion(const IPEndpoint& endpoint) -> IoTask<void>;

private:
    ClientType mDatagramClient            = {};
    std::vector<std::byte> mBuffer        = {};
    static constexpr uint32_t gUdpMaxSize = 65535;
};

template <typename T>
inline ProtoDatagramClient<T>::ProtoDatagramClient(ProtoFactory& factory, ClientType&& datagramClient)
    : Base(factory), mDatagramClient(std::move(datagramClient)) {
    mBuffer.reserve(gUdpMaxSize);
}

template <typename T>
inline ProtoDatagramClient<T>::ProtoDatagramClient(ProtoDatagramClient&& other)
    : Base(std::move(other)), mDatagramClient(std::move(other.mDatagramClient)), mBuffer(std::move(other.mBuffer)) {}

template <typename T>
inline ProtoDatagramClient<T>::~ProtoDatagramClient() noexcept {}

template <typename T>
inline void ProtoDatagramClient<T>::setStreamClient(ClientType&& datagramClient, const bool reset) {
    mDatagramClient = std::move(datagramClient);
    if (reset) {
        mBuffer.clear();
    }
}

template <typename T>
inline auto ProtoDatagramClient<T>::send(const IProto& message, const IPEndpoint& endpoint, StreamFlag flag)
    -> IoTask<void> {
    if (!mDatagramClient) {
        NEKO_LOG_ERROR("Communication", "no datagram client");
        co_return Err(ErrorCode::UnsupportOperator);
    }

    const bool isVerify = static_cast<int>(flag & StreamFlag::VersionVerification) != 0;
    const bool isThread = static_cast<int>(flag & StreamFlag::SerializerInThread) != 0;
    const bool isSlice  = static_cast<int>(flag & StreamFlag::SliceData) != 0;

    if (isSlice) {
        NEKO_LOG_ERROR("Communication", "slice data not support in datagram client!");
        co_return Err(Error(ErrorCode::UnsupportOperator));
    }
    if (isVerify) {
        auto ret = co_await (sendVersion(endpoint) | unstoppable);
        if (!ret) {
            NEKO_LOG_WARN("Communication", "send to verification version failed! error: {}", ret.error().message());
            co_return Err(ret.error());
        }
    }

    auto messageDataRet = co_await Base::serializeMessageData(message, isThread, true);
    if (!messageDataRet) {
        co_return Err(messageDataRet.error());
    }
    auto messageData = std::move(messageDataRet.value());
    if (messageData.empty() || static_cast<int>(messageData.size()) == MessageHeader::size()) {
        co_return Err(Error(ErrorCode::NoData));
    }
    if (messageData.size() > 65527) {
        co_return Err(IoError::MessageTooLarge);
    }

    auto headerData = Base::serializeHeader(MessageHeader(
        static_cast<uint32_t>(messageData.size() - MessageHeader::size()), message.type(), MessageType::Complete));
    if (headerData.empty()) {
        co_return Err(Error(ErrorCode::SerializationError));
    }
    NEKO_ASSERT(static_cast<int>(headerData.size()) == MessageHeader::size(), "Communication", "Header size error");
    memcpy(messageData.data(), headerData.data(), headerData.size());
    NEKO_LOG_INFO("Communication", "Send header: message type: Complete proto type: {} size: {}", message.type(),
                  messageData.size() - MessageHeader::size());

    auto ret = co_await (
        mDatagramClient.sendto({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()}, endpoint) |
        unstoppable);
    if (!ret) {
        co_return Err(ret.error());
    }
    if (ret.value() != messageData.size()) {
        NEKO_LOG_ERROR("Communication", "Send data error, expect: {} -actual: {}", messageData.size(), ret.value());
        co_return Err(IoError::MessageTooLarge);
    }
    co_return {};
}

template <typename T>
inline auto ProtoDatagramClient<T>::recv(StreamFlag flag) -> IoTask<std::pair<IProto, IPEndpoint>> {
    if (!mDatagramClient) {
        NEKO_LOG_ERROR("Communication", "no datagram client");
        co_return Err(ErrorCode::UnsupportOperator);
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

    while (true) {
        mBuffer.resize(gUdpMaxSize, std::byte(0));
        auto ret = co_await (mDatagramClient.recvfrom(mBuffer));
        if (!ret) {
            NEKO_LOG_ERROR("Communication", "Recv message header error: {}", ret.error().message());
            co_return Err(ret.error());
        }

        const uint32_t recvSize = static_cast<uint32_t>(ret.value().first);
        auto endpoint           = ret.value().second;
        if (recvSize < static_cast<uint32_t>(MessageHeader::size())) {
            NEKO_LOG_ERROR("Communication", "Recv message header error: recv size({}) < MessageHeader::size({})",
                           recvSize, MessageHeader::size());
            co_return Err(Error(ErrorCode::InvalidMessageHeader));
        }
        mBuffer.resize(recvSize);

        MessageHeader header;
        BinarySerializer::InputSerializer serializer(reinterpret_cast<char*>(mBuffer.data()), MessageHeader::size());
        auto taggedHeader = makeTags<BinaryTag{.raw_fixed_data = true}>(header);
        if (!serializer(taggedHeader)) {
            NEKO_LOG_ERROR("Communication", "Recv message header error: deserialize error");
            co_return Err(Error(ErrorCode::InvalidMessageHeader));
        }

        switch (header.message_type) {
        case MessageType::VersionVerification: {
            NEKO_LOG_INFO("Communication", "recv header: message type: VersionVerification, lenght: {}", header.length);
            std::span<std::byte> payload(mBuffer.data() + MessageHeader::size(), recvSize - MessageHeader::size());
            auto ret2 = co_await (Base::syncProtocolTable(payload, header));
            if (!ret2) {
                co_return Err(ret2.error());
            }
            break;
        }
        case MessageType::Complete: {
            NEKO_LOG_INFO("Communication", "Recv header: message type: Complete proto type: {} size: {}", header.data,
                          header.length);
            if (recvSize - MessageHeader::size() != header.length) {
                NEKO_LOG_ERROR("Communication", "Recv message length mismatch: {} != {}",
                               recvSize - MessageHeader::size(), header.length);
                co_return Err(Error(ErrorCode::InvalidProtoData));
            }
            auto payloadBegin = mBuffer.begin() + MessageHeader::size();
            auto payloadEnd   = mBuffer.end();
            std::vector<std::byte> payload(payloadBegin, payloadEnd);
            auto message = Base::createProto(header.data);
            auto proto   = co_await Base::finishMessage(std::move(message), header, std::move(payload), flag);
            if (!proto) {
                co_return Err(proto.error());
            }
            co_return std::make_pair(std::move(proto.value()), endpoint);
        }
        case MessageType::Cancel:
        case MessageType::SliceHeader:
        case MessageType::Slice:
        default:
            NEKO_LOG_ERROR("Communication", "Recv unsupported message type: {}.",
                           static_cast<int>(header.message_type));
            co_return Err(Error(ErrorCode::InvalidMessageHeader));
        }
    }
}

template <typename T>
inline auto ProtoDatagramClient<T>::sendVersion(const IPEndpoint& endpoint) -> IoTask<void> {
    auto dataRet = co_await Base::serializeVersionPacket();
    if (!dataRet) {
        co_return Err(dataRet.error());
    }
    auto data = std::move(dataRet.value());
    if (data.empty()) {
        co_return Err(Error(ErrorCode::SerializationError));
    }
    auto ret = co_await mDatagramClient.sendto({reinterpret_cast<std::byte*>(data.data()), data.size()}, endpoint);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send version verification message");
        co_return Err(ret.error());
    }
    NEKO_LOG_INFO("Communication", "Sent version verification message, version: {}", this->mFactory->version());
    co_return {};
}

template <typename T>
inline auto ProtoDatagramClient<T>::close() -> IoTask<void> {
    if (!mDatagramClient) {
        co_return {};
    }
    mDatagramClient.close();
    co_return {};
}

template <typename T>
inline void ProtoDatagramClient<T>::setProtoTable(const uint32_t version,
                                                  const std::map<uint32_t, std::string>& protoTable) {
    this->mProtocolTable = ProtocolTable{version, protoTable};
}

template <typename T>
inline auto ProtoDatagramClient<T>::getProtoTable() const -> const ProtocolTable& {
    return this->mProtocolTable;
}

} // namespace nekoproto
