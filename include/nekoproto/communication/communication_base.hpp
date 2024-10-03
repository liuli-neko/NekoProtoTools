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
// enum | code | message | system code mapping
#define NEKO_CHANNEL_ERROR_CODE_TABLE                                                                                  \
    NEKO_CHANNEL_ERROR(Ok, 0, "ok", 0)                                                                                 \
    NEKO_CHANNEL_ERROR(InvalidMessageHeader, 1, "receive an unrecognized message header", -1)                          \
    NEKO_CHANNEL_ERROR(InvalidProtoType, 2, "receive a message, but proto type is not registed", -1)                   \
    NEKO_CHANNEL_ERROR(InvalidProtoData, 3, "receive a message, but proto parser is error", -1)                        \
    NEKO_CHANNEL_ERROR(ProtoVersionUnsupported, 4, "proto version is not supported", -1)                               \
    NEKO_CHANNEL_ERROR(ConnectionMessageTypeError, 5, "receive a error message in connection state", -1)               \
    NEKO_CHANNEL_ERROR(Timeout, 6, "the operator is timeout", -1)                                                      \
    NEKO_CHANNEL_ERROR(NoData, 7, "serializer maybe failed, return no data.", -1)

enum MessageType {
    Complete,
    Slice, // not implemented
};

class NEKO_PROTO_API MessageHeader {
public:
    inline MessageHeader(uint32_t length = 0, int32_t protoType = 0, uint16_t transType = 0, uint32_t protoVersion = 0)
        : length(length), protoType(protoType), transType(transType), protoVersion(protoVersion) {}
    inline static int size() { return 14; }
    uint32_t length       = 0; // 4 : the length of the message, no't contain the header
    int32_t protoType     = 0; // 4 : the type of the message in proto factory
    uint16_t transType    = 0; // 2 : the type of this transaction
    uint32_t protoVersion = 0; // 4 : the version of protoFactory field

    template <typename SerializerT>
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {
        return serializer(makeFixedLengthField(length), makeFixedLengthField(protoType),
                          makeFixedLengthField(transType), makeFixedLengthField(protoVersion));
    }
    template <typename SerializerT>
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {
        return serializer(makeFixedLengthField(length), makeFixedLengthField(protoType),
                          makeFixedLengthField(transType), makeFixedLengthField(protoVersion));
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

template <ILIAS_NAMESPACE::StreamClient T = ILIAS_NAMESPACE::IStreamClient>
class NEKO_PROTO_API ProtoStreamClient {
public:
    enum Strategy {
        None               = 0,
        SerializerInThread = 0x01,
    };

public:
    ProtoStreamClient() = default;
    ProtoStreamClient(ProtoFactory& factory, ILIAS_NAMESPACE::IoContext& ioContext, T&& streamClient);
    ProtoStreamClient(const ProtoStreamClient&) = delete;
    ProtoStreamClient(ProtoStreamClient&&);
    ~ProtoStreamClient() noexcept;
    auto setStreamClient(T&& streamClient) -> void;
    auto send(const IProto& message, int flag = Strategy::None) -> ILIAS_NAMESPACE::Task<void>;
    auto recv(int flag = Strategy::None) -> ILIAS_NAMESPACE::Task<std::unique_ptr<IProto>>;
    auto sendRaw(std::span<std::byte> data) -> ILIAS_NAMESPACE::Task<void>;
    auto recvRaw(std::span<std::byte> buf) -> ILIAS_NAMESPACE::Task<void>;
    auto close() -> ILIAS_NAMESPACE::Task<void>;

private:
private:
    ProtoFactory* mFactory                 = nullptr;
    ILIAS_NAMESPACE::IoContext* mIoContext = nullptr;
    T mStreamClient;
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
inline auto ProtoStreamClient<T>::send(const IProto& message, int flag) -> ILIAS_NAMESPACE::Task<void> {
    std::vector<char> messageData;
    if (flag & Strategy::SerializerInThread) {
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
    auto header = MessageHeader(messageData.size(), message.type(), MessageType::Complete, mFactory->version());
    std::vector<char> headerData;
    do {
        BinaryOutputSerializer serializer(headerData);
        serializer(header);
    } while (false);
    auto ret = co_await sendRaw({reinterpret_cast<std::byte*>(headerData.data()), headerData.size()});
    if (!ret) {
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    co_return co_await sendRaw({reinterpret_cast<std::byte*>(messageData.data()), messageData.size()});
}

template <ILIAS_NAMESPACE::StreamClient T>
inline auto ProtoStreamClient<T>::recv(int flag) -> ILIAS_NAMESPACE::Task<std::unique_ptr<IProto>> {
    MessageHeader header;
    do {
        std::vector<std::byte> messageHeader(MessageHeader::size());
        auto ret = co_await recvRaw(messageHeader);
        if (!ret) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        BinaryInputSerializer input(reinterpret_cast<char*>(messageHeader.data()), messageHeader.size());
        if (!input(header)) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidMessageHeader));
        }
    } while (false);
    if (header.protoVersion != mFactory->version()) {
        NEKO_LOG_WARN("Communication", "ProtoFactory version mismatch: {} != {}", header.protoVersion,
                      mFactory->version());
    }
    if (header.transType != MessageType::Complete) {
        NEKO_LOG_ERROR("Communication", "unsupported message type: {}", header.transType);
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::ConnectionMessageTypeError));
    }
    auto proto = mFactory->create(header.protoType);
    if (proto == nullptr) {
        NEKO_LOG_ERROR("Communication", "unsupported proto type: {}", header.protoType);
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoType));
    }
    std::vector<std::byte> message(header.length);
    auto ret = co_await recvRaw(message);
    if (!ret) {
        co_return ILIAS_NAMESPACE::Unexpected(ret.error());
    }
    if (flag & Strategy::SerializerInThread) {
        auto ret = co_await detail::ThreadAwaiter<bool>(
            std::bind(&IProto::formData, proto.get(), reinterpret_cast<char*>(message.data()), message.size()));
        if (!ret && !ret.value()) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoData)));
        }
    } else {
        if (!proto->formData(reinterpret_cast<char*>(message.data()), message.size())) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoData));
        }
    }
    co_return std::move(proto);
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
