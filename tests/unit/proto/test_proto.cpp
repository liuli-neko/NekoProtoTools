#include <gtest/gtest.h>

#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/to_string.hpp"
#include "nekoproto/serialization/types/types.hpp"

NEKO_USE_NAMESPACE

template <typename T>
std::string make_enum_string(const std::string& fmt) {
    auto names  = Reflect<T>::names();
    auto values = Reflect<T>::values();
    std::string ret;
    for (int idx = 0; idx < names.size(); ++idx) {
        if (names[idx].size() > 0) {
            std::string tfmt = fmt;
            tfmt.replace(tfmt.find("{enum}"), 6, names[idx]);
            tfmt.replace(tfmt.find("{num}"), 5, std::to_string(values[idx]));
            ret += tfmt;
        }
    }
    return ret;
}

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
    std::optional<int> k;
    std::variant<int, std::string, double> l;
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l)

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
    std::optional<int32_t> d;

    NEKO_SERIALIZER(a, b, c, d)
    NEKO_DECLARE_PROTOCOL(BinaryProto, BinarySerializer)
};

class ProtoTest : public testing::Test {
protected:
    virtual void SetUp() { mFactory.reset(new ProtoFactory()); }
    virtual void TearDown() {}
    std::unique_ptr<ProtoFactory> mFactory;
};

TEST_F(ProtoTest, Reflection) {
    EXPECT_EQ(Reflect<TestP>::name(0), "a");
    EXPECT_EQ(Reflect<TestP>::name(1), "b");
    EXPECT_EQ(Reflect<TestP>::name(2), "c");
    EXPECT_EQ(Reflect<TestP>::name(3), "d");
    EXPECT_EQ(Reflect<TestP>::name(4), "e");
    EXPECT_EQ(Reflect<TestP>::name(5), "f");
    EXPECT_EQ(Reflect<TestP>::name(6), "g");
    EXPECT_EQ(Reflect<TestP>::name(7), "h");
    EXPECT_EQ(Reflect<TestP>::name(8), "i");
    EXPECT_EQ(Reflect<TestP>::name(9), "j");
    EXPECT_EQ(Reflect<TestP>::name(10), "k");
    EXPECT_EQ(Reflect<TestP>::name(11), "l");
    EXPECT_EQ(Reflect<TestP>::name(12), "");
    EXPECT_EQ(Reflect<TestP>::size(), 12);

    TestP testp;
    EXPECT_EQ(Reflect<TestP>::value<0>(testp), 1);
    EXPECT_EQ(Reflect<TestP>::value<1>(testp), "hello");
    EXPECT_TRUE(Reflect<TestP>::value<2>(testp));
    EXPECT_EQ(Reflect<TestP>::value<3>(testp), 3.14);
    EXPECT_EQ(Reflect<TestP>::value<4>(testp), (std::list<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(Reflect<TestP>::value<7>(testp), TEnum_A);

    Reflect<TestP>::value(testp, 0).as<int>()           = 2;
    Reflect<TestP>::value(testp, "b").as<std::string>() = "world";
    Reflect<TestP>::value<2>(testp)                     = false;
    Reflect<TestP>::value<3>(testp)                     = 6.28;
    Reflect<TestP>::value<4>(testp)                     = std::list<int>{6, 7, 8, 9, 10};
    Reflect<TestP>::value<7>(testp)                     = TEnum_B;

    EXPECT_EQ(Reflect<TestP>::value(testp, 0).as<int>(), 2);
    EXPECT_EQ(Reflect<TestP>::value(testp, 1).as<std::string>(), "world");
    EXPECT_FALSE(Reflect<TestP>::value(testp, "c").as<bool>());
    EXPECT_EQ(Reflect<TestP>::value(testp, "d").as<double>(), 6.28);
    EXPECT_EQ(Reflect<TestP>::value<4>(testp), (std::list<int>{6, 7, 8, 9, 10}));
    EXPECT_EQ(Reflect<TestP>::value<7>(testp), TEnum_B);

    const TestP testp2 = testp;
    EXPECT_EQ(Reflect<TestP>::value(testp2, 0).as<const int>(), 2);
    EXPECT_EQ(Reflect<TestP>::value(testp2, 1).as<const std::string>(), "world");
    EXPECT_FALSE(Reflect<TestP>::value(testp2, "c").as<const bool>());
    EXPECT_EQ(Reflect<TestP>::value(testp2, "d").as<const double>(), 6.28);
    EXPECT_EQ(Reflect<TestP>::value<4>(testp2), (std::list<int>{6, 7, 8, 9, 10}));
}

TEST_F(ProtoTest, StructSerialize) {
    EXPECT_EQ(mFactory->protoType<TestP>(), NEKO_RESERVED_PROTO_TYPE_SIZE + 2);
    TestP testp;
    testp.a = 3;
    testp.b = "Struct test";
    testp.c = true;
    testp.d = 3.141592654;
    testp.e = std::list<int>{1, 2, 3};
    testp.f = std::map<std::string, int>{{"a", 1}, {"b", 2}};
    testp.g = std::array<int, 5>{1, 2, 3, 0, 0};
    testp.h = TEnum_A;
    testp.i = StructA{1, "hello", true, 3.141592654, {1, 2, 3}, {{"a", 1}, {"b", 2}}, {1, 2, 3}, TEnum_A};
    testp.j = std::make_tuple(1, "hello");
    testp.l = "this a test for variant";
    std::vector<char> data;
    data = testp.makeProto().toData();
    data.push_back('\0');
    EXPECT_STREQ(
        data.data(),
        "{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A\","
        "\"i\":{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,"
        "3,0,0],\"h\":\"TEnum_A\"},\"j\":[1,\"hello\"],\"l\":\"this a test for variant\"}");
}

TEST_F(ProtoTest, StructDeserialize) {
    std::string str = "{\"a\":3,\"b\":\"Struct "
                      "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],"
                      "\"h\":\"TEnum_A\",\"i\":{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],"
                      "\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],"
                      "\"h\":\"TEnum_A\"},\"j\":[1,\"hello\"],\"k\":1,\"l\":1.114514}";
    std::vector<char> data(str.begin(), str.end());
    TestP testp;
    JsonSerializer::InputSerializer input(data.data(), data.size());
    EXPECT_TRUE(input(testp));
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
    EXPECT_DOUBLE_EQ(std::get<2>(testp.l), 1.114514);
#endif
    TestP tp2;
    tp2.makeProto() = testp;
    EXPECT_STREQ(serializable_to_string(testp).c_str(), serializable_to_string(tp2).c_str());
    NEKO_LOG_DEBUG("unit test", "{}", serializable_to_string(testp));
}

TEST_F(ProtoTest, Base64Covert) {
    const char* str   = "this is a test string";
    auto base64string = Base64Covert::Encode(str);
    base64string.push_back('\0');
    EXPECT_STREQ(base64string.data(), "dGhpcyBpcyBhIHRlc3Qgc3RyaW5n");
    base64string.pop_back();
    auto str2 = Base64Covert::Decode(base64string);
    str2.push_back('\0');
    EXPECT_STREQ(str2.data(), str);

    const char* str3   = "this is a test string2";
    auto base64string2 = Base64Covert::Encode(str3);
    base64string2.push_back('\0');
    EXPECT_STREQ(base64string2.data(), "dGhpcyBpcyBhIHRlc3Qgc3RyaW5nMg==");
    base64string2.pop_back();
    auto str4 = Base64Covert::Decode(base64string2);
    str4.push_back('\0');
    EXPECT_STREQ(str4.data(), str3);

    const char* str5   = "this is a test string21";
    auto base64string3 = Base64Covert::Encode(str5);
    base64string3.push_back('\0');
    EXPECT_STREQ(base64string3.data(), "dGhpcyBpcyBhIHRlc3Qgc3RyaW5nMjE=");
    base64string3.pop_back();
    auto str6 = Base64Covert::Decode(base64string3);
    str6.push_back('\0');
    EXPECT_STREQ(str6.data(), str5);

    const char* str7   = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto base64string4 = Base64Covert::Encode(str7);
    base64string4.push_back('\0');
    EXPECT_STREQ(base64string4.data(),
                 "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVphYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ejAxMjM0NTY3ODkrLw==");
    base64string4.pop_back();
    auto str8 = Base64Covert::Decode(base64string4);
    str8.push_back('\0');
    EXPECT_STREQ(str8.data(), str7);

    auto str9 = Base64Covert::Decode(str7);
    str9.push_back('\0');
    EXPECT_STREQ(str9.data(), "");

    auto str10 = Base64Covert::Decode("");
    str10.push_back('\0');
    EXPECT_STREQ(str10.data(), "");
}

TEST_F(ProtoTest, JsonProtoRef) {
    std::string str = "{\"a\":3,\"b\":\"Struct "
                      "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],"
                      "\"h\":\"TEnum_A\",\"i\":{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],"
                      "\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":"
                      "\"TEnum_A\"},\"j\":[1,\"hello\"],\"l\":23}";
    auto proto      = mFactory->create("TestP");
    proto.fromData(str.data(), str.length());
    auto* rawp = proto.cast<TestP>(); // success cast

    EXPECT_TRUE(proto.cast<BinaryProto>() == nullptr); // failed cast
    // get field test
    int num = {};
    EXPECT_TRUE(proto.getField("a", &num));                                    // success get field
    EXPECT_FALSE(proto.getField("b", &num));                                   // get field by wrong type
    EXPECT_FALSE(proto.getField("unexist field", &num));                       // get unexist field
    EXPECT_STREQ(proto.getField<std::string>("b", "").c_str(), "Struct test"); // success get field
    EXPECT_TRUE(proto.getField<bool>("c", false));                             // success get field
    EXPECT_FALSE(proto.getField<bool>("b", false));                            // get field by wrong type
    EXPECT_FALSE(proto.getField<bool>("unexist field", false));                // get unexist field
    EXPECT_STREQ(proto.getField<std::string>("", "false").c_str(), "false");   // get unexist field
    EXPECT_FALSE(proto.getField<bool>("", false));                             // get unexist field
    EXPECT_EQ(num, 3);

    // set field test
    EXPECT_TRUE(proto.setField("a", 14));                            // success set field
    EXPECT_TRUE(proto.setField("b", std::string("field set test"))); // success set field
    EXPECT_TRUE(proto.setField("c", false));                         // success set field
    EXPECT_FALSE(proto.setField("a", 3.1234));                       // set field by wrong type
    EXPECT_FALSE(proto.setField("unexist field", 3.1234));           // set unexist field
    EXPECT_FALSE(proto.setField("", 3.1234));                        // set unexist field
    EXPECT_EQ(rawp->a, 14);
    EXPECT_STREQ(rawp->b.c_str(), "field set test");
    EXPECT_FALSE(rawp->c);

    EXPECT_DOUBLE_EQ(rawp->d, 3.141592654);
    EXPECT_EQ(rawp->e.size(), 3);
    EXPECT_EQ(rawp->f.size(), 2);
    EXPECT_EQ(rawp->f["a"], 1);
    EXPECT_EQ(rawp->f["b"], 2);
    EXPECT_EQ(rawp->g.size(), 5);
    EXPECT_EQ(rawp->g[0], 1);
    EXPECT_EQ(rawp->g[1], 2);
    EXPECT_EQ(rawp->g[2], 3);
    EXPECT_EQ(rawp->h, TEnum_A);
    EXPECT_EQ(rawp->i.a, 1);
    EXPECT_STREQ(rawp->i.b.c_str(), "hello");
    EXPECT_TRUE(rawp->i.c);
    EXPECT_DOUBLE_EQ(rawp->i.d, 3.141592654);
    EXPECT_EQ(rawp->i.e.size(), 3);
    EXPECT_EQ(rawp->i.f.size(), 2);
    EXPECT_EQ(rawp->i.f["a"], 1);
    EXPECT_EQ(rawp->i.f["b"], 2);
    EXPECT_EQ(rawp->i.g.size(), 5);
    EXPECT_EQ(rawp->i.g[0], 1);
    EXPECT_EQ(rawp->i.g[1], 2);
    EXPECT_EQ(rawp->i.g[2], 3);
    EXPECT_EQ(rawp->i.g[3], 0);
    EXPECT_EQ(rawp->i.g[4], 0);
    EXPECT_EQ(rawp->i.h, TEnum_A);
    EXPECT_EQ(std::get<0>(rawp->j), 1);
    EXPECT_STREQ(std::get<1>(rawp->j).c_str(), "hello");
    NEKO_LOG_DEBUG("unit test", "{}", serializable_to_string(*rawp));
}

TEST_F(ProtoTest, InvalidParams) {
    std::string str = "{\"a\":3}";
    TestP proto;
    EXPECT_FALSE(proto.makeProto().fromData(str.data(), str.length()));
    EXPECT_TRUE(mFactory->create("InvalidP") == nullptr);
    EXPECT_TRUE(mFactory->create(-1) == nullptr);
    str = "{\"a\":3.213123,\"b\":123"
          ",\"c\":12,\"d\":\"dddd\",\"f\":[1,2,3],\"e\":{\"a\":1,\"b\":2},\"h\":[1,2,3,0,0],"
          "\"g\":\"TEnum_A\",\"j\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],"
          "\"TEnum_A\"],\"i\":[1,\"hello\"],\"l\":23}";
    EXPECT_FALSE(proto.makeProto().fromData(str.data(), str.length()));
    str = "aaaaaaaaaaaaa";
    EXPECT_FALSE(proto.makeProto().fromData(str.data(), str.length()));
    str = "{\"a\":3.213123,\"b\":123"
          ",\"c\":12,\"d\":\"dddd\",\"f\":23,\"e\":null,\"h\":23.22,"
          "\"g\":\"TEnum_A\",\"j\":[1,\"hello\",true,3.141592654,[1,2,3],"
          "\"TEnum_A\"],\"i\":[1,\"hello\"],\"l\":23}";
    EXPECT_FALSE(proto.makeProto().fromData(str.data(), str.length()));
    auto dest = TestP::ProtoType::Serialize(proto);
    EXPECT_FALSE(dest.empty());
    EXPECT_FALSE(TestP::ProtoType::Deserialize(str.data(), str.length(), proto));
}

TEST_F(ProtoTest, BinaryProto) {
    BinaryProto proto;
    proto.a   = 24;
    proto.b   = "hello Neko Proto";
    proto.c   = 0x3f3f3f;
    auto data = proto.makeProto().toData();
    NEKO_LOG_DEBUG("unit test", "{}", serializable_to_string(proto));

    BinaryProto proto2;
    EXPECT_TRUE(proto2.makeProto().fromData(data.data(), data.size()));
    EXPECT_EQ(proto2.a, proto.a);
    EXPECT_EQ(proto2.b, proto.b);
    EXPECT_EQ(proto2.c, proto.c);
}

struct ZTypeTest {
    int a                   = 1;
    std::string b           = "field set test";
    bool c                  = false;
    double d                = 3.141592654;
    std::vector<int> e      = {1, 2, 3};
    std::map<int, int> f    = {{1, 1}, {2, 2}};
    std::list<double> g     = {1.1, 2.2, 3.3, 4.4, 5.5};
    std::set<std::string> h = {"a", "b", "c"};
    std::deque<float> i     = {1.1F, 2.2F, 3.3F, 4.4F, 5.5F, 6.6F, 7.7F};
    std::array<int, 7> j    = {1, 2, 3, 4, 5, 6, 7};
    std::tuple<int, std::string, bool, double, std::vector<int>, std::map<int, int>, TEnum> k = {
        1, "hello", true, 3.141592654, {1, 2, 3}, {{1, 1}, {2, 2}}, TEnum_A};
    std::shared_ptr<std::string> l        = std::make_shared<std::string>("hello shared ptr");
    std::multimap<int, std::string> p     = {{1, "hello"}, {2, "world"}, {1, "world"}};
    std::multiset<std::string> q          = {"a", "b", "c", "a", "b"};
    std::vector<std::vector<int>> t       = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    std::bitset<16> v                     = {0x3f3f3f};
    std::pair<std::string, std::string> w = {"hello", "world"};
#if NEKO_CPP_PLUS >= 17
    std::optional<int> x             = 1;
    std::variant<int, std::string> y = "hello";
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l, p, q, t, v, w, x, y);
#else
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l, p, q, t, v, w);
#endif
    NEKO_DECLARE_PROTOCOL(ZTypeTest, JsonSerializer)
};

static const char* gZTypeTestStr =
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    "{\"a\":1,\"b\":\"field set "
    "test\",\"c\":false,\"d\":3.141592654,\"e\":[1,2,3],\"f\":[{\"key\":1,\"value\":1},{\"key\":2,\"value\":2}],\"g\":["
    "1.1,2.2,3.3,4.4,5.5],\"h\":[\"a\",\"b\",\"c\"],\"i\":[1.100000023841858,2.200000047683716,3.299999952316284,4."
    "400000095367432,5.5,6.599999904632568,7.699999809265137],\"j\":[1,2,3,4,5,6,7],\"k\":[1,\"hello\",true,3."
    "141592654,[1,2,3],[{\"key\":1,\"value\":1},{\"key\":2,\"value\":2}],\"TEnum_A\"],\"l\":\"hello shared "
    "ptr\",\"p\":[{\"key\":1,\"value\":\"hello\"},{\"key\":1,\"value\":\"world\"},{\"key\":2,\"value\":\"world\"}],"
    "\"q\":[\"a\","
    "\"a\",\"b\",\"b\",\"c\"],\"t\":[[1,2,3],[4,5,6],[7,8,9]],\"v\":"
    "\"0011111100111111\",\"w\":{\"first\":\"hello\",\"second\":\"world\"},\"x\":1,\"y\":\"hello\"}";
#else
    "{\"a\":1,\"b\":\"field set "
    "test\",\"c\":false,\"d\":3.141592654,\"e\":[1,2,3],\"f\":[{\"key\":1,\"value\":1},{\"key\":2,\"value\":2}],\"g\":["
    "1.1,2.2,3.3,4.4,5.5],\"h\":[\"a\",\"b\",\"c\"],\"i\":[1.1,2.2,3.3,4.4,5.5,6.6,7.7],\"j\":[1,2,3,4,5,6,7],\"k\":[1,"
    "\"hello\",true,3.141592654,[1,2,3],[{\"key\":1,\"value\":1},{\"key\":2,\"value\":2}],\"TEnum_A\"],\"l\":"
    "\"hello shared "
    "ptr\",\"p\":[{\"key\":1,\"value\":\"hello\"},{\"key\":1,\"value\":\"world\"},{\"key\":2,\"value\":\"world\"}],"
    "\"q\":[\"a\",\"a\",\"b\",\"b\",\"c\"],\"t\":[[1,2,3],[4,5,6],[7,8,9]],\"v\":"
    "\"0011111100111111\",\"w\":{\"first\":\"hello\",\"second\":\"world\"},\"x\":1,\"y\":\"hello\"}";
#endif

TEST_F(ProtoTest, AllType) {
    ZTypeTest tproto = {};
    tproto.a         = 1;
    tproto.b         = "field set test";
    tproto.c         = false;
    tproto.d         = 3.141592654;
    tproto.e         = {1, 2, 3};
    tproto.f         = {{1, 1}, {2, 2}};
    tproto.g         = {1.1, 2.2, 3.3, 4.4, 5.5};
    tproto.h         = {"a", "b", "c"};
    tproto.i         = {1.1F, 2.2F, 3.3F, 4.4F, 5.5F, 6.6F, 7.7F};
    tproto.j         = {1, 2, 3, 4, 5, 6, 7};
    tproto.k         = {1, "hello", true, 3.141592654, {1, 2, 3}, {{1, 1}, {2, 2}}, TEnum_A};
    tproto.l         = std::make_shared<std::string>("hello shared ptr");
    tproto.p         = {{1, "hello"}, {2, "world"}, {1, "world"}};
    tproto.q         = {"a", "b", "c", "a", "b"};
    tproto.t         = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    tproto.v         = {0x3f3f3f};
    tproto.w         = {"hello", "world"};
    tproto.x         = 1;
    tproto.y         = "hello";

    auto data = tproto.makeProto().toData();
    data.push_back('\0');
    EXPECT_STREQ(gZTypeTestStr, data.data());
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);

    NEKO_LOG_DEBUG("test", "{}", make_enum_string<TEnum>("{enum}:{num},"));
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}