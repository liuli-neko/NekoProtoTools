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

    CS_DECLARE_PROTO_FIELD(a)
    CS_DECLARE_PROTO_FIELD(b)
    CS_DECLARE_PROTO_FIELD(c)
    CS_DECLARE_PROTO_FIELD(d)
    CS_DECLARE_PROTO_FIELD(e)
    CS_DECLARE_PROTO_FIELD(f)
    CS_DECLARE_PROTO_FIELD(g)
    CS_DECLARE_PROTO_FIELD(h)
    CS_DECLARE_PROTO_FIELD(i)
    CS_DECLARE_PROTO_FIELD(j)
};
CS_DECLARE_PROTO(TestP, "TestP");

TEST(JsonSerializerTest, StructSerialize) {
    TestP testp;
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
    std::vector<char> data;
    data = testp.serialize();
#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
    EXPECT_STREQ(data.data(), "{\"a\":3,\"b\":\"Struct test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,\"hello\"]}");
#else
    EXPECT_STREQ(data.data(), "{\"a\":3,\"b\":\"Struct test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":1,\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],1],\"j\":[1,\"hello\"]}");
#endif
}

TEST(JsonSerializerTest, StructDeserialize) {
    TestP testp;
    std::string str = "{\"a\":3,\"b\":\"Struct test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,\"hello\"]}";
    std::vector<char> data(str.begin(), str.end());
    testp.deserialize(data);
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
}

TEST(JsonSerializerTest, Base64Covert) {
    const char * str = "this is a test string";
    auto base64string = Base64Covert::Encode(str);
    base64string.push_back('\0');
    EXPECT_STREQ(base64string.data(), "dGhpcyBpcyBhIHRlc3Qgc3RyaW5n");
    base64string.pop_back();
    auto str2 = Base64Covert::Decode(base64string);
    str2.push_back('\0');
    EXPECT_STREQ(str2.data(), str);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::debug);
    return RUN_ALL_TESTS();
}