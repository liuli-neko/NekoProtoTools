#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <map>
#include <memory>
#include <limits>
#include <random>
#include <span>
#include <stop_token>
#include <stdexcept>
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
    static auto decompress(std::span<const std::byte> /**/, rpc::NekoRpcCompressionAlgorithm /**/,
                           std::size_t /**/)
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

template <typename Predicate>
auto wait_until(Predicate predicate, std::chrono::milliseconds budget = std::chrono::milliseconds(250)) -> bool {
    using namespace std::chrono_literals;
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        ilias::sleep(1ms).wait();
    }
    return true;
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

TEST(NekoRpcBackend, HandlerExceptionsBecomeInternalErrorResponses) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    server->add = [](int, int) -> ilias::IoTask<int> {
        throw std::runtime_error("handler failure");
        co_return 0;
    };

    std::uint64_t nextId = 0;
    auto request = BinaryRpcBackend::encodeRequest(server->add, false, nextId, 1, 2);
    ASSERT_TRUE(request.has_value());
    auto response = server.processMessage(asBytes(request->message)).wait();
    ASSERT_FALSE(response.empty());
    auto decoded = BinaryRpcBackend::decodeResponse<decltype(server->add)>(asBytes(response), request->id);
    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), make_error_code(RpcError::InternalError));
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

TEST(NekoRpcBackend, ContextAwareBindingExposesCurrentInvocationAndProviderPeerInfo) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.request_timeout = 1s;
    RpcServer<BinaryRpcBackend> server{context, options};
    RpcClient<BinaryRpcBackend> client{context, options};

    bool observed = false;
    server.bindMethodWithContext<"lhs", "rhs">(
        "context.add",
        traits::FunctionT<ilias::IoTask<int>(const RpcRequestContext&, int, int)>(
            [&observed](const RpcRequestContext& requestContext, int lhs, int rhs) -> ilias::IoTask<int> {
                observed = requestContext.method() == "context.add" && !requestContext.requestId().empty() &&
                           requestContext.deadline().has_value() && requestContext.remaining().has_value() &&
                           requestContext.remaining()->count() > 0 && requestContext.cancellationToken().stop_possible() &&
                           requestContext.peer().id == "authenticated-alice" &&
                           requestContext.peer().attributes.contains("tenant") &&
                           requestContext.peer().attributes.at("tenant") == "blue";
                co_return lhs + rhs;
            }));

    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    server.addEndpoint(std::move(serverStream),
                       RpcPeerInfo{.id = "authenticated-alice", .attributes = {{"tenant", "blue"}}});
    client.setEndpoint(std::move(clientStream));

    auto result = client.callRemote<int>("context.add", 20, 22).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), 42);
    EXPECT_TRUE(observed);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ConnectionTaskQueriesAreCurrentOnlyConnectionScopedAndNotPrivileged) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.max_active_requests_global = 1U;
    options.max_queued_requests_global = 4U;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto active = ilias::spawn(client->add(60, 1));
    ASSERT_TRUE(wait_until([&] { return server.metrics().active == 1U; }));

    auto tasksQuery = ilias::spawn(client->rpc.getConnectionTasks());
    ASSERT_TRUE(wait_until([&] { return server.metrics().queued == 1U; }));
    auto statusQuery = ilias::spawn(client->rpc.getConnectionStatus());
    ASSERT_TRUE(wait_until([&] { return server.metrics().queued == 2U; }));
    auto later = ilias::spawn(client->add(0, 2));
    ASSERT_TRUE(wait_until([&] { return server.metrics().queued == 3U; }));

    auto tasksResult = tasksQuery.wait();
    ASSERT_TRUE(tasksResult.has_value());
    ASSERT_TRUE(tasksResult->has_value()) << tasksResult->error().message();
    std::size_t queuedCount = 0U;
    ASSERT_GE(tasksResult->value().size(), 2U);
    for (const auto& [requestId, state] : tasksResult->value()) {
        EXPECT_FALSE(requestId.empty());
        EXPECT_TRUE(state == "queued" || state == "running");
        queuedCount += state == "queued" ? 1U : 0U;
    }
    EXPECT_GE(queuedCount, 2U);

    auto statusResult = statusQuery.wait();
    ASSERT_TRUE(statusResult.has_value());
    ASSERT_TRUE(statusResult->has_value()) << statusResult->error().message();
    EXPECT_GE(statusResult->value().at("in_flight"), 1U);
    EXPECT_GE(statusResult->value().at("queued"), 1U);

    auto activeResult = active.wait();
    auto laterResult = later.wait();
    ASSERT_TRUE(activeResult.has_value());
    ASSERT_TRUE(laterResult.has_value());
    ASSERT_TRUE(activeResult->has_value()) << activeResult->error().message();
    ASSERT_TRUE(laterResult->has_value()) << laterResult->error().message();
    EXPECT_EQ(activeResult->value(), 1);
    EXPECT_EQ(laterResult->value(), 2);

    auto emptyStatus = client->rpc.getConnectionStatus().wait();
    auto emptyTasks = client->rpc.getConnectionTasks().wait();
    ASSERT_TRUE(emptyStatus.has_value()) << emptyStatus.error().message();
    ASSERT_TRUE(emptyTasks.has_value()) << emptyTasks.error().message();
    EXPECT_EQ(emptyStatus->at("active"), 0U);
    EXPECT_EQ(emptyStatus->at("queued"), 0U);
    EXPECT_EQ(emptyStatus->at("in_flight"), 0U);
    EXPECT_TRUE(emptyTasks->empty());

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ConnectionTaskQueriesDoNotExposeOtherConnections) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.max_active_requests_global = 2U;
    options.max_queued_requests_global = 2U;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> first{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> second{context, options};
    connect_endpoint(server, first);
    connect_endpoint(server, second);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto otherConnectionCall = ilias::spawn(first->add(60, 7));
    ASSERT_TRUE(wait_until([&] { return server.metrics().active == 1U; }));

    auto status = second->rpc.getConnectionStatus().wait();
    ASSERT_TRUE(status.has_value()) << status.error().message();
    EXPECT_EQ(status->at("active"), 0U);
    EXPECT_EQ(status->at("queued"), 0U);
    EXPECT_EQ(status->at("in_flight"), 0U);

    auto tasks = second->rpc.getConnectionTasks().wait();
    ASSERT_TRUE(tasks.has_value()) << tasks.error().message();
    EXPECT_TRUE(tasks->empty());

    auto result = otherConnectionCall.wait();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value()) << result->error().message();
    EXPECT_EQ(result->value(), 7);

    first.close();
    second.close();
    server.close();
}

TEST(NekoRpcBackend, ConnectionTimeoutSettingHonorsBoundariesAndAppliesOnlyToFutureCalls) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.request_timeout = 200ms;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> first{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> second{context, options};
    connect_endpoint(server, first);
    connect_endpoint(server, second);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto initialPolicy = first->rpc.getExecutionPolicy().wait();
    ASSERT_TRUE(initialPolicy.has_value()) << initialPolicy.error().message();
    EXPECT_EQ(initialPolicy->at("limits.connection_timeout_ns"), "none");
    EXPECT_EQ(initialPolicy->at("limits.connection_timeout_min_inclusive_ns"), "1");
    EXPECT_EQ(initialPolicy->at("limits.connection_timeout_max_exclusive_ns"), "200000000");

    auto equalToServerLimit = first->rpc.setConnectionTimeout(200'000'000U).wait();
    ASSERT_FALSE(equalToServerLimit.has_value());
    EXPECT_EQ(equalToServerLimit.error(), make_error_code(RpcError::InvalidParams));
    auto tooLargeForDuration =
        first->rpc.setConnectionTimeout(std::numeric_limits<std::uint64_t>::max()).wait();
    ASSERT_FALSE(tooLargeForDuration.has_value());
    EXPECT_EQ(tooLargeForDuration.error(), make_error_code(RpcError::InvalidParams));

    auto clearAtZero = first->rpc.setConnectionTimeout(0U).wait();
    ASSERT_TRUE(clearAtZero.has_value()) << clearAtZero.error().message();
    EXPECT_EQ(clearAtZero.value(), 0U);

    auto rightBoundary = first->rpc.setConnectionTimeout(199'999'999U).wait();
    ASSERT_TRUE(rightBoundary.has_value()) << rightBoundary.error().message();
    EXPECT_EQ(rightBoundary.value(), 199'999'999U);
    auto ordinaryCall = first->add(20, 7).wait();
    ASSERT_TRUE(ordinaryCall.has_value()) << ordinaryCall.error().message();
    EXPECT_EQ(ordinaryCall.value(), 7);

    auto admittedBeforeChange = ilias::spawn(first->add(90, 17));
    ASSERT_TRUE(wait_until([&] { return server.metrics().active == 1U; }));
    auto middleValue = first->rpc.setConnectionTimeout(50'000'000U).wait();
    ASSERT_TRUE(middleValue.has_value()) << middleValue.error().message();
    auto admittedBeforeChangeResult = admittedBeforeChange.wait();
    ASSERT_TRUE(admittedBeforeChangeResult.has_value());
    ASSERT_TRUE(admittedBeforeChangeResult->has_value()) << admittedBeforeChangeResult->error().message();
    EXPECT_EQ(admittedBeforeChangeResult->value(), 17);
    auto firstPolicy = first->rpc.getExecutionPolicy().wait();
    auto secondPolicy = second->rpc.getExecutionPolicy().wait();
    ASSERT_TRUE(firstPolicy.has_value()) << firstPolicy.error().message();
    ASSERT_TRUE(secondPolicy.has_value()) << secondPolicy.error().message();
    EXPECT_EQ(firstPolicy->at("limits.connection_timeout_ns"), "50000000");
    EXPECT_EQ(secondPolicy->at("limits.connection_timeout_ns"), "none");

    auto exceedsConnectionTimeout = first->add(100, 8).wait();
    ASSERT_FALSE(exceedsConnectionTimeout.has_value());
    EXPECT_EQ(exceedsConnectionTimeout.error(), make_error_code(RpcError::DeadlineExceeded));

    auto randomOutOfRange = first->rpc.setConnectionTimeout(731'245'987U).wait();
    ASSERT_FALSE(randomOutOfRange.has_value());
    EXPECT_EQ(randomOutOfRange.error(), make_error_code(RpcError::InvalidParams));
    auto leftBoundary = first->rpc.setConnectionTimeout(1U).wait();
    ASSERT_TRUE(leftBoundary.has_value()) << leftBoundary.error().message();
    auto immediateTimeout = first->add(10, 9).wait();
    ASSERT_FALSE(immediateTimeout.has_value());
    EXPECT_EQ(immediateTimeout.error(), make_error_code(RpcError::DeadlineExceeded));

    first.close();
    second.close();
    server.close();
}

TEST(NekoRpcBackend, MultiplexedClientAllowsFastCallToPassSlowCall) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    std::vector<int> completionOrder;
    auto call = [&](int delayMs, int value) -> ilias::IoTask<int> {
        auto result = co_await client->add(delayMs, value);
        if (result) {
            completionOrder.push_back(result.value());
        }
        co_return result;
    };
    auto slow = ilias::spawn(call(50, 1));
    auto fast = ilias::spawn(call(0, 2));
    auto fastResult = fast.wait();
    auto slowResult = slow.wait();

    ASSERT_TRUE(fastResult.has_value());
    ASSERT_TRUE(slowResult.has_value());
    ASSERT_TRUE(fastResult->has_value());
    ASSERT_TRUE(slowResult->has_value());
    EXPECT_EQ(fastResult->value(), 2);
    EXPECT_EQ(slowResult->value(), 1);
    ASSERT_EQ(completionOrder.size(), 2U);
    EXPECT_EQ(completionOrder.front(), 2);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, RemoteCancellationIsIsolatedByConnection) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> first{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> second{context, options};
    connect_endpoint(server, first);
    connect_endpoint(server, second);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto canceledCall = ilias::spawn(first->add(100, 1));
    auto otherCall = ilias::spawn(second->add(30, 2));
    ilias::sleep(10ms).wait();
    auto canceled = first.cancelRemote(1U).wait();
    ASSERT_TRUE(canceled.has_value()) << canceled.error().message();

    auto canceledResult = canceledCall.wait();
    auto otherResult = otherCall.wait();
    ASSERT_TRUE(canceledResult.has_value());
    ASSERT_TRUE(otherResult.has_value());
    ASSERT_FALSE(canceledResult->has_value());
    EXPECT_EQ(canceledResult->error(), make_error_code(ilias::IoError::Canceled));
    ASSERT_TRUE(otherResult->has_value()) << otherResult->error().message();
    EXPECT_EQ(otherResult->value(), 2);
    EXPECT_EQ(server.metrics().canceled, 1U);

    first.close();
    second.close();
    server.close();
}

TEST(NekoRpcBackend, CallTimeoutCleansPendingAndLateResponseCannotMatchNextCall) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    RpcCallOptions timeoutOptions;
    timeoutOptions.timeout = 10ms;
    auto timedOut = client.callRemoteWithOptions(client->add, timeoutOptions, 60, 1).wait();
    ASSERT_FALSE(timedOut.has_value());
    EXPECT_EQ(timedOut.error(), make_error_code(RpcError::DeadlineExceeded));
    EXPECT_EQ(client.metrics().active, 0U);
    EXPECT_EQ(client.metrics().timed_out, 1U);

    // Let the timed-out call's response arrive before issuing a new call. A
    // stale response is observable only as an unknown id and cannot be reused.
    ilias::sleep(80ms).wait();
    auto next = client->add(0, 42).wait();
    ASSERT_TRUE(next.has_value()) << next.error().message();
    EXPECT_EQ(next.value(), 42);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ExpiredDeadlineAndCancellationStopBeforeOrDuringWait) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    std::atomic<unsigned> invocations{0};
    server->add = [&invocations](int delayMs, int value) -> ilias::IoTask<int> {
        invocations.fetch_add(1U, std::memory_order_relaxed);
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    RpcCallOptions expiredOptions;
    expiredOptions.deadline = RpcCallOptions::Clock::now() - 1ms;
    auto expired = client.callRemoteWithOptions(client->add, expiredOptions, 0, 1).wait();
    ASSERT_FALSE(expired.has_value());
    EXPECT_EQ(expired.error(), make_error_code(RpcError::DeadlineExceeded));
    EXPECT_EQ(invocations.load(std::memory_order_relaxed), 0U);

    std::stop_source stopSource;
    RpcCallOptions cancelOptions;
    cancelOptions.cancellation_token = stopSource.get_token();
    auto waiting = ilias::spawn(client.callRemoteWithOptions(client->add, cancelOptions, 80, 2));
    ASSERT_TRUE(wait_until([&] { return client.metrics().active == 1U; }));
    stopSource.request_stop();
    auto canceled = waiting.wait();
    ASSERT_TRUE(canceled.has_value());
    ASSERT_FALSE(canceled->has_value());
    EXPECT_EQ(canceled->error(), make_error_code(ilias::IoError::Canceled));
    EXPECT_EQ(client.metrics().active, 0U);
    EXPECT_EQ(client.metrics().canceled, 1U);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, GlobalActiveAndQueueBudgetsRejectExcessAcrossConnections) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.max_active_requests_global = 1U;
    options.max_queued_requests_global = 1U;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> first{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> second{context, options};
    connect_endpoint(server, first);
    connect_endpoint(server, second);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto active = ilias::spawn(first->add(60, 1));
    ASSERT_TRUE(wait_until([&] { return server.metrics().active == 1U; }));
    auto queued = ilias::spawn(second->add(0, 2));
    ASSERT_TRUE(wait_until([&] { return server.metrics().queued == 1U; }));

    auto rejected = first->add(0, 3).wait();
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error(), make_error_code(RpcError::Overloaded));

    auto activeResult = active.wait();
    auto queuedResult = queued.wait();
    ASSERT_TRUE(activeResult.has_value());
    ASSERT_TRUE(queuedResult.has_value());
    ASSERT_TRUE(activeResult->has_value()) << activeResult->error().message();
    ASSERT_TRUE(queuedResult->has_value()) << queuedResult->error().message();
    EXPECT_EQ(activeResult->value(), 1);
    EXPECT_EQ(queuedResult->value(), 2);

    const auto metrics = server.metrics();
    EXPECT_EQ(metrics.active, 0U);
    EXPECT_EQ(metrics.queued, 0U);
    EXPECT_EQ(metrics.completed, 2U);
    EXPECT_EQ(metrics.rejected, 1U);

    first.close();
    second.close();
    server.close();
}

TEST(NekoRpcBackend, ServerRequestTimeoutReleasesGlobalCapacity) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.max_active_requests_global = 1U;
    options.max_queued_requests_global = 0U;
    options.request_timeout = 10ms;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto timedOut = client->add(60, 1).wait();
    ASSERT_FALSE(timedOut.has_value());
    EXPECT_EQ(timedOut.error(), make_error_code(RpcError::DeadlineExceeded));
    EXPECT_EQ(server.metrics().timed_out, 1U);
    EXPECT_EQ(server.metrics().active, 0U);

    auto next = client->add(0, 2).wait();
    ASSERT_TRUE(next.has_value()) << next.error().message();
    EXPECT_EQ(next.value(), 2);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ServerRequestTimeoutIncludesQueueWaitAndHandlerExecution) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.max_active_requests_global = 1U;
    options.max_queued_requests_global = 1U;
    options.request_timeout = 120ms;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto first = ilias::spawn(client->add(70, 1));
    ASSERT_TRUE(wait_until([&] { return server.metrics().active == 1U; }));
    auto second = ilias::spawn(client->add(80, 2));
    ASSERT_TRUE(wait_until([&] { return server.metrics().queued == 1U; }));

    auto firstResult = first.wait();
    auto secondResult = second.wait();
    ASSERT_TRUE(firstResult.has_value());
    ASSERT_TRUE(secondResult.has_value());
    ASSERT_TRUE(firstResult->has_value()) << firstResult->error().message();
    EXPECT_EQ(firstResult->value(), 1);
    ASSERT_FALSE(secondResult->has_value());
    EXPECT_EQ(secondResult->error(), make_error_code(RpcError::DeadlineExceeded));

    const auto metrics = server.metrics();
    EXPECT_EQ(metrics.active, 0U);
    EXPECT_EQ(metrics.queued, 0U);
    EXPECT_EQ(metrics.completed, 2U);
    EXPECT_EQ(metrics.timed_out, 1U);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ClientPendingBudgetRejectsWithoutDroppingExistingCall) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    options.max_pending_calls = 1U;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto first = ilias::spawn(client->add(40, 1));
    ASSERT_TRUE(wait_until([&] { return client.metrics().active == 1U; }));
    auto rejected = client->add(0, 2).wait();
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error(), make_error_code(ilias::IoError::WouldBlock));
    EXPECT_EQ(client.metrics().rejected, 1U);

    auto firstResult = first.wait();
    ASSERT_TRUE(firstResult.has_value());
    ASSERT_TRUE(firstResult->has_value()) << firstResult->error().message();
    EXPECT_EQ(firstResult->value(), 1);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ClientCloseCompletesAnInflightCallAndUpdatesConnectionState) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    server->add = [](int delayMs, int value) -> ilias::IoTask<int> {
        co_await ilias::sleep(std::chrono::milliseconds(delayMs));
        co_return value;
    };

    auto call = ilias::spawn(client->add(200, 1));
    ASSERT_TRUE(wait_until([&] { return client.metrics().active == 1U; }));
    client.close();
    EXPECT_FALSE(client.isConnected());
    EXPECT_EQ(client.metrics().active, 0U);

    auto result = call.wait();
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->has_value());
    EXPECT_EQ(result->error(), make_error_code(RpcError::ClientNotInit));

    server.close();
}

TEST(NekoRpcBackend, ServerCloseInterruptsAPartialFrameBody) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    server.addEndpoint(std::move(serverStream));

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> { co_return lhs + rhs; };
    std::uint64_t nextId = 0;
    auto request = BinaryRpcBackend::encodeRequest(server->add, false, nextId, 1, 2);
    ASSERT_TRUE(request.has_value());
    const auto headerSize = rpc::NekoRpcFrameCodec::headerSize();
    ASSERT_GT(request->message.size(), headerSize);

    auto header = std::span<const std::byte>{reinterpret_cast<const std::byte*>(request->message.data()), headerSize};
    auto written = ilias::io::writeAll(clientStream, header).wait();
    ASSERT_TRUE(written.has_value()) << written.error().message();
    ASSERT_EQ(written.value(), headerSize);
    // Give the endpoint loop time to consume the header and block on the
    // advertised body. The close assertion below is the public behavior under
    // test; no internal receive state is inspected.
    ilias::sleep(5ms).wait();

    const auto started = std::chrono::steady_clock::now();
    server.close();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_LT(elapsed, 250ms);
    detail::close_stream(clientStream);
}

TEST(NekoRpcBackend, CallOptionsCoverZeroNegativeRandomAndArbitraryValidInputs) {
    using namespace std::chrono_literals;
    ilias::PlatformContext context;
    context.install();
    BinaryRpcBackend::Options options;
    options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
    connect_endpoint(server, client);

    std::atomic<unsigned> invocations{0};
    server->add = [&invocations](int lhs, int rhs) -> ilias::IoTask<int> {
        invocations.fetch_add(1U, std::memory_order_relaxed);
        co_return lhs + rhs;
    };

    const std::chrono::nanoseconds invalidTimeouts[] = {0ns, -1ns, -1s};
    for (const auto invalidTimeout : invalidTimeouts) {
        RpcCallOptions invalid;
        invalid.timeout = invalidTimeout;
        auto result = client.callRemoteWithOptions(client->add, invalid, 1, 2).wait();
        ASSERT_FALSE(result.has_value()) << "timeout=" << invalidTimeout.count();
        EXPECT_EQ(result.error(), make_error_code(RpcError::DeadlineExceeded));
    }
    EXPECT_EQ(invocations.load(std::memory_order_relaxed), 0U);

    // Both controls are legal. The earlier one defines the observable limit.
    RpcCallOptions earlierDeadline;
    earlierDeadline.timeout = 24h;
    earlierDeadline.deadline = RpcCallOptions::Clock::now() - 1ns;
    auto expired = client.callRemoteWithOptions(client->add, earlierDeadline, 3, 4).wait();
    ASSERT_FALSE(expired.has_value());
    EXPECT_EQ(expired.error(), make_error_code(RpcError::DeadlineExceeded));
    EXPECT_EQ(invocations.load(std::memory_order_relaxed), 0U);

    // Fixed-seed random samples exercise ordinary values without making CI
    // failures non-reproducible or relying on implementation-specific ids.
    std::mt19937 generator(0x4E504302U);
    std::uniform_int_distribution<int> values(-100000, 100000);
    RpcCallOptions generous;
    generous.timeout = 24h;
    for (unsigned sample = 0; sample < 32U; ++sample) {
        const int lhs = values(generator);
        const int rhs = values(generator);
        auto result = client.callRemoteWithOptions<int>("add", generous, lhs, rhs).wait();
        ASSERT_TRUE(result.has_value()) << "sample=" << sample << " lhs=" << lhs << " rhs=" << rhs
                                        << " error=" << result.error().message();
        EXPECT_EQ(result.value(), lhs + rhs) << "sample=" << sample;
    }
    EXPECT_EQ(invocations.load(std::memory_order_relaxed), 32U);

    client.close();
    server.close();
}

TEST(NekoRpcBackend, ZeroClientServerAndConnectionCapacitiesRejectWithoutInvokingUserCode) {
    ilias::PlatformContext context;
    context.install();
    std::atomic<unsigned> invocations{0};

    BinaryRpcBackend::Options zeroGlobal;
    zeroGlobal.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    zeroGlobal.max_active_requests_global = 0U;
    zeroGlobal.max_queued_requests_global = std::numeric_limits<std::size_t>::max();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> globalServer{context, zeroGlobal};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> globalClient{context};
    globalServer->add = [&invocations](int lhs, int rhs) -> ilias::IoTask<int> {
        invocations.fetch_add(1U, std::memory_order_relaxed);
        co_return lhs + rhs;
    };
    connect_endpoint(globalServer, globalClient);
    auto globalRejected = globalClient->add(1, 2).wait();
    ASSERT_FALSE(globalRejected.has_value());
    EXPECT_EQ(globalRejected.error(), make_error_code(RpcError::Overloaded));
    EXPECT_EQ(globalServer.metrics().rejected, 1U);
    EXPECT_EQ(invocations.load(std::memory_order_relaxed), 0U);
    globalClient.close();
    globalServer.close();

    BinaryRpcBackend::Options zeroPending;
    zeroPending.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    zeroPending.max_pending_calls = 0U;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> pendingServer{context, zeroPending};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> pendingClient{context, zeroPending};
    pendingServer->add = [&invocations](int lhs, int rhs) -> ilias::IoTask<int> {
        invocations.fetch_add(1U, std::memory_order_relaxed);
        co_return lhs + rhs;
    };
    connect_endpoint(pendingServer, pendingClient);
    auto pendingRejected = pendingClient->add(1, 2).wait();
    ASSERT_FALSE(pendingRejected.has_value());
    EXPECT_EQ(pendingRejected.error(), make_error_code(ilias::IoError::WouldBlock));
    EXPECT_EQ(pendingClient.metrics().rejected, 1U);
    EXPECT_EQ(invocations.load(std::memory_order_relaxed), 0U);
    pendingClient.close();
    pendingServer.close();

    BinaryRpcBackend::Options zeroConnection;
    zeroConnection.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    zeroConnection.max_inflight_requests_per_connection = 0U;
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> connectionServer{context, zeroConnection};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> connectionClient{context, zeroConnection};
    connectionServer->add = [&invocations](int lhs, int rhs) -> ilias::IoTask<int> {
        invocations.fetch_add(1U, std::memory_order_relaxed);
        co_return lhs + rhs;
    };
    connect_endpoint(connectionServer, connectionClient);
    auto connectionRejected = connectionClient->add(1, 2).wait();
    ASSERT_FALSE(connectionRejected.has_value());
    EXPECT_EQ(connectionRejected.error(), make_error_code(RpcError::Overloaded));
    EXPECT_TRUE(connectionClient.isConnected());
    EXPECT_EQ(invocations.load(std::memory_order_relaxed), 0U);
    connectionClient.close();
    connectionServer.close();
}

TEST(NekoRpcBackend, ZeroAndNegativeServerTimeoutsAreImmediateAndReleaseCapacity) {
    using namespace std::chrono_literals;
    for (const auto timeout : {0ns, -1ns}) {
        ilias::PlatformContext context;
        context.install();
        BinaryRpcBackend::Options options;
        options.method_id = BinaryRpcBackend::MethodIdMode::Disable;
        options.max_active_requests_global = 1U;
        options.max_queued_requests_global = 0U;
        options.request_timeout = timeout;
        RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context, options};
        RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context, options};
        connect_endpoint(server, client);

        std::atomic<unsigned> invocations{0};
        server->add = [&invocations](int lhs, int rhs) -> ilias::IoTask<int> {
            invocations.fetch_add(1U, std::memory_order_relaxed);
            co_return lhs + rhs;
        };

        auto result = client->add(1, 2).wait();
        ASSERT_FALSE(result.has_value()) << "timeout=" << timeout.count();
        EXPECT_EQ(result.error(), make_error_code(RpcError::DeadlineExceeded));
        EXPECT_EQ(invocations.load(std::memory_order_relaxed), 0U);
        const auto metrics = server.metrics();
        EXPECT_EQ(metrics.active, 0U);
        EXPECT_EQ(metrics.queued, 0U);
        EXPECT_EQ(metrics.timed_out, 1U);

        client.close();
        server.close();
    }
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

TEST(NekoRpcBackend, MethodIdTableRejectsSparseOversizedAndDuplicateRemoteStateAtomically) {
    rpc::NekoRpcMethodIdTable table(4U);
    ASSERT_TRUE(table.applyRemoteTable(
        {{.id = 0, .name = "add", .signature_hash = 1, .state = rpc::NekoRpcMethodState::Active}}, 1U));
    const auto originalEntries = table.entries();
    const auto originalVersion = table.version();

    EXPECT_FALSE(table.applyRemoteTable(
        {{.id = 3, .name = "sparse", .signature_hash = 2, .state = rpc::NekoRpcMethodState::Active}}, 2U));
    EXPECT_FALSE(table.applyRemoteTable(
        {{.id = 0, .name = "same", .signature_hash = 1, .state = rpc::NekoRpcMethodState::Active},
         {.id = 1, .name = "same", .signature_hash = 2, .state = rpc::NekoRpcMethodState::Active}},
        2U));
    EXPECT_FALSE(table.applyRemoteDelta(
        {{.id = 1, .name = "one", .signature_hash = 2, .state = rpc::NekoRpcMethodState::Active},
         {.id = 1, .name = "duplicate", .signature_hash = 3, .state = rpc::NekoRpcMethodState::Active}},
        2U));
    EXPECT_FALSE(table.applyRemoteDelta(
        {{.id = 1000000U, .name = "huge", .signature_hash = 4, .state = rpc::NekoRpcMethodState::Active}},
        2U));

    EXPECT_EQ(table.version(), originalVersion);
    ASSERT_EQ(table.entries().size(), originalEntries.size());
    EXPECT_EQ(table.entries().front().name, originalEntries.front().name);

    auto invalidWireState = rpc::NekoRpcMethodIdExtension::methodTableValue(
        {{.id = 0, .name = "add", .signature_hash = 1, .state = rpc::NekoRpcMethodState::Active}});
    ASSERT_GT(invalidWireState.size(), 13U);
    invalidWireState[13] = std::byte{2};
    rpc::NekoRpcFrameCodec::ExtensionMapType extensions;
    extensions[rpc::NekoRpcExtensionType::MethodTable] = rpc::NekoRpcExtensionCodec::asBytes(invalidWireState);
    std::vector<rpc::NekoRpcMethodEntry> parsed;
    EXPECT_FALSE(rpc::NekoRpcMethodIdExtension::parseMethodTable(extensions, parsed));
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
                                                             rpc::NekoRpcCompressionAlgorithm::RunLength,
                                                             payload.size());
    ASSERT_TRUE(restored.has_value()) << restored.error().message();
    EXPECT_EQ(restored.value(), payload);
}

TEST(NekoRpcBackend, CompressionCodecRejectsOutputOverBudgetBeforeGrowth) {
    const std::array compressed{std::byte{0xFF}, std::byte{'A'}};
    auto restored = rpc::NekoRpcCompressionCodec::decompress(
        compressed, rpc::NekoRpcCompressionAlgorithm::RunLength, 64U);
    ASSERT_FALSE(restored.has_value());
    EXPECT_EQ(restored.error(), make_error_code(ilias::IoError::MessageTooLarge));
}

TEST(NekoRpcBackend, FrameLimitsApplyToBackendOwnedEncodeAndDirectDecodePaths) {
    BinaryRpcTestApi api;
    api.add.setRemoteName("add");

    auto defaultContext = BinaryRpcBackend::makeClientContext();
    auto defaultSession = BinaryRpcBackend::makeClientPeerSession(defaultContext);
    auto request = BinaryRpcBackend::encodeRequest(defaultContext, defaultSession, api.add, false, 20, 22);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    BinaryRpcBackend::Options limitedOptions;
    limitedOptions.method_id = BinaryRpcBackend::MethodIdMode::Disable;
    limitedOptions.frame_limits.max_payload_bytes = 1U;
    auto limitedClient = BinaryRpcBackend::makeClientContext(limitedOptions);
    auto limitedClientSession = BinaryRpcBackend::makeClientPeerSession(limitedClient);
    auto oversizedEncode =
        BinaryRpcBackend::encodeRequest(limitedClient, limitedClientSession, api.add, false, 20, 22);
    ASSERT_FALSE(oversizedEncode.has_value());
    EXPECT_EQ(oversizedEncode.error(), make_error_code(ilias::IoError::MessageTooLarge));

    std::vector<detail::RpcMethodMetadata> methods{add_metadata("i32 add(i32 a, i32 b)")};
    auto limitedServer = BinaryRpcBackend::makeServerContext(limitedOptions, methods);
    auto limitedServerSession = BinaryRpcBackend::makeServerPeerSession(limitedServer);
    auto validated = BinaryRpcBackend::validateMessage(limitedServer, asBytes(request->message));
    ASSERT_FALSE(validated.has_value());
    EXPECT_EQ(validated.error(), make_error_code(ilias::IoError::MessageTooLarge));
    EXPECT_FALSE(BinaryRpcBackend::decodeIncoming(limitedServer, limitedServerSession, asBytes(request->message)).ok);
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
    EXPECT_TRUE(contains(methods.value(), "rpc.get_execution_policy"));
    EXPECT_TRUE(contains(methods.value(), "rpc.set_connection_timeout"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_connection_status"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_connection_tasks"));

    auto method_info = client->rpc.getMethodInfo("rpc.get_method_list").wait();
    ASSERT_TRUE(method_info.has_value()) << method_info.error().message();

    auto statusMethodInfo = client->rpc.getMethodInfo("rpc.get_connection_status").wait();
    auto tasksMethodInfo = client->rpc.getMethodInfo("rpc.get_connection_tasks").wait();
    ASSERT_TRUE(statusMethodInfo.has_value()) << statusMethodInfo.error().message();
    ASSERT_TRUE(tasksMethodInfo.has_value()) << tasksMethodInfo.error().message();
    EXPECT_NE(statusMethodInfo->find("map<string, u64> rpc.get_connection_status()"), std::string::npos);
    EXPECT_NE(tasksMethodInfo->find("map<string, string> rpc.get_connection_tasks()"), std::string::npos);

    auto bound_methods = client->rpc.getBindedMethodList().wait();
    ASSERT_TRUE(bound_methods.has_value()) << bound_methods.error().message();
    EXPECT_TRUE(contains(bound_methods.value(), "rpc.get_method_list"));
    EXPECT_TRUE(contains(bound_methods.value(), "rpc.get_bind_method_list"));

    auto policy = client->rpc.getExecutionPolicy().wait();
    ASSERT_TRUE(policy.has_value()) << policy.error().message();
    EXPECT_EQ(policy->at("privacy_scope"), "per_client_contract_only");
    EXPECT_EQ(policy->at("deadline.propagation"), "connection_timeout_builtin_and_client_cancel");
    EXPECT_EQ(policy->at("deadline.effective_rule"), "minimum_connection_timeout_and_server_limit");
    EXPECT_EQ(policy->at("deadline.covers"), "queue_and_handler");
    EXPECT_EQ(policy->at("status.tasks"), "current_only_no_history");
    EXPECT_TRUE(policy->contains("limits.max_inflight_requests_per_connection"));
    EXPECT_TRUE(policy->contains("limits.max_frame_bytes"));
    EXPECT_EQ(policy->at("limits.connection_timeout_ns"), "none");
    EXPECT_EQ(policy->at("limits.connection_timeout_max_exclusive_ns"), "unbounded");
    EXPECT_FALSE(policy->contains("limits.max_active_requests_global"));
    EXPECT_FALSE(policy->contains("limits.max_queued_requests_global"));
    EXPECT_FALSE(policy->contains("metrics.active"));

    const auto oneDay = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(24)).count());
    auto longTimeout = client->rpc.setConnectionTimeout(oneDay).wait();
    ASSERT_TRUE(longTimeout.has_value()) << longTimeout.error().message();
    EXPECT_EQ(longTimeout.value(), oneDay);
    auto longTimeoutPolicy = client->rpc.getExecutionPolicy().wait();
    ASSERT_TRUE(longTimeoutPolicy.has_value()) << longTimeoutPolicy.error().message();
    EXPECT_EQ(longTimeoutPolicy->at("limits.connection_timeout_ns"), std::to_string(oneDay));
    auto clearedTimeout = client->rpc.setConnectionTimeout(0U).wait();
    ASSERT_TRUE(clearedTimeout.has_value()) << clearedTimeout.error().message();

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
