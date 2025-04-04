#include <ilias/ilias.hpp>
#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <iostream>

#include <gtest/gtest.h>

// #include <gtest/gtest.h>
#include "nekoproto/jsonrpc/jsonrpc.hpp"

NEKO_USE_NAMESPACE

struct Copytest {
    Copytest() { std::cout << "construct" << std::endl; }
    Copytest(int tt) : a(tt) { std::cout << "construct" << std::endl; }
    Copytest(const Copytest& other) : a(other.a) { std::cout << "copy construct" << std::endl; }
    Copytest(Copytest&& other) : a(other.a) { std::cout << "move construct" << std::endl; }
    Copytest& operator=(const Copytest& other) {
        a = other.a;
        std::cout << "copy assign" << std::endl;
        return *this;
    }
    Copytest& operator=(Copytest&& other) {
        a = other.a;
        std::cout << "move assign" << std::endl;
        return *this;
    }
    ~Copytest() { std::cout << "destruct" << std::endl; }

    int a;

    NEKO_SERIALIZER(a)
};

class Protocol {
public:
    RpcMethod<int(int, int), "test1"> test1;
    RpcMethod<void(int, int), "test2"> test2;
    RpcMethod<void(), "test3"> test3;
    RpcMethod<int(), "test4"> test4;
    RpcMethod<std::string(std::string, double), "test5"> test5;
    RpcMethod<std::string(std::tuple<int, double, bool>), "test6"> test6;
    RpcMethod<std::string(), "test7"> test7;
    RpcMethod<int(std::nullptr_t), "test8"> test8;
    RpcMethod<std::vector<int>(std::vector<bool>), "test9"> test9;
    RpcMethod<Copytest(int tt), "test10"> test10;
};

class JsonRpcTest : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}

    static ILIAS_NAMESPACE::PlatformContext* gContext;
};

TEST_F(JsonRpcTest, BindAndCall) {
    JsonRpcServer<Protocol> server{*gContext};
    JsonRpcClient<Protocol> client{*gContext};

    ilias_wait server.start("tcp://127.0.0.1:12335");
    ilias_wait client.connect("tcp://127.0.0.1:12335");

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> { co_return a1 + b1; };
    server->test2 = [](int a1, int b1) { std::cout << a1 << " " << b1 << std::endl; };
    server->test3 = []() { std::cout << "this is test3!" << std::endl; };
    server->test4 = []() { return 114514; };
    server->test5 = [](std::string a1, double b1) -> std::string { return a1 + std::to_string(b1); };
    server->test6 = [](std::tuple<int, double, bool> a1) -> std::string {
        return std::to_string(std::get<0>(a1)) + std::to_string(std::get<1>(a1)) + std::to_string(std::get<2>(a1));
    };
    server->test8 = [](std::nullptr_t /*unused*/) -> ilias::IoTask<int> { co_return 114514; };
    server->test9 = [](std::vector<bool> a1) -> std::vector<int> {
        std::vector<int> res;
        int dd = 1;
        for (auto&& val : a1) {
            res.push_back(val ? dd : 0);
            dd <<= 1;
        }
        return res;
    };
    server->test10 = [](int tt) -> Copytest { return Copytest{tt + 114514}; };

    {
        auto res = (ilias_wait client->test1(1, 2));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 3);
    }
    EXPECT_TRUE(ilias_wait client->test2(1, 2));
    EXPECT_TRUE(ilias_wait client->test3());
    {
        auto res = ilias_wait client->test4();
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (ilias_wait client->test5("hello", 3.14));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "hello" + std::to_string(3.14));
    }
    {
        auto res = (ilias_wait client->test6(std::make_tuple(1, 3.14, true)));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), std::to_string(1) + std::to_string(3.14) + std::to_string(true));
    }
    {
        auto res = (ilias_wait client->test7());
        EXPECT_EQ(res.error(), JsonRpcError::MethodNotBind);
    }
    {
        auto res = (ilias_wait client->test8(nullptr));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (ilias_wait client->test9({true, false, true, true, false}));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), (std::vector<int>{1, 0, 4, 8, 0}));
    }
    {
        Copytest tt;
        tt.a = 114514;
        EXPECT_EQ((ilias_wait client->test10(1)).value().a, 114515);
    }
    client.close();
    server.close();
}

TEST_F(JsonRpcTest, BindAndCallUdp) {
    JsonRpcServer<Protocol> server{*gContext};
    JsonRpcClient<Protocol> client{*gContext};

    ilias_wait server.start("udp://127.0.0.1:12335-127.0.0.1:12336");
    ilias_wait client.connect("udp://127.0.0.1:12336-127.0.0.1:12335");

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> { co_return a1 + b1; };
    server->test2 = [](int a1, int b1) { std::cout << a1 << " " << b1 << std::endl; };
    server->test3 = []() { std::cout << "this is test3!" << std::endl; };
    server->test4 = []() { return 114514; };
    server->test5 = [](std::string a1, double b1) -> std::string { return a1 + std::to_string(b1); };
    server->test6 = [](std::tuple<int, double, bool> a1) -> std::string {
        return std::to_string(std::get<0>(a1)) + std::to_string(std::get<1>(a1)) + std::to_string(std::get<2>(a1));
    };
    server->test8 = [](std::nullptr_t /*unused*/) -> ilias::IoTask<int> { co_return 114514; };
    server->test9 = [](std::vector<bool> a1) -> std::vector<int> {
        std::vector<int> res;
        int dd = 1;
        for (auto&& val : a1) {
            res.push_back(val ? dd : 0);
            dd <<= 1;
        }
        return res;
    };
    server->test10 = [](int tt) -> Copytest { return Copytest{tt + 114514}; };

    {
        auto res = (ilias_wait client->test1(1, 2));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 3);
    }
    EXPECT_TRUE(ilias_wait client->test2(1, 2));
    EXPECT_TRUE(ilias_wait client->test3());
    {
        auto res = ilias_wait client->test4();
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (ilias_wait client->test5("hello", 3.14));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "hello" + std::to_string(3.14));
    }
    {
        auto res = (ilias_wait client->test6(std::make_tuple(1, 3.14, true)));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), std::to_string(1) + std::to_string(3.14) + std::to_string(true));
    }
    {
        auto res = (ilias_wait client->test7());
        EXPECT_EQ(res.error(), JsonRpcError::MethodNotBind);
    }
    {
        auto res = (ilias_wait client->test8(nullptr));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (ilias_wait client->test9({true, false, true, true, false}));
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), (std::vector<int>{1, 0, 4, 8, 0}));
    }
    {
        Copytest tt;
        tt.a = 114514;
        EXPECT_EQ((ilias_wait client->test10(1)).value().a, 114515);
    }
    client.close();
    server.close();
}

TEST_F(JsonRpcTest, Batch) {
    JsonRpcServer<Protocol> server{*gContext};

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> {
        co_await ILIAS_NAMESPACE::sleep(std::chrono::milliseconds(10));
        co_return a1 + b1;
    };
    server->test2 = [](int a1, int b1) { std::cout << a1 << " " << b1 << std::endl; };
    server->test3 = []() { std::cout << "this is test3!" << std::endl; };

    auto ret = ilias_wait server.callMethod(R"([
            {"jsonrpc":"2.0", "method":"test1", "params":[1,2], "id":1},
            {"jsonrpc":"2.0", "method":"test2", "params":[1,2], "id":2},
            {"jsonrpc":"2.0", "method":"test3", "params":[], "id":3}
            ])");
    std::cout << std::string_view{ret.data(), ret.size()} << std::endl;
}

TEST_F(JsonRpcTest, Notification) {
    JsonRpcServer<Protocol> server{*gContext};
    JsonRpcClient<Protocol> client{*gContext};

    ilias_wait server.start("udp://127.0.0.1:12335-127.0.0.1:12336");
    ilias_wait client.connect("udp://127.0.0.1:12336-127.0.0.1:12335");

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> { co_return a1 + b1; };
    server->test2 = [](int a1, int b1) { std::cout << a1 << " " << b1 << std::endl; };
    server->test3 = []() { std::cout << "this is test3!" << std::endl; };
    server->test4 = []() {
        std::cout << "this is test4!" << std::endl;
        return 0;
    };

    ilias_wait client->test1.notification(1, 2);
    ilias_wait client->test2.notification(1, 2);
    ilias_wait client->test3.notification();
    ilias_wait client->test4.notification();
}

ILIAS_NAMESPACE::PlatformContext* JsonRpcTest::gContext = nullptr;

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    ILIAS_NAMESPACE::PlatformContext context;
    JsonRpcTest::gContext = &context;

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}