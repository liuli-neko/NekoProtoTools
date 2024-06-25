#include <iostream>

#include <gtest/gtest.h>

#include "../core/json_serializer.hpp"
#include "../core/json_serializer_binary.hpp"
#include "../core/json_serializer_contrain.hpp"
#include "../core/json_serializer_enum.hpp"
#include "../core/json_serializer_struct.hpp"
#include "../core/proto_base.hpp"
#include "../core/serializer_base.hpp"

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

#if (defined(_MSC_VER) && _MSVC_LANG <= 201402L) || (defined(__GNUC__) && __cplusplus < 201703L)
NEKO_BEGIN_NAMESPACE
template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, StructA, void> {
    static bool toJsonValue(WriterT& writer, const StructA& value) {
        auto ret = writer.StartArray();
        ret      = JsonConvert<WriterT, ValueT, int>::toJsonValue(writer, value.a) && ret;
        ret      = JsonConvert<WriterT, ValueT, std::string>::toJsonValue(writer, value.b) && ret;
        ret      = JsonConvert<WriterT, ValueT, bool>::toJsonValue(writer, value.c) && ret;
        ret      = JsonConvert<WriterT, ValueT, double>::toJsonValue(writer, value.d) && ret;
        ret      = JsonConvert<WriterT, ValueT, std::list<int>>::toJsonValue(writer, value.e) && ret;
        ret      = JsonConvert<WriterT, ValueT, std::map<std::string, int>>::toJsonValue(writer, value.f) && ret;
        ret      = JsonConvert<WriterT, ValueT, std::array<int, 5>>::toJsonValue(writer, value.g) && ret;
        ret      = JsonConvert<WriterT, ValueT, TEnum>::toJsonValue(writer, value.h) && ret;
        ret      = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(StructA* result, const ValueT& value) {
        if (result == nullptr || !value.IsArray()) {
            return false;
        }
        auto ret = JsonConvert<WriterT, ValueT, int>::fromJsonValue(&result->a, value[0]);
        ret      = JsonConvert<WriterT, ValueT, std::string>::fromJsonValue(&result->b, value[1]) && ret;
        ret      = JsonConvert<WriterT, ValueT, bool>::fromJsonValue(&result->c, value[2]) && ret;
        ret      = JsonConvert<WriterT, ValueT, double>::fromJsonValue(&result->d, value[3]) && ret;
        ret      = JsonConvert<WriterT, ValueT, std::list<int>>::fromJsonValue(&result->e, value[4]) && ret;
        ret      = JsonConvert<WriterT, ValueT, std::map<std::string, int>>::fromJsonValue(&result->f, value[5]) && ret;
        ret      = JsonConvert<WriterT, ValueT, std::array<int, 5>>::fromJsonValue(&result->g, value[6]) && ret;
        ret      = JsonConvert<WriterT, ValueT, TEnum>::fromJsonValue(&result->h, value[7]) && ret;
        return ret;
    }
};

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
    TestA c;

    NEKO_SERIALIZER(a, b)
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
    using WriterType = JsonWriter<>;
    using ValueType  = JsonValue;

protected:
    virtual void SetUp() {
        writer = std::make_shared<WriterType>(buffer);
        writer->StartObject();
    }
    virtual void TearDown() { writer.reset(); }
    std::shared_ptr<WriterType> writer;
    OutBufferWrapper buffer;
};

TEST_F(JsonSerializerTest, Int) {
    int a = 1;
    writer->Key("a");
    JsonConvert<WriterType, ValueType, int>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":1}");
}

TEST_F(JsonSerializerTest, String) {
    std::string a = "hello";
    writer->Key("a");
    JsonConvert<WriterType, ValueType, std::string>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":\"hello\"}");
}

TEST_F(JsonSerializerTest, Bool) {
    bool a = true;
    writer->Key("a");
    JsonConvert<WriterType, ValueType, bool>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":true}");
}

TEST_F(JsonSerializerTest, Double) {
    double a = 3.14;
    writer->Key("a");
    JsonConvert<WriterType, ValueType, double>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":3.14}");
}

TEST_F(JsonSerializerTest, List) {
    std::list<int> a = {1, 2, 3, 4, 5};
    writer->Key("a");
    JsonConvert<WriterType, ValueType, std::list<int>>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");
}

TEST_F(JsonSerializerTest, Map) {
    std::map<std::string, int> a = {{"a", 1}, {"b", 2}, {"c", 3}};
    writer->Key("a");
    JsonConvert<WriterType, ValueType, std::map<std::string, int>>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":{\"a\":1,\"b\":2,\"c\":3}}");
}

TEST_F(JsonSerializerTest, Array) {
    std::array<int, 5> a = {1, 2, 3, 4, 5};
    writer->Key("a");
    JsonConvert<WriterType, ValueType, std::array<int, 5>>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");
}

TEST_F(JsonSerializerTest, Enum) {
    TEnum a = TEnum_A;
    writer->Key("a");
    JsonConvert<WriterType, ValueType, TEnum>::toJsonValue(*writer, a);
    writer->EndObject();
    buffer.Put('\0');
    const char* str = buffer.GetString();
#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
    EXPECT_STREQ(str, "{\"a\":\"TEnum_A(1)\"}");
#else
    EXPECT_STREQ(str, "{\"a\":1}");
#endif
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
    writer->Key("a");
    JsonConvert<WriterType, ValueType, TestP>::toJsonValue(*writer, testp);
    writer->EndObject();
    buffer.Put(0);
    const char* str = buffer.GetString();
#if NEKO_CPP_PLUS >= 17
    const char* answer =
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A(1)"
        "\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,"
        "\"hello\"],\"k\":[1,2,3,4,5],\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]"
        "},{\"a\":[{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":"
        "[1.0,2.0,3.0,4.0,5.0]}]}]}}}";
#else
    const char* answer =
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":1,\"i\":[1,"
        "\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],1],\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5],"
        "\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":[{\"a\":12.9,\"b\":["
        "1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]}]}]}}}";
#endif
    EXPECT_STREQ(str, answer);

    TestP testp2;
    JsonSerializer serializer;
    serializer.startDeserialize(std::vector<char>{str, str + strlen(str)});
    serializer.get("a", 1, &testp2);
    serializer.endDeserialize();
    EXPECT_EQ(testp.a, testp2.a);
    EXPECT_STREQ(testp.b.c_str(), testp2.b.c_str());
    EXPECT_EQ(testp.c, testp2.c);
    EXPECT_DOUBLE_EQ(testp.d, testp2.d);
    EXPECT_EQ(testp.e, testp2.e);
    EXPECT_EQ(testp.f, testp2.f);
    EXPECT_EQ(testp.g, testp2.g);
    EXPECT_EQ(testp.h, testp2.h);
    EXPECT_EQ(testp.k, testp2.k);
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    ProtoFactory factor(1, 0, 0);
    return RUN_ALL_TESTS();
}