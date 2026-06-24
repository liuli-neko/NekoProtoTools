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

#include "nekoproto/rpc/backend_base.hpp"
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
    using Codec = detail::NekoRpcFrameCodec;
    using Id = typename Codec::Id;
    using Message = typename Codec::Message;
    using ResponseValues = typename Codec::ResponseValues;
    using Kind = detail::NekoRpcKind;
    using Flag = detail::NekoRpcFlag;
    using Header = typename Codec::Header;
    using DecodedRequest = typename Codec::DecodedRequest;
    using DecodeResult = typename Codec::DecodeResult;

    struct EncodedRequest {
        Message message;
        Id id = 0;
    };

private:
    template <ilias::Stream StreamT>
    class StreamEndpoint;

public:
    static DecodeResult decodeIncoming(std::span<const std::byte> message) {
        return Codec::decodeIncoming(message, CodecId);
    }

    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view {
        return Codec::methodName(request);
    }
    static auto id(const DecodedRequest& request) noexcept -> const Id& { return Codec::id(request); }
    static bool expectsResponse(const DecodedRequest& request) noexcept { return Codec::expectsResponse(request); }

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

    static Message encodeResponses(const ResponseValues& responses, bool batch) {
        return Codec::encodeResponses(responses, batch);
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
    static constexpr std::size_t HeaderSize = Codec::HeaderSize;

    static auto _encodeFrame(Kind kind, Id id, std::string_view method, const Message& payload,
                            std::uint8_t flags) -> Message {
        return Codec::encodeFrame(kind, id, method, payload, flags, CodecId);
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
        return Codec::decodeFrame(data);
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
            auto headerRet =
                co_await ilias::io::readAll(mStream, std::span<std::byte>{headerBytes.data(), headerBytes.size()});
            if (!headerRet) {
                co_return ilias::Err(headerRet.error());
            }
            if (headerRet.value() != HeaderSize) {
                co_return ilias::Err(ilias::IoError::UnexpectedEOF);
            }

            const auto header = std::span<const std::byte>{headerBytes.data(), headerBytes.size()};
            auto bodySizeRet = Codec::headerBodySize(header, CodecId);
            if (!bodySizeRet) {
                co_return ilias::Err(bodySizeRet.error());
            }

            const auto bodySize = bodySizeRet.value();
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
