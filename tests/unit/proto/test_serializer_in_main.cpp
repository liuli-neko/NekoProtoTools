#include <gtest/gtest.h>

#include "proto/binary_serializer.hpp"
#include "proto/json_serializer.hpp"
#include "proto/proto_base.hpp"
#include "proto/serializer_base.hpp"
#include "proto/simd_json_serializer.hpp"
#include "proto/to_string.hpp"
#include "proto/types/types.hpp"

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

#if NEKO_CPP_PLUS < 17
NEKO_BEGIN_NAMESPACE
template <typename Serializer>
inline bool save(Serializer& sa, const StructA& value) {
    auto ret = sa.startArray((uint32_t)8);
    ret      = sa(value.a, value.b, value.c, value.d, value.e, value.f, value.g, value.h) && ret;
    ret      = sa.endArray() && ret;
    return ret;
}

template <typename Serializer>
inline bool load(Serializer& sa, StructA& value) {
    uint32_t size;
    auto ret = sa(makeSizeTag(size));
    if (size != 8) {
        NEKO_LOG_ERROR("unit test", "struct size mismatch: json obejct size {} != struct size 8", size);
        return false;
    }
    ret = sa(value.a, value.b, value.c, value.d, value.e, value.f, value.g, value.h) && ret;
    return ret;
}
NEKO_END_NAMESPACE
#endif

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
#if NEKO_CPP_PLUS >= 17
    std::optional<int> k;
    std::variant<int, std::string, double> l;
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l)
#else
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j)
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

struct zTypeTest1 {
    std::unordered_map<int, int> a;
    std::unordered_map<int, std::string> b;
    std::unordered_set<int> c;
    std::unordered_multimap<double, int> d;
    std::unordered_multiset<double> e;
    std::unordered_map<std::string, std::shared_ptr<std::string>> f;
    std::unique_ptr<std::string> g;

    NEKO_SERIALIZER(a, b, c, d, e, f, g)
};

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    std::string str = "{\"a\":3,\"b\":\"Struct "
                      "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],"
                      "\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],"
                      "\"TEnum_A(1)\"],\"j\":[1,\"hello\"],\"k\":1,\"l\":1.114514}";
    std::vector<char> data(str.begin(), str.end());
    TestP testp;
    SimdJsonSerializer::InputSerializer input(data.data(), data.size());
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
#if NEKO_CPP_PLUS >= 17
    EXPECT_EQ(testp.k.value_or(-1), 1);
    EXPECT_EQ(testp.l.index(), 2);
    // EXPECT_DOUBLE_EQ(std::get<2>(testp.l), 1.114514);
#endif
    TestP tp2;
    tp2.makeProto() = testp;
    EXPECT_STREQ(SerializableToString(testp).c_str(), SerializableToString(tp2).c_str());
    NEKO_LOG_INFO("unit test", "{}", SerializableToString(testp));

    zTypeTest1 zt;
    zt.a = {{1, 1}, {2, 2}};
    zt.b = {{1, "world"}};
    zt.c = {1, 2, 3};
    zt.d = {{1.1, 1}, {1.1, 1.2}, {2.2, 2}};
    zt.e = {1.1, 2.2, 3.3, 3.3};
    zt.f = {{"hello", std::make_shared<std::string>("world")}, {"nullptr", nullptr}};
    zt.g = std::make_unique<std::string>("hello");
    std::vector<char> data_t1;
    JsonSerializer::OutputSerializer output(data_t1);
    output(zt);
    output.end();
    data_t1.push_back('\0');

    zTypeTest1 zt1;
    JsonSerializer::InputSerializer input_zt1(data_t1.data(), data_t1.size());
    input_zt1(zt1);
    EXPECT_EQ(zt.a, zt1.a);
    EXPECT_EQ(zt.b, zt1.b);
    EXPECT_EQ(zt.c, zt1.c);
    EXPECT_EQ(zt.d, zt1.d);
    EXPECT_EQ(zt.e, zt1.e);
    EXPECT_EQ(*zt.g, *zt1.g);
    EXPECT_EQ(*zt.f["hello"], *zt1.f["hello"]);
    EXPECT_EQ(zt.f["nullptr"], zt1.f["nullptr"]);
    return 0;
}