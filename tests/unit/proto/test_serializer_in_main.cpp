#include <gtest/gtest.h>

#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/json/simd_json_serializer.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/to_string.hpp"
#include "nekoproto/serialization/types/types.hpp"

NEKO_USE_NAMESPACE

enum TEnum { TEnum_A = 1, TEnum_B = 2, TEnum_C = 3 };

struct StructA {
    int a;
    std::string b;
    bool c;
    double d;
    std::list<int> e;
    std::map<std::string, int> f;
    std::array<int, 5> g;
    TEnum h;
};

struct TestP {
    int a                        = 1;
    std::string b                = "hello";
    bool c                       = true;
    double d                     = 3.14;
    std::list<int> e             = {1, 2, 3, 4, 5};
    std::map<std::string, int> f = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::array<int, 5> g         = {1, 2, 3, 4, 5};
    TEnum h                      = TEnum_A;
    StructA i = {1, "hello", true, 3.14, {1, 2, 3, 4, 5}, {{"a", 1}, {"b", 2}, {"c", 3}}, {1, 2, 3, 4, 5}, TEnum_A};
    std::tuple<int, std::string> j = {1, "hello"};
    std::vector<double> m          = {1.1, 2.2, 3.3, 2.0, 1.0, 0.0, 1.11451555213339};
#if NEKO_CPP_PLUS >= 17
    std::optional<int> k;
    std::variant<int, std::string, double> l;
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l, m)
#else
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, m)
#endif
    NEKO_DECLARE_PROTOCOL(TestP, JsonSerializer)
};

struct UnusedProto {
    int a;
    NEKO_SERIALIZER(a)
    NEKO_DECLARE_PROTOCOL(UnusedProto, JsonSerializer)
};

struct BinaryProto {
    int32_t a     = 1;
    std::string b = "hello";
    uint32_t c    = 3;

    NEKO_SERIALIZER(a, b, c)
    NEKO_DECLARE_PROTOCOL(BinaryProto, BinarySerializer)
};

struct ZTypeTest1 {
    std::unordered_map<int, int> a;
    std::unordered_map<int, std::string> b;
    std::unordered_set<int> c;
    std::unordered_multimap<double, int> d;
    std::unordered_multiset<double> e;
    std::unordered_map<std::string, std::shared_ptr<std::string>> f;
    std::unique_ptr<std::string> g;

    NEKO_SERIALIZER(a, b, c, d, e, f, g)
};

struct ZTypeTest2 {
    JsonSerializer::JsonValue value;

    // NEKO_SERIALIZER(value)
};

struct ZTypeTest3 {
    ZTypeTest1 value;

    // NEKO_SERIALIZER(value)
};

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);
    testing::InitGoogleTest(&argc, argv);
    std::string str =
        "{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A"
        "\",\"i\":{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,"
        "2,3,0,0],\"h\":\"TEnum_A\"},\"j\":[1,\"hello\"],\"k\":1,\"l\":1.114514,\"m\":[1.1,2.2,3.3,2,1,0,1."
        "11451555213339]}";
    std::vector<char> data(str.begin(), str.end());
    TestP testp;
#ifdef NEKO_PROTO_ENABLE_SIMDJSON
    SimdJsonSerializer::InputSerializer input(data.data(), data.size());
#else
    JsonSerializer::InputSerializer input(data.data(), data.size());
#endif
    auto ret = input(testp);
    EXPECT_TRUE(ret);
    EXPECT_EQ(testp.a, 3);
    EXPECT_STREQ(testp.b.c_str(), "Struct test");
    EXPECT_TRUE(testp.c);
    EXPECT_DOUBLE_EQ(testp.d, 3.141592654);
    EXPECT_EQ(testp.e.size(), 3);
    EXPECT_EQ(testp.f.size(), 2);
    EXPECT_EQ(testp.f["a"], 1);
    EXPECT_EQ(testp.f["b"], 2);
    EXPECT_EQ(testp.g.size(), 5);
    EXPECT_EQ(testp.g[0], 1);
    EXPECT_EQ(testp.g[1], 2);
    EXPECT_EQ(testp.g[2], 3);
    EXPECT_EQ(testp.h, TEnum_A);
    EXPECT_EQ(testp.i.a, 1);
    EXPECT_STREQ(testp.i.b.c_str(), "hello");
    EXPECT_TRUE(testp.i.c);
    EXPECT_DOUBLE_EQ(testp.i.d, 3.141592654);
    EXPECT_EQ(testp.i.e.size(), 3);
    EXPECT_EQ(testp.i.f.size(), 2);
    EXPECT_EQ(testp.i.f["a"], 1);
    EXPECT_EQ(testp.i.f["b"], 2);
    EXPECT_EQ(testp.i.g.size(), 5);
    EXPECT_EQ(testp.i.g[0], 1);
    EXPECT_EQ(testp.i.g[1], 2);
    EXPECT_EQ(testp.i.g[2], 3);
    EXPECT_EQ(testp.i.g[3], 0);
    EXPECT_EQ(testp.i.g[4], 0);
    EXPECT_EQ(testp.i.h, TEnum_A);
    EXPECT_EQ(std::get<0>(testp.j), 1);
    EXPECT_STREQ(std::get<1>(testp.j).c_str(), "hello");
    EXPECT_EQ(testp.k.value_or(-1), 1);
    EXPECT_EQ(testp.l.index(), 2);
    // EXPECT_DOUBLE_EQ(std::get<2>(testp.l), 1.114514);
    TestP tp2;
    tp2.makeProto() = testp;
    EXPECT_STREQ(serializable_to_string(testp).c_str(), serializable_to_string(tp2).c_str());
    NEKO_LOG_DEBUG("unit test", "{}", serializable_to_string(testp));

    std::vector<char> outbuf;
    {
#ifdef NEKO_PROTO_ENABLE_SIMDJSON
        SimdJsonSerializer::OutputSerializer out(outbuf);
#else
        JsonSerializer::OutputSerializer out(outbuf);
#endif
        out(tp2);
    }
    outbuf.push_back('\0');
    NEKO_LOG_DEBUG("unit test", "{}", outbuf.data());

    {
        TestP tp3;
#ifdef NEKO_PROTO_ENABLE_SIMDJSON
        SimdJsonSerializer::InputSerializer in(outbuf.data(), outbuf.size() - 1);
#else
        JsonSerializer::InputSerializer in(outbuf.data(), outbuf.size() - 1);
#endif
        EXPECT_TRUE(in(tp3));
        EXPECT_STREQ(serializable_to_string(tp3).c_str(), serializable_to_string(tp2).c_str());
    }
    ZTypeTest1 zt;
    zt.a = {{1, 1}, {2, 2}};
    zt.b = {{1, "world"}};
    zt.c = {1, 2, 3};
    zt.d = {{1.1, 1}, {1.1, 1.2}, {2.2, 2}};
    zt.e = {1.1, 2.2, 3.3, 3.3};
    zt.f = {{"hello", std::make_shared<std::string>("world")}, {"nullptr", nullptr}};
    zt.g = std::make_unique<std::string>("hello");
    std::vector<char> dataT1;
    JsonSerializer::OutputSerializer output(dataT1);
    output(zt);
    output.end();
    dataT1.push_back('\0');

    ZTypeTest1 zt1;
    JsonSerializer::InputSerializer inputZt1(dataT1.data(), dataT1.size() - 1);
    inputZt1(zt1);
    EXPECT_EQ(zt.a, zt1.a);
    EXPECT_EQ(zt.b, zt1.b);
    EXPECT_EQ(zt.c, zt1.c);
    EXPECT_EQ(zt.d, zt1.d);
    EXPECT_EQ(zt.e, zt1.e);
    EXPECT_EQ(*zt.g, *zt1.g);
    EXPECT_EQ(*zt.f["hello"], *zt1.f["hello"]);
    EXPECT_EQ(zt.f["nullptr"], zt1.f["nullptr"]);

    ZTypeTest3 zt3;
    zt3.value = ZTypeTest1{{{1, 1}, {2, 2}},
                           {{1, "world"}},
                           {1, 2, 3},
                           {{1.1, 1}, {1.1, 1.2}, {2.2, 2}},
                           {1.1, 2.2, 3.3, 3.3},
                           {{"hello", std::make_shared<std::string>("world")}, {"nullptr", nullptr}},
                           std::make_unique<std::string>("hello")};
    std::vector<char> dataT3;
    {
        JsonSerializer::OutputSerializer outputT3(dataT3);
        outputT3(zt3);
    }

    dataT3.push_back('\0');
    ZTypeTest2 zt2;
    {
        JsonSerializer::InputSerializer inputT3(dataT3.data(), dataT3.size() - 1);
        inputT3(zt2);
        NEKO_LOG_DEBUG("unit test", "{}", dataT3.data());
        EXPECT_TRUE(zt2.value.hasValue());
        EXPECT_TRUE(zt2.value.isObject());
        EXPECT_EQ(zt2.value.size(), 7);
        EXPECT_EQ(zt2.value["a"].size(), 2);
        EXPECT_EQ(zt2.value["b"].size(), 1);
        EXPECT_EQ(zt2.value["c"].size(), 3);
        EXPECT_EQ(zt2.value["d"].size(), 3);
        EXPECT_FALSE(zt2.value["ff"]);
        {
            JsonSerializer::InputSerializer input(zt2.value);
            input(zt1);
        }
    }
    EXPECT_EQ(zt3.value.a, zt1.a);
    EXPECT_EQ(zt3.value.b, zt1.b);
    EXPECT_EQ(zt3.value.c, zt1.c);
    EXPECT_EQ(zt3.value.d, zt1.d);
    EXPECT_EQ(zt3.value.e, zt1.e);
    EXPECT_EQ(*zt3.value.g, *zt1.g);
    EXPECT_EQ(*zt3.value.f["hello"], *zt1.f["hello"]);
    EXPECT_EQ(zt3.value.f["nullptr"], zt1.f["nullptr"]);

    return RUN_ALL_TESTS();
}