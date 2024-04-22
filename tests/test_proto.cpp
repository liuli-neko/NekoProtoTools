#include <iostream>

#include <gtest/gtest.h>

#include "../proto/cc_proto_base.hpp"
#include "../proto/cc_proto_json_serializer.hpp"
#include "../proto/cc_proto_json_serializer_enum.hpp"
#include "../proto/cc_proto_json_serializer_struct.hpp"
#include "../proto/cc_proto_json_serializer_contrain.hpp"
#include "../proto/cc_proto_json_serializer_binary.hpp"

CS_PROTO_USE_NAMESPACE

CS_PROTO_SET_TYPE_START(1)

enum TEnum {
    TEnum_A = 1,
    TEnum_B = 2,
    TEnum_C = 3
};

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

#if (defined(_MSC_VER) && _MSVC_LANG <= 201402L) || ( defined(__GNUC__) && cplusplus < 201703L)
CS_PROTO_BEGIN_NAMESPACE
template <>
struct JsonConvert<StructA, void> {
  static void toJsonValue(JsonWriter &writer, const StructA &value) {
    writer.StartArray();
    JsonConvert<int>::toJsonValue(writer, value.a);
    JsonConvert<std::string>::toJsonValue(writer, value.b);
    JsonConvert<bool>::toJsonValue(writer, value.c);
    JsonConvert<double>::toJsonValue(writer, value.d);
    JsonConvert<std::list<int>>::toJsonValue(writer, value.e);
    JsonConvert<std::map<std::string, int>>::toJsonValue(writer, value.f);
    JsonConvert<std::array<int, 5>>::toJsonValue(writer, value.g);
    JsonConvert<TEnum>::toJsonValue(writer, value.h);
    writer.EndArray();
  }
  static bool fromJsonValue(StructA *result, const JsonValue &value) {
    if (result == nullptr || !value.IsArray()) {
        return false;
    }
    JsonConvert<int>::fromJsonValue(&result->a, value[0]);
    JsonConvert<std::string>::fromJsonValue(&result->b, value[1]);
    JsonConvert<bool>::fromJsonValue(&result->c, value[2]);
    JsonConvert<double>::fromJsonValue(&result->d, value[3]);
    JsonConvert<std::list<int>>::fromJsonValue(&result->e, value[4]);
    JsonConvert<std::map<std::string, int>>::fromJsonValue(&result->f, value[5]);
    JsonConvert<std::array<int, 5>>::fromJsonValue(&result->g, value[6]);
    JsonConvert<TEnum>::fromJsonValue(&result->h, value[7]);
    return true;
  }
};

CS_PROTO_END_NAMESPACE
#endif

struct TestP : public ProtoBase<TestP> {
    CS_PROPERTY_FIELD(int, a) = 1;
    CS_PROPERTY_FIELD(std::string, b) = "hello";
    CS_PROPERTY_FIELD(bool, c) = true;
    CS_PROPERTY_FIELD(double, d) = 3.14;
    CS_PROPERTY_FIELD(std::list<int>, e) = {1, 2, 3, 4, 5};
    typedef std::map<std::string, int> TMap;
    CS_PROPERTY_CONTAINER_FIELD(TMap, f) = {{"a", 1}, {"b", 2}, {"c", 3}};
    typedef std::array<int, 5> TArray;
    CS_PROPERTY_CONTAINER_FIELD(TArray, g) = {1, 2, 3, 4, 5};
    CS_PROPERTY_FIELD(TEnum, h) = TEnum_A;
    CS_PROPERTY_FIELD(StructA, i) = {1, "hello", true, 3.14, {1, 2, 3, 4, 5}, {{"a", 1}, {"b", 2}, {"c", 3}}, {1, 2, 3, 4, 5}, TEnum_A};
    typedef std::tuple<int, std::string> TTuple;
    CS_PROPERTY_FIELD(TTuple, j) = {1, "hello"};
    CS_PROPERTY_CONTAINER_FIELD(std::vector<int>, k) = {1, 2, 3, 4, 5};
};
CS_DECLARE_PROTO(TestP, "TestP");

class JsonSerializerTest : public testing::Test {
protected:
    virtual void SetUp() {
        writer = std::make_shared<JsonWriter>(buffer);
        writer->StartObject();
    }
    virtual void TearDown() {
        writer.reset();
    }
    std::shared_ptr<JsonWriter> writer;
    rapidjson::StringBuffer buffer;
};

TEST_F(JsonSerializerTest, Int) {
    int a = 1;
    writer->Key("a");
    JsonConvert<int>::toJsonValue(*writer, a);
    writer->EndObject();
    const char * str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":1}");
}

TEST_F(JsonSerializerTest, String) {
    std::string a = "hello";
    writer->Key("a");
    JsonConvert<std::string>::toJsonValue(*writer, a);
    writer->EndObject();
    const char * str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":\"hello\"}");
}

TEST_F(JsonSerializerTest, Bool) {
    bool a = true;
    writer->Key("a");
    JsonConvert<bool>::toJsonValue(*writer, a);
    writer->EndObject();
    const char * str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":true}");
}

TEST_F(JsonSerializerTest, Double) {
    double a = 3.14;
    writer->Key("a");
    JsonConvert<double>::toJsonValue(*writer, a);
    writer->EndObject();
    const char * str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":3.14}");
}

TEST_F(JsonSerializerTest, List) {
    std::list<int> a = {1, 2, 3, 4, 5};
    writer->Key("a");
    JsonConvert<std::list<int>>::toJsonValue(*writer, a);
    writer->EndObject();
    const char * str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");
}

TEST_F(JsonSerializerTest, Map) {
    std::map<std::string, int> a = {{"a", 1}, {"b", 2}, {"c", 3}};
    writer->Key("a");
    JsonConvert<std::map<std::string, int>>::toJsonValue(*writer, a);
    writer->EndObject();
    const char * str = buffer.GetString();
    EXPECT_STREQ(str, "{\"a\":{\"a\":1,\"b\":2,\"c\":3}}");
}

TEST_F(JsonSerializerTest, Array) {
        std::array<int, 5> a = {1, 2, 3, 4, 5};
        writer->Key("a");
        JsonConvert<std::array<int, 5>>::toJsonValue(*writer, a);
        writer->EndObject();
        const char * str = buffer.GetString();
        EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");
}

TEST_F(JsonSerializerTest, Enum) {
    TEnum a = TEnum_A;
    writer->Key("a");
    JsonConvert<TEnum>::toJsonValue(*writer, a);
    writer->EndObject();
    const char * str = buffer.GetString();
#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
    EXPECT_STREQ(str, "{\"a\":\"TEnum_A(1)\"}");
#else
    EXPECT_STREQ(str, "{\"a\":1}");
#endif
}

TEST_F(JsonSerializerTest, Struct) {
    TestP testp;
    // {3, "Struct test", true, 3.141592654, {1, 2, 3}, {{"a", 1}, {"b", 2}}, {1, 2, 3}, TEnum_A, {1, "hello"}};
    testp.mutable_a() = 3;
    testp.mutable_b() = "Struct test";
    testp.mutable_c() = true;
    testp.mutable_d() = 3.141592654;
    testp.mutable_e() = {1, 2, 3};
    testp.mutable_f() = {{"a", 1}, {"b", 2}};
    testp.mutable_g() = {1, 2, 3};
    testp.mutable_h() = TEnum_A;
    testp.mutable_i() = {1, "hello", true, 3.141592654, {1, 2, 3}, {{"a", 1}, {"b", 2}}, {1, 2, 3}, TEnum_A};
    testp.mutable_j() = {1, "hello"};
    testp.set_k({1, 2, 3, 4, 5});
    writer->Key("a");
    JsonConvert<TestP>::toJsonValue(*writer, testp);
    writer->EndObject();
    const char * str = buffer.GetString();
#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
    EXPECT_STREQ(str, "{\"a\":{\"a\":3,\"b\":\"Struct test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5]}}");
#else
    EXPECT_STREQ(str, "{\"a\":{\"a\":3,\"b\":\"Struct test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"1\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"1\"],\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5]}}");
#endif
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}