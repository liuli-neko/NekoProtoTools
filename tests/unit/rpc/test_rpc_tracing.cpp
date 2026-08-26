#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <ilias/io/duplex.hpp>
#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>

#include "nekoproto/jsonrpc/backend.hpp"
#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include "nekoproto/rpc/console.hpp"
#include "nekoproto/rpc/rpc.hpp"
#include "nekoproto/rpc/tracing.hpp"

using namespace nekoproto;
using namespace std::chrono_literals;

class RpcTracingTest : public ::testing::Test {
protected:
    void SetUp() override {
        mContext = std::make_unique<ilias::PlatformContext>();
        mContext->install();
        RpcTraceRegistry::instance().clear();
    }

    void TearDown() override {
        RpcTraceRegistry::instance().clear();
        mContext.reset();
    }

    std::unique_ptr<ilias::PlatformContext> mContext;
};

TEST_F(RpcTracingTest, TrackLifecycleAndMetrics) {
    JsonRpcServer<> server{*mContext};
    JsonRpcClient<> client{*mContext};

    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    server.addEndpoint(std::move(serverStream), RpcPeerInfo{.id = "test-client-1", .attributes = {{"role", "tester"}}});
    client.setEndpoint(std::move(clientStream));

    server.bindMethod<"a", "b">("calc.add", traits::FunctionT<int(int, int)>([](int a, int b) {
        return a + b;
    }));

    auto result = client.callRemote<int>("calc.add", 15, 27).wait();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result.value(), 42);

    auto json = RpcTraceRegistry::instance().snapshotJson();
    EXPECT_TRUE(json.find("calc.add") != std::string::npos);
    EXPECT_TRUE(json.find("test-client-1") != std::string::npos);
    EXPECT_TRUE(json.find("Completed") != std::string::npos);
    EXPECT_TRUE(json.find("\"response_bytes\":0") == std::string::npos);
    EXPECT_TRUE(json.find("\"request_bytes\":0") == std::string::npos);

    client.close();
    server.close();
}

TEST_F(RpcTracingTest, TrackFailedMethodNotFound) {
    JsonRpcServer<> server{*mContext};
    JsonRpcClient<> client{*mContext};

    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    server.addEndpoint(std::move(serverStream), RpcPeerInfo{.id = "test-client-2"});
    client.setEndpoint(std::move(clientStream));

    auto result = client.callRemote<int>("unknown.method", 100).wait();
    EXPECT_FALSE(result.has_value());

    auto json = RpcTraceRegistry::instance().snapshotJson();
    EXPECT_TRUE(json.find("unknown.method") != std::string::npos);
    EXPECT_TRUE(json.find("Failed") != std::string::npos || json.find("Method not found") != std::string::npos);

    client.close();
    server.close();
}

TEST_F(RpcTracingTest, TrackTimeoutAndDeadline) {
    JsonRpcBackend::Options options;
    options.request_timeout = 20ms;
    JsonRpcServer<> server{*mContext, options};
    JsonRpcClient<> client{*mContext, options};

    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    server.addEndpoint(std::move(serverStream), RpcPeerInfo{.id = "test-client-3"});
    client.setEndpoint(std::move(clientStream));

    server.bindMethod("slow.op", traits::FunctionT<ilias::IoTask<int>()>([]() -> ilias::IoTask<int> {
        co_await ilias::sleep(200ms);
        co_return 100;
    }));

    auto result = client.callRemote<int>("slow.op").wait();
    EXPECT_FALSE(result.has_value());

    auto json = RpcTraceRegistry::instance().snapshotJson();
    EXPECT_TRUE(json.find("slow.op") != std::string::npos);
    EXPECT_TRUE(json.find("TimedOut") != std::string::npos || json.find("timed_out\":1") != std::string::npos);

    client.close();
    server.close();
}

TEST_F(RpcTracingTest, CancelActiveCallViaRegistry) {
    JsonRpcServer<> server{*mContext};
    JsonRpcClient<> client{*mContext};

    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    server.addEndpoint(std::move(serverStream), RpcPeerInfo{.id = "test-client-4"});
    client.setEndpoint(std::move(clientStream));

    server.bindMethod("long.job", traits::FunctionT<ilias::IoTask<int>()>([]() -> ilias::IoTask<int> {
        co_await ilias::sleep(2s);
        co_return 42;
    }));

    auto handle = ilias::spawn([&client]() -> ilias::Task<ilias::Result<int, std::error_code>> {
        co_return co_await client.callRemote<int>("long.job");
    });

    // Wait for the request to arrive and start executing
    ilias::sleep(50ms).wait();

    // Cancel active call via registry
    bool canceled = RpcTraceRegistry::instance().requestCancelByTraceId(1);
    if (!canceled) {
        canceled = RpcTraceRegistry::instance().requestCancel("0");
    }
    EXPECT_TRUE(canceled);

    auto result = handle.wait();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().has_value());

    auto json = RpcTraceRegistry::instance().snapshotJson();
    EXPECT_TRUE(json.find("long.job") != std::string::npos);
    EXPECT_TRUE(json.find("Canceled") != std::string::npos || json.find("canceled\":1") != std::string::npos);

    client.close();
    server.close();
}

TEST_F(RpcTracingTest, WebUiHttpEndpoints) {
    RpcTracingWebUi webui("127.0.0.1:18099");
    ASSERT_TRUE(webui.install());

    auto clientTask = []() -> ilias::Task<void> {
        auto streamRes = co_await ilias::TcpStream::connect("127.0.0.1:18099");
        EXPECT_TRUE(streamRes.has_value());
        if (!streamRes) co_return;
        auto stream = ilias::BufStream(std::move(*streamRes));

        // Test GET /
        std::string req = "GET / HTTP/1.1\r\nHost: 127.0.0.1:18099\r\n\r\n";
        auto writeRes = co_await stream.writeAll(std::span<const std::byte>{reinterpret_cast<const std::byte*>(req.data()), req.size()});
        EXPECT_TRUE(writeRes.has_value());
        (void)co_await stream.flush();

        std::string line;
        EXPECT_TRUE(co_await stream.readline(line, "\r\n"));
        EXPECT_TRUE(line.find("200 OK") != std::string::npos);

        // Read headers until end
        while (true) {
            std::string header;
            if (!co_await stream.readline(header, "\r\n")) break;
            if (header == "\r\n") break;
        }

        // Test GET /api/calls
        req = "GET /api/calls HTTP/1.1\r\nHost: 127.0.0.1:18099\r\n\r\n";
        writeRes = co_await stream.writeAll(std::span<const std::byte>{reinterpret_cast<const std::byte*>(req.data()), req.size()});
        EXPECT_TRUE(writeRes.has_value());
        (void)co_await stream.flush();

        line.clear();
        EXPECT_TRUE(co_await stream.readline(line, "\r\n"));
        EXPECT_TRUE(line.find("200 OK") != std::string::npos);
        co_return;
    };

    clientTask().wait();
}

#include "../common/common_main.cpp.in"

