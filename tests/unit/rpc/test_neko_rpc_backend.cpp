#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

NEKO_USE_NAMESPACE

namespace {

auto asBytes(const std::vector<char>& message) -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(message.data()), message.size()};
}

struct BinaryRpcTestApi {
    RpcMethod<int(int, int), "add"> add;
    RpcMethod<void(int), "sink"> sink;

    NEKO_SERIALIZER(add, sink)
};

struct UnsupportedBinaryRpcValue {
    UnsupportedBinaryRpcValue() {}
};

struct NoCompressionCodec {
    using Message = detail::NekoRpcFrameCodec::Message;

    static constexpr auto preferredAlgorithm() noexcept -> detail::NekoRpcCompressionAlgorithm {
        return detail::NekoRpcCompressionAlgorithm::None;
    }
    static constexpr bool supports(detail::NekoRpcCompressionAlgorithm algorithm) noexcept {
        return algorithm == detail::NekoRpcCompressionAlgorithm::None;
    }

    static auto compress(std::span<const std::byte>, detail::NekoRpcCompressionAlgorithm)
        -> ilias::Result<Message, std::error_code> {
        return ilias::Err(RpcError::InvalidRequest);
    }
    static auto decompress(std::span<const std::byte>, detail::NekoRpcCompressionAlgorithm)
        -> ilias::Result<Message, std::error_code> {
        return ilias::Err(RpcError::InvalidRequest);
    }
};

using NoCompressionRpcBackend = NekoRpcBackend<BinarySerializer, 0, NoCompressionCodec>;

static_assert(RpcBackend<BinaryRpcBackend>);
static_assert(RpcBackend<NoCompressionRpcBackend>);
static_assert(BackendSerializable<BinaryRpcBackend, int>);
static_assert(BackendSerializable<BinaryRpcBackend, std::tuple<int, int>>);
static_assert(!BackendSerializable<BinaryRpcBackend, UnsupportedBinaryRpcValue>);

auto contains(const std::vector<std::string>& values, std::string_view expected) -> bool {
    return std::any_of(values.begin(), values.end(),
                       [expected](const std::string& value) { return value == expected; });
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

template <typename Backend, typename Server, typename Client>
void connect_endpoint(Server& server, Client& client, typename Backend::Options options) {
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
    server.addEndpoint(Backend::makeServerEndpoint(std::move(serverStream), server.methodDatas(), options));
    client.setEndpoint(Backend::makeClientEndpoint(std::move(clientStream), options));
}

template <typename Server, typename Client>
void connect_endpoint(Server& server, Client& client, BinaryRpcBackend::Options options) {
    connect_endpoint<BinaryRpcBackend>(server, client, std::move(options));
}

} // namespace

TEST(NekoRpcBackend, CallsRegisteredMethodThroughBinaryFrame) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};
    EXPECT_FALSE(client.isConnected());

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    std::uint64_t nextId = 0;
    auto request         = BinaryRpcBackend::encodeRequest(server->add, false, nextId, 20, 22);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto response = server.processMessage(asBytes(request.value().message)).wait();
    ASSERT_FALSE(response.empty());

    auto decoded = BinaryRpcBackend::decodeResponse<decltype(server->add)>(asBytes(response), request.value().id);
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
    auto rawClientEndpoint            = BinaryRpcBackend::makeEndpoint(std::move(clientStream));
    static_assert(MessageEndpoint<decltype(rawClientEndpoint)>);
    auto clientEndpoint = BinaryRpcBackend::makeEndpoint(std::move(rawClientEndpoint));
    static_assert(MessageEndpoint<decltype(clientEndpoint)>);
    auto serverEndpoint = BinaryRpcBackend::makeEndpoint(std::move(serverStream));
    static_assert(MessageEndpoint<decltype(serverEndpoint)>);

    std::uint64_t nextId = 0;
    auto request         = BinaryRpcBackend::encodeRequest(server->add, false, nextId, 20, 22);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto sent = clientEndpoint.send(asBytes(request.value().message)).wait();
    ASSERT_TRUE(sent.has_value()) << sent.error().message();
    EXPECT_EQ(sent.value(), request.value().message.size());

    auto flushed = clientEndpoint.flush().wait();
    ASSERT_TRUE(flushed.has_value()) << flushed.error().message();

    std::vector<std::byte> received;
    auto recved = serverEndpoint.recv(received).wait();
    ASSERT_TRUE(recved.has_value()) << recved.error().message();
    EXPECT_EQ(recved.value(), request.value().message.size());
    ASSERT_EQ(received.size(), request.value().message.size());
    EXPECT_TRUE(std::equal(received.begin(), received.end(),
                           reinterpret_cast<const std::byte*>(request.value().message.data())));

    auto shutdown = clientEndpoint.shutdown().wait();
    ASSERT_TRUE(shutdown.has_value()) << shutdown.error().message();
    serverEndpoint.close();
}

TEST(NekoRpcBackend, RequireMethodIdModeCallsThroughNegotiatedTable) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    BinaryRpcBackend::Options options;
    options.methodId = BinaryRpcBackend::MethodIdMode::Require;
    connect_endpoint(server, client, options);

    auto result = client->add(20, 22).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), 42);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodIdTableDynamicUpdatesTrackCompatibility) {
    const auto addHash  = detail::NekoRpcMethodIdTable::signatureHash("add", "i32 add(i32, i32)");
    const auto sinkHash = detail::NekoRpcMethodIdTable::signatureHash("sink", "void sink(i32)");
    const auto mulHash  = detail::NekoRpcMethodIdTable::signatureHash("mul", "i32 mul(i32, i32)");

    detail::NekoRpcMethodIdTable table;
    table.reset({
        {.id = 0, .name = "add", .signatureHash = addHash, .state = detail::NekoRpcMethodState::Active},
        {.id = 1, .name = "sink", .signatureHash = sinkHash, .state = detail::NekoRpcMethodState::Active},
    });
    const auto version1 = table.version();
    ASSERT_EQ(version1, detail::NekoRpcMethodIdExtension::InitialTableVersion);

    const auto* add  = table.findByName("add");
    const auto* sink = table.findByName("sink");
    ASSERT_NE(add, nullptr);
    ASSERT_NE(sink, nullptr);
    const auto addId  = add->id;
    const auto sinkId = sink->id;

    const auto* mul = table.upsert("mul", mulHash);
    ASSERT_NE(mul, nullptr);
    EXPECT_GT(mul->id, sinkId);
    EXPECT_GT(table.version(), version1);

    auto compatibleOld = table.resolve(addId, version1, addHash, true);
    EXPECT_EQ(compatibleOld.status, detail::NekoRpcMethodIdResolveStatus::Ok);
    ASSERT_NE(compatibleOld.entry, nullptr);
    EXPECT_EQ(compatibleOld.entry->name, "add");

    auto mismatch = table.resolve(addId, table.version(), addHash ^ 1U, true);
    EXPECT_EQ(mismatch.status, detail::NekoRpcMethodIdResolveStatus::SignatureMismatch);

    ASSERT_TRUE(table.remove("sink"));
    auto removed = table.resolve(sinkId, version1, sinkHash, true);
    EXPECT_EQ(removed.status, detail::NekoRpcMethodIdResolveStatus::MethodRemoved);

    table.setMinimumCompatibleVersion(table.version());
    auto outdated = table.resolve(addId, version1, addHash, true);
    EXPECT_EQ(outdated.status, detail::NekoRpcMethodIdResolveStatus::TableOutdated);
}

TEST(NekoRpcBackend, MethodIdDeltaRoundTripsTableUpdates) {
    const auto addHash  = detail::NekoRpcMethodIdTable::signatureHash("add", "i32 add(i32, i32)");
    const auto sinkHash = detail::NekoRpcMethodIdTable::signatureHash("sink", "void sink(i32)");
    const auto mulHash  = detail::NekoRpcMethodIdTable::signatureHash("mul", "i32 mul(i32, i32)");

    detail::NekoRpcMethodIdTable serverTable;
    serverTable.reset({
        {.id = 0, .name = "add", .signatureHash = addHash, .state = detail::NekoRpcMethodState::Active},
        {.id = 1, .name = "sink", .signatureHash = sinkHash, .state = detail::NekoRpcMethodState::Active},
    });
    detail::NekoRpcMethodIdTable clientTable;
    clientTable.applyRemoteTable(serverTable.entries(), serverTable.version());

    const auto* mul = serverTable.upsert("mul", mulHash);
    ASSERT_NE(mul, nullptr);
    auto deltaTlv = detail::NekoRpcMethodIdExtension::methodTableDeltaTlv({*mul});
    std::vector<detail::NekoRpcMethodEntry> parsedDelta;
    ASSERT_TRUE(detail::NekoRpcMethodIdExtension::parseMethodTableDelta(
        detail::NekoRpcExtensionCodec::asBytes(deltaTlv), parsedDelta));
    ASSERT_TRUE(clientTable.applyRemoteDelta(parsedDelta, serverTable.version()));

    auto mulResolved = clientTable.resolve(mul->id, clientTable.version(), mulHash, true);
    EXPECT_EQ(mulResolved.status, detail::NekoRpcMethodIdResolveStatus::Ok);
    ASSERT_NE(mulResolved.entry, nullptr);
    EXPECT_EQ(mulResolved.entry->name, "mul");

    const auto* sink = serverTable.findByName("sink");
    ASSERT_NE(sink, nullptr);
    const auto sinkId = sink->id;
    ASSERT_TRUE(serverTable.remove("sink"));
    const auto* tombstone = serverTable.findById(sinkId);
    ASSERT_NE(tombstone, nullptr);
    deltaTlv = detail::NekoRpcMethodIdExtension::methodTableDeltaTlv({*tombstone});
    ASSERT_TRUE(detail::NekoRpcMethodIdExtension::parseMethodTableDelta(
        detail::NekoRpcExtensionCodec::asBytes(deltaTlv), parsedDelta));
    ASSERT_TRUE(clientTable.applyRemoteDelta(parsedDelta, serverTable.version()));

    auto sinkResolved = clientTable.resolve(sinkId, clientTable.version(), sinkHash, true);
    EXPECT_EQ(sinkResolved.status, detail::NekoRpcMethodIdResolveStatus::MethodRemoved);
}

TEST(NekoRpcBackend, CompressionTlvRoundTripsNegotiationStrategy) {
    auto modeTlv = detail::NekoRpcCompressionExtension::modeTlv(detail::NekoRpcFeatureMode::Auto);
    auto algorithmTlv =
        detail::NekoRpcCompressionExtension::algorithmTlv(detail::NekoRpcCompressionAlgorithm::RunLength);
    auto minSizeTlv = detail::NekoRpcCompressionExtension::minPayloadSizeTlv(256);
    modeTlv.insert(modeTlv.end(), algorithmTlv.begin(), algorithmTlv.end());
    modeTlv.insert(modeTlv.end(), minSizeTlv.begin(), minSizeTlv.end());

    auto bytes = detail::NekoRpcExtensionCodec::asBytes(modeTlv);
    EXPECT_EQ(detail::NekoRpcCompressionExtension::parseMode(bytes), detail::NekoRpcFeatureMode::Auto);
    EXPECT_EQ(detail::NekoRpcCompressionExtension::parseAlgorithm(bytes),
              detail::NekoRpcCompressionAlgorithm::RunLength);
    std::uint32_t minSize = 0;
    ASSERT_TRUE(detail::NekoRpcCompressionExtension::readMinPayloadSize(bytes, minSize));
    EXPECT_EQ(minSize, 256U);
}

TEST(NekoRpcBackend, CompressionCodecRunLengthShrinksAndRestoresPayload) {
    detail::NekoRpcFrameCodec::Message payload(512, 'A');
    auto compressed = detail::NekoRpcCompressionCodec::compress(detail::NekoRpcExtensionCodec::asBytes(payload),
                                                                detail::NekoRpcCompressionAlgorithm::RunLength);
    ASSERT_TRUE(compressed.has_value()) << compressed.error().message();
    ASSERT_LT(compressed.value().size(), payload.size());

    auto restored = detail::NekoRpcCompressionCodec::decompress(
        detail::NekoRpcExtensionCodec::asBytes(compressed.value()), detail::NekoRpcCompressionAlgorithm::RunLength);
    ASSERT_TRUE(restored.has_value()) << restored.error().message();
    EXPECT_EQ(restored.value(), payload);
}

TEST(NekoRpcBackend, RequireCompressionCallsThroughCompressedPayloads) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};

    server.bindMethod("echo", traits::FunctionT<ilias::IoTask<std::string>(std::string)>(
                                  [](std::string value) -> ilias::IoTask<std::string> { co_return value; }));

    auto stats = std::make_shared<BinaryRpcBackend::CompressionStats>();
    BinaryRpcBackend::Options options;
    options.methodId                  = BinaryRpcBackend::MethodIdMode::Disable;
    options.compression               = BinaryRpcBackend::CompressionMode::Require;
    options.compressionMinPayloadSize = 16;
    options.compressionStats          = stats;
    connect_endpoint(server, client, options);

    std::string payload(512, 'A');
    auto result = client.callRemote<std::string>("echo", payload).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), payload);
    EXPECT_GE(stats->compressionAttempts.load(), 2U);
    EXPECT_GE(stats->compressedFrames.load(), 2U);
    EXPECT_GE(stats->decompressedFrames.load(), 2U);
    EXPECT_GT(stats->compressionInputBytes.load(), stats->compressionOutputBytes.load());
    EXPECT_GT(stats->decompressionOutputBytes.load(), stats->decompressionInputBytes.load());
    EXPECT_EQ(stats->compressionErrors.load(), 0U);
    EXPECT_EQ(stats->decompressionErrors.load(), 0U);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ReplaceableCompressionCodecCanRejectRequiredCompression) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<NoCompressionRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<NoCompressionRpcBackend, BinaryRpcTestApi> client{context};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    NoCompressionRpcBackend::Options options;
    options.methodId         = NoCompressionRpcBackend::MethodIdMode::Disable;
    options.compression      = NoCompressionRpcBackend::CompressionMode::Require;
    auto stats               = std::make_shared<NoCompressionRpcBackend::CompressionStats>();
    options.compressionStats = stats;
    connect_endpoint<NoCompressionRpcBackend>(server, client, options);

    auto result = client->add(20, 22).wait();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().value(), static_cast<int>(RpcError::CompressionRequiredButUnsupported));
    EXPECT_EQ(stats->compressionAttempts.load(), 0U);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodIdTableRefreshAddsMethodWithinConnection) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    BinaryRpcBackend::Options options;
    options.methodId = BinaryRpcBackend::MethodIdMode::Require;
    connect_endpoint(server, client, options);

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

TEST(NekoRpcBackend, MethodIdErrorRefreshesAndRetriesWithStringFallback) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };

    BinaryRpcBackend::Options options;
    options.methodId = BinaryRpcBackend::MethodIdMode::Auto;
    connect_endpoint(server, client, options);

    auto first = client->add(1, 2).wait();
    ASSERT_TRUE(first.has_value()) << first.error().message();
    EXPECT_EQ(first.value(), 3);

    server.bindMethod<"lhs", "rhs">("add",
                                    traits::FunctionT<ilias::IoTask<int>(int, int)>(
                                        [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs * 10 + rhs; }));

    auto retried = client->add(4, 2).wait();
    ASSERT_TRUE(retried.has_value()) << retried.error().message();
    EXPECT_EQ(retried.value(), 42);

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

    std::uint64_t nextId = 0;
    auto request         = BinaryRpcBackend::encodeRequest(server->sink, true, nextId, 42);
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
    EXPECT_EQ(method_info.value(), "array<string> rpc.get_method_list()");

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
