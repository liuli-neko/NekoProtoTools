#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <ilias/buffer.hpp>
#include <ilias/io.hpp>
#include <ilias/io/error.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/result.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/task.hpp>

#include "nekoproto/global/log.hpp"
#include "nekoproto/rpc/backend_base.hpp"
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/parsing/parser.hpp"
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

template <typename Serializer, std::uint8_t CodecId = 0, typename CompressionCodecT = detail::NekoRpcCompressionCodec>
struct NekoRpcBackend {
    using Codec            = detail::NekoRpcFrameCodec;
    using CompressionCodec = CompressionCodecT;
    using CompressionStats = detail::NekoRpcCompressionStats;
    using Id               = typename Codec::Id;
    using Message          = typename Codec::Message;
    using ResponseValues   = typename Codec::ResponseValues;
    using Kind             = detail::NekoRpcKind;
    using Flag             = detail::NekoRpcFlag;
    using Header           = typename Codec::Header;
    using DecodedRequest   = typename Codec::DecodedRequest;
    using DecodeResult     = typename Codec::DecodeResult;

    using MethodIdMode         = detail::NekoRpcFeatureMode;
    using CompressionMode      = detail::NekoRpcFeatureMode;
    using CompressionAlgorithm = detail::NekoRpcCompressionAlgorithm;

    struct Options {
        MethodIdMode methodId                   = MethodIdMode::Auto;
        CompressionMode compression             = CompressionMode::Disable;
        std::uint32_t compressionMinPayloadSize = 64;
        std::shared_ptr<CompressionStats> compressionStats;
    };

    struct EncodedRequest {
        Message message;
        Id id = 0;
    };

private:
    enum class EndpointRole {
        Plain,
        Client,
        Server,
    };

    template <ilias::Stream StreamT>
    class StreamEndpoint;

public:
    template <typename T>
    static consteval bool serializable() {
        if constexpr (std::is_void_v<T>) {
            return true;
        } else {
            return detail::parser_serializable<typename Serializer::Reader, typename Serializer::Writer,
                                               std::decay_t<T>>;
        }
    }

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
        static_assert(BackendSerializable<NekoRpcBackend, Params>,
                      "NekoRpcBackend: method parameters are not serializable by this Serializer");
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
        static_assert(BackendSerializable<NekoRpcBackend, typename Method::RawReturnType>,
                      "NekoRpcBackend: method return type is not serializable by this Serializer");
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
        static_assert(BackendSerializable<NekoRpcBackend, Params>,
                      "NekoRpcBackend: request parameters are not serializable by this Serializer");
        auto payload = _encodePayload(Params{std::forward<Args>(args)...});
        if (!payload) {
            return ilias::Err(RpcError::InvalidParams);
        }

        Id id           = 0;
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
            .id      = id,
        };
    }

    template <typename Method>
    static auto decodeResponse(std::span<const std::byte> buffer, const Id& expectedId)
        -> ilias::Result<typename Method::RawReturnType, std::error_code> {
        static_assert(BackendSerializable<NekoRpcBackend, typename Method::RawReturnType>,
                      "NekoRpcBackend: response type is not serializable by this Serializer");
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
        return StreamEndpoint<StreamT>{std::move(stream), EndpointRole::Plain, Options{}, {}};
    }

    template <RpcMessageEndpoint EndpointT>
    static auto makeEndpoint(EndpointT endpoint) {
        return endpoint;
    }

    template <ilias::Stream StreamT>
    static auto makeClientEndpoint(StreamT stream, Options options = {}) {
        return StreamEndpoint<StreamT>{std::move(stream), EndpointRole::Client, options, {}};
    }

    template <ilias::Stream StreamT, typename Methods>
    static auto makeServerEndpoint(StreamT stream, Methods&& methods, Options options = {}) {
        return StreamEndpoint<StreamT>{std::move(stream), EndpointRole::Server, options,
                                       _methodEntries(std::forward<Methods>(methods))};
    }

private:
    static constexpr std::size_t HeaderSize = Codec::HeaderSize;
    using FrameParts                        = typename Codec::FrameParts;

    template <typename Methods>
    static auto _methodEntries(Methods&& methods) -> std::vector<detail::NekoRpcMethodEntry> {
        std::vector<detail::NekoRpcMethodEntry> entries;
        std::uint64_t id = 0;
        for (const auto& method : methods) {
            std::string_view description;
            if constexpr (requires { method.description; }) {
                description = method.description;
            }
            std::string name{method.name};
            entries.push_back({
                .id            = id++,
                .name          = name,
                .signatureHash = detail::NekoRpcMethodIdTable::signatureHash(name, description),
                .state         = detail::NekoRpcMethodState::Active,
            });
        }
        return entries;
    }

    static auto _methodEntriesFromMetadata(const std::vector<detail::RpcMethodMetadata>& methods)
        -> std::vector<detail::NekoRpcMethodEntry> {
        return _methodEntries(methods);
    }

    static constexpr auto _featureModeName(MethodIdMode mode) noexcept -> std::string_view {
        switch (mode) {
        case MethodIdMode::Disable:
            return "disable";
        case MethodIdMode::Auto:
            return "auto";
        case MethodIdMode::Require:
            return "require";
        }
        return "unknown";
    }

    static constexpr auto _compressionAlgorithmName(CompressionAlgorithm algorithm) noexcept -> std::string_view {
        switch (algorithm) {
        case CompressionAlgorithm::None:
            return "none";
        case CompressionAlgorithm::RunLength:
            return "run_length";
        }
        return "unknown";
    }

    static constexpr auto _kindName(Kind kind) noexcept -> std::string_view {
        switch (kind) {
        case Kind::Request:
            return "request";
        case Kind::Response:
            return "response";
        case Kind::Notify:
            return "notify";
        case Kind::Cancel:
            return "cancel";
        case Kind::Hello:
            return "hello";
        }
        return "unknown";
    }

    static constexpr auto _methodIdResolveStatusName(detail::NekoRpcMethodIdResolveStatus status) noexcept
        -> std::string_view {
        switch (status) {
        case detail::NekoRpcMethodIdResolveStatus::Ok:
            return "ok";
        case detail::NekoRpcMethodIdResolveStatus::TableOutdated:
            return "table_outdated";
        case detail::NekoRpcMethodIdResolveStatus::MethodNotFound:
            return "method_not_found";
        case detail::NekoRpcMethodIdResolveStatus::MethodRemoved:
            return "method_removed";
        case detail::NekoRpcMethodIdResolveStatus::SignatureMismatch:
            return "signature_mismatch";
        }
        return "unknown";
    }

    static auto _encodeFrame(Kind kind, Id id, std::string_view method, const Message& payload, std::uint8_t flags)
        -> Message {
        return Codec::encodeFrame(kind, id, method, payload, flags, CodecId);
    }

    static auto _encodeErrorFrame(Id id, std::error_code error) -> Message {
        const NekoRpcError rpcError{
            .code    = static_cast<std::int32_t>(error.value()),
            .message = error.message(),
            .data    = {},
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
        StreamEndpoint(StreamT stream, EndpointRole role, Options options,
                       std::vector<detail::NekoRpcMethodEntry> methods)
            : mStream(std::move(stream)), mRole(role), mOptions(options) {
            mMethodTable.reset(std::move(methods), detail::NekoRpcMethodIdExtension::InitialTableVersion);
        }

        auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> {
            while (true) {
                Message frame;
                if (auto ret = co_await _readFrame(frame); !ret) {
                    co_return ilias::Err(ret.error());
                }
                if (_isHello(frame)) {
                    if (mRole == EndpointRole::Server) {
                        if (auto ret = co_await _handleClientHello(frame); !ret) {
                            co_return ilias::Err(ret.error());
                        }
                        continue;
                    }
                    if (mRole == EndpointRole::Client) {
                        _handleServerHello(frame);
                        continue;
                    }
                }
                auto decompressed = _decompressIncomingFrame(frame);
                if (!decompressed) {
                    co_return ilias::Err(decompressed.error());
                }
                frame = std::move(decompressed.value());
                if (mRole == EndpointRole::Server && !mHandshakeDone) {
                    if (mOptions.methodId == MethodIdMode::Require) {
                        co_return ilias::Err(RpcError::InvalidRequest);
                    }
                    mHandshakeDone = true;
                }
                if (mRole == EndpointRole::Server) {
                    auto rewritten = _rewriteIncomingMethodId(frame);
                    if (!rewritten) {
                        if (auto ret = co_await _sendMethodIdError(frame, rewritten.error()); !ret) {
                            co_return ilias::Err(ret.error());
                        }
                        continue;
                    }
                    frame = std::move(rewritten.value());
                }
                if (mRole == EndpointRole::Client) {
                    if (auto retried = co_await _retryMethodIdErrorIfNeeded(frame); !retried) {
                        co_return ilias::Err(retried.error());
                    } else if (retried.value()) {
                        continue;
                    }
                    _forgetPendingRetry(frame);
                }
                buffer.resize(frame.size());
                std::memcpy(buffer.data(), frame.data(), frame.size());
                co_return buffer.size();
            }
        }

        auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> {
            if (mRole == EndpointRole::Client) {
                if (auto ret = co_await _ensureClientHandshake(); !ret) {
                    co_return ilias::Err(ret.error());
                }
            }
            Message frame(reinterpret_cast<const char*>(buffer.data()),
                          reinterpret_cast<const char*>(buffer.data()) + buffer.size());
            Message originalFrame = frame;
            if (mRole == EndpointRole::Client) {
                frame = _rewriteOutgoingMethodName(frame);
                _rememberPendingRetry(originalFrame, frame);
            }
            auto compressed = _compressOutgoingFrame(frame);
            if (!compressed) {
                co_return ilias::Err(compressed.error());
            }
            if (auto ret = co_await _writeFrame(compressed.value()); !ret) {
                co_return ilias::Err(ret.error());
            }
            co_return buffer.size();
        }

        auto refreshRpcMethods(std::vector<detail::RpcMethodMetadata> methods) -> ilias::IoTask<void> {
            if (mRole != EndpointRole::Server) {
                co_return {};
            }
            std::vector<detail::NekoRpcMethodEntry> delta;
            _syncMethodTable(_methodEntriesFromMetadata(methods), delta);
            if (delta.empty() || !mHandshakeDone || !mMethodIdEnabled) {
                co_return {};
            }
            NEKO_LOG_INFO("rpc", "rpc backend method-id table refresh: version={} min_version={} delta_count={}",
                          mMethodTable.version(), mMethodTable.minimumCompatibleVersion(), delta.size());
            auto update = _methodTableHello(delta, false);
            if (auto ret = co_await _writeFrame(update); !ret) {
                co_return ilias::Err(ret.error());
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

        auto shutdown() -> ilias::IoTask<void> {
            if constexpr (requires(StreamT& stream) { stream.shutdown(); }) {
                if (auto ret = co_await flush(); !ret) {
                    co_return ilias::Err(ret.error());
                }
                co_return co_await mStream.shutdown();
            } else {
                close();
                co_return {};
            }
        }

        auto flush() -> ilias::IoTask<void> {
            if constexpr (requires(StreamT& stream) { stream.flush(); }) {
                co_return co_await mStream.flush();
            } else {
                co_return {};
            }
        }

    private:
        auto _readFrame(Message& buffer) -> ilias::IoTask<void> {
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
            auto bodySizeRet  = Codec::headerBodySize(header, CodecId);
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
                mStream, std::span<std::byte>{reinterpret_cast<std::byte*>(buffer.data() + HeaderSize), bodySize});
            if (!bodyRet) {
                co_return ilias::Err(bodyRet.error());
            }
            if (bodyRet.value() != bodySize) {
                co_return ilias::Err(ilias::IoError::UnexpectedEOF);
            }
            co_return {};
        }

        auto _writeFrame(const Message& frame) -> ilias::IoTask<void> {
            auto guard = co_await mWriteMutex->lock();
            auto ret   = co_await ilias::io::writeAll(
                mStream, std::span<const std::byte>{reinterpret_cast<const std::byte*>(frame.data()), frame.size()});
            if (!ret) {
                co_return ilias::Err(ret.error());
            }
            if (ret.value() != frame.size()) {
                co_return ilias::Err(ilias::IoError::WriteZero);
            }
            if (auto flushRet = co_await mStream.flush(); !flushRet) {
                co_return ilias::Err(flushRet.error());
            }
            co_return {};
        }

        auto _ensureClientHandshake() -> ilias::IoTask<void> {
            if (mHandshakeDone ||
                (mOptions.methodId == MethodIdMode::Disable && mOptions.compression == CompressionMode::Disable)) {
                mHandshakeDone = true;
                co_return {};
            }
            Message helloExtensions;
            if (mOptions.methodId != MethodIdMode::Disable) {
                auto methodIdTlv = detail::NekoRpcMethodIdExtension::modeTlv(mOptions.methodId);
                helloExtensions.insert(helloExtensions.end(), methodIdTlv.begin(), methodIdTlv.end());
            }
            if (mOptions.compression != CompressionMode::Disable) {
                auto compressionTlv = detail::NekoRpcCompressionExtension::modeTlv(mOptions.compression);
                auto algorithmTlv =
                    detail::NekoRpcCompressionExtension::algorithmTlv(CompressionCodec::preferredAlgorithm());
                auto minSizeTlv =
                    detail::NekoRpcCompressionExtension::minPayloadSizeTlv(mOptions.compressionMinPayloadSize);
                helloExtensions.insert(helloExtensions.end(), compressionTlv.begin(), compressionTlv.end());
                helloExtensions.insert(helloExtensions.end(), algorithmTlv.begin(), algorithmTlv.end());
                helloExtensions.insert(helloExtensions.end(), minSizeTlv.begin(), minSizeTlv.end());
            }
            NEKO_LOG_INFO("rpc", "rpc backend client hello: method_id={} compression={} algorithm={} min_payload={}",
                          _featureModeName(mOptions.methodId), _featureModeName(mOptions.compression),
                          _compressionAlgorithmName(CompressionCodec::preferredAlgorithm()),
                          mOptions.compressionMinPayloadSize);
            auto hello = Codec::encodeHello(detail::NekoRpcExtensionCodec::asBytes(helloExtensions), CodecId);
            if (auto ret = co_await _writeFrame(hello); !ret) {
                co_return ilias::Err(ret.error());
            }
            Message response;
            if (auto ret = co_await _readFrame(response); !ret) {
                co_return ilias::Err(ret.error());
            }
            if (!_isHello(response) || !_handleServerHello(response)) {
                co_return ilias::Err(RpcError::InvalidRequest);
            }
            if (mOptions.methodId == MethodIdMode::Require && !mMethodIdEnabled) {
                NEKO_LOG_WARN("rpc", "rpc backend method-id negotiation failed: required by client");
                co_return ilias::Err(RpcError::MethodIdRequiredButUnsupported);
            }
            if (mOptions.compression == CompressionMode::Require && !mCompressionEnabled) {
                NEKO_LOG_WARN("rpc", "rpc backend compression negotiation failed: required by client");
                co_return ilias::Err(RpcError::CompressionRequiredButUnsupported);
            }
            mHandshakeDone = true;
            co_return {};
        }

        auto _handleClientHello(const Message& frame) -> ilias::IoTask<void> {
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts)) {
                co_return ilias::Err(RpcError::InvalidRequest);
            }
            const auto clientMode            = detail::NekoRpcMethodIdExtension::parseMode(parts.extensions);
            const auto clientCompressionMode = detail::NekoRpcCompressionExtension::parseMode(parts.extensions);
            const auto clientCompressionAlgorithm =
                detail::NekoRpcCompressionExtension::parseAlgorithm(parts.extensions);
            std::uint32_t clientMinPayloadSize = 0;
            detail::NekoRpcCompressionExtension::readMinPayloadSize(parts.extensions, clientMinPayloadSize);
            const bool canEnable = mOptions.methodId != MethodIdMode::Disable && clientMode != MethodIdMode::Disable &&
                                   !mMethodTable.empty();
            mMethodIdEnabled    = canEnable;
            mCompressionEnabled = mOptions.compression != CompressionMode::Disable &&
                                  clientCompressionMode != CompressionMode::Disable &&
                                  clientCompressionAlgorithm != CompressionAlgorithm::None &&
                                  CompressionCodec::supports(clientCompressionAlgorithm);
            mCompressionAlgorithm = mCompressionEnabled ? clientCompressionAlgorithm : CompressionAlgorithm::None;
            if (mCompressionEnabled && clientMinPayloadSize > mCompressionMinPayloadSize) {
                mCompressionMinPayloadSize = clientMinPayloadSize;
            }
            NEKO_LOG_INFO("rpc",
                          "rpc backend server hello: client_method_id={} method_id_enabled={} table_version={} "
                          "method_count={} client_compression={} compression_enabled={} algorithm={} min_payload={}",
                          _featureModeName(clientMode), mMethodIdEnabled, mMethodTable.version(),
                          mMethodTable.entries().size(), _featureModeName(clientCompressionMode), mCompressionEnabled,
                          _compressionAlgorithmName(mCompressionAlgorithm), mCompressionMinPayloadSize);

            Message extensions = detail::NekoRpcMethodIdExtension::modeTlv(mMethodIdEnabled ? MethodIdMode::Auto
                                                                                            : MethodIdMode::Disable);
            if (mMethodIdEnabled) {
                const auto versionTlv    = detail::NekoRpcMethodIdExtension::tableVersionTlv(mMethodTable.version());
                const auto minVersionTlv = detail::NekoRpcMethodIdExtension::minimumCompatibleVersionTlv(
                    mMethodTable.minimumCompatibleVersion());
                const auto tableTlv = detail::NekoRpcMethodIdExtension::methodTableTlv(mMethodTable.entries());
                extensions.insert(extensions.end(), versionTlv.begin(), versionTlv.end());
                extensions.insert(extensions.end(), minVersionTlv.begin(), minVersionTlv.end());
                extensions.insert(extensions.end(), tableTlv.begin(), tableTlv.end());
            }
            if (mOptions.compression != CompressionMode::Disable || clientCompressionMode != CompressionMode::Disable) {
                const auto compressionTlv = detail::NekoRpcCompressionExtension::modeTlv(
                    mCompressionEnabled ? CompressionMode::Auto : CompressionMode::Disable);
                extensions.insert(extensions.end(), compressionTlv.begin(), compressionTlv.end());
                if (mCompressionEnabled) {
                    const auto algorithmTlv = detail::NekoRpcCompressionExtension::algorithmTlv(mCompressionAlgorithm);
                    const auto minSizeTlv =
                        detail::NekoRpcCompressionExtension::minPayloadSizeTlv(mCompressionMinPayloadSize);
                    extensions.insert(extensions.end(), algorithmTlv.begin(), algorithmTlv.end());
                    extensions.insert(extensions.end(), minSizeTlv.begin(), minSizeTlv.end());
                }
            }
            auto response = Codec::encodeHello(detail::NekoRpcExtensionCodec::asBytes(extensions), CodecId);
            if (auto ret = co_await _writeFrame(response); !ret) {
                co_return ilias::Err(ret.error());
            }
            if ((mOptions.methodId == MethodIdMode::Require || clientMode == MethodIdMode::Require) && !canEnable) {
                NEKO_LOG_WARN("rpc",
                              "rpc backend method-id negotiation failed: server_mode={} client_mode={} "
                              "method_count={}",
                              _featureModeName(mOptions.methodId), _featureModeName(clientMode),
                              mMethodTable.entries().size());
                co_return ilias::Err(RpcError::MethodIdRequiredButUnsupported);
            }
            if ((mOptions.compression == CompressionMode::Require ||
                 clientCompressionMode == CompressionMode::Require) &&
                !mCompressionEnabled) {
                NEKO_LOG_WARN("rpc",
                              "rpc backend compression negotiation failed: server_mode={} client_mode={} "
                              "client_algorithm={}",
                              _featureModeName(mOptions.compression), _featureModeName(clientCompressionMode),
                              _compressionAlgorithmName(clientCompressionAlgorithm));
                co_return ilias::Err(RpcError::CompressionRequiredButUnsupported);
            }
            mHandshakeDone = true;
            co_return {};
        }

        bool _handleServerHello(const Message& frame) {
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts)) {
                return false;
            }
            const auto mode              = detail::NekoRpcMethodIdExtension::parseMode(parts.extensions);
            mMethodIdEnabled             = mode != MethodIdMode::Disable;
            std::string_view tableUpdate = "none";
            std::size_t tableEntryCount  = 0;
            if (mMethodIdEnabled) {
                std::uint64_t version = 0;
                if (!detail::NekoRpcMethodIdExtension::readTableVersion(parts.extensions, version)) {
                    return false;
                }
                std::vector<detail::NekoRpcMethodEntry> entries;
                if (detail::NekoRpcMethodIdExtension::parseMethodTable(parts.extensions, entries)) {
                    tableUpdate     = "full";
                    tableEntryCount = entries.size();
                    if (!mMethodTable.applyRemoteTable(std::move(entries), version)) {
                        return false;
                    }
                } else if (detail::NekoRpcMethodIdExtension::parseMethodTableDelta(parts.extensions, entries)) {
                    tableUpdate     = "delta";
                    tableEntryCount = entries.size();
                    if (!mMethodTable.applyRemoteDelta(entries, version)) {
                        return false;
                    }
                } else if (mMethodTable.empty()) {
                    return false;
                }
                std::uint64_t minimumVersion = 0;
                if (detail::NekoRpcMethodIdExtension::readMinimumCompatibleVersion(parts.extensions, minimumVersion)) {
                    mMethodTable.setMinimumCompatibleVersion(minimumVersion);
                }
            }
            const auto compressionMode = detail::NekoRpcCompressionExtension::parseMode(parts.extensions);
            if (compressionMode != CompressionMode::Disable) {
                mCompressionAlgorithm = detail::NekoRpcCompressionExtension::parseAlgorithm(parts.extensions);
                mCompressionEnabled   = mCompressionAlgorithm != CompressionAlgorithm::None &&
                                      CompressionCodec::supports(mCompressionAlgorithm);
                std::uint32_t minPayloadSize = 0;
                if (detail::NekoRpcCompressionExtension::readMinPayloadSize(parts.extensions, minPayloadSize)) {
                    mCompressionMinPayloadSize = minPayloadSize;
                }
            } else {
                std::span<const std::byte> compressionValue;
                if (detail::NekoRpcExtensionCodec::findTlv(
                        parts.extensions, detail::NekoRpcCompressionExtension::ModeTlv, compressionValue)) {
                    mCompressionEnabled   = false;
                    mCompressionAlgorithm = CompressionAlgorithm::None;
                }
            }
            NEKO_LOG_INFO("rpc",
                          "rpc backend client accepted hello: method_id={} table_update={} table_version={} "
                          "min_version={} table_entries={} compression={} algorithm={} min_payload={}",
                          mMethodIdEnabled, tableUpdate, mMethodTable.version(),
                          mMethodTable.minimumCompatibleVersion(), tableEntryCount, mCompressionEnabled,
                          _compressionAlgorithmName(mCompressionAlgorithm), mCompressionMinPayloadSize);
            mHandshakeDone = true;
            return true;
        }

        bool _isHello(const Message& frame) const {
            FrameParts parts;
            return Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts) &&
                   parts.header.kind == Kind::Hello;
        }

        auto _methodTableHello(const std::vector<detail::NekoRpcMethodEntry>& entries, bool full) const -> Message {
            Message extensions = detail::NekoRpcMethodIdExtension::modeTlv(mMethodIdEnabled ? MethodIdMode::Auto
                                                                                            : MethodIdMode::Disable);
            if (mMethodIdEnabled) {
                const auto versionTlv    = detail::NekoRpcMethodIdExtension::tableVersionTlv(mMethodTable.version());
                const auto minVersionTlv = detail::NekoRpcMethodIdExtension::minimumCompatibleVersionTlv(
                    mMethodTable.minimumCompatibleVersion());
                const auto tableTlv = full ? detail::NekoRpcMethodIdExtension::methodTableTlv(entries)
                                           : detail::NekoRpcMethodIdExtension::methodTableDeltaTlv(entries);
                extensions.insert(extensions.end(), versionTlv.begin(), versionTlv.end());
                extensions.insert(extensions.end(), minVersionTlv.begin(), minVersionTlv.end());
                extensions.insert(extensions.end(), tableTlv.begin(), tableTlv.end());
            }
            return Codec::encodeHello(detail::NekoRpcExtensionCodec::asBytes(extensions), CodecId);
        }

        auto _syncMethodTable(const std::vector<detail::NekoRpcMethodEntry>& activeEntries,
                              std::vector<detail::NekoRpcMethodEntry>& delta) -> void {
            delta.clear();
            for (const auto& entry : activeEntries) {
                const auto* current = mMethodTable.findByName(entry.name);
                if (current != nullptr && current->state == detail::NekoRpcMethodState::Active &&
                    current->signatureHash == entry.signatureHash) {
                    continue;
                }
                if (const auto* updated = mMethodTable.upsert(entry.name, entry.signatureHash); updated != nullptr) {
                    delta.push_back(*updated);
                }
            }

            std::vector<std::string> removedNames;
            for (const auto& current : mMethodTable.entries()) {
                if (current.state != detail::NekoRpcMethodState::Active || current.name.empty()) {
                    continue;
                }
                bool stillActive = false;
                for (const auto& entry : activeEntries) {
                    if (entry.name == current.name) {
                        stillActive = true;
                        break;
                    }
                }
                if (!stillActive) {
                    removedNames.push_back(current.name);
                }
            }
            for (const auto& name : removedNames) {
                const auto* current = mMethodTable.findByName(name);
                if (current == nullptr) {
                    continue;
                }
                const auto id = current->id;
                if (mMethodTable.remove(name)) {
                    if (const auto* tombstone = mMethodTable.findById(id); tombstone != nullptr) {
                        delta.push_back(*tombstone);
                    }
                }
            }
        }

        auto _addCompressionStat(std::atomic<std::uint64_t> CompressionStats::* counter, std::uint64_t value = 1) const
            -> void {
            if (mOptions.compressionStats == nullptr) {
                return;
            }
            ((*mOptions.compressionStats).*counter).fetch_add(value, std::memory_order_relaxed);
        }

        auto _compressOutgoingFrame(const Message& frame) -> ilias::Result<Message, std::error_code> {
            if (!mCompressionEnabled || mCompressionAlgorithm == CompressionAlgorithm::None) {
                return frame;
            }
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts)) {
                return ilias::Err(RpcError::InvalidRequest);
            }
            if (parts.header.kind == Kind::Hello || (parts.header.flags & Flag::Compressed) != 0U) {
                return frame;
            }
            if (parts.payload.empty() || parts.payload.size() < mCompressionMinPayloadSize) {
                _addCompressionStat(&CompressionStats::skippedSmallPayloads);
                NEKO_LOG_DEBUG("rpc", "rpc backend compression skipped: kind={} id={} payload={} min_payload={}",
                               _kindName(parts.header.kind), parts.header.id, parts.payload.size(),
                               mCompressionMinPayloadSize);
                return frame;
            }
            _addCompressionStat(&CompressionStats::compressionAttempts);
            _addCompressionStat(&CompressionStats::compressionInputBytes, parts.payload.size());
            auto compressed = CompressionCodec::compress(parts.payload, mCompressionAlgorithm);
            if (!compressed) {
                _addCompressionStat(&CompressionStats::compressionErrors);
                NEKO_LOG_WARN("rpc", "rpc backend compression failed: kind={} id={} algorithm={} error={}",
                              _kindName(parts.header.kind), parts.header.id,
                              _compressionAlgorithmName(mCompressionAlgorithm), compressed.error().message());
                return ilias::Err(compressed.error());
            }
            if (compressed.value().size() >= parts.payload.size()) {
                _addCompressionStat(&CompressionStats::skippedIneffectiveFrames);
                NEKO_LOG_DEBUG("rpc",
                               "rpc backend compression skipped: kind={} id={} algorithm={} input={} output={} "
                               "reason=ineffective",
                               _kindName(parts.header.kind), parts.header.id,
                               _compressionAlgorithmName(mCompressionAlgorithm), parts.payload.size(),
                               compressed.value().size());
                return frame;
            }
            _addCompressionStat(&CompressionStats::compressedFrames);
            _addCompressionStat(&CompressionStats::compressionOutputBytes, compressed.value().size());
            NEKO_LOG_INFO("rpc", "rpc backend compressed frame: kind={} id={} algorithm={} input={} output={}",
                          _kindName(parts.header.kind), parts.header.id,
                          _compressionAlgorithmName(mCompressionAlgorithm), parts.payload.size(),
                          compressed.value().size());

            Message method     = detail::NekoRpcExtensionCodec::copyBytes(parts.method);
            Message extensions = detail::NekoRpcExtensionCodec::copyBytes(parts.extensions);
            auto flags         = static_cast<std::uint8_t>(parts.header.flags | Flag::Compressed);
            return Codec::encodeFrame(parts.header.kind, parts.header.id,
                                      std::string_view{method.data(), method.size()},
                                      detail::NekoRpcExtensionCodec::asBytes(extensions),
                                      detail::NekoRpcExtensionCodec::asBytes(compressed.value()), flags, CodecId);
        }

        auto _decompressIncomingFrame(const Message& frame) -> ilias::Result<Message, std::error_code> {
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts)) {
                return ilias::Err(RpcError::InvalidRequest);
            }
            if ((parts.header.flags & Flag::Compressed) == 0U) {
                return frame;
            }
            if (!mCompressionEnabled || mCompressionAlgorithm == CompressionAlgorithm::None ||
                parts.header.kind == Kind::Hello) {
                _addCompressionStat(&CompressionStats::decompressionInputBytes, parts.payload.size());
                _addCompressionStat(&CompressionStats::decompressionErrors);
                NEKO_LOG_WARN("rpc", "rpc backend decompression rejected: kind={} id={} compression_enabled={}",
                              _kindName(parts.header.kind), parts.header.id, mCompressionEnabled);
                return ilias::Err(RpcError::InvalidRequest);
            }
            _addCompressionStat(&CompressionStats::decompressionInputBytes, parts.payload.size());
            auto payload = CompressionCodec::decompress(parts.payload, mCompressionAlgorithm);
            if (!payload) {
                _addCompressionStat(&CompressionStats::decompressionErrors);
                NEKO_LOG_WARN("rpc", "rpc backend decompression failed: kind={} id={} algorithm={} error={}",
                              _kindName(parts.header.kind), parts.header.id,
                              _compressionAlgorithmName(mCompressionAlgorithm), payload.error().message());
                return ilias::Err(payload.error());
            }
            _addCompressionStat(&CompressionStats::decompressedFrames);
            _addCompressionStat(&CompressionStats::decompressionOutputBytes, payload.value().size());
            NEKO_LOG_INFO("rpc", "rpc backend decompressed frame: kind={} id={} algorithm={} input={} output={}",
                          _kindName(parts.header.kind), parts.header.id,
                          _compressionAlgorithmName(mCompressionAlgorithm), parts.payload.size(),
                          payload.value().size());
            Message method     = detail::NekoRpcExtensionCodec::copyBytes(parts.method);
            Message extensions = detail::NekoRpcExtensionCodec::copyBytes(parts.extensions);
            auto flags         = static_cast<std::uint8_t>(parts.header.flags & ~Flag::Compressed);
            if (extensions.empty()) {
                flags = static_cast<std::uint8_t>(flags & ~Flag::HasExtensions);
            }
            return Codec::encodeFrame(parts.header.kind, parts.header.id,
                                      std::string_view{method.data(), method.size()},
                                      detail::NekoRpcExtensionCodec::asBytes(extensions),
                                      detail::NekoRpcExtensionCodec::asBytes(payload.value()), flags, CodecId);
        }

        auto _sendMethodIdError(const Message& frame, std::error_code error) -> ilias::IoTask<void> {
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts)) {
                co_return ilias::Err(RpcError::InvalidRequest);
            }
            if (mMethodIdEnabled) {
                NEKO_LOG_WARN("rpc",
                              "rpc backend method-id request failed: error={} current_version={} sending_full_table=1",
                              error.message(), mMethodTable.version());
                auto update = _methodTableHello(mMethodTable.entries(), true);
                if (auto ret = co_await _writeFrame(update); !ret) {
                    co_return ilias::Err(ret.error());
                }
            }
            if (parts.header.kind == Kind::Request) {
                NEKO_LOG_WARN("rpc", "rpc backend method-id error response: id={} error={}", parts.header.id,
                              error.message());
                auto response   = _encodeErrorFrame(parts.header.id, error);
                auto compressed = _compressOutgoingFrame(response);
                if (!compressed) {
                    co_return ilias::Err(compressed.error());
                }
                if (auto ret = co_await _writeFrame(compressed.value()); !ret) {
                    co_return ilias::Err(ret.error());
                }
            }
            co_return {};
        }

        auto _rememberPendingRetry(const Message& originalFrame, const Message& sentFrame) -> void {
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(sentFrame), CodecId, parts) ||
                parts.header.kind != Kind::Request) {
                return;
            }
            if ((parts.header.flags & Flag::MethodId) == 0U) {
                mPendingRetries.erase(parts.header.id);
                return;
            }
            mPendingRetries[parts.header.id] = PendingRetry{
                .frame = originalFrame,
                .used  = false,
            };
        }

        static bool _retryableMethodIdErrorCode(std::int32_t code) {
            return code == static_cast<std::int32_t>(RpcError::MethodIdNotNegotiated) ||
                   code == static_cast<std::int32_t>(RpcError::MethodTableOutdated) ||
                   code == static_cast<std::int32_t>(RpcError::MethodIdNotFound) ||
                   code == static_cast<std::int32_t>(RpcError::MethodIdRemoved) ||
                   code == static_cast<std::int32_t>(RpcError::MethodSignatureMismatch);
        }

        auto _retryMethodIdErrorIfNeeded(const Message& frame) -> ilias::IoTask<bool> {
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts) ||
                parts.header.kind != Kind::Response || (parts.header.flags & Flag::Error) == 0U) {
                co_return false;
            }
            NekoRpcError error;
            Message payload = detail::NekoRpcExtensionCodec::copyBytes(parts.payload);
            if (!_decodePayload(payload, error) || !_retryableMethodIdErrorCode(error.code)) {
                co_return false;
            }
            auto item = mPendingRetries.find(parts.header.id);
            if (item == mPendingRetries.end() || item->second.used) {
                co_return false;
            }
            item->second.used = true;
            NEKO_LOG_INFO("rpc", "rpc backend method-id retry with string method: id={} error_code={}", parts.header.id,
                          error.code);
            auto retryFrame = _compressOutgoingFrame(item->second.frame);
            if (!retryFrame) {
                co_return ilias::Err(retryFrame.error());
            }
            if (auto ret = co_await _writeFrame(retryFrame.value()); !ret) {
                co_return ilias::Err(ret.error());
            }
            co_return true;
        }

        auto _forgetPendingRetry(const Message& frame) -> void {
            FrameParts parts;
            if (Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts) &&
                parts.header.kind == Kind::Response) {
                mPendingRetries.erase(parts.header.id);
            }
        }

        auto _rewriteOutgoingMethodName(const Message& frame) -> Message {
            if (!mMethodIdEnabled) {
                return frame;
            }
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts) ||
                (parts.header.kind != Kind::Request && parts.header.kind != Kind::Notify) ||
                (parts.header.flags & Flag::MethodId) != 0U) {
                return frame;
            }
            const auto method = std::string(reinterpret_cast<const char*>(parts.method.data()), parts.method.size());
            auto entry        = mMethodTable.findByName(method);
            if (entry == nullptr) {
                return frame;
            }

            Message methodBytes;
            detail::NekoRpcExtensionCodec::appendU64(methodBytes, entry->id);
            auto extensionsRet = detail::NekoRpcExtensionCodec::withoutTlv(
                parts.extensions, detail::NekoRpcMethodIdExtension::TableVersionTlv);
            if (!extensionsRet) {
                return frame;
            }
            auto signatureRet =
                detail::NekoRpcExtensionCodec::withoutTlv(detail::NekoRpcExtensionCodec::asBytes(extensionsRet.value()),
                                                          detail::NekoRpcMethodIdExtension::SignatureHashTlv);
            if (!signatureRet) {
                return frame;
            }
            auto extensions   = std::move(signatureRet.value());
            auto versionTlv   = detail::NekoRpcMethodIdExtension::tableVersionTlv(mMethodTable.version());
            auto signatureTlv = detail::NekoRpcMethodIdExtension::signatureHashTlv(entry->signatureHash);
            extensions.insert(extensions.end(), versionTlv.begin(), versionTlv.end());
            extensions.insert(extensions.end(), signatureTlv.begin(), signatureTlv.end());
            Message payload = detail::NekoRpcExtensionCodec::copyBytes(parts.payload);
            auto flags      = static_cast<std::uint8_t>(parts.header.flags | Flag::MethodId);
            NEKO_LOG_INFO("rpc", "rpc backend method-id encode: method={} id={} table_version={} signature={}", method,
                          entry->id, mMethodTable.version(), entry->signatureHash);
            return Codec::encodeFrame(parts.header.kind, parts.header.id,
                                      std::string_view{methodBytes.data(), methodBytes.size()},
                                      detail::NekoRpcExtensionCodec::asBytes(extensions),
                                      detail::NekoRpcExtensionCodec::asBytes(payload), flags, CodecId);
        }

        auto _rewriteIncomingMethodId(const Message& frame) -> ilias::Result<Message, std::error_code> {
            FrameParts parts;
            if (!Codec::parseFrame(detail::NekoRpcExtensionCodec::asBytes(frame), CodecId, parts)) {
                return ilias::Err(RpcError::InvalidRequest);
            }
            const auto isRequest = parts.header.kind == Kind::Request || parts.header.kind == Kind::Notify;
            if (!isRequest) {
                return frame;
            }
            if ((parts.header.flags & Flag::MethodId) == 0U) {
                if (mOptions.methodId == MethodIdMode::Require && mMethodIdEnabled) {
                    NEKO_LOG_WARN("rpc", "rpc backend method-id request rejected: string method used while required");
                    return ilias::Err(RpcError::MethodIdRequiredButUnsupported);
                }
                return frame;
            }
            if (!mMethodIdEnabled || parts.method.size() != sizeof(std::uint64_t)) {
                NEKO_LOG_WARN("rpc", "rpc backend method-id request rejected: negotiated={} method_size={}",
                              mMethodIdEnabled, parts.method.size());
                return ilias::Err(RpcError::MethodIdNotNegotiated);
            }
            std::uint64_t version = 0;
            if (!detail::NekoRpcMethodIdExtension::readTableVersion(parts.extensions, version)) {
                NEKO_LOG_WARN("rpc", "rpc backend method-id request rejected: missing table version");
                return ilias::Err(RpcError::MethodTableOutdated);
            }
            std::uint64_t signatureHash = 0;
            const bool hasSignature =
                detail::NekoRpcMethodIdExtension::readSignatureHash(parts.extensions, signatureHash);
            const auto methodId = detail::NekoRpcExtensionCodec::readU64(parts.method, 0);
            auto resolved       = mMethodTable.resolve(methodId, version, signatureHash, true);
            if (!hasSignature && resolved.status == detail::NekoRpcMethodIdResolveStatus::SignatureMismatch) {
                NEKO_LOG_WARN("rpc",
                              "rpc backend method-id request rejected: id={} client_version={} server_version={} "
                              "status=signature_mismatch",
                              methodId, version, mMethodTable.version());
                return ilias::Err(RpcError::MethodSignatureMismatch);
            }
            switch (resolved.status) {
            case detail::NekoRpcMethodIdResolveStatus::Ok:
                break;
            case detail::NekoRpcMethodIdResolveStatus::TableOutdated:
                NEKO_LOG_WARN("rpc",
                              "rpc backend method-id request rejected: id={} client_version={} server_version={} "
                              "status={}",
                              methodId, version, mMethodTable.version(), _methodIdResolveStatusName(resolved.status));
                return ilias::Err(RpcError::MethodTableOutdated);
            case detail::NekoRpcMethodIdResolveStatus::MethodNotFound:
                NEKO_LOG_WARN("rpc",
                              "rpc backend method-id request rejected: id={} client_version={} server_version={} "
                              "status={}",
                              methodId, version, mMethodTable.version(), _methodIdResolveStatusName(resolved.status));
                return ilias::Err(RpcError::MethodIdNotFound);
            case detail::NekoRpcMethodIdResolveStatus::MethodRemoved:
                NEKO_LOG_WARN("rpc",
                              "rpc backend method-id request rejected: id={} client_version={} server_version={} "
                              "status={}",
                              methodId, version, mMethodTable.version(), _methodIdResolveStatusName(resolved.status));
                return ilias::Err(RpcError::MethodIdRemoved);
            case detail::NekoRpcMethodIdResolveStatus::SignatureMismatch:
                NEKO_LOG_WARN("rpc",
                              "rpc backend method-id request rejected: id={} client_version={} server_version={} "
                              "status={}",
                              methodId, version, mMethodTable.version(), _methodIdResolveStatusName(resolved.status));
                return ilias::Err(RpcError::MethodSignatureMismatch);
            }

            auto extensionsRet = detail::NekoRpcExtensionCodec::withoutTlv(
                parts.extensions, detail::NekoRpcMethodIdExtension::TableVersionTlv);
            if (!extensionsRet) {
                return ilias::Err(extensionsRet.error());
            }
            auto signatureRet =
                detail::NekoRpcExtensionCodec::withoutTlv(detail::NekoRpcExtensionCodec::asBytes(extensionsRet.value()),
                                                          detail::NekoRpcMethodIdExtension::SignatureHashTlv);
            if (!signatureRet) {
                return ilias::Err(signatureRet.error());
            }
            auto extensions = std::move(signatureRet.value());
            Message payload = detail::NekoRpcExtensionCodec::copyBytes(parts.payload);
            auto flags      = static_cast<std::uint8_t>(parts.header.flags & ~Flag::MethodId);
            if (extensions.empty()) {
                flags = static_cast<std::uint8_t>(flags & ~Flag::HasExtensions);
            }
            NEKO_LOG_INFO("rpc",
                          "rpc backend method-id decode: id={} method={} client_version={} server_version={} "
                          "signature={}",
                          methodId, resolved.entry->name, version, mMethodTable.version(), signatureHash);
            return Codec::encodeFrame(parts.header.kind, parts.header.id, resolved.entry->name,
                                      detail::NekoRpcExtensionCodec::asBytes(extensions),
                                      detail::NekoRpcExtensionCodec::asBytes(payload), flags, CodecId);
        }

        StreamT mStream;
        EndpointRole mRole = EndpointRole::Plain;
        Options mOptions;
        struct PendingRetry {
            Message frame;
            bool used = false;
        };
        bool mHandshakeDone                        = false;
        bool mMethodIdEnabled                      = false;
        bool mCompressionEnabled                   = false;
        CompressionAlgorithm mCompressionAlgorithm = CompressionAlgorithm::None;
        std::uint32_t mCompressionMinPayloadSize   = mOptions.compressionMinPayloadSize;
        detail::NekoRpcMethodIdTable mMethodTable;
        std::map<Id, PendingRetry> mPendingRetries;
        std::unique_ptr<ilias::Mutex> mWriteMutex = std::make_unique<ilias::Mutex>();
    };
};

using BinaryRpcBackend = NekoRpcBackend<BinarySerializer, 0>;

NEKO_END_NAMESPACE
