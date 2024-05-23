#include <iostream>

#include <gtest/gtest.h>

#include "../proto/cc_proto_base.hpp"
#include "../proto/cc_proto_json_serializer.hpp"
#include "../proto/cc_proto_json_serializer_enum.hpp"
#include "../proto/cc_proto_json_serializer_struct.hpp"
#include "../proto/cc_proto_json_serializer_contrain.hpp"
#include "../proto/cc_proto_json_serializer_binary.hpp"

#include "../proto/cc_serializer_base.hpp"

CS_PROTO_USE_NAMESPACE

enum TEnum {
    TEnum_A = 1,
    TEnum_B = 2,
    TEnum_C = 3
};

struct TestProto {
int aads_ddfd;
std::string ccsdfg;
bool c124142;
double d;
std::list<int> e;
std::map<std::string, int> f;
std::array<int, 5> g;
TEnum h;

CS_SERIALIZER(aads_ddfd, ccsdfg, c124142, d, e, f, g, h)
};

TEST(TestProto, TestHelpFunction) {
    JsonSerializer serializer;
    TestProto p { 1, "abc", true, 1.23, {1, 2, 3}, {{"a", 1}, {"b", 2}}, {1, 2, 3, 4, 5}, TEnum_A };
    serializer.startSerialize();
    EXPECT_TRUE(p.serialize(serializer));
    std::vector<char> data;
    EXPECT_TRUE(serializer.endSerialize(&data));
    EXPECT_TRUE(data.size() > 0);
    std::cout << std::string(data.data(), data.size()) << std::endl;
    TestProto p2;
    EXPECT_TRUE(serializer.startDeserialize(data));
    EXPECT_TRUE(p2.deserialize(serializer));
    EXPECT_TRUE(serializer.endDeserialize());
    ASSERT_EQ(p.aads_ddfd, p2.aads_ddfd);
    ASSERT_EQ(p.ccsdfg, p2.ccsdfg);
    ASSERT_EQ(p.c124142, p2.c124142);
    ASSERT_EQ(p.d, p2.d);
    ASSERT_EQ(p.e, p2.e);
    return;
}

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

#if (defined(_MSC_VER) && _MSVC_LANG <= 201402L) || ( defined(__GNUC__) && __cplusplus < 201703L)
CS_PROTO_BEGIN_NAMESPACE
template <>
struct JsonConvert<StructA, void> {
  static bool toJsonValue(JsonWriter &writer, const StructA &value) {
    auto ret = writer.StartArray();
    ret = JsonConvert<int>::toJsonValue(writer, value.a) && ret;
    ret = JsonConvert<std::string>::toJsonValue(writer, value.b) && ret;
    ret = JsonConvert<bool>::toJsonValue(writer, value.c) && ret;
    ret = JsonConvert<double>::toJsonValue(writer, value.d) && ret;
    ret = JsonConvert<std::list<int>>::toJsonValue(writer, value.e) && ret;
    ret = JsonConvert<std::map<std::string, int>>::toJsonValue(writer, value.f) && ret;
    ret = JsonConvert<std::array<int, 5>>::toJsonValue(writer, value.g) && ret;
    ret = JsonConvert<TEnum>::toJsonValue(writer, value.h) && ret;
    ret = writer.EndArray() && ret;
    return ret;
  }
  static bool fromJsonValue(StructA *result, const JsonValue &value) {
    if (result == nullptr || !value.IsArray()) {
        return false;
    }
    auto ret = JsonConvert<int>::fromJsonValue(&result->a, value[0]);
    ret = JsonConvert<std::string>::fromJsonValue(&result->b, value[1]) && ret;
    ret = JsonConvert<bool>::fromJsonValue(&result->c, value[2]) && ret;
    ret = JsonConvert<double>::fromJsonValue(&result->d, value[3]) && ret;
    ret = JsonConvert<std::list<int>>::fromJsonValue(&result->e, value[4]) && ret;
    ret = JsonConvert<std::map<std::string, int>>::fromJsonValue(&result->f, value[5]) && ret;
    ret = JsonConvert<std::array<int, 5>>::fromJsonValue(&result->g, value[6]) && ret;
    ret = JsonConvert<TEnum>::fromJsonValue(&result->h, value[7]) && ret;
    return ret;
  }
};

CS_PROTO_END_NAMESPACE
#endif

struct TestA : public ProtoBase<TestA> {
    int a = 1;
    std::string b = "dsadfsd";

    CS_SERIALIZER(a, b)
};

struct TestP : public ProtoBase<TestP> {
    int a = 1;
    std::string b = "hello";
    bool c = true;
    double d = 3.14;
    std::list<int> e = {1, 2, 3, 4, 5};
    std::map<std::string, int> f = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::array<int, 5> g = {1, 2, 3, 4, 5};
    TEnum h = TEnum_A;
    StructA i = {1, "hello", true, 3.14, {1, 2, 3, 4, 5}, {{"a", 1}, {"b", 2}, {"c", 3}}, {1, 2, 3, 4, 5}, TEnum_A};
    std::tuple<int, std::string> j = {1, "hello"};
    std::vector<int> k = {1, 2, 3, 4, 5};
    TestA l;

    CS_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l)
};

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
    JsonConvert<TestP>::toJsonValue(*writer, testp);
    writer->EndObject();
    const char * str = buffer.GetString();
#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
    EXPECT_STREQ(str, "{\"a\":{\"a\":3,\"b\":\"Struct test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5],\"l\":{\"a\":1,\"b\":\"dsadfsd\"}}}");
#else
    EXPECT_STREQ(str, "{\"a\":{\"a\":3,\"b\":\"Struct test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":1,\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],1],\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5],\"l\":{\"a\":1,\"b\":\"dsadfsd\"}}}");
#endif
}

int main(int argc, char **argv) {
    std::cout << "CS_CPP_PLUS: " << CS_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    ProtoFactory factor(1, 0, 0);
    return RUN_ALL_TESTS();
}