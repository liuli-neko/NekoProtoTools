#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <ilias/io/duplex.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>

#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include "nekoproto/rpc/rpc.hpp"
#include "nekoproto/serialization/json_serializer.hpp"

NEKO_USE_NAMESPACE

namespace {

auto asBytes(const std::vector<std::byte>& message) -> std::span<const std::byte> {
    return {message.data(), message.size()};
}

struct BinaryRpcTestApi {
    RpcMethodSpec<int(int, int), rpc_args<"a", "b">> add;
    RpcMethodSpec<void(int), rpc_args<"a">> sink;
};

struct UnsupportedBinaryRpcValue {
    UnsupportedBinaryRpcValue() {}
};

struct NoCompressionCodec {
    using Message = rpc::NekoRpcFrameCodec::MessageType;

    static constexpr auto preferred_algorithm() noexcept -> rpc::NekoRpcCompressionAlgorithm {
        return rpc::NekoRpcCompressionAlgorithm::None;
    }
    static constexpr bool supports(rpc::NekoRpcCompressionAlgorithm algorithm) noexcept {
        return algorithm == rpc::NekoRpcCompressionAlgorithm::None;
    }

    static auto compress(std::span<const std::byte> /**/, rpc::NekoRpcCompressionAlgorithm /**/)
        -> ilias::Result<Message, std::error_code> {
        return ilias::Err(RpcError::InvalidRequest);
    }
    static auto decompress(std::span<const std::byte> /**/, rpc::NekoRpcCompressionAlgorithm /**/)
        -> ilias::Result<Message, std::error_code> {
        return ilias::Err(RpcError::InvalidRequest);
    }
};

using NoCompressionRpcBackend = NekoRpcBackend<BinarySerializer, 0, NoCompressionCodec>;
using JsonSerializedRpcBackend = NekoRpcBackend<JsonSerializer, 1>;

static_assert(RpcBackend<BinaryRpcBackend>);
static_assert(RpcBackend<NoCompressionRpcBackend>);
static_assert(RpcBackend<JsonSerializedRpcBackend>);
static_assert(BackendSerializable<BinaryRpcBackend, int>);
static_assert(BackendSerializable<BinaryRpcBackend, std::tuple<int, int>>);
static_assert(BackendSerializable<JsonSerializedRpcBackend, std::tuple<int, int>>);
static_assert(!BackendSerializable<BinaryRpcBackend, UnsupportedBinaryRpcValue>);

auto contains(const std::vector<std::string>& values, std::string_view expected) -> bool {
    return std::any_of(values.begin(), values.end(),
                       [expected](const std::string& value) { return value == expected; });
}

auto add_metadata(std::string signature) -> detail::RpcMethodMetadata {
    return {.name = "add",
            .signature = std::move(signature),
            .description = {},
            .rpcVersion = {},
            .argNames = {"a", "b"},
            .isNotification = false,
            .isBind = true};
}

auto method_entries(const rpc::NekoRpcMethodIdTable& table) -> std::vector<rpc::NekoRpcMethodEntry> {
    return {table.entries().begin(), table.entries().end()};
}

template <typename Server, typename Client>
void connect_endpoint(Server& server, Client& client) {
#if 0
    auto serverStream = (detail::make_udp_stream_client("udp://127.0.0.1:" + std::to_string(12337 + NEKO_CPP_PLUS) +
                                                        "-127.0.0.1:" + std::to_string(12338 + NEKO_CPP_PLUS)))
                            .wait()
                            .value();
    auto clientStream = (detail::make_udp_stream_client("udp://127.0.0.1:" + std::to_string(12338 + NEKO_CPP_PLUS) +
                                                        "-127.0.0.1:" + std::to_string(12337 + NEKO_CPP_PLUS)))
                            .wait()
                            .value();
#else
    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
#endif
    server.addEndpoint(std::move(serverStream));
    client.setEndpoint(std::move(clientStream));
}

} // namespace

TEST(NekoRpcBackend, CallsRegisteredMethodThroughBinaryFrame) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};
    EXPECT_FALSE(client.isConnected());

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    std::uint64_t next_id = 0;
    auto request          = BinaryRpcBackend::encodeRequest(server->add, false, next_id, 20, 22);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto response = server.processMessage(asBytes(request.value().message)).wait();
    ASSERT_FALSE(response.empty());

    auto decoded = BinaryRpcBackend::decodeResponse<decltype(server->add)>(asBytes(response), request.value().id);
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message();
    EXPECT_EQ(decoded.value(), 42);
}

TEST(NekoRpcBackend, JsonSerializedBackendCallsRegisteredMethodThroughFrame) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<JsonSerializedRpcBackend, BinaryRpcTestApi> server{context};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    std::uint64_t next_id = 0;
    auto request          = JsonSerializedRpcBackend::encodeRequest(server->add, false, next_id, 20, 22);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto response = server.processMessage(asBytes(request.value().message)).wait();
    ASSERT_FALSE(response.empty());

    auto decoded =
        JsonSerializedRpcBackend::decodeResponse<decltype(server->add)>(asBytes(response), request.value().id);
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message();
    EXPECT_EQ(decoded.value(), 42);
}

TEST(NekoRpcBackend, CallsThroughIliasDuplexStreamEndpoint) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};
    connect_endpoint(server, client);

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    auto result = client->add(20, 22).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), 42);

    auto result2 = client->rpc.getMethodInfoList().wait();
    ASSERT_TRUE(result2.has_value()) << result2.error().message();
    EXPECT_EQ(result2.value().size(), 2 + (RpcServer<BinaryRpcBackend>::BuiltinMethodsCount));
    for (auto& method : result2.value()) {
        std::cout << "method: " << method << std::endl;
    }

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MakeEndpointAcceptsIliasStreamAndMessageEndpoint) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    auto raw_client_endpoint          = BinaryRpcBackend::makeEndpoint(std::move(clientStream));
    static_assert(MessageEndpoint<decltype(raw_client_endpoint)>);
    auto client_endpoint = BinaryRpcBackend::makeEndpoint(std::move(raw_client_endpoint));
    static_assert(MessageEndpoint<decltype(client_endpoint)>);
    auto server_endpoint = BinaryRpcBackend::makeEndpoint(std::move(serverStream));
    static_assert(MessageEndpoint<decltype(server_endpoint)>);

    std::uint64_t next_id = 0;
    auto request          = BinaryRpcBackend::encodeRequest(server->add, false, next_id, 20, 22);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto sent = client_endpoint.send(asBytes(request.value().message)).wait();
    ASSERT_TRUE(sent.has_value()) << sent.error().message();
    EXPECT_EQ(sent.value(), request.value().message.size());

    auto flushed = client_endpoint.flush().wait();
    ASSERT_TRUE(flushed.has_value()) << flushed.error().message();

    std::vector<std::byte> received;
    auto recved = server_endpoint.recv(received).wait();
    ASSERT_TRUE(recved.has_value()) << recved.error().message();
    EXPECT_EQ(recved.value(), request.value().message.size());
    ASSERT_EQ(received.size(), request.value().message.size());
    EXPECT_TRUE(std::equal(received.begin(), received.end(),
                           reinterpret_cast<const std::byte*>(request.value().message.data())));

    auto shutdown = client_endpoint.shutdown().wait();
    ASSERT_TRUE(shutdown.has_value()) << shutdown.error().message();
    server_endpoint.close();
}

TEST(NekoRpcBackend, RequireMethodIdModeCallsThroughNegotiatedTable) {
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Require;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    connect_endpoint(server, client);

    auto result = client->add(20, 22).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), 42);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodIdTableDynamicUpdatesTrackCompatibility) {
    const auto add_hash  = rpc::NekoRpcMethodIdTable::signatureHash("add", "i32 add(i32, i32)");
    const auto sink_hash = rpc::NekoRpcMethodIdTable::signatureHash("sink", "void sink(i32)");
    const auto mul_hash  = rpc::NekoRpcMethodIdTable::signatureHash("mul", "i32 mul(i32, i32)");

    rpc::NekoRpcMethodIdTable table;
    table.reset(
        {
            {.id = 0, .name = "add", .signature_hash = add_hash, .state = rpc::NekoRpcMethodState::Active},
            {.id = 1, .name = "sink", .signature_hash = sink_hash, .state = rpc::NekoRpcMethodState::Active},
        },
        rpc::NekoRpcMethodIdExtension::InitialTableVersion);
    const auto version1 = table.version();
    ASSERT_EQ(version1, rpc::NekoRpcMethodIdExtension::InitialTableVersion);

    const auto* add  = table.findByName("add");
    const auto* sink = table.findByName("sink");
    ASSERT_NE(add, nullptr);
    ASSERT_NE(sink, nullptr);
    const auto add_id  = add->id;
    const auto sink_id = sink->id;

    const auto* mul = table.upsert("mul", mul_hash);
    ASSERT_NE(mul, nullptr);
    EXPECT_GT(mul->id, sink_id);
    EXPECT_GT(table.version(), version1);

    auto compatible_old = table.resolve(add_id, version1, add_hash, true);
    EXPECT_EQ(compatible_old.status, rpc::NekoRpcMethodIdResolveStatus::Ok);
    ASSERT_NE(compatible_old.entry, nullptr);
    EXPECT_EQ(compatible_old.entry->name, "add");

    auto mismatch = table.resolve(add_id, table.version(), add_hash ^ 1U, true);
    EXPECT_EQ(mismatch.status, rpc::NekoRpcMethodIdResolveStatus::SignatureMismatch);

    ASSERT_TRUE(table.remove("sink"));
    auto removed = table.resolve(sink_id, version1, sink_hash, true);
    EXPECT_EQ(removed.status, rpc::NekoRpcMethodIdResolveStatus::MethodRemoved);

    table.setMinimumCompatibleVersion(table.version());
    auto outdated = table.resolve(add_id, version1, add_hash, true);
    EXPECT_EQ(outdated.status, rpc::NekoRpcMethodIdResolveStatus::TableOutdated);
}

TEST(NekoRpcBackend, MethodIdDeltaRoundTripsTableUpdates) {
    const auto addHash  = rpc::NekoRpcMethodIdTable::signatureHash("add", "i32 add(i32, i32)");
    const auto sinkHash = rpc::NekoRpcMethodIdTable::signatureHash("sink", "void sink(i32)");
    const auto mulHash  = rpc::NekoRpcMethodIdTable::signatureHash("mul", "i32 mul(i32, i32)");

    rpc::NekoRpcMethodIdTable serverTable;
    serverTable.reset(
        {
            {.id = 0, .name = "add", .signature_hash = addHash, .state = rpc::NekoRpcMethodState::Active},
            {.id = 1, .name = "sink", .signature_hash = sinkHash, .state = rpc::NekoRpcMethodState::Active},
        },
        rpc::NekoRpcMethodIdExtension::InitialTableVersion);
    rpc::NekoRpcMethodIdTable clientTable;
    clientTable.applyRemoteTable(serverTable.entries(), serverTable.version());

    const auto* mul = serverTable.upsert("mul", mulHash);
    ASSERT_NE(mul, nullptr);
    auto deltaValue = rpc::NekoRpcMethodIdExtension::methodTableDeltaValue({*mul});
    rpc::NekoRpcFrameCodec::ExtensionMapType deltaExtensions;
    deltaExtensions[rpc::NekoRpcExtensionType::MethodTableDelta] = rpc::NekoRpcExtensionCodec::asBytes(deltaValue);
    std::vector<rpc::NekoRpcMethodEntry> parsedDelta;
    ASSERT_TRUE(rpc::NekoRpcMethodIdExtension::parseMethodTableDelta(deltaExtensions, parsedDelta));
    ASSERT_TRUE(clientTable.applyRemoteDelta(parsedDelta, serverTable.version()));

    auto mulResolved = clientTable.resolve(mul->id, clientTable.version(), mulHash, true);
    EXPECT_EQ(mulResolved.status, rpc::NekoRpcMethodIdResolveStatus::Ok);
    ASSERT_NE(mulResolved.entry, nullptr);
    EXPECT_EQ(mulResolved.entry->name, "mul");

    const auto* sink = serverTable.findByName("sink");
    ASSERT_NE(sink, nullptr);
    const auto sinkId = sink->id;
    ASSERT_TRUE(serverTable.remove("sink"));
    const auto* tombstone = serverTable.findById(sinkId);
    ASSERT_NE(tombstone, nullptr);
    deltaValue = rpc::NekoRpcMethodIdExtension::methodTableDeltaValue({*tombstone});
    deltaExtensions[rpc::NekoRpcExtensionType::MethodTableDelta] = rpc::NekoRpcExtensionCodec::asBytes(deltaValue);
    ASSERT_TRUE(rpc::NekoRpcMethodIdExtension::parseMethodTableDelta(deltaExtensions, parsedDelta));
    ASSERT_TRUE(clientTable.applyRemoteDelta(parsedDelta, serverTable.version()));

    auto sinkResolved = clientTable.resolve(sinkId, clientTable.version(), sinkHash, true);
    EXPECT_EQ(sinkResolved.status, rpc::NekoRpcMethodIdResolveStatus::MethodRemoved);
}

TEST(NekoRpcBackend, MethodIdErrorResponseCarriesRefreshTableWithinBudget) {
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Auto;

    std::vector<detail::RpcMethodMetadata> methods{add_metadata("i32 add(i32 a, i32 b)")};
    auto serverContext = BinaryRpcBackend::makeServerContext(options, methods);
    auto serverSession = BinaryRpcBackend::makeServerPeerSession(serverContext);
    serverSession.method_id_enabled = true;

    auto clientContext = BinaryRpcBackend::makeClientContext(options);
    auto clientSession = BinaryRpcBackend::makeClientPeerSession(clientContext);
    clientSession.method_id_enabled = true;
    clientSession.remote_method_table.reset(method_entries(serverContext.method_table),
                                            serverContext.method_table.version());

    BinaryRpcTestApi api;
    api.add.setRemoteName("add");
    auto request = BinaryRpcBackend::encodeRequest(clientContext, clientSession, api.add, false, 4, 2);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    methods = {add_metadata("i32 add(i32 lhs, i32 rhs)")};
    BinaryRpcBackend::refreshMethodCatalog(serverContext, methods);

    auto decoded = BinaryRpcBackend::decodeIncoming(serverContext, serverSession, asBytes(request.value().message));
    ASSERT_TRUE(decoded.ok);
    ASSERT_TRUE(decoded.requests.empty());
    ASSERT_EQ(decoded.responses.size(), 1U);

    auto response = BinaryRpcBackend::encodeResponses(serverContext, serverSession, decoded.responses, false);
    BinaryRpcBackend::Codec::FrameParts parts;
    ASSERT_TRUE(BinaryRpcBackend::Codec::parseFrame(asBytes(response), 0, parts));
    ASSERT_NE((parts.header.flags & BinaryRpcBackend::Flag::Error), 0);
    EXPECT_TRUE(parts.extensions.contains(rpc::NekoRpcExtensionType::MethodTable));

    auto failed =
        BinaryRpcBackend::decodeResponse<decltype(api.add)>(clientContext, clientSession, asBytes(response),
                                                            request.value().id);
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), make_error_code(RpcError::MethodIdRemoved));
    EXPECT_EQ(clientSession.remote_method_table.version(), serverContext.method_table.version());
    const auto* refreshed = clientSession.remote_method_table.findByName("add");
    ASSERT_NE(refreshed, nullptr);
    EXPECT_EQ(refreshed->id, 1U);
}

TEST(NekoRpcBackend, MethodIdErrorResponseOmitsRefreshTableOverBudget) {
    BinaryRpcBackend::Options options;
    options.method_id                             = BinaryRpcBackend::MethodIdMode::Auto;
    options.max_auto_method_table_extension_bytes = 60;

    std::vector<detail::RpcMethodMetadata> methods{add_metadata("i32 add(i32 a, i32 b)")};
    auto serverContext = BinaryRpcBackend::makeServerContext(options, methods);
    auto serverSession = BinaryRpcBackend::makeServerPeerSession(serverContext);
    serverSession.method_id_enabled = true;

    auto clientContext = BinaryRpcBackend::makeClientContext(options);
    auto clientSession = BinaryRpcBackend::makeClientPeerSession(clientContext);
    clientSession.method_id_enabled = true;
    clientSession.remote_method_table.reset(method_entries(serverContext.method_table),
                                            serverContext.method_table.version());

    BinaryRpcTestApi api;
    api.add.setRemoteName("add");
    auto request = BinaryRpcBackend::encodeRequest(clientContext, clientSession, api.add, false, 4, 2);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    methods = {add_metadata("i32 add(i32 lhs, i32 rhs)")};
    BinaryRpcBackend::refreshMethodCatalog(serverContext, methods);

    auto decoded = BinaryRpcBackend::decodeIncoming(serverContext, serverSession, asBytes(request.value().message));
    ASSERT_TRUE(decoded.ok);
    ASSERT_EQ(decoded.responses.size(), 1U);

    auto response = BinaryRpcBackend::encodeResponses(serverContext, serverSession, decoded.responses, false);
    BinaryRpcBackend::Codec::FrameParts parts;
    ASSERT_TRUE(BinaryRpcBackend::Codec::parseFrame(asBytes(response), 0, parts));
    EXPECT_FALSE(parts.extensions.contains(rpc::NekoRpcExtensionType::MethodTable));
    EXPECT_TRUE(parts.extensions.contains(rpc::NekoRpcExtensionType::MethodTableVersion));
}

TEST(NekoRpcBackend, CompressionExtensionsRoundTripNegotiationContext) {
    auto algorithmValue = rpc::NekoRpcExtensionCodec::enumValue(rpc::NekoRpcCompressionAlgorithm::RunLength);
    auto minSizeValue   = rpc::NekoRpcExtensionCodec::integerValue<std::uint32_t>(256);

    rpc::NekoRpcFrameCodec::ExtensionMapType extensions;
    extensions[rpc::NekoRpcExtensionType::Compression]          = {};
    extensions[rpc::NekoRpcExtensionType::CompressionAlgorithm] = rpc::NekoRpcExtensionCodec::asBytes(algorithmValue);
    extensions[rpc::NekoRpcExtensionType::CompressionMinPayloadSize] =
        rpc::NekoRpcExtensionCodec::asBytes(minSizeValue);

    rpc::NekoRpcFrameCodec::MessageType bytes;
    ASSERT_TRUE(rpc::NekoRpcExtensionCodec::appendTlvs(bytes, extensions));

    rpc::NekoRpcFrameCodec::ExtensionMapType parsed;
    ASSERT_TRUE(rpc::NekoRpcExtensionCodec::loadTlvs(rpc::NekoRpcExtensionCodec::asBytes(bytes), parsed));
    EXPECT_TRUE(parsed.contains(rpc::NekoRpcExtensionType::Compression));

    rpc::NekoRpcCompressionAlgorithm algorithm = rpc::NekoRpcCompressionAlgorithm::None;
    ASSERT_TRUE(
        rpc::NekoRpcExtensionCodec::readEnumTlv(parsed, rpc::NekoRpcExtensionType::CompressionAlgorithm, algorithm));
    EXPECT_EQ(algorithm, rpc::NekoRpcCompressionAlgorithm::RunLength);
    std::uint32_t minSize = 0;
    ASSERT_TRUE(rpc::NekoRpcExtensionCodec::readIntegerTlv(parsed, rpc::NekoRpcExtensionType::CompressionMinPayloadSize,
                                                           minSize));
    EXPECT_EQ(minSize, 256U);
}

TEST(NekoRpcBackend, CompressionCodecRunLengthShrinksAndRestoresPayload) {
    rpc::NekoRpcFrameCodec::MessageType payload(512, static_cast<std::byte>('A'));
    auto compressed = rpc::NekoRpcCompressionCodec::compress(rpc::NekoRpcExtensionCodec::asBytes(payload),
                                                             rpc::NekoRpcCompressionAlgorithm::RunLength);
    ASSERT_TRUE(compressed.has_value()) << compressed.error().message();
    ASSERT_LT(compressed.value().size(), payload.size());

    auto restored = rpc::NekoRpcCompressionCodec::decompress(rpc::NekoRpcExtensionCodec::asBytes(compressed.value()),
                                                             rpc::NekoRpcCompressionAlgorithm::RunLength);
    ASSERT_TRUE(restored.has_value()) << restored.error().message();
    EXPECT_EQ(restored.value(), payload);
}

TEST(NekoRpcBackend, RequireCompressionCallsThroughCompressedPayloads) {
    ilias::PlatformContext context;
    context.install();
    auto stats = std::make_shared<BinaryRpcBackend::CompressionStats>();
    BinaryRpcBackend::Options options;
    options.method_id                    = BinaryRpcBackend::MethodIdMode::Disable;
    options.compression                  = BinaryRpcBackend::CompressionMode::Require;
    options.compression_min_payload_size = 16;
    options.compression_stats            = stats;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};

    server.bindMethod("echo", traits::FunctionT<ilias::IoTask<std::string>(std::string)>(
                                  [](std::string value) -> ilias::IoTask<std::string> { co_return value; }));

    connect_endpoint(server, client);

    std::string payload(512, 'A');
    auto result = client.callRemote<std::string>("echo", payload).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), payload);
    EXPECT_GE(stats->compression_attempts.load(), 2U);
    EXPECT_GE(stats->compressed_frames.load(), 2U);
    EXPECT_GE(stats->decompressed_frames.load(), 2U);
    EXPECT_GT(stats->compression_input_bytes.load(), stats->compression_output_bytes.load());
    EXPECT_GT(stats->decompression_output_bytes.load(), stats->decompression_input_bytes.load());
    EXPECT_EQ(stats->compression_errors.load(), 0U);
    EXPECT_EQ(stats->decompression_errors.load(), 0U);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ReplaceableCompressionCodecFallsBackWhenUnsupported) {
    ilias::PlatformContext context;
    context.install();
    NoCompressionRpcBackend::Options options;
    options.method_id         = NoCompressionRpcBackend::MethodIdMode::Disable;
    options.compression       = NoCompressionRpcBackend::CompressionMode::Auto;
    auto stats                = std::make_shared<NoCompressionRpcBackend::CompressionStats>();
    options.compression_stats = stats;
    RpcServer<NoCompressionRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<NoCompressionRpcBackend, BinaryRpcTestApi> client{context, options};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    connect_endpoint(server, client);

    auto result = client->add(20, 22).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), 42);
    EXPECT_EQ(stats->compression_attempts.load(), 0U);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodIdTableRefreshAddsMethodWithinConnection) {
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Require;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    connect_endpoint(server, client);

    auto first = client->add(20, 22).wait();
    ASSERT_TRUE(first.has_value()) << first.error().message();
    EXPECT_EQ(first.value(), 42);

    server.bindMethod("late", traits::FunctionT<ilias::IoTask<int>(int, int)>(
                                  [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; }));
    auto drained = client->rpc.getMethodList().wait();
    ASSERT_TRUE(drained.has_value()) << drained.error().message();

    auto late = client.callRemote<int>("late", 20, 22).wait();
    ASSERT_TRUE(late.has_value()) << late.error().message();
    EXPECT_EQ(late.value(), 42);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodIdErrorCanBeReturnedWhenClientRecoveryIsDisabled) {
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Auto;
    options.retry_method_id_error_once = false;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    connect_endpoint(server, client);

    auto first = client->add(1, 2).wait();
    ASSERT_TRUE(first.has_value()) << first.error().message();
    EXPECT_EQ(first.value(), 3);

    server.bindMethod<"lhs", "rhs">("add",
                                    traits::FunctionT<ilias::IoTask<int>(int, int)>(
                                        [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs * 10 + rhs; }));

    auto failed = client->add(4, 2).wait();
    ASSERT_FALSE(failed.has_value());
    EXPECT_GE(failed.error().value(), static_cast<int>(RpcError::MethodIdNotNegotiated));
    EXPECT_LE(failed.error().value(), static_cast<int>(RpcError::MethodSignatureMismatch));

    auto fallback = client->add(4, 2).wait();
    ASSERT_TRUE(fallback.has_value()) << fallback.error().message();
    EXPECT_EQ(fallback.value(), 42);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodIdErrorRefreshesTableAndRetriesOnceOnClient) {
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Auto;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    connect_endpoint(server, client);

    auto first = client->add(1, 2).wait();
    ASSERT_TRUE(first.has_value()) << first.error().message();
    EXPECT_EQ(first.value(), 3);

    int rebound_calls = 0;
    server.bindMethod<"lhs", "rhs">("add", traits::FunctionT<ilias::IoTask<int>(int, int)>(
                                               [&rebound_calls](int lhs, int rhs) -> ilias::IoTask<int> {
                                                   ++rebound_calls;
                                                   co_return lhs * 10 + rhs;
                                               }));

    auto recovered = client->add(4, 2).wait();
    ASSERT_TRUE(recovered.has_value()) << recovered.error().message();
    EXPECT_EQ(recovered.value(), 42);
    EXPECT_EQ(rebound_calls, 1);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, NotificationDoesNotReturnResponse) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};

    bool called  = false;
    server->sink = [&called](int value) -> ilias::IoTask<void> {
        called = value == 42;
        co_return {};
    };

    std::uint64_t next_id = 0;
    auto request          = BinaryRpcBackend::encodeRequest(server->sink, true, next_id, 42);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto response = server.processMessage(asBytes(request.value().message)).wait();
    EXPECT_TRUE(response.empty());
    EXPECT_TRUE(called);
}

TEST(NekoRpcBackend, BuiltinMethodsAvailableOnClientByDefault) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend> server{context};
    RpcClient<BinaryRpcBackend> client{context};
    connect_endpoint(server, client);

    auto methods = client->rpc.getMethodList().wait();
    ASSERT_TRUE(methods.has_value()) << methods.error().message();
    EXPECT_TRUE(contains(methods.value(), "rpc.get_method_list"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_method_info"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_method_info_list"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_bind_method_list"));

    auto method_info = client->rpc.getMethodInfo("rpc.get_method_list").wait();
    ASSERT_TRUE(method_info.has_value()) << method_info.error().message();

    auto bound_methods = client->rpc.getBindedMethodList().wait();
    ASSERT_TRUE(bound_methods.has_value()) << bound_methods.error().message();
    EXPECT_TRUE(contains(bound_methods.value(), "rpc.get_method_list"));
    EXPECT_TRUE(contains(bound_methods.value(), "rpc.get_bind_method_list"));

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodNotFoundReturnsRpcError) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};

    RpcMethod<int(), "missing"> missing;
    std::uint64_t nextId = 0;
    auto request         = BinaryRpcBackend::encodeRequest(missing, false, nextId);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto response = server.processMessage(asBytes(request.value().message)).wait();
    ASSERT_FALSE(response.empty());

    auto decoded = BinaryRpcBackend::decodeResponse<decltype(missing)>(asBytes(response), request.value().id);
    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error().value(), static_cast<int>(RpcError::MethodNotFound));
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
