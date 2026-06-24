#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <ilias/buffer.hpp>
#include <ilias/io/error.hpp>
#include <ilias/result.hpp>
#include <ilias/task.hpp>
#include <ilias/io.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/traits.hpp>

#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {

template <typename Tuple>
struct NekoRpcDecayTuple;

template <typename... Args>
struct NekoRpcDecayTuple<std::tuple<Args...>> {
    using type = std::tuple<std::decay_t<Args>...>;
};

template <typename Tuple>
using NekoRpcDecayTupleT = typename NekoRpcDecayTuple<Tuple>::type;

} // namespace detail

struct NekoRpcError {
    std::int32_t code = static_cast<std::int32_t>(RpcError::InternalError);
    std::string message;
    std::vector<std::byte> data;

    NEKO_SERIALIZER(code, message, data)
};

template <typename Serializer, std::uint8_t CodecId = 0>
struct NekoRpcBackend {
    using Id = std::uint64_t;
    using Message = std::vector<char>;
    using ResponseValues = std::vector<Message>;

    enum class Kind : std::uint8_t {
        Request = 1,
        Response = 2,
        Notify = 3,
        Cancel = 4,
        Hello = 5,
    };

    enum Flag : std::uint8_t {
        Error = 1U << 0U,
        MethodId = 1U << 1U,
        HasExtensions = 1U << 2U,
    };

    struct Header {
        std::uint16_t magic = Magic;
        std::uint8_t version = Version;
        Kind kind = Kind::Request;
        std::uint8_t flags = 0;
        std::uint8_t codec = CodecId;
        std::uint16_t extensionSize = 0;
        Id id = 0;
        std::uint32_t methodSize = 0;
        std::uint32_t payloadSize = 0;
    };

    struct DecodedRequest {
        Header header;
        std::string method;
        Message payload;
    };

    struct DecodeResult {
        bool ok = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
    };

    struct EncodedRequest {
        Message message;
        Id id = 0;
    };

private:
    template <ilias::Stream StreamT>
    class StreamEndpoint;

public:
    static DecodeResult decodeIncoming(std::span<const std::byte> message) {
        DecodeResult result;
        auto frame = _decodeFrame(message);
        if (!frame || frame.value().header.codec != CodecId) {
            return result;
        }

        auto& request = frame.value();
        if (request.header.kind == Kind::Cancel || request.header.kind == Kind::Hello) {
            result.ok = true;
            return result;
        }
        if (request.header.kind != Kind::Request && request.header.kind != Kind::Notify) {
            return result;
        }
        if ((request.header.flags & Flag::Error) != 0U || (request.header.flags & Flag::MethodId) != 0U ||
            request.method.empty()) {
            return result;
        }

        result.ok = true;
        result.requests.emplace_back(std::move(request));
        return result;
    }

    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view { return request.method; }
    static auto id(const DecodedRequest& request) noexcept -> const Id& { return request.header.id; }
    static bool expectsResponse(const DecodedRequest& request) noexcept {
        return request.header.kind == Kind::Request;
    }

    template <typename Method>
    static auto decodeParams(const DecodedRequest& request)
        -> ilias::Result<detail::NekoRpcDecayTupleT<typename Method::RawParamsType>, std::error_code> {
        using Params = detail::NekoRpcDecayTupleT<typename Method::RawParamsType>;
        Params params;
        if (!_decodePayload(request.payload, params)) {
            return ilias::Err(RpcError::InvalidParams);
        }
        return params;
    }

    template <typename Method>
    static auto invoke(Method& method, detail::NekoRpcDecayTupleT<typename Method::RawParamsType> params)
        -> ilias::IoTask<typename Method::RawReturnType> {
        if constexpr (std::tuple_size_v<decltype(params)> == 0) {
            if constexpr (std::is_void_v<typename Method::RawReturnType>) {
                co_await method();
                co_return {};
            } else {
                co_return co_await method();
            }
        } else {
            if constexpr (std::is_void_v<typename Method::RawReturnType>) {
                co_await std::apply(method, std::move(params));
                co_return {};
            } else {
                co_return co_await std::apply(method, std::move(params));
            }
        }
    }

    template <typename Method>
    static void appendMethodReturn(ResponseValues& responses, const DecodedRequest& request,
                                   ilias::Result<typename Method::RawReturnType, std::error_code> result) {
        if (!expectsResponse(request)) {
            return;
        }
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            if (result) {
                responses.emplace_back(_encodeFrame(Kind::Response, request.header.id, {}, {}, 0));
            } else {
                responses.emplace_back(_encodeErrorFrame(request.header.id, result.error()));
            }
        } else {
            if (result) {
                auto payload = _encodePayload(result.value());
                if (payload) {
                    responses.emplace_back(_encodeFrame(Kind::Response, request.header.id, {}, payload.value(), 0));
                } else {
                    responses.emplace_back(_encodeErrorFrame(request.header.id, RpcError::InternalError));
                }
            } else {
                responses.emplace_back(_encodeErrorFrame(request.header.id, result.error()));
            }
        }
    }

    static void appendError(ResponseValues& responses, const DecodedRequest& request, std::error_code error) {
        if (!expectsResponse(request)) {
            return;
        }
        responses.emplace_back(_encodeErrorFrame(request.header.id, error));
    }

    static Message encodeResponses(const ResponseValues& responses, bool /*batch*/) {
        if (responses.empty()) {
            return {};
        }
        return responses.front();
    }

    template <typename Method, typename... Args>
    static auto encodeRequest(Method& method, bool notification, std::uint64_t& nextId, Args&&... args)
        -> ilias::Result<EncodedRequest, std::error_code> {
        using Params = std::tuple<std::decay_t<Args>...>;
        auto payload = _encodePayload(Params{std::forward<Args>(args)...});
        if (!payload) {
            return ilias::Err(RpcError::InvalidParams);
        }

        Id id = 0;
        const auto kind = notification ? Kind::Notify : Kind::Request;
        if (!notification) {
            if (nextId == std::numeric_limits<Id>::max()) {
                nextId = 1;
            } else {
                ++nextId;
            }
            id = nextId;
        }

        return EncodedRequest{
            .message = _encodeFrame(kind, id, method.name(), payload.value(), 0),
            .id = id,
        };
    }

    template <typename Method>
    static auto decodeResponse(std::span<const std::byte> buffer, const Id& expectedId)
        -> ilias::Result<typename Method::RawReturnType, std::error_code> {
        auto frame = _decodeFrame(buffer);
        if (!frame) {
            return ilias::Err(frame.error());
        }
        const auto& response = frame.value();
        if (response.header.kind != Kind::Response) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        if (response.header.codec != CodecId) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        if (response.header.id != expectedId) {
            return ilias::Err(RpcError::ResponseIdNotMatch);
        }
        if ((response.header.flags & Flag::Error) != 0U) {
            NekoRpcError error;
            if (!_decodePayload(response.payload, error)) {
                return ilias::Err(RpcError::InternalError);
            }
            return ilias::Err(std::error_code(error.code, RpcErrorCategory::instance()));
        }
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            return {};
        } else {
            typename Method::RawReturnType value;
            if (!_decodePayload(response.payload, value)) {
                return ilias::Err(RpcError::InvalidParams);
            }
            return value;
        }
    }

    static std::error_code clientNotInitError() { return RpcError::ClientNotInit; }
    static std::error_code notificationOk() { return RpcError::Ok; }

    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream) {
        return StreamEndpoint<StreamT>{std::move(stream)};
    }

private:
    static constexpr std::uint16_t Magic = 0x4E52U;
    static constexpr std::uint8_t Version = 1U;
    static constexpr std::size_t HeaderSize = 24U;

    static void _appendU8(Message& out, std::uint8_t value) { out.push_back(static_cast<char>(value)); }
    static void _appendU16(Message& out, std::uint16_t value) {
        out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
        out.push_back(static_cast<char>(value & 0xFFU));
    }
    static void _appendU32(Message& out, std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            out.push_back(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU));
        }
    }
    static void _appendU64(Message& out, std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            out.push_back(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU));
        }
    }

    static std::uint8_t _byteAt(std::span<const std::byte> data, std::size_t offset) {
        return static_cast<std::uint8_t>(data[offset]);
    }
    static std::uint16_t _readU16(std::span<const std::byte> data, std::size_t offset) {
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(_byteAt(data, offset)) << 8U) |
                                          static_cast<std::uint16_t>(_byteAt(data, offset + 1U)));
    }
    static std::uint32_t _readU32(std::span<const std::byte> data, std::size_t offset) {
        std::uint32_t value = 0;
        for (std::size_t ix = 0; ix < 4; ++ix) {
            value = (value << 8U) | _byteAt(data, offset + ix);
        }
        return value;
    }
    static std::uint64_t _readU64(std::span<const std::byte> data, std::size_t offset) {
        std::uint64_t value = 0;
        for (std::size_t ix = 0; ix < 8; ++ix) {
            value = (value << 8U) | _byteAt(data, offset + ix);
        }
        return value;
    }

    static auto _encodeFrame(Kind kind, Id id, std::string_view method, const Message& payload,
                            std::uint8_t flags) -> Message {
        Message out;
        const auto methodSize = static_cast<std::uint32_t>(method.size());
        const auto payloadSize = static_cast<std::uint32_t>(payload.size());
        out.reserve(HeaderSize + method.size() + payload.size());
        _appendU16(out, Magic);
        _appendU8(out, Version);
        _appendU8(out, static_cast<std::uint8_t>(kind));
        _appendU8(out, flags);
        _appendU8(out, CodecId);
        _appendU16(out, 0);
        _appendU64(out, id);
        _appendU32(out, methodSize);
        _appendU32(out, payloadSize);
        out.insert(out.end(), method.begin(), method.end());
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    static auto _encodeErrorFrame(Id id, std::error_code error) -> Message {
        const NekoRpcError rpcError{
            .code = static_cast<std::int32_t>(error.value()),
            .message = error.message(),
            .data = {},
        };
        auto payload = _encodePayload(rpcError);
        if (!payload) {
            return _encodeFrame(Kind::Response, id, {}, {}, Flag::Error);
        }
        return _encodeFrame(Kind::Response, id, {}, payload.value(), Flag::Error);
    }

    static auto _decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code> {
        if (data.size() < HeaderSize) {
            return ilias::Err(RpcError::InvalidRequest);
        }

        Header header;
        header.magic = _readU16(data, 0);
        header.version = _byteAt(data, 2);
        header.kind = static_cast<Kind>(_byteAt(data, 3));
        header.flags = _byteAt(data, 4);
        header.codec = _byteAt(data, 5);
        header.extensionSize = _readU16(data, 6);
        header.id = _readU64(data, 8);
        header.methodSize = _readU32(data, 16);
        header.payloadSize = _readU32(data, 20);

        if (header.magic != Magic || header.version != Version) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        if (!_knownKind(header.kind)) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        const auto expectedSize = HeaderSize + static_cast<std::size_t>(header.methodSize) +
                                  static_cast<std::size_t>(header.extensionSize) +
                                  static_cast<std::size_t>(header.payloadSize);
        if (expectedSize != data.size()) {
            return ilias::Err(RpcError::InvalidRequest);
        }

        const auto methodBegin = HeaderSize;
        const auto extensionBegin = methodBegin + static_cast<std::size_t>(header.methodSize);
        const auto payloadBegin = extensionBegin + static_cast<std::size_t>(header.extensionSize);
        if (!_extensionsSupported(data.subspan(extensionBegin, header.extensionSize))) {
            return ilias::Err(RpcError::InvalidRequest);
        }

        DecodedRequest request;
        request.header = header;
        request.method.assign(reinterpret_cast<const char*>(data.data() + methodBegin), header.methodSize);
        request.payload.resize(header.payloadSize);
        if (header.payloadSize > 0U) {
            std::memcpy(request.payload.data(), data.data() + payloadBegin, header.payloadSize);
        }
        return request;
    }

    static bool _knownKind(Kind kind) {
        switch (kind) {
        case Kind::Request:
        case Kind::Response:
        case Kind::Notify:
        case Kind::Cancel:
        case Kind::Hello:
            return true;
        default:
            return false;
        }
    }

    static bool _extensionsSupported(std::span<const std::byte> extensions) {
        std::size_t offset = 0;
        while (offset < extensions.size()) {
            if (extensions.size() - offset < 4U) {
                return false;
            }
            const auto type = _readU16(extensions, offset);
            const auto size = _readU16(extensions, offset + 2U);
            offset += 4U;
            if (extensions.size() - offset < size) {
                return false;
            }
            if ((type & 0x8000U) != 0U) {
                return false;
            }
            offset += size;
        }
        return true;
    }

    template <typename T>
    static auto _encodePayload(const T& value) -> ilias::Result<Message, std::error_code> {
        Message payload;
        typename Serializer::OutputSerializer out(payload);
        if (out(value) && out.end()) {
            return payload;
        }
        return ilias::Err(RpcError::InvalidParams);
    }

    template <typename T>
    static bool _decodePayload(const Message& payload, T& value) {
        typename Serializer::InputSerializer in(payload.data(), payload.size());
        return in(value);
    }

    template <ilias::Stream StreamT>
    class StreamEndpoint {
    public:
        explicit StreamEndpoint(StreamT stream) : mStream(std::move(stream)) {}

        auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<void> {
            std::array<std::byte, HeaderSize> headerBytes{};
            auto headerRet = co_await ilias::io::readAll(mStream, std::span<std::byte>{headerBytes.data(), headerBytes.size()});
            if (!headerRet) {
                co_return ilias::Err(headerRet.error());
            }
            if (headerRet.value() != HeaderSize) {
                co_return ilias::Err(ilias::IoError::UnexpectedEOF);
            }

            const auto header = std::span<const std::byte>{headerBytes.data(), headerBytes.size()};
            if (_readU16(header, 0) != Magic || _byteAt(header, 2) != Version ||
                !_knownKind(static_cast<Kind>(_byteAt(header, 3))) || _byteAt(header, 5) != CodecId) {
                co_return ilias::Err(RpcError::InvalidRequest);
            }

            const auto extensionSize = static_cast<std::size_t>(_readU16(header, 6));
            const auto methodSize = static_cast<std::size_t>(_readU32(header, 16));
            const auto payloadSize = static_cast<std::size_t>(_readU32(header, 20));
            const auto maxSize = std::numeric_limits<std::size_t>::max();
            if (methodSize > maxSize - extensionSize ||
                methodSize + extensionSize > maxSize - payloadSize ||
                methodSize + extensionSize + payloadSize > maxSize - HeaderSize) {
                co_return ilias::Err(RpcError::InvalidRequest);
            }

            const auto bodySize = methodSize + extensionSize + payloadSize;
            buffer.resize(HeaderSize + bodySize);
            std::memcpy(buffer.data(), headerBytes.data(), HeaderSize);
            if (bodySize == 0U) {
                co_return {};
            }

            auto bodyRet = co_await ilias::io::readAll(
                mStream, std::span<std::byte>{buffer.data() + HeaderSize, bodySize});
            if (!bodyRet) {
                co_return ilias::Err(bodyRet.error());
            }
            if (bodyRet.value() != bodySize) {
                co_return ilias::Err(ilias::IoError::UnexpectedEOF);
            }
            co_return {};
        }

        auto send(std::span<const std::byte> buffer) -> ilias::IoTask<void> {
            auto writeRet = co_await ilias::io::writeAll(mStream, buffer);
            if (!writeRet) {
                co_return ilias::Err(writeRet.error());
            }
            if (writeRet.value() != buffer.size()) {
                co_return ilias::Err(ilias::IoError::WriteZero);
            }
            if (auto flushRet = co_await mStream.flush(); !flushRet) {
                co_return ilias::Err(flushRet.error());
            }
            co_return {};
        }

        auto close() -> void {
            if constexpr (requires(StreamT& stream) { stream.close(); }) {
                mStream.close();
            }
        }

        auto cancel() -> void {
            if constexpr (requires(StreamT& stream) { stream.cancel(); }) {
                mStream.cancel();
            } else {
                close();
            }
        }

    private:
        StreamT mStream;
    };
};

using BinaryRpcBackend = NekoRpcBackend<BinarySerializer, 0>;

NEKO_END_NAMESPACE
