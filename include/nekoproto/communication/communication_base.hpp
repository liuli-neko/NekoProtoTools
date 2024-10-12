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
    using ClientType = T;
    using IoContext  = ILIAS_NAMESPACE::IoContext;
    template <typename U>
    using Task = ILIAS_NAMESPACE::Task<U>;
    template <typename U>
    using Result = ILIAS_NAMESPACE::Result<U>;
    template <typename U>
    using Unexpected  = ILIAS_NAMESPACE::Unexpected<U>;
    using Error       = ILIAS_NAMESPACE::Error;
    using SystemError = ILIAS_NAMESPACE::SystemError;

public:
    ProtoStreamClient() = default;
    ProtoStreamClient(ProtoFactory& factory, IoContext& ioContext, T&& streamClient);
    ProtoStreamClient(const ProtoStreamClient&) = delete;
    ProtoStreamClient(ProtoStreamClient&&);
    ~ProtoStreamClient() noexcept;
    auto setStreamClient(ClientType&& streamClient, bool reconnect = false) -> void;
    auto send(const IProto& message, StreamFlag flag = StreamFlag::None) -> Task<void>;
    auto recv(StreamFlag flag = StreamFlag::None) -> Task<std::unique_ptr<IProto>>;
    auto close() -> Task<void>;

private:
    auto sendRaw(std::span<std::byte> data) -> Task<void>;
    auto recvRaw(std::span<std::byte> buf) -> Task<void>;
    auto sendVersion() -> Task<void>;
    auto sendSlice(std::span<std::byte> data, const uint32_t offset) -> Task<void>;
    auto sendCancel(const uint32_t data = 0) -> Task<void>;

private:
    ProtoFactory* mFactory               = nullptr;
    IoContext* mIoContext                = nullptr;
    ClientType mStreamClient             = {};
    std::vector<std::byte> mBuffer       = {};
    MessageHeader mHeader                = {};
    std::unique_ptr<IProto> mMessage     = {};
    uint32_t mSliceSizeCount             = 0;
    static constexpr uint32_t gSliceSize = 1200;
};

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoFactory& factory, IoContext& ioContext, T&& streamClient)
    : mFactory(&factory), mIoContext(&ioContext), mStreamClient(std::move(streamClient)) {}

template <ILIAS_NAMESPACE::StreamClient T>
inline ProtoStreamClient<T>::ProtoStreamClient(ProtoStreamClient&& othre)
    : mFactory(othre.mFactory), mIoContext(othre.mIoContext), mStreamClient(std::move(othre.mStreamClient)),
      mBuffer(std::move(othre.mBuffer)), mHeader(std::move(othre.mHeader)), mMessage(std::move(othre.mMessage)),
      mSliceSizeCount(othre.mSliceSizeCount) {}

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
inline auto ProtoStreamClient<T>::send(const IProto& message, StreamFlag flag) -> Task<void> {
    bool isVerify = static_cast<int>(flag & StreamFlag::VersionVerification) != 0;
    bool isThread = static_cast<int>(flag & StreamFlag::SerializerInThread) != 0;
    bool isSlice  = static_cast<int>(flag & StreamFlag::SliceData) != 0;

    // verify proto factory
    if (isVerify) {
        auto ret = co_await sendVersion();
        if (!ret) {
            NEKO_LOG_WARN("Communication", "send to verification version failed!");
            co_return Unexpected(ret.error());
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
        co_return Unexpected(Error(ErrorCode::NoData));
    }
    if (messageData.size() > std::numeric_limits<uint32_t>::max()) {
        co_return Unexpected(Error(ErrorCode::MessageTooLarge));
    }
    if (isSlice) {
        // slice data
        uint32_t offset = 0;
        auto header     = MessageHeader(messageData.size(), message.type(), MessageType::SliceHeader);
        std::vector<char> headerData;
        do {
            BinaryOutputSerializer serializer(headerData);
            if (!serializer(header)) {
                co_return Unexpected(Error(ErrorCode::SerializationError));
            }
        } while (false);
        auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
        NEKO_LOG_INFO("Communication", "Sending slice header, protocol: {}, size: {}", message.type(),
                      messageData.size());
        while (true) {
            auto sliceSize =
                std::min(static_cast<uint32_t>(messageData.size()) - offset, gSliceSize - MessageHeader::size());
            NEKO_ASSERT(sliceSize > 0, "Communication", "Slice size is 0");
            if (!ret) {
                if (ret.error() == Error::Canceled) {
                    co_await sendCancel(message.type());
                }
                co_return Unexpected(ret.error());
            }
            ret = co_await sendSlice({reinterpret_cast<std::byte*>(messageData.data() + offset), sliceSize}, offset);
            offset += sliceSize;
            if (offset >= messageData.size()) {
                break;
            }
        }
        co_return Result<void>();
    } else {
        // complete data
        auto header = MessageHeader(messageData.size() - MessageHeader::size(), message.type(), MessageType::Complete);
        std::vector<char> headerData;
        do {
            BinaryOutputSerializer serializer(headerData);
            if (!serializer(header)) {
                co_return Unexpected(Error(ErrorCode::SerializationError));
            }
        } while (false);
        NEKO_ASSERT(headerData.size() == MessageHeader::size(), "Communication", "Header size is not correct");
        memcpy(messageData.data(), headerData.data(), headerData.size());
        NEKO_LOG_INFO("Communication", "Send header: -message type: {} -data: {} -length: {}", header.messageType,
                      header.data, header.length);
        co_return co_await sendRaw({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()});
    }
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::recv(StreamFlag flag) -> Task<std::unique_ptr<IProto>> {
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
            co_return Unexpected(ret.error());
        }
        BinaryInputSerializer input(reinterpret_cast<char*>(messageHeader.data()), messageHeader.size());
        if (!input(mHeader)) {
            co_return Unexpected(Error(ErrorCode::InvalidMessageHeader));
        }
        NEKO_LOG_INFO("Communication", "recv message header: -message type: {} -data: {} -length: {}",
                      mHeader.messageType, mHeader.data, mHeader.length);
        // process header
        switch (mHeader.messageType) {
        case MessageType::Cancel: {
            mBuffer.clear();
            mMessage        = nullptr;
            mSliceSizeCount = 0;
            co_return Unexpected(Error::Canceled);
        }
        case MessageType::VersionVerification: {
            if (mHeader.data != mFactory->version()) {
                NEKO_LOG_ERROR("Communication", "ProtoFactory version mismatch: {} != {}", mHeader.data,
                               mFactory->version());
                co_return Unexpected(Error(ErrorCode::ProtocolFactoryVersionMismatch));
            }
            break;
        }
        case MessageType::Complete: {
            mMessage = mFactory->create(mHeader.data);
            if (mMessage == nullptr) {
                NEKO_LOG_ERROR("Communication", "unsupported proto type: {}", mHeader.data);
                co_return Unexpected(Error(ErrorCode::InvalidProtoType));
            }
            mBuffer.resize(mHeader.length);
            auto ret = co_await recvRaw(mBuffer);
            if (!ret) {
                co_return Unexpected(ret.error());
            }
            isComplete = true;
        }
        case MessageType::SliceHeader: {
            mMessage = mFactory->create(mHeader.data);
            mBuffer.resize(mHeader.length);
            if (mMessage == nullptr) {
                co_return Unexpected(Error(ErrorCode::InvalidProtoType));
            }
            break;
        }
        case MessageType::Slice: {
            NEKO_ASSERT(mBuffer.size() >= mHeader.data + mHeader.length, "Communication",
                        "mBuffer size({}) is too small, slice header offset({}) length({})", mBuffer.size(),
                        mHeader.data, mHeader.length);
            auto ret = co_await recvRaw({mBuffer.data() + mHeader.data, mHeader.length});
            if (!ret) {
                co_return Unexpected(ret.error());
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
            co_return Unexpected(Error(ErrorCode::InvalidMessageHeader));
        }
    }
    if (mMessage == nullptr) {
        co_return Unexpected(Error(ErrorCode::UnrecognizedMessage));
    }
    if (static_cast<int>(flag & StreamFlag::SerializerInThread)) {
        auto ret = co_await detail::ThreadAwaiter<bool>(
            std::bind(&IProto::formData, mMessage.get(), reinterpret_cast<char*>(mBuffer.data()), mBuffer.size()));
        if (!ret && !ret.value()) {
            co_return Unexpected(ret.error_or(Error(ErrorCode::InvalidProtoData)));
        }
    } else {
        if (!mMessage->formData(reinterpret_cast<char*>(mBuffer.data()), mBuffer.size())) {
            co_return Unexpected(Error(ErrorCode::InvalidProtoData));
        }
    }
    co_return std::move(mMessage);
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::recvRaw(std::span<std::byte> buf) -> ILIAS_NAMESPACE::Task<void> {
    int readsize = 0;
    while (readsize < buf.size()) {
        Result<size_t> ret = co_await mStreamClient.read({buf.data() + readsize, buf.size() - readsize});
        if (ret) {
            if (ret.value() == 0) {
                co_return Unexpected(Error::ConnectionReset);
            }
            readsize += ret.value();
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
            co_return Unexpected(ret.error());
        }
    }
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendVersion() -> Task<void> {
    auto header = MessageHeader(0, mFactory->version(), MessageType::VersionVerification);
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        if (!serializer(header)) {
            co_return Unexpected(Error(ErrorCode::SerializationError));
        }
    } while (false);
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send version verification message");
        co_return Unexpected(ret.error());
    }
    NEKO_LOG_INFO("Communication", "Sent version verification message, version: {}", mFactory->version());
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendSlice(std::span<std::byte> data, const uint32_t offset) -> Task<void> {
    auto header = MessageHeader(data.size(), offset, MessageType::Slice);
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        if (!serializer(header)) {
            co_return Unexpected(Error(ErrorCode::SerializationError));
        }
    } while (false);
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    NEKO_LOG_INFO("Communication", "Sending slice, size: {}, offset: {}", data.size(), offset);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send message");
        co_return Unexpected(ret.error());
    }
    ret = co_await sendRaw(data);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send slice");
        co_return Unexpected(ret.error());
    }
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendCancel(const uint32_t data) -> Task<void> {
    auto header = MessageHeader(0, data, MessageType::Cancel);
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        if (!serializer(header)) {
            co_return Unexpected(Error(ErrorCode::SerializationError));
        }
    } while (false);
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    co_return Result<void>();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::close() -> Task<void> {
    co_return co_await mStreamClient.shutdown();
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::sendRaw(std::span<std::byte> data) -> Task<void> {
    int sended = 0;
    while (sended < data.size()) {
        Result<size_t> ret = co_await mStreamClient.write({data.data() + sended, data.size() - sended});
        if (ret) {
            if (ret.value() == 0) {
                co_return Unexpected(Error::ConnectionReset);
            }
            sended += ret.value();
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
            co_return Unexpected(ret.error());
        }
    }
    co_return Result<void>();
}

template <typename T = ILIAS_NAMESPACE::UdpClient>
class NEKO_PROTO_API ProtoDatagramClient {
    using ClientType = T;
    using IoContext  = ILIAS_NAMESPACE::IoContext;
    template <typename U>
    using Task = ILIAS_NAMESPACE::Task<U>;
    template <typename U>
    using Result     = ILIAS_NAMESPACE::Result<U>;
    using IPEndpoint = ILIAS_NAMESPACE::IPEndpoint;
    template <typename U>
    using Unexpected  = ILIAS_NAMESPACE::Unexpected<U>;
    using Error       = ILIAS_NAMESPACE::Error;
    using SystemError = ILIAS_NAMESPACE::SystemError;

public:
    ProtoDatagramClient() = default;
    ProtoDatagramClient(ProtoFactory& factory, IoContext& ioContext, ClientType&& streamClient);
    ProtoDatagramClient(const ProtoDatagramClient&) = delete;
    ProtoDatagramClient(ProtoDatagramClient&&);
    ~ProtoDatagramClient() noexcept;
    auto setStreamClient(ClientType&& streamClient, const bool reset = false) -> void;
    auto send(const IProto& message, const IPEndpoint& endpoint, StreamFlag flag = StreamFlag::None) -> Task<void>;
    auto recv(StreamFlag flag = StreamFlag::None) -> Task<std::pair<std::unique_ptr<IProto>, IPEndpoint>>;
    auto close() -> Task<void>;

private:
    auto sendVersion(const IPEndpoint& endpoint) -> Task<void>;
    auto sendSlice(const IPEndpoint& endpoint, std::span<std::byte> data, const uint32_t offset) -> Task<void>;
    auto sendCancel(const IPEndpoint& endpoint, const uint32_t data = 0) -> Task<void>;

private:
    ProtoFactory* mFactory                 = nullptr;
    ILIAS_NAMESPACE::IoContext* mIoContext = nullptr;
    ClientType mDatagramClient             = {};
    std::vector<std::byte> mBuffer         = {};
    MessageHeader mHeader                  = {};
    std::unique_ptr<IProto> mMessage       = nullptr;
    std::set<uint32_t> mSliceIdCount       = {};
    uint32_t mSliceSizeCount               = 0;
    static constexpr uint32_t gSliceSize   = 1200;
};

template <typename T>
inline ProtoDatagramClient<T>::ProtoDatagramClient(ProtoFactory& factory, ILIAS_NAMESPACE::IoContext& ioContext,
                                                   ClientType&& client)
    : mFactory(&factory), mIoContext(&ioContext), mDatagramClient(std::move(client)) {}

template <typename T>
inline ProtoDatagramClient<T>::ProtoDatagramClient(ProtoDatagramClient<T>&& other) {
    mFactory        = other.mFactory;
    mIoContext      = other.mIoContext;
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
        mHeader = {};
        mMessage.reset();
        mSliceIdCount.clear();
    }
}

template <typename T>
inline auto ProtoDatagramClient<T>::send(const IProto& message, const IPEndpoint& endpoint, StreamFlag flag)
    -> Task<void> {
    // verify proto factory
    if (static_cast<int>(flag & StreamFlag::VersionVerification)) {
        auto ret = co_await sendVersion(endpoint);
        if (!ret) {
            NEKO_LOG_WARN("Communication", "send to verification version failed!");
            co_return Unexpected(ret.error());
        }
    }
    // proto to data

    std::vector<char> messageData;
    if (!static_cast<int>(flag & StreamFlag::SliceData)) {
        messageData.resize(MessageHeader::size(), 0);
    }
    if (static_cast<int>(flag & StreamFlag::SerializerInThread)) {
        co_await detail::ThreadAwaiter<bool>{std::bind(
            static_cast<bool (IProto::*)(std::vector<char>&) const>(&IProto::toData), &message, std::ref(messageData))};
    } else {
        message.toData(messageData);
    }
    if (messageData.empty() ||
        (!static_cast<int>(flag & StreamFlag::SliceData) && messageData.size() == MessageHeader::size())) {
        co_return Unexpected(Error(ErrorCode::NoData));
    }
    if (messageData.size() > std::numeric_limits<uint32_t>::max()) {
        co_return Unexpected(Error(ErrorCode::MessageTooLarge));
    }
    if (static_cast<int>(flag & StreamFlag::SliceData)) {
        // slice data
        uint32_t offset = 0;
        auto header     = MessageHeader(messageData.size(), message.type(), MessageType::SliceHeader);
        std::vector<char> headerData;
        do {
            BinaryOutputSerializer serializer(headerData);
            if (!serializer(header)) {
                co_return Unexpected(Error(ErrorCode::SerializationError));
            }
        } while (false);
        auto ret = co_await mDatagramClient.sendto({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()},
                                                   endpoint);
        NEKO_LOG_INFO("Communication", "Sending slice header, protocol: {}, size: {}", message.type(),
                      messageData.size());
        while (true) {
            auto sliceSize =
                std::min(static_cast<uint32_t>(messageData.size()) - offset, gSliceSize - MessageHeader::size());
            NEKO_ASSERT(sliceSize > 0, "Communication", "Slice size is 0");
            if (!ret) {
                if (ret.error() == Error::Canceled) {
                    co_await sendCancel(endpoint, message.type());
                }
                co_return Unexpected(ret.error());
            }
            ret = co_await sendSlice(endpoint, {reinterpret_cast<std::byte*>(messageData.data() + offset), sliceSize},
                                     offset);
            offset += sliceSize;
            if (offset >= messageData.size()) {
                break;
            }
        }
        co_return Result<void>();
    } else {
        // complete data
        if (messageData.size() > 65527) {
            co_return Unexpected(Error(ErrorCode::MessageTooLarge));
        }
        auto header = MessageHeader(messageData.size() - MessageHeader::size(), message.type(), MessageType::Complete);
        std::vector<char> headerData;
        do {
            BinaryOutputSerializer serializer(headerData);
            if (!serializer(header)) {
                co_return Unexpected(Error(ErrorCode::SerializationError));
            }
        } while (false);
        NEKO_ASSERT(headerData.size() == MessageHeader::size(), "Communication", "Header size error");
        memcpy(messageData.data(), headerData.data(), headerData.size());
        NEKO_LOG_INFO("Communication", "Send header: -message type: {} -data: {} -length: {}", header.messageType,
                      header.data, header.length);
        co_return co_await mDatagramClient.sento({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()},
                                                 endpoint);
    }
}

template <typename T>
inline auto ProtoDatagramClient<T>::recv(StreamFlag flag) -> Task<std::pair<std::unique_ptr<IProto>, IPEndpoint>> {
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
    IPEndpoint endpoint;
    while (!isComplete) {
        // receive header
        mHeader = MessageHeader();
        std::vector<std::byte> messageHeader(MessageHeader::size());
        auto ret = co_await mDatagramClient.recvfrom(messageHeader, endpoint);
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        BinaryInputSerializer input(reinterpret_cast<char*>(messageHeader.data()), messageHeader.size());
        if (!input(mHeader)) {
            co_return Unexpected(Error(ErrorCode::InvalidMessageHeader));
        }
        NEKO_LOG_INFO("Communication", "recv message header: -message type: {} -data: {} -length: {}",
                      mHeader.messageType, mHeader.data, mHeader.length);
        // process header
        switch (mHeader.messageType) {
        case MessageType::Cancel: {
            mBuffer.clear();
            mMessage        = nullptr;
            mSliceSizeCount = 0;
            co_return Unexpected(Error::Canceled);
        }
        case MessageType::VersionVerification: {
            if (mHeader.data != mFactory->version()) {
                NEKO_LOG_ERROR("Communication", "ProtoFactory version mismatch: {} != {}", mHeader.data,
                               mFactory->version());
                co_return Unexpected(Error(ErrorCode::ProtocolFactoryVersionMismatch));
            }
            break;
        }
        case MessageType::Complete: {
            mMessage = mFactory->create(mHeader.data);
            if (mMessage == nullptr) {
                NEKO_LOG_ERROR("Communication", "unsupported proto type: {}", mHeader.data);
                co_return Unexpected(Error(ErrorCode::InvalidProtoType));
            }
            mBuffer.resize(mHeader.length);
            auto ret = co_await mDatagramClient.recvfrom(mBuffer, endpoint);
            if (!ret) {
                co_return Unexpected(ret.error());
            }
            isComplete = true;
        }
        case MessageType::SliceHeader: {
            mMessage = mFactory->create(mHeader.data);
            mBuffer.resize(mHeader.length);
            if (mMessage == nullptr) {
                co_return Unexpected(Error(ErrorCode::InvalidProtoType));
            }
            break;
        }
        case MessageType::Slice: {
            NEKO_ASSERT(mBuffer.size() >= mHeader.data + mHeader.length, "Communication",
                        "mBuffer size({}) is too small, slice header offset({}) length({})", mBuffer.size(),
                        mHeader.data, mHeader.length);
            auto ret = co_await mDatagramClient.recvfrom({mBuffer.data() + mHeader.data, mHeader.length}, endpoint);
            if (!ret) {
                co_return Unexpected(ret.error());
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
            co_return Unexpected(Error(ErrorCode::InvalidMessageHeader));
        }
    }
    if (mMessage == nullptr) {
        co_return Unexpected(Error(ErrorCode::UnrecognizedMessage));
    }
    if (static_cast<int>(flag & StreamFlag::SerializerInThread)) {
        auto ret = co_await detail::ThreadAwaiter<bool>(
            std::bind(&IProto::formData, mMessage.get(), reinterpret_cast<char*>(mBuffer.data()), mBuffer.size()));
        if (!ret && !ret.value()) {
            co_return Unexpected(ret.error_or(Error(ErrorCode::InvalidProtoData)));
        }
    } else {
        if (!mMessage->formData(reinterpret_cast<char*>(mBuffer.data()), mBuffer.size())) {
            co_return Unexpected(Error(ErrorCode::InvalidProtoData));
        }
    }
    co_return std::make_pair(std::move(mMessage), endpoint);
}

template <typename T>
inline auto ProtoDatagramClient<T>::close() -> Task<void> {
    mDatagramClient.close();
    co_return Result<void>{};
}

template <typename T>
inline auto ProtoDatagramClient<T>::sendVersion(const IPEndpoint& endpoint) -> Task<void> {
    auto header = MessageHeader(0, mFactory->version(), MessageType::VersionVerification);
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        if (!serializer(header)) {
            co_return Unexpected(Error(ErrorCode::SerializationError));
        }
    } while (false);
    auto ret =
        co_await mDatagramClient.sendto({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()}, endpoint);
    if (!ret) {
        NEKO_LOG_WARN("Communication", "Failed to send version verification message");
        co_return Unexpected(ret.error());
    }
    NEKO_LOG_INFO("Communication", "Sent version verification message, version: {}", mFactory->version());
    co_return Result<void>();
}

template <typename T>
inline auto ProtoDatagramClient<T>::sendSlice(const IPEndpoint& endpoint, std::span<std::byte> data,
                                              const uint32_t offset) -> Task<void> {
    // TODO: Add support for sending slices
    co_return Result<void>();
}

template <typename T>
inline auto ProtoDatagramClient<T>::sendCancel(const IPEndpoint& endpoint, const uint32_t data) -> Task<void> {
    // TODO: Add support for sending cancel messages
    co_return Result<void>();
}

NEKO_END_NAMESPACE
