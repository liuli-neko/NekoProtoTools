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
#include <queue>

#include "detail/thread_await.hpp"
#include "nekoproto/proto/binary_serializer.hpp"
#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/proto/serializer_base.hpp"

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
        return serializer(makeFixedLengthField(length), makeFixedLengthField(data), makeFixedLengthField(messageType));
    }
    template <typename SerializerT>
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {
        return serializer(makeFixedLengthField(length), makeFixedLengthField(data), makeFixedLengthField(messageType));
    }
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
    NEKO_CHANNEL_ERROR(MessageTooLarge, 8, "message size is too large to send.", -1)                                   \
    NEKO_CHANNEL_ERROR(ProtocolFactoryVersionMismatch, 9, "protocol factory version mismatch", -1)                     \
    NEKO_CHANNEL_ERROR(SerializationError, 10, "serialization failed, return error.", -1)

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
};

inline StreamFlag operator|(StreamFlag a, StreamFlag b) {
    return static_cast<StreamFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline StreamFlag operator&(StreamFlag a, StreamFlag b) {
    return static_cast<StreamFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline StreamFlag operator^(StreamFlag a, StreamFlag b) {
    return static_cast<StreamFlag>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
}
inline bool operator!(StreamFlag a) { return static_cast<uint32_t>(a) == 0; }

template <ILIAS_NAMESPACE::StreamClient T = ILIAS_NAMESPACE::IStreamClient>
class NEKO_PROTO_API ProtoStreamClient {

public:
    ProtoStreamClient() = default;
    ProtoStreamClient(ProtoFactory& factory, ILIAS_NAMESPACE::IoContext& ioContext, T&& streamClient);
    ProtoStreamClient(const ProtoStreamClient&) = delete;
    ProtoStreamClient(ProtoStreamClient&&);
    ~ProtoStreamClient() noexcept;
    auto setStreamClient(T&& streamClient) -> void;
    auto send(const IProto& message, StreamFlag flag = StreamFlag::None) -> ILIAS_NAMESPACE::Task<void>;
    auto recv(StreamFlag flag = StreamFlag::None) -> ILIAS_NAMESPACE::Task<std::unique_ptr<IProto>>;
    auto close() -> ILIAS_NAMESPACE::Task<void>;

private:
    auto sendRaw(std::span<std::byte> data) -> ILIAS_NAMESPACE::Task<void>;
    auto recvRaw(std::span<std::byte> buf) -> ILIAS_NAMESPACE::Task<void>;
    auto sendVersion() -> ILIAS_NAMESPACE::Task<void>;
    auto sendSlice(std::span<std::byte> data, const uint32_t offset) -> ILIAS_NAMESPACE::Task<void>;
    auto sendCancel(const uint32_t data = 0) -> ILIAS_NAMESPACE::Task<void>;

private:
    ProtoFactory* mFactory                 = nullptr;
    ILIAS_NAMESPACE::IoContext* mIoContext = nullptr;
    T mStreamClient;
    std::vector<std::byte> mBuffer;
    MessageHeader mHeader;
    std::unique_ptr<IProto> mMessage;
    uint32_t mSliceSizeCount             = 0;
    static constexpr uint32_t gSliceSize = 1200;
};

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoFactory& factory, ILIAS_NAMESPACE::IoContext& ioContext,
                                               T&& streamClient)
    : mFactory(&factory), mIoContext(&ioContext), mStreamClient(std::move(streamClient)) {}

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoStreamClient&& othre)
    : mFactory(othre.mFactory), mIoContext(othre.mIoContext), mStreamClient(std::move(othre.mStreamClient)) {}

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::~ProtoStreamClient() noexcept {}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::setStreamClient(T&& streamClient) -> void {
    mStreamClient = std::move(streamClient);
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::send(const IProto& message, StreamFlag flag) -> ILIAS_NAMESPACE::Task<void> {
    // verify proto factory
    if (static_cast<int>(flag & StreamFlag::VersionVerification)) {
        auto ret = co_await sendVersion();
        if (!ret) {
            NEKO_LOG_WARN("Communication", "send to verification version failed!");
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }
    // proto to data
    std::vector<char> messageData;
    if (static_cast<int>(flag & StreamFlag::SerializerInThread)) {
        auto ret = co_await detail::ThreadAwaiter<std::vector<char>>{std::bind(&IProto::toData, &message)};
        if (ret && ret.value().size()) {
            messageData = std::move(ret.value());
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error(ErrorCode::NoData)));
        }
    } else {
        messageData = message.toData();
        if (messageData.empty()) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::NoData));
        }
    }
    if (messageData.size() > std::numeric_limits<uint32_t>::max()) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::MessageTooLarge));
    }
    if (static_cast<int>(flag & StreamFlag::SliceData)) {
        // slice data
        uint32_t offset = 0;
        auto header     = MessageHeader(messageData.size(), message.type(), MessageType::SliceHeader);
        std::vector<char> headerData;
        do {
            BinaryOutputSerializer serializer(headerData);
            if (!serializer(header)) {
                co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::SerializationError));
            }
        } while (false);
        auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
        NEKO_LOG_INFO("Communication", "Sending slice header, protocol: {}, size: {}", message.type(),
                      messageData.size());
        while (true) {
            auto sliceSize =
                std::min(static_cast<uint32_t>(messageData.size()) - offset, gSliceSize - MessageHeader::size());
            if (!ret) {
                if (ret.error() == ILIAS_NAMESPACE::Error::Canceled) {
                    co_await sendCancel(message.type());
                }
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
            ret = co_await sendSlice({reinterpret_cast<std::byte*>(messageData.data() + offset), sliceSize}, offset);
            offset += sliceSize;
            if (offset >= messageData.size()) {
                break;
            }
        }
        co_return ILIAS_NAMESPACE::Result<void>();
    } else {
        // complete data
        auto header = MessageHeader(messageData.size(), message.type(), MessageType::Complete);
        std::vector<char> headerData;
        do {
            BinaryOutputSerializer serializer(headerData);
            if (!serializer(header)) {
                co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::SerializationError));
            }
        } while (false);
        auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
        NEKO_LOG_INFO("Communication", "Send header: -message type: {} -data: {} -length: {}", header.messageType,
                      header.data, header.length);
        if (!ret) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        co_return co_await sendRaw({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()});
    }
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::recv(StreamFlag flag) -> ILIAS_NAMESPACE::Task<std::unique_ptr<IProto>> {
    if (static_cast<int>(flag & StreamFlag::SliceData)) {
        NEKO_LOG_WARN("Communication", "recv function can't set StreamFlag::SliceData, ignore it, if you want to recv "
                                       "slice data, please set it in send function.");
    }
    if (static_cast<int>(flag & StreamFlag::VersionVerification)) {
        NEKO_LOG_WARN("Communication",
                      "recv function can't set StreamFlag::VersionVerification, ignore it, if you want to "
                      "verification protofactory version, please set it in send function.");
    }
    bool isComplete = false;
    while (!isComplete) {
        // receive header
        mHeader = MessageHeader();
        std::vector<std::byte> messageHeader(MessageHeader::size());
        auto ret = co_await recvRaw(messageHeader);
        if (!ret) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        BinaryInputSerializer input(reinterpret_cast<char*>(messageHeader.data()), messageHeader.size());
        if (!input(mHeader)) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidMessageHeader));
        }
        NEKO_LOG_INFO("Communication", "recv message header: -message type: {} -data: {} -length: {}",
                      mHeader.messageType, mHeader.data, mHeader.length);
        // process header
        switch (mHeader.messageType) {
        case MessageType::Cancel: {
            mBuffer.clear();
            mMessage        = nullptr;
            mSliceSizeCount = 0;
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        case MessageType::VersionVerification: {
            if (mHeader.data != mFactory->version()) {
                NEKO_LOG_ERROR("Communication", "ProtoFactory version mismatch: {} != {}", mHeader.data,
                               mFactory->version());
                co_return ILIAS_NAMESPACE::Unexpected(
                    ILIAS_NAMESPACE::Error(ErrorCode::ProtocolFactoryVersionMismatch));
            }
            break;
        }
        case MessageType::Complete: {
            mMessage = mFactory->create(mHeader.data);
            if (mMessage == nullptr) {
                NEKO_LOG_ERROR("Communication", "unsupported proto type: {}", mHeader.data);
                co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoType));
            }
            mBuffer.resize(mHeader.length);
            auto ret = co_await recvRaw(mBuffer);
            if (!ret) {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
            isComplete = true;
        }
        case MessageType::SliceHeader: {
            mMessage = mFactory->create(mHeader.data);
            mBuffer.resize(mHeader.length);
            if (mMessage == nullptr) {
                co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoType));
            }
            break;
        }
        case MessageType::Slice: {
            NEKO_ASSERT(mBuffer.size() >= mHeader.data + mHeader.length, "Communication",
                        "mBuffer size({}) is too small, slice header offset({}) length({})", mBuffer.size(),
                        mHeader.data, mHeader.length);
            auto ret = co_await recvRaw({mBuffer.data() + mHeader.data, mHeader.length});
            if (!ret) {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
            mSliceSizeCount += mHeader.length;
            NEKO_LOG_INFO("Communication", "Received slice, total size: {}", mSliceSizeCount);
            if (mSliceSizeCount == mBuffer.size()) {
                NEKO_LOG_INFO("Communication", "Received complete message");
                mSliceSizeCount = 0;
                isComplete      = true;
            }
            break;
        }
        default:
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidMessageHeader));
        }
    }
    if (mMessage == nullptr) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::UnrecognizedMessage));
    }
    if (static_cast<int>(flag & StreamFlag::SerializerInThread)) {
        auto ret = co_await detail::ThreadAwaiter<bool>(
            std::bind(&IProto::formData, mMessage.get(), reinterpret_cast<char*>(mBuffer.data()), mBuffer.size()));
        if (!ret && !ret.value()) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoData)));
        }
    } else {
        if (!mMessage->formData(reinterpret_cast<char*>(mBuffer.data()), mBuffer.size())) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoData));
        }
    }
    co_return std::move(mMessage);
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::recvRaw(std::span<std::byte> buf) -> ILIAS_NAMESPACE::Task<void> {
    int readsize = 0;
    while (readsize < buf.size()) {
        ILIAS_NAMESPACE::Result<size_t> ret =
            co_await mStreamClient.read({buf.data() + readsize, buf.size() - readsize});
        if (ret) {
            if (ret.value() == 0) {
                co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::ConnectionReset);
            }
            readsize += ret.value();
#ifdef _WIN32
        } else if (ret.error() == ILIAS_NAMESPACE::SystemError(WSAEWOULDBLOCK) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(WSAEINPROGRESS) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(WSAENOBUFS)) {
#else
        } else if (ret.error() == ILIAS_NAMESPACE::SystemError(EAGAIN) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(EWOULDBLOCK) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(ENOBUFS)) {
#endif
            co_await ILIAS_NAMESPACE::sleep(std::chrono::milliseconds(10));
            continue;
        } else if (ret.error() == ILIAS_NAMESPACE::SystemError(EINTR)) {
            continue;
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }
    co_return ILIAS_NAMESPACE::Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendVersion() -> ILIAS_NAMESPACE::Task<void> {
    auto header = MessageHeader(0, mFactory->version(), MessageType::VersionVerification);
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        if (!serializer(header)) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::SerializationError));
        }
    } while (false);
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send version verification message");
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    NEKO_LOG_INFO("Communication", "Sent version verification message, version: {}", mFactory->version());
    co_return ILIAS_NAMESPACE::Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendSlice(std::span<std::byte> data, const uint32_t offset)
    -> ILIAS_NAMESPACE::Task<void> {
    auto header = MessageHeader(data.size(), offset, MessageType::Slice);
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        if (!serializer(header)) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::SerializationError));
        }
    } while (false);
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    NEKO_LOG_INFO("Communication", "Sending slice, size: {}, offset: {}", data.size(), offset);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send message");
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    ret = co_await sendRaw(data);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send slice");
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    co_return ILIAS_NAMESPACE::Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendCancel(const uint32_t data) -> ILIAS_NAMESPACE::Task<void> {
    auto header = MessageHeader(0, data, MessageType::Cancel);
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        if (!serializer(header)) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::SerializationError));
        }
    } while (false);
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    co_return ILIAS_NAMESPACE::Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::close() -> ILIAS_NAMESPACE::Task<void> {
    co_return co_await mStreamClient.shutdown();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendRaw(std::span<std::byte> data) -> ILIAS_NAMESPACE::Task<void> {
    int sended = 0;
    while (sended < data.size()) {
        ILIAS_NAMESPACE::Result<size_t> ret =
            co_await mStreamClient.write({data.data() + sended, data.size() - sended});
        if (ret) {
            if (ret.value() == 0) {
                co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::ConnectionReset);
            }
            sended += ret.value();
#ifdef _WIN32
        } else if (ret.error() == ILIAS_NAMESPACE::SystemError(WSAEWOULDBLOCK) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(WSAEINPROGRESS) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(WSAENOBUFS)) {
#else
        } else if (ret.error() == ILIAS_NAMESPACE::SystemError(EAGAIN) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(EWOULDBLOCK) ||
                   ret.error() == ILIAS_NAMESPACE::SystemError(ENOBUFS)) {
#endif
            co_await ILIAS_NAMESPACE::sleep(std::chrono::milliseconds(10));
            continue;
        } else if (ret.error() == ILIAS_NAMESPACE::SystemError(EINTR)) {
            continue;
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }
    co_return ILIAS_NAMESPACE::Result<void>();
}

NEKO_END_NAMESPACE
