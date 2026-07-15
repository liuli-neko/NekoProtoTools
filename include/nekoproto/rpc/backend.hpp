#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <optional>
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
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/rpc/private/backend_base.hpp"
#include "nekoproto/rpc/private/backend_compression.hpp"
#include "nekoproto/rpc/private/backend_method_id.hpp"
#include "nekoproto/rpc/private/backend_support.hpp"
#include "nekoproto/rpc/private/stream_endpoint.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, std::uint8_t CodecId = 0, typename CompressionCodecT = rpc::NekoRpcCompressionCodec>
struct NekoRpcBackend {
    using Codec              = rpc::NekoRpcFrameCodec;
    using CompressionCodec   = CompressionCodecT;
    using CompressionStats   = rpc::NekoRpcCompressionStats;
    using Id                 = typename Codec::IdType;
    using Message            = typename Codec::MessageType;
    using Kind               = rpc::NekoRpcKind;
    using Flag               = rpc::NekoRpcFlag;
    using Header             = typename Codec::Header;
    using DecodedRequest     = typename Codec::DecodedRequest;
    using WireResponseValues = typename Codec::ResponseValuesType;
    using Extension          = typename Codec::ExtensionType;
    using ExtensionMap       = typename Codec::ExtensionMapType;
    using ExtensionStore     = std::map<Extension, Message>;

    using MethodIdMode         = rpc::NekoRpcFeatureMode;
    using CompressionMode      = rpc::NekoRpcFeatureMode;
    using CompressionAlgorithm = rpc::NekoRpcCompressionAlgorithm;

    struct Options {
        MethodIdMode method_id                     = MethodIdMode::Auto;
        CompressionMode compression                = CompressionMode::Disable;
        std::uint32_t compression_min_payload_size = 64;
        bool retry_method_id_error_once            = true;
        std::size_t max_auto_method_table_extension_bytes =
            Codec::MaxExtensionBytes;
        rpc::NekoRpcFrameLimits frame_limits;
        std::size_t max_decompressed_payload_bytes = 16U * 1024U * 1024U;
        std::size_t max_compression_ratio = 130U;
        std::size_t max_method_entries = 64U * 1024U;
        std::size_t max_pending_calls = 1024U;
        std::size_t max_inflight_requests_per_connection = 1024U;
        std::size_t max_active_requests_global = 4096U;
        std::size_t max_queued_requests_global = 4096U;
        std::optional<std::chrono::nanoseconds> request_timeout;
        std::shared_ptr<CompressionStats> compression_stats;
    };

    struct EncodedRequest {
        Message message;
        Id id = 0;
    };

    struct ResponseValue {
        Header header;
        Message method;
        ExtensionStore extensions;
        Message payload;
    };

    using ResponseValues = std::vector<ResponseValue>;

    struct DecodeResult {
        bool ok    = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
        ResponseValues responses;
    };

    struct ClientContext {
        Options options;
        Id next_id = 0;
    };

    struct ServerContext {
        Options options;
        rpc::NekoRpcMethodIdTable method_table;
    };

    struct PeerSession {
        explicit PeerSession(std::size_t max_method_entries = rpc::NekoRpcMethodIdTable::DefaultMaxEntries)
            : remote_method_table(max_method_entries) {}

        bool handshake_done                        = false;
        bool method_id_enabled                     = false;
        bool compression_enabled                   = false;
        CompressionAlgorithm compression_algorithm = CompressionAlgorithm::None;
        std::uint32_t compression_min_payload_size = 64;
        rpc::NekoRpcMethodIdTable remote_method_table;
    };

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

    static auto makeClientContext(Options options = {}) -> ClientContext {
        return ClientContext{.options = std::move(options), .next_id = 0};
    }

    template <typename Methods>
    static auto makeServerContext(Options options, Methods&& methods) -> ServerContext {
        const auto max_method_entries = options.max_method_entries;
        ServerContext context{.options = std::move(options),
                              .method_table = rpc::NekoRpcMethodIdTable(max_method_entries)};
        context.method_table.reset(rpc::NekoRpcMethodEntries(std::forward<Methods>(methods)),
                                   rpc::NekoRpcMethodIdExtension::InitialTableVersion);
        return context;
    }

    template <typename Methods>
    static auto makeServerContext(Methods&& methods) -> ServerContext {
        return makeServerContext(Options{}, std::forward<Methods>(methods));
    }

    static auto makeClientPeerSession(const ClientContext& context) -> PeerSession {
        PeerSession session(context.options.max_method_entries);
        session.compression_min_payload_size = context.options.compression_min_payload_size;
        return session;
    }

    static auto makeServerPeerSession(const ServerContext& context) -> PeerSession {
        PeerSession session(context.options.max_method_entries);
        session.compression_min_payload_size = context.options.compression_min_payload_size;
        return session;
    }

    static auto validateMessage(const ClientContext& context, std::span<const std::byte> message)
        -> ilias::Result<void, std::error_code> {
        return _validateFrameSize(message, context.options.frame_limits);
    }

    static auto validateMessage(const ServerContext& context, std::span<const std::byte> message)
        -> ilias::Result<void, std::error_code> {
        return _validateFrameSize(message, context.options.frame_limits);
    }

    static auto refreshMethodCatalog(ServerContext& context, std::vector<detail::RpcMethodMetadata> methods) -> void {
        std::vector<rpc::NekoRpcMethodEntry> delta;
        rpc::NekoRpcSyncMethodTable(context.method_table, rpc::NekoRpcMethodEntriesFromMetadata(methods), delta);
        if (!delta.empty()) {
            NEKO_LOG_INFO("rpc", "rpc backend method catalog refresh: version={} min_version={} delta_count={}",
                          context.method_table.version(), context.method_table.minimumCompatibleVersion(),
                          delta.size());
        }
    }

    static auto decodeIncoming(ServerContext& context, PeerSession& session, std::span<const std::byte> message)
        -> DecodeResult {
        DecodeResult result;
        if (!_validFrameSize(message, context.options.frame_limits)) {
            NEKO_LOG_WARN("rpc", "rpc backend incoming frame rejected: reason=size_limit bytes={}", message.size());
            return result;
        }
        typename Codec::FrameParts parts;
        if (!Codec::parseFrame(message, CodecId, parts)) {
            NEKO_LOG_WARN("rpc", "rpc backend incoming frame rejected: reason=parse_failed bytes={}", message.size());
            return result;
        }
        NEKO_LOG_TRACE("rpc", "rpc backend incoming frame: kind={} flags={} id={} payload={}",
                       Reflect<rpc::NekoRpcKind>::name(parts.header.kind),
                       Reflect<rpc::NekoRpcFlag>::flagsToString(parts.header.flags), parts.header.id,
                       parts.payload.size());

        // Extension transforms are backend state transitions. Keep the original
        // frame parsed once and switch only the payload view when decompression
        // succeeds, so endpoint code never has to rewrite protocol frames.
        auto decompressed = rpc::neko_rpc_decompress_incoming_payload<NekoRpcBackend>(context.options, session, parts);
        if (!decompressed) {
            result.ok = true;
            NEKO_LOG_WARN("rpc", "rpc backend incoming extension failed: id={} error={}", parts.header.id,
                          decompressed.error().message());
            _appendFrameError(result.responses, parts, decompressed.error());
            return result;
        }
        std::optional<Message> payload_storage;
        std::span<const std::byte> payload_view = parts.payload;
        if (decompressed.value()) {
            payload_storage = std::move(*decompressed.value());
            payload_view    = rpc::NekoRpcExtensionCodec::asBytes(*payload_storage);
        }

        if (parts.header.kind == Kind::Cancel || parts.header.kind == Kind::Hello) {
            result.ok = true;
            NEKO_LOG_TRACE("rpc", "rpc backend incoming control frame: kind={} id={}",
                           Reflect<rpc::NekoRpcKind>::name(parts.header.kind), parts.header.id);
            return result;
        }
        if (parts.header.kind != Kind::Request && parts.header.kind != Kind::Notify) {
            NEKO_LOG_WARN("rpc", "rpc backend incoming frame rejected: reason=unexpected_kind kind={}",
                          Reflect<rpc::NekoRpcKind>::name(parts.header.kind));
            return result;
        }
        if ((parts.header.flags & Flag::Error) != 0U) {
            NEKO_LOG_WARN("rpc", "rpc backend incoming frame rejected: reason=request_marked_error id={}",
                          parts.header.id);
            return result;
        }

        // Method-id resolution belongs here because it depends on backend-owned
        // session state and extension TLVs, not on endpoint transport behavior.
        auto method_name = rpc::NekoRpcIncomingMethodName<NekoRpcBackend>(context, session, parts);
        if (!method_name) {
            result.ok = true;
            _appendFrameError(result.responses, parts, method_name.error(),
                              _methodIdErrorResponseExtensions(context, session, method_name.error()));
            return result;
        }
        if (method_name.value().empty()) {
            return result;
        }

        auto header  = parts.header;
        header.flags = static_cast<std::uint8_t>(header.flags & ~Flag::Compressed);
        header.flags = static_cast<std::uint8_t>(header.flags & ~Flag::MethodId);

        DecodedRequest request;
        request.header = header;
        request.method = std::move(method_name.value());
        if (payload_storage) {
            request.payload = std::move(*payload_storage);
        } else {
            request.payload.assign(payload_view.begin(), payload_view.end());
        }

        result.ok = true;
        result.requests.emplace_back(std::move(request));
        NEKO_LOG_TRACE("rpc", "rpc backend incoming request decoded: method={} id={} payload={}",
                       result.requests.back().method, result.requests.back().header.id,
                       result.requests.back().payload.size());
        return result;
    }

    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view {
        return Codec::methodName(request);
    }
    static auto id(const DecodedRequest& request) noexcept -> const Id& { return Codec::id(request); }
    static bool expectsResponse(const DecodedRequest& request) noexcept { return Codec::expectsResponse(request); }

    template <typename Method>
    static auto decodeParams(const DecodedRequest& request, const Method& /*method*/)
        -> ilias::Result<rpc::NekoRpcDecayTupleType<typename Method::RawParamsType>, std::error_code> {
        using Params = rpc::NekoRpcDecayTupleType<typename Method::RawParamsType>;
        static_assert(BackendSerializable<NekoRpcBackend, Params>,
                      "NekoRpcBackend: method parameters are not serializable by this Serializer");
        Params params;
        if (!_decodePayload(request.payload, params)) {
            return ilias::Err(RpcError::InvalidParams);
        }
        return params;
    }

    template <typename Method>
    static auto invoke(Method& method, rpc::NekoRpcDecayTupleType<typename Method::RawParamsType> params)
        -> ilias::IoTask<typename Method::RawReturnType> {
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            co_await std::apply(method, std::move(params));
            co_return {};
        } else {
            co_return co_await std::apply(method, std::move(params));
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

        if (!result) {
            responses.emplace_back(_makeErrorResponse(request.header.id, result.error()));
            return;
        }

        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            responses.emplace_back(_makeResponse(request.header.id, {}, 0));
        } else {
            if (auto payload = _encodePayload(result.value())) {
                responses.emplace_back(_makeResponse(request.header.id, std::move(payload.value()), 0));
            } else {
                responses.emplace_back(_makeErrorResponse(request.header.id, RpcError::InternalError));
            }
        }
    }

    static void appendError(ResponseValues& responses, const DecodedRequest& request, std::error_code error) {
        if (expectsResponse(request)) {
            responses.emplace_back(_makeErrorResponse(request.header.id, error));
        }
    }

    static Message encodeResponses(const ResponseValues& responses, bool batch) {
        WireResponseValues frames;
        frames.reserve(responses.size());
        for (const auto& response : responses) {
            auto frame = _encodeResponseFrame(response);
            if (_validFrameSize(frame, rpc::NekoRpcFrameLimits{})) {
                frames.emplace_back(std::move(frame));
            }
        }
        return Codec::encodeResponses(frames, batch);
    }

    static Message encodeResponses(ServerContext& context, PeerSession& session, const ResponseValues& responses,
                                   bool batch) {
        WireResponseValues frames;
        frames.reserve(responses.size());
        for (const auto& response : responses) {
            if (auto encoded = _encodeResponseFrame(context.options, session, response)) {
                frames.emplace_back(std::move(encoded.value()));
            } else {
                auto fallback = _encodeResponseFrame(response);
                if (_validFrameSize(fallback, context.options.frame_limits)) {
                    frames.emplace_back(std::move(fallback));
                }
            }
        }
        return Codec::encodeResponses(frames, batch);
    }

    template <typename Method, typename... Args>
    static auto encodeRequest(Method& method, bool notification, std::uint64_t& next_id, Args&&... args)
        -> ilias::Result<EncodedRequest, std::error_code> {
        ClientContext context;
        context.next_id = next_id;
        PeerSession session;
        auto encoded = encodeRequest(context, session, method, notification, std::forward<Args>(args)...);
        next_id      = context.next_id;
        return encoded;
    }

    template <typename Method, typename... Args>
    static auto encodeRequest(ClientContext& context, PeerSession& session, Method& method, bool notification,
                              Args&&... args) -> ilias::Result<EncodedRequest, std::error_code> {
        using Params = std::tuple<std::decay_t<Args>...>;
        static_assert(BackendSerializable<NekoRpcBackend, Params>,
                      "NekoRpcBackend: request parameters are not serializable by this Serializer");

        ILIAS_TRY(auto payload, _encodePayload(Params{std::forward<Args>(args)...}));

        Id id = 0;
        if (!notification) {
            id = ((context.next_id == std::numeric_limits<Id>::max()) ? (context.next_id = 1) : (++context.next_id));
        }

        Header header{.kind = notification ? Kind::Notify : Kind::Request, .flags = 0, .codec = CodecId, .id = id};

        Message method_bytes;
        ExtensionStore extensions;
        // Prefer method-id only when the negotiated table contains this method.
        // Otherwise the backend falls back to the method name without involving
        // the endpoint or dispatcher in policy.
        if (session.method_id_enabled) {
            if (const auto* entry = session.remote_method_table.findByName(method.name()); entry != nullptr) {
                rpc::NekoRpcExtensionCodec::appendInteger(method_bytes, entry->id);
                extensions[Extension::MethodTableVersion] =
                    rpc::NekoRpcExtensionCodec::integerValue(session.remote_method_table.version());
                extensions[Extension::MethodSignatureHash] =
                    rpc::NekoRpcExtensionCodec::integerValue(entry->signature_hash);
                header.flags = static_cast<std::uint8_t>(header.flags | Flag::MethodId);
            }
        }
        if (method_bytes.empty()) {
            const auto name = method.name();
            method_bytes.assign(reinterpret_cast<const std::byte*>(name.data()),
                                reinterpret_cast<const std::byte*>(name.data() + name.size()));
        }

        FrameParts request_frame{
            .header     = header,
            .method     = method_bytes,
            .extensions = _extensionViews(extensions),
            .payload    = std::move(payload),
        };
        NEKO_LOG_TRACE("rpc", "rpc backend encode request: method={} id={} notification={} method_id={} payload={}",
                       method.name(), id, notification, (header.flags & Flag::MethodId) != 0U,
                       request_frame.payload.size());
        ILIAS_TRY(auto frame,
                  rpc::neko_rpc_encode_outgoing_frame<NekoRpcBackend>(context.options, session, request_frame));

        return EncodedRequest{.message = std::move(frame), .id = id};
    }

    template <typename Method>
    static auto decodeResponse(std::span<const std::byte> buffer, const Id& expected_id)
        -> ilias::Result<typename Method::RawReturnType, std::error_code> {
        ClientContext context;
        PeerSession session;
        return decodeResponse<Method>(context, session, buffer, expected_id);
    }

    template <typename Method>
    static auto decodeResponse(ClientContext& context, PeerSession& session, std::span<const std::byte> buffer,
                               const Id& expected_id)
        -> ilias::Result<typename Method::RawReturnType, std::error_code> {
        static_assert(BackendSerializable<NekoRpcBackend, typename Method::RawReturnType>,
                      "NekoRpcBackend: response type is not serializable by this Serializer");

        ILIAS_TRYV(_validateFrameSize(buffer, context.options.frame_limits));
        FrameParts parts;
        if (!Codec::parseFrame(buffer, CodecId, parts)) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        NEKO_LOG_TRACE("rpc", "rpc backend decode response frame: id={} expected={} flags={} payload={}",
                       parts.header.id, expected_id, Reflect<rpc::NekoRpcFlag>::flagsToString(parts.header.flags),
                       parts.payload.size());

        std::span<const std::byte> payload_view = parts.payload;
        ILIAS_TRY(auto decompressed,
                  rpc::neko_rpc_decompress_incoming_payload<NekoRpcBackend>(context.options, session, parts));
        std::optional<Message> payload_storage;
        if (decompressed) {
            payload_storage = std::move(*decompressed);
            payload_view    = rpc::NekoRpcExtensionCodec::asBytes(*payload_storage);
        }

        if (parts.header.kind != Kind::Response || parts.header.codec != CodecId) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        if (parts.header.id != expected_id) {
            return ilias::Err(RpcError::ResponseIdNotMatch);
        }

        if ((parts.header.flags & Flag::Error) != 0U) {
            rpc::NekoRpcError error;
            if (!_decodePayload(payload_view, error)) {
                return ilias::Err(RpcError::InternalError);
            }
            auto ec = std::error_code(error.code, RpcErrorCategory::instance());
            if (_isMethodIdError(ec)) {
                const bool updated = _applyMethodIdTableExtensions(session, parts.extensions);
                NEKO_LOG_WARN("rpc", "rpc backend method-id response error: id={} error=\"{}\" table_updated={}",
                              parts.header.id, ec.message(), updated);
                if (!updated) {
                    session.method_id_enabled = false;
                    session.remote_method_table.reset();
                }
            }
            return ilias::Err(ec);
        }

        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            return {};
        } else {
            typename Method::RawReturnType value;
            if (!_decodePayload(payload_view, value)) {
                return ilias::Err(RpcError::InvalidParams);
            }
            NEKO_LOG_TRACE("rpc", "rpc backend response decoded: id={} payload={}", parts.header.id,
                           payload_view.size());
            return value;
        }
    }

    template <typename Endpoint>
    static auto recoverClientCall(ClientContext& context, PeerSession& session, Endpoint& endpoint,
                                  std::error_code error, std::size_t retry_count) -> ilias::IoTask<bool> {
        (void)endpoint;
        if (!context.options.retry_method_id_error_once || retry_count != 0U ||
            !feature_enabled(context.options.method_id) || !_isMethodIdError(error)) {
            co_return false;
        }

        if (!session.method_id_enabled || session.remote_method_table.empty()) {
            // A receiver task owns the stream after the initial handshake, so
            // recovery cannot perform a competing recv. Auto mode safely falls
            // back to a named request for the one permitted retry.
            if (context.options.method_id == MethodIdMode::Require) {
                co_return false;
            }
            session.method_id_enabled = false;
            session.handshake_done    = true;
            NEKO_LOG_INFO("rpc", "rpc backend method-id recovery: error=\"{}\" named_fallback retry=1",
                          error.message());
        } else {
            NEKO_LOG_INFO("rpc", "rpc backend method-id recovery: error=\"{}\" response_table retry=1",
                          error.message());
        }
        co_return true;
    }

    static std::error_code clientNotInitError() { return RpcError::ClientNotInit; }
    static std::error_code notificationOk() { return RpcError::Ok; }

    template <typename Endpoint>
    static auto ensureClientReady(ClientContext& context, PeerSession& session, Endpoint& endpoint)
        -> ilias::IoTask<void> {
        if (session.handshake_done ||
            (!feature_enabled(context.options.method_id) && !feature_enabled(context.options.compression))) {
            session.handshake_done = true;
            co_return {};
        }

        ExtensionStore hello_extensions;
        if (feature_enabled(context.options.method_id)) {
            hello_extensions[Extension::MethodId] = {};
        }

        const auto compression_algorithm = CompressionCodec::preferred_algorithm();
        if (feature_enabled(context.options.compression) && compression_algorithm != CompressionAlgorithm::None &&
            CompressionCodec::supports(compression_algorithm)) {
            hello_extensions[Extension::Compression] = {};
            hello_extensions[Extension::CompressionAlgorithm] =
                rpc::NekoRpcExtensionCodec::enumValue(compression_algorithm);
            hello_extensions[Extension::CompressionMinPayloadSize] =
                rpc::NekoRpcExtensionCodec::integerValue(context.options.compression_min_payload_size);
        }

        if (hello_extensions.empty()) {
            session.handshake_done = true;
            co_return {};
        }

        NEKO_LOG_INFO("rpc", "rpc backend client hello begin: method_id_mode={} compression_mode={} min_payload={}",
                      static_cast<unsigned>(context.options.method_id),
                      static_cast<unsigned>(context.options.compression), context.options.compression_min_payload_size);
        auto hello = _encodeHello(hello_extensions);
        ILIAS_CO_TRY(auto sent, co_await endpoint.send(rpc::NekoRpcExtensionCodec::asBytes(hello)));
        if (sent != hello.size()) {
            co_return ilias::Err(ilias::IoError::WriteZero);
        }

        std::vector<std::byte> response;
        ILIAS_CO_TRY(auto received, co_await endpoint.recv(response));
        if (received == 0U || !handleClientControl(context, session, response)) {
            co_return ilias::Err(RpcError::InvalidRequest);
        }

        if (context.options.method_id == MethodIdMode::Require && !session.method_id_enabled) {
            co_return ilias::Err(RpcError::MethodIdRequiredButUnsupported);
        }
        if (context.options.compression == CompressionMode::Require && !session.compression_enabled) {
            co_return ilias::Err(RpcError::CompressionRequiredButUnsupported);
        }

        session.handshake_done = true;
        co_return {};
    }

    static bool handleClientControl(ClientContext& context, PeerSession& session, std::span<const std::byte> message) {
        (void)context;
        if (!_isHello(message)) {
            return false;
        }
        return _handleServerHello(session, message);
    }

    static auto encodeCancel(Id id) -> Message {
        Header header{.kind = Kind::Cancel, .codec = CodecId, .id = id};
        return Codec::encodeFrame({.header = header, .method = {}, .extensions = {}, .payload = {}});
    }

    static auto cancelId(std::span<const std::byte> message) -> std::optional<Id> {
        FrameParts parts;
        if (!Codec::parseFrame(message, CodecId, parts) || parts.header.kind != Kind::Cancel ||
            !parts.method.empty() || !parts.extensions.empty() || !parts.payload.empty()) {
            return std::nullopt;
        }
        return parts.header.id;
    }

    static auto responseId(std::span<const std::byte> message) -> std::optional<Id> {
        FrameParts parts;
        if (!Codec::parseFrame(message, CodecId, parts) || parts.header.kind != Kind::Response) {
            return std::nullopt;
        }
        return parts.header.id;
    }

    template <typename Endpoint>
    static auto handleServerControl(ServerContext& context, PeerSession& session, Endpoint& endpoint,
                                    std::span<const std::byte> message) -> ilias::IoTask<bool> {
        if (!_isHello(message)) {
            co_return false;
        }
        ILIAS_CO_TRY(auto response, _handleClientHello(context, session, message));
        ILIAS_CO_TRY(auto sent, co_await endpoint.send(rpc::NekoRpcExtensionCodec::asBytes(response)));
        if (sent != response.size()) {
            co_return ilias::Err(ilias::IoError::WriteZero);
        }
        co_return true;
    }

    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream) {
        return rpc::NekoRpcStreamEndpoint<StreamT, Codec, CodecId>{std::move(stream)};
    }

    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream, const Options& options) {
        return rpc::NekoRpcStreamEndpoint<StreamT, Codec, CodecId>{std::move(stream), options.frame_limits};
    }

    template <MessageEndpoint EndpointT>
    static auto makeEndpoint(EndpointT endpoint) {
        return endpoint;
    }

    template <ilias::Stream StreamT>
    static auto makeClientEndpoint(StreamT stream) {
        return makeEndpoint(std::move(stream));
    }

    template <ilias::Stream StreamT>
    static auto makeClientEndpoint(StreamT stream, const Options& options) {
        return makeEndpoint(std::move(stream), options);
    }

    template <ilias::Stream StreamT, typename Methods>
    static auto makeServerEndpoint(StreamT stream, Methods&& /*methods*/) {
        return makeEndpoint(std::move(stream));
    }

    template <ilias::Stream StreamT, typename Methods>
    static auto makeServerEndpoint(StreamT stream, Methods&& /*methods*/, const Options& options) {
        return makeEndpoint(std::move(stream), options);
    }

private:
    using FrameParts = typename Codec::FrameParts;

    static auto _validateFrameSize(std::span<const std::byte> frame, const rpc::NekoRpcFrameLimits& limits)
        -> ilias::Result<void, std::error_code> {
        const auto header_size = Codec::headerSize();
        if (frame.size() < header_size) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        ILIAS_TRY(auto body_size, Codec::headerBodySize(frame.first(header_size), CodecId, limits));
        if (body_size != frame.size() - header_size) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        return {};
    }

    static auto _validFrameSize(std::span<const std::byte> frame, const rpc::NekoRpcFrameLimits& limits) -> bool {
        return static_cast<bool>(_validateFrameSize(frame, limits));
    }

    static auto _extensionViews(const ExtensionStore& extensions) -> ExtensionMap {
        ExtensionMap views;
        for (const auto& [type, value] : extensions) {
            views.emplace(type, rpc::NekoRpcExtensionCodec::asBytes(value));
        }
        return views;
    }

    static auto _ownExtensions(const ExtensionMap& extensions) -> ExtensionStore {
        ExtensionStore owned;
        for (const auto& [type, value] : extensions) {
            owned[type] = rpc::NekoRpcExtensionCodec::copyBytes(value);
        }
        return owned;
    }

    static auto _encodeHello(const ExtensionStore& extensions) -> Message {
        return Codec::encodeHello(_extensionViews(extensions), CodecId);
    }

    static constexpr bool feature_enabled(MethodIdMode mode) noexcept { return mode != MethodIdMode::Disable; }

    static auto _extensionWireSize(const ExtensionStore& extensions) -> std::optional<std::size_t> {
        std::size_t size = 0;
        for (const auto& [type, value] : extensions) {
            (void)type;
            if (value.size() > std::numeric_limits<std::uint16_t>::max() ||
                size > Codec::MaxExtensionBytes - 4U ||
                size + 4U > Codec::MaxExtensionBytes - value.size()) {
                return std::nullopt;
            }
            size += 4U + value.size();
        }
        return size;
    }

    static bool _extensionsFitAutoBudget(const Options& options, const ExtensionStore& extensions) {
        const auto size = _extensionWireSize(extensions);
        return size && *size <= std::min(options.max_auto_method_table_extension_bytes, Codec::MaxExtensionBytes);
    }

    static auto _methodIdTableExtensions(const ServerContext& context, bool include_table) -> ExtensionStore {
        ExtensionStore extensions;
        extensions[Extension::MethodId] = {};
        extensions[Extension::MethodTableVersion] =
            rpc::NekoRpcExtensionCodec::integerValue(context.method_table.version());
        extensions[Extension::MethodMinimumCompatibleVersion] =
            rpc::NekoRpcExtensionCodec::integerValue(context.method_table.minimumCompatibleVersion());
        if (include_table) {
            extensions[Extension::MethodTable] =
                rpc::NekoRpcMethodIdExtension::methodTableValue(context.method_table.entries());
        }
        return extensions;
    }

    static auto _methodIdErrorResponseExtensions(const ServerContext& context, const PeerSession& session,
                                                 std::error_code error) -> ExtensionStore {
        if (!session.method_id_enabled || !_isMethodIdError(error) || context.method_table.empty() ||
            context.options.max_auto_method_table_extension_bytes == 0U) {
            return {};
        }

        // Method-id resolution failed before dispatcher invocation, so this
        // error response is allowed to carry recovery metadata. Keep it bounded:
        // small tables ride the failed response, large tables force the client
        // into an explicit refresh path instead of surprise multi-frame pushes.
        auto extensions = _methodIdTableExtensions(context, true);
        if (_extensionsFitAutoBudget(context.options, extensions)) {
            NEKO_LOG_INFO("rpc", "rpc backend method-id error response carries table: version={} entries={}",
                          context.method_table.version(), context.method_table.entries().size());
            return extensions;
        }

        auto hint = _methodIdTableExtensions(context, false);
        if (_extensionsFitAutoBudget(context.options, hint)) {
            NEKO_LOG_INFO("rpc",
                          "rpc backend method-id error response omits oversized table: version={} entries={}",
                          context.method_table.version(), context.method_table.entries().size());
            return hint;
        }

        NEKO_LOG_INFO("rpc", "rpc backend method-id error response omits table hint: version={} entries={}",
                      context.method_table.version(), context.method_table.entries().size());
        return {};
    }

    static auto _appendMethodIdHelloExtensions(const ServerContext& context, ExtensionStore& extensions) -> void {
        // Client Hello is also an explicit request for negotiated extension
        // state. Send the table immediately when it fits the same automatic
        // budget; otherwise only acknowledge the feature/version and let a
        // future explicit refresh transfer the larger data.
        auto table = _methodIdTableExtensions(context, true);
        if (_extensionsFitAutoBudget(context.options, table)) {
            extensions.insert(table.begin(), table.end());
            return;
        }

        auto hint = _methodIdTableExtensions(context, false);
        if (_extensionsFitAutoBudget(context.options, hint)) {
            NEKO_LOG_INFO("rpc", "rpc backend server hello omits oversized method table: version={} entries={}",
                          context.method_table.version(), context.method_table.entries().size());
            extensions.insert(hint.begin(), hint.end());
        }
    }

    static bool _applyMethodIdTableExtensions(PeerSession& session, const ExtensionMap& extensions) {
        if (!extensions.contains(Extension::MethodTable) && !extensions.contains(Extension::MethodTableDelta)) {
            return false;
        }

        // The receiver treats method table TLVs as a peer-session update. The
        // response error still propagates to policy code; this only refreshes
        // the state a retry would use.
        std::uint64_t version = 0;
        if (!rpc::NekoRpcExtensionCodec::readIntegerTlv(extensions, Extension::MethodTableVersion, version)) {
            return false;
        }

        std::vector<rpc::NekoRpcMethodEntry> entries;
        bool applied = false;
        if (rpc::NekoRpcMethodIdExtension::parseMethodTable(extensions, entries)) {
            applied = session.remote_method_table.applyRemoteTable(std::move(entries), version);
        } else if (rpc::NekoRpcMethodIdExtension::parseMethodTableDelta(extensions, entries)) {
            applied = session.remote_method_table.applyRemoteDelta(entries, version);
        }
        if (!applied) {
            return false;
        }

        std::uint64_t minimum_version = 0;
        if (rpc::NekoRpcExtensionCodec::readIntegerTlv(extensions, Extension::MethodMinimumCompatibleVersion,
                                                       minimum_version)) {
            session.remote_method_table.setMinimumCompatibleVersion(minimum_version);
        }
        session.method_id_enabled = true;
        return true;
    }

    static auto _makeResponse(Id id, Message payload, std::uint8_t flags, ExtensionStore extensions = {})
        -> ResponseValue {
        Header header{.kind = Kind::Response, .flags = flags, .codec = CodecId, .id = id};
        return ResponseValue{.header = header,
                             .method = {},
                             .extensions = std::move(extensions),
                             .payload = std::move(payload)};
    }

    static auto _makeErrorResponse(Id id, std::error_code error, ExtensionStore extensions = {}) -> ResponseValue {
        const rpc::NekoRpcError rpc_error{
            .code    = static_cast<std::int32_t>(error.value()),
            .message = error.message(),
            .data    = {},
        };
        auto payload = _encodePayload(rpc_error);
        if (!payload) {
            return _makeResponse(id, {}, Flag::Error, std::move(extensions));
        }
        return _makeResponse(id, std::move(payload.value()), Flag::Error, std::move(extensions));
    }

    static auto _encodeResponseFrame(const ResponseValue& response) -> Message {
        return Codec::encodeFrame({
            .header     = response.header,
            .method     = rpc::NekoRpcExtensionCodec::asBytes(response.method),
            .extensions = _extensionViews(response.extensions),
            .payload    = rpc::NekoRpcExtensionCodec::asBytes(response.payload),
        });
    }

    static auto _encodeResponseFrame(const Options& options, PeerSession& session, const ResponseValue& response)
        -> ilias::Result<Message, std::error_code> {
        return rpc::neko_rpc_encode_outgoing_frame<NekoRpcBackend>(
            options, session,
            {
                .header     = response.header,
                .method     = rpc::NekoRpcExtensionCodec::asBytes(response.method),
                .extensions = _extensionViews(response.extensions),
                .payload    = rpc::NekoRpcExtensionCodec::asBytes(response.payload),
            });
    }

    static auto _decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code> {
        return Codec::decodeFrame(data);
    }

    template <typename T>
    static auto _encodePayload(const T& value) -> ilias::Result<Message, std::error_code> {
        // Serializer-provided byte output keeps binary frame assembly single-pass.
        // Text serializers may still opt in by writing their JSON bytes directly.
        if constexpr (rpc::NekoRpcByteOutputSerializerAvailable<Serializer, Message>::value) {
            Message payload;
            typename Serializer::ByteOutputSerializer out(payload);
            if (out(value) && out.end()) {
                return payload;
            }
        } else {
            std::vector<char> payload;
            typename Serializer::OutputSerializer out(payload);
            if (out(value) && out.end()) {
                Message message(payload.size());
                if (!payload.empty()) {
                    std::memcpy(message.data(), payload.data(), payload.size());
                }
                return message;
            }
        }
        return ilias::Err(RpcError::InvalidParams);
    }

    template <typename T>
    static bool _decodePayload(std::span<const std::byte> payload, T& value) {
        typename Serializer::InputSerializer in(reinterpret_cast<const char*>(payload.data()), payload.size());
        return in(value);
    }

    static auto _appendFrameError(ResponseValues& responses, const FrameParts& parts, std::error_code error,
                                  ExtensionStore extensions = {}) -> void {
        if (parts.header.kind == Kind::Request) {
            responses.emplace_back(_makeErrorResponse(parts.header.id, error, std::move(extensions)));
        }
    }

    static bool _isHello(std::span<const std::byte> frame) {
        FrameParts parts;
        return Codec::parseFrame(frame, CodecId, parts) && parts.header.kind == Kind::Hello;
    }

    static auto _handleClientHello(ServerContext& context, PeerSession& session, std::span<const std::byte> frame)
        -> ilias::Result<Message, std::error_code> {
        FrameParts parts;
        if (!Codec::parseFrame(frame, CodecId, parts)) {
            return ilias::Err(RpcError::InvalidRequest);
        }

        const bool client_supports_method_id              = parts.extensions.contains(Extension::MethodId);
        CompressionAlgorithm client_compression_algorithm = CompressionAlgorithm::None;
        const bool has_client_compression_algorithm       = rpc::NekoRpcExtensionCodec::readEnumTlv(
            parts.extensions, Extension::CompressionAlgorithm, client_compression_algorithm);

        std::uint32_t client_min_payload_size = 0;
        rpc::NekoRpcExtensionCodec::readIntegerTlv(parts.extensions, Extension::CompressionMinPayloadSize,
                                                   client_min_payload_size);

        const bool client_supports_compression = parts.extensions.contains(Extension::Compression) &&
                                                 has_client_compression_algorithm &&
                                                 client_compression_algorithm != CompressionAlgorithm::None &&
                                                 CompressionCodec::supports(client_compression_algorithm);

        session.method_id_enabled =
            feature_enabled(context.options.method_id) && client_supports_method_id && !context.method_table.empty();
        session.compression_enabled = feature_enabled(context.options.compression) && client_supports_compression;
        session.compression_algorithm =
            session.compression_enabled ? client_compression_algorithm : CompressionAlgorithm::None;
        if (session.compression_enabled && client_min_payload_size > session.compression_min_payload_size) {
            session.compression_min_payload_size = client_min_payload_size;
        }

        NEKO_LOG_INFO("rpc",
                      "rpc backend server hello: client_method_id={} method_id_enabled={} table_version={} "
                      "method_count={} client_compression={} compression_enabled={} algorithm={} min_payload={}",
                      client_supports_method_id, session.method_id_enabled, context.method_table.version(),
                      context.method_table.entries().size(), client_supports_compression, session.compression_enabled,
                      rpc::neko_rpc_compression_algorithm_name(session.compression_algorithm),
                      session.compression_min_payload_size);

        ExtensionStore extensions;
        if (session.method_id_enabled) {
            _appendMethodIdHelloExtensions(context, extensions);
        }
        if (session.compression_enabled) {
            extensions[Extension::Compression] = {};
            extensions[Extension::CompressionAlgorithm] =
                rpc::NekoRpcExtensionCodec::enumValue(session.compression_algorithm);
            extensions[Extension::CompressionMinPayloadSize] =
                rpc::NekoRpcExtensionCodec::integerValue(session.compression_min_payload_size);
        }

        session.handshake_done = true;
        return _encodeHello(extensions);
    }

    static bool _handleServerHello(PeerSession& session, std::span<const std::byte> frame) {
        FrameParts parts;
        if (!Codec::parseFrame(frame, CodecId, parts)) {
            return false;
        }

        session.method_id_enabled = false;
        if (parts.extensions.contains(Extension::MethodId)) {
            session.method_id_enabled = _applyMethodIdTableExtensions(session, parts.extensions);
            if (!session.method_id_enabled && !session.remote_method_table.empty()) {
                session.method_id_enabled = true;
            }
        }

        session.compression_enabled   = parts.extensions.contains(Extension::Compression);
        session.compression_algorithm = CompressionAlgorithm::None;
        if (session.compression_enabled) {
            if (!rpc::NekoRpcExtensionCodec::readEnumTlv(parts.extensions, Extension::CompressionAlgorithm,
                                                         session.compression_algorithm) ||
                session.compression_algorithm == CompressionAlgorithm::None ||
                !CompressionCodec::supports(session.compression_algorithm)) {
                return false;
            }
            std::uint32_t min_payload_size = 0;
            if (rpc::NekoRpcExtensionCodec::readIntegerTlv(parts.extensions, Extension::CompressionMinPayloadSize,
                                                           min_payload_size)) {
                session.compression_min_payload_size = min_payload_size;
            }
        }

        NEKO_LOG_INFO("rpc",
                      "rpc backend client accepted hello: method_id={} table_version={} min_version={} "
                      "compression={} algorithm={} min_payload={}",
                      session.method_id_enabled, session.remote_method_table.version(),
                      session.remote_method_table.minimumCompatibleVersion(), session.compression_enabled,
                      rpc::neko_rpc_compression_algorithm_name(session.compression_algorithm),
                      session.compression_min_payload_size);

        session.handshake_done = true;
        return true;
    }

    static bool _isMethodIdError(std::error_code error) {
        if (&error.category() != &RpcErrorCategory::instance()) {
            return false;
        }
        switch (static_cast<RpcError>(error.value())) {
        case RpcError::MethodIdNotNegotiated:
        case RpcError::MethodTableOutdated:
        case RpcError::MethodIdNotFound:
        case RpcError::MethodIdRemoved:
        case RpcError::MethodSignatureMismatch:
            return true;
        default:
            return false;
        }
    }
};

using BinaryRpcBackend = NekoRpcBackend<BinarySerializer, 0>;

NEKO_END_NAMESPACE
