#include <ilias/io/duplex.hpp>
#include <ilias/platform.hpp>
#include <iostream>

#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "ilias/defines.hpp"
#include "ilias/io/error.hpp"
#include "ilias/result.hpp" // IWYU pragma: export
#include "nekoproto/global/global.hpp"
#include "nekoproto/jsonrpc/backend.hpp"
#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"
#include "nekoproto/serialization/reflection.hpp"

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

struct MXXParams {
    int param1;
    std::string param2;
    std::vector<int> param3;

    NEKO_SERIALIZER(param1, param2, param3)
};

struct UnsupportedRpcValue {
    UnsupportedRpcValue() {}
};

static_assert(traits::Serializable<int>);
static_assert(traits::Serializable<std::string>);
static_assert(traits::Serializable<std::vector<int>>);
static_assert(traits::Serializable<MXXParams>);
static_assert(!traits::Serializable<UnsupportedRpcValue>);
static_assert(traits::IsSerializable<void>::value);

int add(int aa, int bb) { return aa + bb; }

struct MyFunc {
    static int execute(int aa, int bb) { return aa + bb; }
};

ilias::IoTask<std::string> testxx(MXXParams params) {
    std::string ret = std::to_string(params.param1) + params.param2;
    for (auto& ii : params.param3) {
        ret += "," + std::to_string(ii);
    }
    co_return ret;
}

ilias::Task<std::string> failed_testxx(MXXParams params) {
    std::string ret = std::to_string(params.param1) + params.param2;
    for (auto& ii : params.param3) {
        ret += "," + std::to_string(ii);
    }
    co_return ret;
}

ilias::IoTask<std::string> test_optional_params(std::optional<MXXParams> params) {
    if (params) {
        std::string ret = std::to_string(params->param1) + params->param2;
        for (auto& ii : params->param3) {
            ret += "," + std::to_string(ii);
        }
        co_return ret;
    }
    co_return "no params";
};

static_assert(NekoProto::detail::func_nameof<add> == "add");
static_assert(NekoProto::detail::func_nameof<MyFunc::execute> == "execute");
static_assert(NekoProto::detail::func_nameof<testxx> == "testxx");
static_assert(NekoProto::detail::func_nameof<test_optional_params> == "test_optional_params");

extern int test1(int, int);
struct Protocol {
    RpcMethod<int(int, int), "test1", "num1", "num2"> test1;
    RpcMethod<void(int, int), "test2", "num1", "num2"> test2;
    RpcMethod<void(), "test3"> test3;
    RpcMethod<int(), "test4"> test4;
    RpcMethod<std::string(std::string, double), "test5"> test5;
    RpcMethod<std::string(std::tuple<int, double, bool>), "test6"> test6;
    RpcMethod<std::string(), "test7"> test7;
    RpcMethod<int(std::nullptr_t), "test8"> test8;
    RpcMethod<std::vector<int>(std::vector<bool>), "test9"> test9;
    RpcMethod<Copytest(int tt), "test10"> test10;
    RpcMethod<std::string(MXXParams), "testxx"> test11;
    RpcMethodF<add, "a", "b"> funcAdd;
    RpcMethodF<MyFunc::execute, "a", "b"> funcAdd2;
};

struct NestedLeaf {
    RpcMethod<int(int), "sum", "value"> sum;
    NEKO_SERIALIZER(sum)
};

struct NestedBranch {
    NestedLeaf b;
    NEKO_SERIALIZER(b)
};

struct NestedApi {
    NestedBranch a;
    RpcMethod<int(int), "root", "value"> root;
    NEKO_SERIALIZER(a, root)
};

struct CommonApi {
    RpcMethod<std::string(), "version"> version;
    NEKO_SERIALIZER(version)
};

struct NoPrefixApi {
    CommonApi common;
    NEKO_SERIALIZER(make_tags<rpc_no_prefix_tag>(common))
};

class JsonRpcTest : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}

    static ilias::PlatformContext* gContext;
};

template <typename Server, typename Client>
static void connectWithDuplex(Server& server, Client& client) {
    auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);
    server.addEndpoint(std::move(serverStream));
    client.setEndpoint(std::move(clientStream));
}

TEST_F(JsonRpcTest, BindAndCall) {
    JsonRpcServer<Protocol> server{*gContext};
    JsonRpcClient<Protocol> client{*gContext};
    connectWithDuplex(server, client);

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> { co_return a1 + b1; };
    server->test2 = [](int a1, int b1) -> ilias::IoTask<void> {
        std::cout << a1 << " " << b1 << std::endl;
        co_return {};
    };
    server->test3 = []() -> ilias::IoTask<void> {
        std::cout << "this is test3!" << std::endl;
        co_return {};
    };
    server->test4 = []() -> ilias::IoTask<int> { co_return 114514; };
    server->test5 = [](std::string a1, double b1) -> ilias::IoTask<std::string> { co_return a1 + std::to_string(b1); };
    server->test6 = [](std::tuple<int, double, bool> a1) -> ilias::IoTask<std::string> {
        co_return std::to_string(std::get<0>(a1)) + std::to_string(std::get<1>(a1)) + std::to_string(std::get<2>(a1));
    };
    server->test8 = [](std::nullptr_t /*unused*/) -> ilias::IoTask<int> { co_return 114514; };
    server->test9 = [](std::vector<bool> a1) -> ilias::IoTask<std::vector<int>> {
        std::vector<int> res;
        int dd = 1;
        for (auto&& val : a1) {
            res.push_back(val ? dd : 0);
            dd <<= 1;
        }
        co_return res;
    };
    server->test10 = [](int tt) -> ilias::IoTask<Copytest> { co_return Copytest{tt + 114514}; };
    server.bindMethod("test11", traits::FunctionT<ilias::IoTask<std::string>(int)>(
                                    [](int aa) -> ilias::IoTask<std::string> { co_return std::to_string(aa); }));
    server.bindMethod<add>();
    server.bindMethod<MyFunc::execute>();
    server.bindMethod<testxx>();
    server.bindMethod<test_optional_params>();
    // IoTask<T> can return with error, but Task<T> can not, so we can not bind it, if you want to bind coroutine
    // method, please use IoTask<T>. server.bindMethod<failed_testxx>(); // this can not compile
    {
        auto res = (client->test1(1, 2).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 3);
    }
    EXPECT_TRUE(client->test2(1, 2).wait());
    EXPECT_TRUE(client->test3().wait());
    {
        auto res = client->test4().wait();
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (client->test5("hello", 3.14).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "hello" + std::to_string(3.14));
    }
    {
        auto res = (client->test6(std::make_tuple(1, 3.14, true)).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), std::to_string(1) + std::to_string(3.14) + std::to_string(true));
    }
    {
        auto res = (client->test7().wait());
        EXPECT_EQ(res.error(), JsonRpcError::MethodNotBind);
    }
    {
        auto res = (client->test8(nullptr).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (client->test9({true, false, true, true, false}).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), (std::vector<int>{1, 0, 4, 8, 0}));
    }
    {
        Copytest tt;
        tt.a = 114514;
        EXPECT_EQ((client->test10(1)).wait().value().a, 114515);
    }
    {
        auto res = (client.callRemote<std::string>("test11", 114514).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "114514");
    }
    {
        auto res = (client.callRemote<std::string>("test12", 114514, 114514).wait());
        EXPECT_EQ(res.error().value(), (int)JsonRpcError::MethodNotFound);
    }
    {
        auto res = (client.callRemote<add>(114514, 114514).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 229028);
    }
    {
        auto res = (client.callRemote<MyFunc::execute>(114514, 114514).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 229028);
    }
    {
        MXXParams params;
        params.param1 = 114514;
        params.param2 = "hello";
        params.param3 = std::vector<int>{1, 2, 3, 4, 5};
        auto res      = (client.callRemote<testxx>(params).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "114514hello,1,2,3,4,5");
    }
    {
        auto res = (client.callRemote<test_optional_params>(std::optional<MXXParams>{}).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "no params");
    }
    client.close();
    server.close();
}

TEST_F(JsonRpcTest, BindAndCallDuplex) {
    JsonRpcServer<Protocol> server{*gContext};
    JsonRpcClient<Protocol> client{*gContext};

    connectWithDuplex(server, client);

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> { co_return a1 + b1; };
    server->test2 = [](int a1, int b1) -> ilias::IoTask<void> {
        std::cout << a1 << " " << b1 << std::endl;
        co_return {};
    };
    server->test3 = []() -> ilias::IoTask<void> {
        std::cout << "this is test3!" << std::endl;
        co_return {};
    };
    server->test4 = []() -> ilias::IoTask<int> { co_return 114514; };
    server->test5 = [](std::string a1, double b1) -> ilias::IoTask<std::string> { co_return a1 + std::to_string(b1); };
    server->test6 = [](std::tuple<int, double, bool> a1) -> ilias::IoTask<std::string> {
        co_return std::to_string(std::get<0>(a1)) + std::to_string(std::get<1>(a1)) + std::to_string(std::get<2>(a1));
    };
    server->test8 = [](std::nullptr_t /*unused*/) -> ilias::IoTask<int> { co_return 114514; };
    server->test9 = [](std::vector<bool> a1) -> ilias::IoTask<std::vector<int>> {
        std::vector<int> res;
        int dd = 1;
        for (auto&& val : a1) {
            res.push_back(val ? dd : 0);
            dd <<= 1;
        }
        co_return res;
    };
    server->test10 = [](int tt) -> ilias::IoTask<Copytest> { co_return Copytest{tt + 114514}; };
    server->test11 = [](MXXParams params) -> ilias::IoTask<std::string> {
        co_return std::to_string(params.param1) + params.param2 + std::to_string(params.param3.size());
    };
    server.bindMethod("test11", traits::FunctionT<ilias::IoTask<std::string>(int)>(
                                    [](int aa) -> ilias::IoTask<std::string> { co_return std::to_string(aa); }));

    {
        auto res = (client.callRemote<test1>(1, 2).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 3);
    }

    {
        auto res = (client.callRemote<int>("test1", 1, 2).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 3);
    }

    {
        auto res = (client.callRemote<int, "num1", "num2">("test1", 1, 2).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 3);
    }

    {
        auto res = (client.callRemote<int, "num2", "num3">("test1", 1, 2).wait());
        EXPECT_FALSE(res.has_value());
    }

    EXPECT_TRUE(client->test2(1, 2).wait());
    EXPECT_TRUE(client->test3().wait());
    {
        auto res = client->test4().wait();
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (client->test5("hello", 3.14).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "hello" + std::to_string(3.14));
    }
    {
        auto res = (client->test6(std::make_tuple(1, 3.14, true)).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), std::to_string(1) + std::to_string(3.14) + std::to_string(true));
    }
    {
        auto res = (client->test7().wait());
        EXPECT_EQ(res.error(), JsonRpcError::MethodNotBind);
    }
    {
        auto res = (client->test8(nullptr).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), 114514);
    }
    {
        auto res = (client->test9({true, false, true, true, false}).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), (std::vector<int>{1, 0, 4, 8, 0}));
    }
    {
        Copytest tt;
        tt.a = 114514;
        EXPECT_EQ((client->test10(1).wait()).value().a, 114515);
    }
    {
        MXXParams params;
        params.param1 = 114514;
        params.param2 = "hello";
        params.param3 = {1, 2, 3, 4};
        auto res      = (client->test11(params).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "114514hello4");
    }
    {
        auto res = (client.callRemote<std::string>("test11", 114514).wait());
        EXPECT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), "114514");
    }
    client.close();
    server.close();
}

TEST_F(JsonRpcTest, Batch) {
    JsonRpcServer<Protocol> server{*gContext};

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> {
        // some bug in ilias::sleep
        // co_await ilias::sleep(std::chrono::milliseconds(10));
        co_return a1 + b1;
    };
    server->test2 = [](int a1, int b1) -> ilias::IoTask<void> {
        std::cout << a1 << " " << b1 << std::endl;
        co_return {};
    };
    server->test3 = []() -> ilias::IoTask<void> {
        std::cout << "this is test3!" << std::endl;
        co_return {};
    };

    auto ret = server
                   .callMethod(R"([
            {"jsonrpc":"2.0", "method":"test1", "params":[1,2], "id":1},
            {"jsonrpc":"2.0", "method":"test2", "params":[1,2], "id":2},
            {"jsonrpc":"2.0", "method":"test3", "params":[], "id":3}
            ])")
                   .wait();
    std::cout << std::string_view{ret.data(), ret.size()} << std::endl;
}

TEST_F(JsonRpcTest, NestedApiNames) {
    JsonRpcServer<NestedApi> server{*gContext};

    EXPECT_EQ(server->root.name(), "root");
    EXPECT_EQ(server->a.b.sum.name(), "a.b.sum");

    server->a.b.sum = [](int value) -> ilias::IoTask<int> { co_return value + 1; };
    auto ret = server.callMethod(R"({"jsonrpc":"2.0","method":"a.b.sum","params":[41],"id":1})").wait();
    auto text = std::string_view{ret.data(), ret.size()};
    EXPECT_NE(text.find(R"("result":42)"), std::string_view::npos);
}

TEST_F(JsonRpcTest, NoPrefixTagKeepsCppPath) {
    JsonRpcServer<NoPrefixApi> server{*gContext};

    EXPECT_EQ(server->common.version.name(), "version");

    server->common.version = []() -> ilias::IoTask<std::string> { co_return "1.0"; };
    auto ret = server.callMethod(R"({"jsonrpc":"2.0","method":"version","params":[],"id":1})").wait();
    auto text = std::string_view{ret.data(), ret.size()};
    EXPECT_NE(text.find(R"("result":"1.0")"), std::string_view::npos);
}

TEST_F(JsonRpcTest, Notification) {
    JsonRpcServer<Protocol> server{*gContext};
    JsonRpcClient<Protocol> client{*gContext};

    connectWithDuplex(server, client);

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> { co_return a1 + b1; };
    server->test2 = [](int a1, int b1) -> ilias::IoTask<void> {
        std::cout << a1 << " " << b1 << std::endl;
        co_return {};
    };
    server->test3 = []() -> ilias::IoTask<void> {
        std::cout << "this is test3!" << std::endl;
        co_return {};
    };
    server->test4 = []() -> ilias::IoTask<int> {
        std::cout << "this is test4!" << std::endl;
        co_return 0;
    };
    server.bindMethod("test11", traits::FunctionT<ilias::IoTask<void>(int)>([](int aa) -> ilias::IoTask<void> {
                          std::cout << "test11 called with " << aa << std::endl;
                          co_return {};
                      }));

    client->test1.notification(1, 2).wait();
    client->test2.notification(1, 2).wait();
    client->test3.notification().wait();
    client->test4.notification().wait();
    client.notifyRemote<void>("test11", 114514).wait();

    ilias::sleep(std::chrono::milliseconds(100)).wait();

    client.close();
    server.close();
}

TEST_F(JsonRpcTest, Basic) {
    JsonRpcServer<Protocol> server{*gContext};
    JsonRpcClient<Protocol> client{*gContext};

    connectWithDuplex(server, client);

    server->test1 = [](int a1, int b1) -> ilias::IoTask<int> { co_return a1 + b1; };
    server.bindMethod<"aa", "bb">("test11",
                                  traits::FunctionT<ilias::IoTask<int>(int, int)>(
                                      [](int aa, int bb) -> ilias::IoTask<int> { co_return (aa * 10) + bb; }));
    auto methodDatas = server.methodDatas();

    auto methods = client->rpc.getMethodList().wait();
    ASSERT_TRUE(methods.has_value());
    EXPECT_EQ(methods.value().size(), 14 + JsonRpcServer<>::BuiltinMethodsCount);
    for (size_t idx = 0; idx < methods.value().size(); idx ++) {
        EXPECT_EQ(methods.value()[idx], methodDatas[idx].name);
    }
    
    methods = client->rpc.getBindedMethodList().wait();
    ASSERT_TRUE(methods.has_value());
    EXPECT_EQ(methods.value().size(), 4 + JsonRpcServer<>::BuiltinMethodsCount);
    int idx     = 0;
    for (auto method : methodDatas) {
        if (method.isBind) {
            EXPECT_EQ(methods.value()[idx++], method.name);
        }
    }

    auto methodInfo = client->rpc.getMethodInfo("test1").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "int test1(int num1, int num2)");

    methodInfo = client->rpc.getMethodInfo("test2").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "void test2(int num1, int num2)");

    methodInfo = client->rpc.getMethodInfo("test3").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "void test3()");

    methodInfo = client->rpc.getMethodInfo("test4").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "int test4()");

    methodInfo = client->rpc.getMethodInfo("test5").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "std::string test5(std::string, double)");

    methodInfo = client->rpc.getMethodInfo("test6").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "std::string test6(std::tuple<int, double, bool>)");

    methodInfo = client->rpc.getMethodInfo("test7").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "std::string test7()");

    methodInfo = client->rpc.getMethodInfo("test8").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "int test8(std::nullptr_t)");

    methodInfo = client->rpc.getMethodInfo("test9").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "std::vector<int> test9(std::vector<bool>)");

    methodInfo = client->rpc.getMethodInfo("test10").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "Copytest test10(int)");

    methodInfo = client->rpc.getMethodInfo("test11").wait();
    ASSERT_TRUE(methodInfo.has_value());
    EXPECT_EQ(methodInfo.value(), "int test11(int aa, int bb)");

    auto result = client.callRemote<int>("test11", 1, 2).wait();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 12);

    result = client.callRemote<int, "bb", "aa">("test11", 1, 2).wait();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 21);

    result = client.callRemote<int, "aa", "bb">("test11", 1, 2).wait();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 12);

    result = client.callRemote<add, "b", "a">(1, 2).wait();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3);

    result = client.callRemote<MyFunc::execute, "a", "b">(1, 2).wait();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3);

    auto methodInfoList = client->rpc.getMethodInfoList().wait();
    ASSERT_TRUE(methodInfoList.has_value());
    EXPECT_EQ(methodInfoList.value().size(), 14 + JsonRpcServer<>::BuiltinMethodsCount);

    server.bindMethod<>("test112", traits::FunctionT<ilias::IoTask<void>()>([]() -> ilias::IoTask<void> {
                            std::cout << "test112: sleep 5s start" << std::endl;
                            co_await ilias::sleep(std::chrono::milliseconds(5000));
                            std::cout << "test112: sleep 5s end" << std::endl;
                            co_return {};
                        }));
    [&]() -> ilias::Task<void> {
        co_await ilias::whenAll(
            [&server]() -> ilias::Task<void> {
                std::cout << "cancel test112: sleep 1s start" << std::endl;
                co_await ilias::sleep(std::chrono::milliseconds(1000));
                server.cancelAll();
                co_return;
            }(),
            client.callRemote<void>("test112"));
    }()
                 .wait();

    client.close();
    server.close();
}

ilias::PlatformContext* JsonRpcTest::gContext = nullptr;

#define CUSTOM_MAIN                                                                                                    \
    ilias::PlatformContext context;                                                                          \
    context.install();                                                                                                 \
    JsonRpcTest::gContext = &context;

#include "../common/common_main.cpp.in" // IWYU pragma: export
