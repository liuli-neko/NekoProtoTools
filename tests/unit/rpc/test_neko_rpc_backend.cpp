#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <ilias/io/duplex.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>

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

auto contains(const std::vector<std::string>& values, std::string_view expected) -> bool {
    return std::any_of(values.begin(), values.end(), [expected](const std::string& value) {
        return value == expected;
    });
}

} // namespace

TEST(NekoRpcBackend, CallsRegisteredMethodThroughBinaryFrame) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};
    RpcClient<BinaryRpcBackend, BinaryRpcTestApi> client{context};
    EXPECT_FALSE(client.isConnected());

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> {
        co_return lhs + rhs;
    };

    std::uint64_t nextId = 0;
    auto request = BinaryRpcBackend::encodeRequest(server->add, false, nextId, 20, 22);
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
    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);

    server.addEndpoint(std::move(serverStream));
    client.setEndpoint(std::move(clientStream));

    server->add = [](int lhs, int rhs) -> ilias::IoTask<int> {
        co_return lhs + rhs;
    };

    auto result = client->add(20, 22).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), 42);

    auto result2 = client->rpc.getMethodInfoList().wait();
    ASSERT_TRUE(result2.has_value()) << result2.error().message();
    EXPECT_EQ(result2.value().size(), 2 + (RpcServer<BinaryRpcBackend>::BuiltinMethodsCount));
    for (auto &method : result2.value()) {
        std::cout << "method: " << method << std::endl;
    }

    client.close();
    server.close();
}

TEST(NekoRpcBackend, NotificationDoesNotReturnResponse) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};

    bool called = false;
    server->sink = [&called](int value) -> ilias::IoTask<void> {
        called = value == 42;
        co_return {};
    };

    std::uint64_t nextId = 0;
    auto request = BinaryRpcBackend::encodeRequest(server->sink, true, nextId, 42);
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
    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);

    server.addEndpoint(std::move(serverStream));
    client.setEndpoint(std::move(clientStream));

    auto methods = client->rpc.getMethodList().wait();
    ASSERT_TRUE(methods.has_value()) << methods.error().message();
    EXPECT_TRUE(contains(methods.value(), "rpc.get_method_list"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_method_info"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_method_info_list"));
    EXPECT_TRUE(contains(methods.value(), "rpc.get_bind_method_list"));

    auto methodInfo = client->rpc.getMethodInfo("rpc.get_method_list").wait();
    ASSERT_TRUE(methodInfo.has_value()) << methodInfo.error().message();
    EXPECT_EQ(methodInfo.value(), "std::vector<std::string> rpc.get_method_list()");

    auto boundMethods = client->rpc.getBindedMethodList().wait();
    ASSERT_TRUE(boundMethods.has_value()) << boundMethods.error().message();
    EXPECT_TRUE(contains(boundMethods.value(), "rpc.get_method_list"));
    EXPECT_TRUE(contains(boundMethods.value(), "rpc.get_bind_method_list"));

    client.close();
    server.close();
}

TEST(NekoRpcBackend, MethodNotFoundReturnsRpcError) {
    ilias::PlatformContext context;
    context.install();
    RpcServer<BinaryRpcBackend, BinaryRpcTestApi> server{context};

    RpcMethod<int(), "missing"> missing;
    std::uint64_t nextId = 0;
    auto request = BinaryRpcBackend::encodeRequest(missing, false, nextId);
    ASSERT_TRUE(request.has_value()) << request.error().message();

    auto response = server.processMessage(asBytes(request.value().message)).wait();
    ASSERT_FALSE(response.empty());

    auto decoded = BinaryRpcBackend::decodeResponse<decltype(missing)>(asBytes(response), request.value().id);
    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error().value(), static_cast<int>(RpcError::MethodNotFound));
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
