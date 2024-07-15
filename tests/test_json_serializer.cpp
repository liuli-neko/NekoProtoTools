#include <iostream>

#include <gtest/gtest.h>

#include "../core/binary_serializer.hpp"
#include "../core/json_serializer.hpp"
#include "../core/proto_base.hpp"
#include "../core/serializer_base.hpp"
#include "../core/to_string.hpp"
#include "../core/types/array.hpp"
#include "../core/types/binary_data.hpp"
#include "../core/types/enum.hpp"
#include "../core/types/list.hpp"
#include "../core/types/map.hpp"
#include "../core/types/set.hpp"
#include "../core/types/struct_unwrap.hpp"
#include "../core/types/tuple.hpp"
#include "../core/types/variant.hpp"
#include "../core/types/vector.hpp"

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
        NEKO_LOG_ERROR("struct size mismatch: json obejct size {} != struct size 8", size);
        return false;
    }
    ret = sa(value.a, value.b, value.c, value.d, value.e, value.f, value.g, value.h) && ret;
    return ret;
}
NEKO_END_NAMESPACE
#endif

struct TestA {
    int a         = 1;
    std::string b = "dsadfsd";

    NEKO_SERIALIZER(a, b)
    NEKO_DECLARE_PROTOCOL(TestA, JsonSerializer)
};

struct TestB {
    double a              = 12.9;
    std::vector<double> b = {1, 2, 3, 4, 5};
#if NEKO_CPP_PLUS >= 17
    std::optional<TestA> c;
    NEKO_SERIALIZER(a, b, c)
#else
    NEKO_SERIALIZER(a, b)
#endif
    NEKO_DECLARE_PROTOCOL(TestB, JsonSerializer)
};

struct TestC {
    std::vector<TestB> a = {TestB(), TestB(), TestB()};

    NEKO_SERIALIZER(a)
    NEKO_DECLARE_PROTOCOL(TestC, JsonSerializer)
};

struct TestD {
    std::tuple<TestA, TestB, TestC> a = {TestA(), TestB(), TestC()};

    NEKO_SERIALIZER(a)
    NEKO_DECLARE_PROTOCOL(TestD, JsonSerializer)
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
    std::vector<int> k             = {1, 2, 3, 4, 5};
    TestD l;

    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l)
    NEKO_DECLARE_PROTOCOL(TestP, JsonSerializer)
};

class JsonSerializerTest : public testing::Test {
public:
    using WriterType = detail::JsonWriter<>;
    using ValueType  = detail::JsonValue;
    JsonSerializerTest() : buffer(), output(buffer) {}

protected:
    virtual void SetUp() {}
    virtual void TearDown() {}
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer output;
};

TEST_F(JsonSerializerTest, Int) {
    int a = 1;
    output.startObject(1);
    output(makeNameValuePair("a", a));
    output.end();
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":1}");
}

TEST_F(JsonSerializerTest, String) {
    std::string a = "hello";
    output.startObject(1);
    output(makeNameValuePair("a", a));
    output.end();
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":\"hello\"}");
}

TEST_F(JsonSerializerTest, Bool) {
    bool a = true;
    output.startObject(1);
    output(makeNameValuePair("a", a));
    output.end();
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":true}");
}

TEST_F(JsonSerializerTest, Double) {
    double a = 3.14;
    output.startObject(1);
    output(makeNameValuePair("a", a));
    output.end();
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":3.14}");
}

TEST_F(JsonSerializerTest, List) {
    std::list<int> a = {1, 2, 3, 4, 5};
    output.startObject(1);
    output(makeNameValuePair("a", a));
    output.end();
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");
}

TEST_F(JsonSerializerTest, Map) {
    std::map<std::string, int> a = {{"a", 1}, {"b", 2}, {"c", 3}};
    output.startObject(1);
    output(makeNameValuePair("a", a));
    output.end();
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":{\"a\":1,\"b\":2,\"c\":3}}");
}

TEST_F(JsonSerializerTest, Array) {
    std::array<int, 5> a = {1, 2, 3, 4, 5};
    output.startObject(1);
    output(makeNameValuePair("a", a));
    output.end();
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");
}

TEST_F(JsonSerializerTest, Struct) {
    TestP testp;
    // {3, "Struct test", true, 3.141592654, {1, 2, 3}, {{"a", 1}, {"b", 2}}, {1, 2, 3}, TEnum_A, {1, "hello"}};
    testp.a = 3;
    testp.b = "Struct test";
    testp.c = true;
    testp.d = 3.141592654;
    testp.e = {1, 2, 3};
    testp.f = {{"a", 1}, {"b", 2}};
    testp.g = {1, 2, 3};
    testp.h = TEnum_A;
    testp.i = {1, "hello", true, 3.141592654, {1, 2, 3}, {{"a", 1}, {"b", 2}}, {1, 2, 3}, TEnum_A};
    testp.j = {1, "hello"};
    testp.k = std::vector<int>({1, 2, 3, 4, 5});
#if NEKO_CPP_PLUS >= 17
    std::get<1>(testp.l.a).c = TestA{1221, "this is a test for optional"};
#endif
    output.startObject(1);
    output(makeNameValuePair("a", testp));
    output.end();
#if NEKO_CPP_PLUS >= 17
    const char* answer =
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A(1)"
        "\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,"
        "\"hello\"],\"k\":[1,2,3,4,5],\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]"
        ",\"c\":{\"a\":1221,\"b\":\"this is a test for "
        "optional\"}},{\"a\":[{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":"
        "12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]}]}]}}}";
#else
    const char* answer =
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":1,\"i\":[1,"
        "\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],1],\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5],"
        "\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":[{\"a\":12.9,\"b\":["
        "1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]}]}]}}}";
#endif
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, answer);

    TestP testp2;
    {
        JsonSerializer::InputSerializer input(buffer.data(), buffer.size());
        input.startNode();
        input(makeNameValuePair("a", testp2));
    }
    EXPECT_EQ(testp.a, testp2.a);
    EXPECT_STREQ(testp.b.c_str(), testp2.b.c_str());
    EXPECT_EQ(testp.c, testp2.c);
    EXPECT_DOUBLE_EQ(testp.d, testp2.d);
    EXPECT_EQ(testp.e, testp2.e);
    EXPECT_EQ(testp.f, testp2.f);
    EXPECT_EQ(testp.g, testp2.g);
    EXPECT_EQ(testp.h, testp2.h);
    EXPECT_EQ(testp.k, testp2.k);
    NEKO_LOG_INFO("{}", SerializableToString(testp));

    {
        std::vector<char> buffer;
        JsonOutputSerializer<detail::PrettyJsonWriter<>> output(
            buffer, makePrettyJsonWriter(JsonOutputFormatOptions::Default()));
        output.startObject(1);
        output(makeNameValuePair("a", testp));
        output.end();
        buffer.push_back('\0');
        const char* str = buffer.data();
        NEKO_LOG_INFO("{}", str);
    }
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    ProtoFactory factor(1, 0, 0);
    return RUN_ALL_TESTS();
}