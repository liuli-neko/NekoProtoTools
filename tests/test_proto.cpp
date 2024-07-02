#include <gtest/gtest.h>

#include "../core/binary_serializer.hpp"
#include "../core/dump_to_string.hpp"
#include "../core/json_serializer.hpp"
#include "../core/json_serializer_binary.hpp"
#include "../core/json_serializer_container.hpp"
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

#if NEKO_CPP_PLUS < 17
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
        if (result == nullptr || !value.IsArray() || value.Size() != 8) {
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

class ProtoTest : public testing::Test {
protected:
    virtual void SetUp() { factory.reset(new ProtoFactory()); }
    virtual void TearDown() {}
    std::unique_ptr<ProtoFactory> factory;
};

TEST_F(ProtoTest, StructSerialize) {
#if NEKO_CPP_PLUS >= 17
    EXPECT_EQ(factory->proto_type<TestP>(), 3);
#else
    EXPECT_EQ(factory->proto_type<TestP>(), 2);
#endif
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
#if NEKO_CPP_PLUS >= 17
    testp.l = "this a test for variant";
#endif
    std::vector<char> data;
    data = testp.makeProto().toData();
    data.push_back('\0');
#if NEKO_CPP_PLUS >= 17
    EXPECT_STREQ(data.data(), "{\"a\":3,\"b\":\"Struct "
                              "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,"
                              "0,0],\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},"
                              "[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,\"hello\"],\"l\":\"this a test for variant\"}");
#else
    EXPECT_STREQ(data.data(),
                 "{\"a\":3,\"b\":\"Struct "
                 "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":1,"
                 "\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],1],\"j\":[1,\"hello\"]}");
#endif
}

TEST_F(ProtoTest, StructDeserialize) {
    std::string str = "{\"a\":3,\"b\":\"Struct "
                      "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],"
                      "\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],"
                      "\"TEnum_A(1)\"],\"j\":[1,\"hello\"],\"k\":1,\"l\":1.114514}";
    std::vector<char> data(str.begin(), str.end());
    TestP testp;
    EXPECT_TRUE(testp.makeProto().formData(data));
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
    NEKO_LOG_INFO("{}", SerializableToString(testp));
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
                      "\"h\":\"TEnum_A(1)\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],"
                      "\"TEnum_A(1)\"],\"j\":[1,\"hello\"],\"l\":23}";
    auto proto      = factory->create("TestP");
    proto->formData(std::vector<char>(str.data(), str.data() + str.length()));
    auto rawp = proto->cast<TestP>(); // success cast

    EXPECT_TRUE(proto->cast<BinaryProto>() == nullptr); // failed cast

    // get field test
    int a = {};
    EXPECT_TRUE(proto->getField("a", &a));                                      // success get field
    EXPECT_FALSE(proto->getField("b", &a));                                     // get field by wrong type
    EXPECT_FALSE(proto->getField("unexist field", &a));                         // get unexist field
    EXPECT_STREQ(proto->getField<std::string>("b", "").c_str(), "Struct test"); // success get field
    EXPECT_TRUE(proto->getField<bool>("c", false));                             // success get field
    EXPECT_FALSE(proto->getField<bool>("b", false));                            // get field by wrong type
    EXPECT_FALSE(proto->getField<bool>("unexist field", false));                // get unexist field
    EXPECT_STREQ(proto->getField<std::string>("", "false").c_str(), "false");   // get unexist field
    EXPECT_FALSE(proto->getField<bool>("", false));                             // get unexist field
    EXPECT_EQ(a, 3);

    // set field test
    EXPECT_TRUE(proto->setField("a", 14));                            // success set field
    EXPECT_TRUE(proto->setField("b", std::string("field set test"))); // success set field
    EXPECT_TRUE(proto->setField("c", false));                         // success set field
    EXPECT_FALSE(proto->setField("a", 3.1234));                       // set field by wrong type
    EXPECT_FALSE(proto->setField("unexist field", 3.1234));           // set unexist field
    EXPECT_FALSE(proto->setField("", 3.1234));                        // set unexist field
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
    NEKO_LOG_INFO("{}", SerializableToString(*rawp));
}

TEST_F(ProtoTest, InvalidParams) {
    std::string str = "{\"a\":3}";
    TestP p;
    EXPECT_FALSE(p.makeProto().formData(std::vector<char>(str.data(), str.data() + str.length())));
    EXPECT_TRUE(factory->create("InvalidP") == nullptr);
    EXPECT_TRUE(factory->create(-1) == nullptr);
    str = "{\"a\":3.213123,\"b\":123"
          ",\"c\":12,\"d\":\"dddd\",\"f\":[1,2,3],\"e\":{\"a\":1,\"b\":2},\"h\":[1,2,3,0,0],"
          "\"g\":\"TEnum_A(1)\",\"j\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],"
          "\"TEnum_A(1)\"],\"i\":[1,\"hello\"],\"l\":23}";
    EXPECT_FALSE(p.makeProto().formData(std::vector<char>(str.data(), str.data() + str.length())));
    str = "aaaaaaaaaaaaa";
    EXPECT_FALSE(p.makeProto().formData(std::vector<char>(str.data(), str.data() + str.length())));
    str = "{\"a\":3.213123,\"b\":123"
          ",\"c\":12,\"d\":\"dddd\",\"f\":23,\"e\":null,\"h\":23.22,"
          "\"g\":\"TEnum_A(1)\",\"j\":[1,\"hello\",true,3.141592654,[1,2,3],"
          "\"TEnum_A(1)\"],\"i\":[1,\"hello\"],\"l\":23}";
    EXPECT_FALSE(p.makeProto().formData(std::vector<char>(str.data(), str.data() + str.length())));
    auto d = TestP::ProtoType::Serialize(p);
    EXPECT_FALSE(d.empty());
    EXPECT_FALSE(TestP::ProtoType::Deserialize(std::vector<char>(str.data(), str.data() + str.length()), p));
}

TEST_F(ProtoTest, BinaryProto) {
    BinaryProto proto;
    proto.a        = 24;
    proto.b        = "hello Neko Proto";
    proto.c        = 0x3f3f3f;
    auto data      = proto.makeProto().toData();
    auto base64str = Base64Covert::Encode(data);
    base64str.push_back('\0');
    EXPECT_STREQ(base64str.data(), "AAAAGAAAAAAAAAAQaGVsbG8gTmVrbyBQcm90bwA/Pz8=");
    NEKO_LOG_INFO("{}", SerializableToString(proto));
}

// struct test
#if NEKO_CPP_PLUS >= 17
struct TestStruct_1 {
    std::vector<int> field_0_VxQU2 = {-1303476798, 636087841,   -991516800, 1729982524, -664473937,
                                      365625509,   -1188336142, 1070174036, -319992932, -1671208923};

    NEKO_SERIALIZER(field_0_VxQU2)
    NEKO_DECLARE_PROTOCOL(TestStruct_1, JsonSerializer)
};

struct TestStruct_2 {
    bool field_0_5iZxY   = true;
    double field_1_Awhr8 = 1.8234809459629538e+299;

    NEKO_SERIALIZER(field_0_5iZxY, field_1_Awhr8)
    NEKO_DECLARE_PROTOCOL(TestStruct_2, JsonSerializer)
};

struct TestStruct_3 {
    std::string field_0_TNIeV = "IRXBIWLwpe";
    std::string field_1_gZcO1 = "aGEmBxVTN5";
    bool field_2_8aFUC        = false;

    NEKO_SERIALIZER(field_0_TNIeV, field_1_gZcO1, field_2_8aFUC)
    NEKO_DECLARE_PROTOCOL(TestStruct_3, JsonSerializer)
};

struct TestStruct_4 {
    int field_0_AcPcF              = 1661638798;
    int field_1_OhUYJ              = -1193634240;
    std::vector<int> field_2_ZYt9z = {2146707235, 2087309939,  1010310152, 1653508214,  -1085370252,
                                      1945350993, -1327495987, 627017663,  -1156751299, 114894873};
    double field_3_Belwr           = -1.851280738575505e+299;

    NEKO_SERIALIZER(field_0_AcPcF, field_1_OhUYJ, field_2_ZYt9z, field_3_Belwr)
    NEKO_DECLARE_PROTOCOL(TestStruct_4, JsonSerializer)
};

struct TestStruct_5 {
    std::string field_0_vTYkM      = "QM352irEJj";
    int field_1_SNVoB              = -1659854616;
    double field_2_smUks           = 9.939052837023292e+299;
    int field_3_EBttJ              = 1634133016;
    std::vector<int> field_4_C0rtA = {1570369841,  -1790060393, 1875821487,  -370981138,  985050100,
                                      -1435166299, -158961214,  -1965774228, -1862695458, -1295531408};

    NEKO_SERIALIZER(field_0_vTYkM, field_1_SNVoB, field_2_smUks, field_3_EBttJ, field_4_C0rtA)
    NEKO_DECLARE_PROTOCOL(TestStruct_5, JsonSerializer)
};

struct TestStruct_6 {
    std::string field_0_b3BJ9                = "Sc8OzSLE4R";
    bool field_1_RTCFt                       = false;
    std::map<std::string, int> field_2_tJu3L = {{"ahHyq4e0FW", 1552399962},  {"CLDo3SWrX4", -1302427362},
                                                {"WiTZJUgltc", -1168387995}, {"0oDJPcZHEg", 1439556402},
                                                {"yCbrEuwqX7", 2056298920},  {"heydhKPbxA", 55895328},
                                                {"4WOeIkZgJo", 1915432394},  {"HwHCIm9Ycb", -1525230409},
                                                {"FHnKDkZKwE", 2105295171},  {"62hYbArQGQ", 796157191}};
    bool field_3_FjhPW                       = true;
    double field_4_3Kr32                     = -8.749991922931685e+299;
    int field_5_fg2X4                        = 196522747;

    NEKO_SERIALIZER(field_0_b3BJ9, field_1_RTCFt, field_2_tJu3L, field_3_FjhPW, field_4_3Kr32, field_5_fg2X4)
    NEKO_DECLARE_PROTOCOL(TestStruct_6, JsonSerializer)
};

struct TestStruct_7 {
    bool field_0_3z5T7                       = false;
    double field_1_D0vpR                     = 9.334586894821297e+299;
    std::map<std::string, int> field_2_WRWUG = {{"e4lvL287Os", -561115363},  {"F6Z2vvxu3q", -1669336129},
                                                {"BFPaQJUkOs", 137688080},   {"zY4akvnlxX", -1889204862},
                                                {"9VCNswi0O2", -615244861},  {"GYlaDrVAPe", 792689941},
                                                {"0Mpq3oJOUX", -1773097592}, {"YDM9xqwADI", 445206905},
                                                {"wkn2org39u", -1397971640}, {"Np36teywP2", 1445088232}};
    std::string field_3_Ysxe7                = "jKpuYjcCkw";
    std::map<std::string, int> field_4_c7I8J = {{"4JtRq0yDXF", -1106774063}, {"KD77jpJo3T", -1172179429},
                                                {"6EmFXNNfg9", -1401739684}, {"dwZ8gnwZI1", 67100063},
                                                {"etexRTzMdh", 1281692562},  {"WrbyCoDBDc", -147987615},
                                                {"h3WMVPtiBu", 1141396250},  {"QDCWVXd8l9", -1713748885},
                                                {"GXJhlXUtez", 975581660},   {"vHqqZ4Touo", -1443375674}};
    int field_5_dlac8                        = -1180939915;
    double field_6_wzhPL                     = 2.037513968034562e+299;

    NEKO_SERIALIZER(field_0_3z5T7, field_1_D0vpR, field_2_WRWUG, field_3_Ysxe7, field_4_c7I8J, field_5_dlac8,
                    field_6_wzhPL)
    NEKO_DECLARE_PROTOCOL(TestStruct_7, JsonSerializer)
};

struct TestStruct_8 {
    std::string field_0_TX0Qu      = "w6AoPAX6qf";
    std::vector<int> field_1_nzhVL = {-707140474, 431565436,   -1551195325, 1060328527, 808942624,
                                      1826032904, -1885516077, -1223255586, 1780000919, 831676668};
    bool field_2_UhFvx             = true;
    std::vector<int> field_3_8m2Oo = {-2103973190, -1159813699, 962782793,  1645486993, 1384741956,
                                      -64164509,   -524775054,  -833820206, 984307700,  504147998};
    std::vector<int> field_4_qGnnY = {-1805159016, -1404005359, 2047243918, -600091429, -932362989,
                                      1990907740,  -893551758,  293924709,  -143274695, -1180296799};
    std::vector<int> field_5_eGRND = {1286915394, -1932195953, -2062919627, 1427740637,  969403866,
                                      906072091,  -1025711657, 1528566097,  -1762693740, 445020271};
    int field_6_F2bQJ              = -1576878479;
    int field_7_gMPgO              = -140460733;

    NEKO_SERIALIZER(field_0_TX0Qu, field_1_nzhVL, field_2_UhFvx, field_3_8m2Oo, field_4_qGnnY, field_5_eGRND,
                    field_6_F2bQJ, field_7_gMPgO)
    NEKO_DECLARE_PROTOCOL(TestStruct_8, JsonSerializer)
};

struct TestStruct_9 {
    bool field_0_mKGii                       = false;
    int field_1_2fntF                        = 1431714376;
    std::vector<int> field_2_VqFpW           = {-1462612226, -1038748748, -1839934088, -1284262904, -1855872667,
                                                677903695,   -307049640,  -281883507,  -378661075,  1377424481};
    bool field_3_qrnPp                       = true;
    std::map<std::string, int> field_4_wMDg7 = {{"n65UEH7no3", 879076386},   {"SQ81yylc4m", -197558902},
                                                {"Bm0dnjE114", -1594021619}, {"SMK5Aabcy7", -694750448},
                                                {"WWRZ9q6Kd5", -1952829802}, {"1DLc6fHG13", 2062304569},
                                                {"sPfxnIrFj1", 874968073},   {"74LCWSnGC1", -1301373156},
                                                {"SLhbbpP9Am", 929735946},   {"oSDTOReUuF", 1137541383}};
    int field_5_kSaZj                        = -1847112382;
    std::vector<int> field_6_fQ45q           = {339287397, 1959084000, -440055945,  -939306983, 722227858,
                                                591684379, 1292113210, -1051930260, 2110201950, -1646296554};
    double field_7_ppJwn                     = 2.181325028499985e+299;
    double field_8_QJl6W                     = -4.944086537154058e+299;

    NEKO_SERIALIZER(field_0_mKGii, field_1_2fntF, field_2_VqFpW, field_3_qrnPp, field_4_wMDg7, field_5_kSaZj,
                    field_6_fQ45q, field_7_ppJwn, field_8_QJl6W)
    NEKO_DECLARE_PROTOCOL(TestStruct_9, JsonSerializer)
};

struct TestStruct_10 {
    std::map<std::string, int> field_0_KFHZx = {{"havsiRomUO", -1495454041}, {"rDaeGG5ZTo", 224879970},
                                                {"smaQGbgFoU", -1210265489}, {"zpAXAuuQFB", -2046207857},
                                                {"yFZJW4qwg5", -11241242},   {"5HfmoqYYNY", 98967938},
                                                {"r1hJqBlP15", 1133240743},  {"GvtV8R4FBq", 1854200512},
                                                {"IJyo6najmC", 462101157},   {"cwEwY6yJH2", -793597976}};
    std::vector<int> field_1_hhaj8           = {-783213428, -1064225895, 425804589,  -590190615, 985211111,
                                                1081518842, 48218918,    1703493918, -173322037, -1651004201};
    bool field_2_KQlXO                       = true;
    std::vector<int> field_3_R2kXR           = {-906477422, -645559302, 282321310, -1339941021, 329414541,
                                                -810807126, 1184793468, 158989611, -959363502,  -45541305};
    double field_4_4XdlZ                     = -2.0444423872431487e+299;
    bool field_5_evoQC                       = true;
    bool field_6_Ce1PE                       = false;
    bool field_7_K6fIt                       = true;
    std::vector<int> field_8_54FI1           = {-252669677, -1494999350, 64840047,   2145396933, 1260546172,
                                                -627961708, -313578823,  -621709602, 1215021058, -779952205};
    double field_9_s2UeV                     = 2.9372331354867702e+299;

    NEKO_SERIALIZER(field_0_KFHZx, field_1_hhaj8, field_2_KQlXO, field_3_R2kXR, field_4_4XdlZ, field_5_evoQC,
                    field_6_Ce1PE, field_7_K6fIt, field_8_54FI1, field_9_s2UeV)
    NEKO_DECLARE_PROTOCOL(TestStruct_10, JsonSerializer)
};

struct TestStruct_11 {
    std::string field_0_Lahct                = "FC1h6mx4I0";
    std::map<std::string, int> field_1_aw939 = {
        {"CRGfTBqPOV", 668198021},  {"2hy7PwZT31", -1079767253}, {"VFgUVbEoI2", 186939967}, {"K8i23ZGJWl", -226048837},
        {"hyUVdfy1Pk", 820381214},  {"PNxDeral3T", 1312258055},  {"X4mA08zZtT", 613859423}, {"8SjxLpn7JB", -1859710014},
        {"oMXqykemna", -945526981}, {"G6zbzPn1PJ", 915780344}};
    std::string field_2_GsXEA                = "AZlkapiy56";
    bool field_3_vq13f                       = true;
    std::map<std::string, int> field_4_r7FwN = {{"i89cSqxoFJ", -1679613183}, {"ucVbsThP2r", -90413322},
                                                {"6mxU7K2pdv", 429347904},   {"2eyDm6OvPG", -133861309},
                                                {"fZ9kpe7V8t", 1672358963},  {"hurYtQH7eu", 1632630610},
                                                {"d6XqaDZBFY", -1187584538}, {"40hX7PdaVX", -778704261},
                                                {"U2QofoD9J4", 1837399168},  {"WmyBMPMXAe", 399177191}};
    std::string field_5_LPdJn                = "kt3JC7M7rA";
    std::map<std::string, int> field_6_UYN1k = {{"K18wvwGpwR", -952693066},  {"yukpEeTMiE", -1388708548},
                                                {"BZG3fCeU5E", 1017439119},  {"fLcXGFyNTF", 1950125644},
                                                {"THcanti9Er", 1535913583},  {"AqWDRtVrWe", -1840113418},
                                                {"RaQlVR9ias", 1717144566},  {"vIGhDnZyo1", 542046729},
                                                {"pn7hABtyFh", -2095261745}, {"SITN77YWiP", -1645915781}};
    std::vector<int> field_7_NW2zM           = {2137547146, -636710084, -1371054304, 103731950, 1594568499,
                                                1863769137, 1371973805, -1092710635, 127848089, -2129919491};
    bool field_8_bydWV                       = false;
    int field_9_TckjT                        = -1136967562;
    double field_10_3IOUB                    = -6.75525271817254e+299;

    NEKO_SERIALIZER(field_0_Lahct, field_1_aw939, field_2_GsXEA, field_3_vq13f, field_4_r7FwN, field_5_LPdJn,
                    field_6_UYN1k, field_7_NW2zM, field_8_bydWV, field_9_TckjT, field_10_3IOUB)
    NEKO_DECLARE_PROTOCOL(TestStruct_11, JsonSerializer)
};

struct TestStruct_12 {
    double field_0_XCGro                      = -7.450100228285111e+299;
    std::string field_1_bys2q                 = "hNV1AjOUe6";
    bool field_2_9SgwO                        = false;
    std::vector<int> field_3_aGEeN            = {307291796, 1483535330, 2018477363,  -799411059, -1246306780,
                                                 -44655970, 1395219667, -1614456090, 842509812,  -733928503};
    double field_4_PNKrc                      = 6.174446445780748e+299;
    std::map<std::string, int> field_5_Qigly  = {{"iQk7ZVmBSV", 935267865},  {"OgXbp1wqyV", -1298672115},
                                                 {"SQfyFWve79", 1173603130}, {"FwY8EuWRwJ", -2078149448},
                                                 {"CzHsqxQISG", 1253111289}, {"xVot6g6dYL", 805787109},
                                                 {"eUAzdhjrew", -777514127}, {"mCiKgP1iYs", 1323233945},
                                                 {"f2wrJIvXc3", -737156435}, {"xew7vuX4EG", 481650435}};
    std::vector<int> field_6_17rZJ            = {1692494731, -641583597, 334872774,   1974239226, 180858666,
                                                 183676321,  1610919088, -2050127153, 154241722,  -289838027};
    int field_7_9nJuF                         = 1164147709;
    double field_8_YfaBp                      = 2.0190919606282057e+299;
    int field_9_W71MG                         = 489605656;
    std::map<std::string, int> field_10_52guk = {
        {"mQKBRzt5Rp", -957291013}, {"ChrpnHivJr", 889617370}, {"QM5meyJqFf", 1432599828},  {"GcHD1ZApId", -1154048722},
        {"nqONrDEweT", 2341009},    {"4KFTKhUPoz", 506178390}, {"k7DJCfSuRl", -1757937915}, {"b9TIpspzr5", 956490052},
        {"TW87LnsJSm", 1579141968}, {"aHfXUexs0N", 1641939469}};
    std::string field_11_m1LhZ = "feAofChVVD";

    NEKO_SERIALIZER(field_0_XCGro, field_1_bys2q, field_2_9SgwO, field_3_aGEeN, field_4_PNKrc, field_5_Qigly,
                    field_6_17rZJ, field_7_9nJuF, field_8_YfaBp, field_9_W71MG, field_10_52guk, field_11_m1LhZ)
    NEKO_DECLARE_PROTOCOL(TestStruct_12, JsonSerializer)
};

struct TestStruct_13 {
    double field_0_crnth                     = 4.8155193540237054e+299;
    std::vector<int> field_1_FnLs0           = {240392687,  -2092681892, 1109405762,  -1220141697, 1472084186,
                                                -669318899, -1850688626, -2024362222, 298132585,   1689397292};
    bool field_2_HSeNh                       = true;
    bool field_3_WyVWQ                       = false;
    std::map<std::string, int> field_4_NWHng = {
        {"Bzi2wCzWi3", 854686066},   {"Ai3pK0V2Le", 141672047},  {"1GLQQlyYuu", 1471443829}, {"Iybu6tb21p", 166122858},
        {"H5wDzgAD3z", -2030730806}, {"Au7rsz4GSw", 1538393020}, {"1wIZ8DA6WB", 1813233888}, {"kbW9rBUaSj", 872743700},
        {"6DsyP9n1iG", 1922848006},  {"aQE0rcmSvD", 1371241446}};
    std::vector<int> field_5_NYSvM            = {-677321194, -1874250655, 1957124263, 1038855721, -981345291,
                                                 2141714046, -810522595,  1134711886, 549048405,  -515021334};
    int field_6_TqIbX                         = 1032610142;
    std::map<std::string, int> field_7_NboCS  = {{"6RcPahNqwy", -243119753},  {"VIywGKlB0E", -1224721740},
                                                 {"WsJisUzjGr", 650886963},   {"L5uauj73LS", 1699688734},
                                                 {"9r7uM3njQM", -1785534166}, {"MdvoO0xqQ7", 609626628},
                                                 {"rHtQGYKLWa", 1813046762},  {"WqIoynaTxI", -1942198960},
                                                 {"V3RWpZzTb5", 587151661},   {"rNjFv1ivFc", -2037495891}};
    std::map<std::string, int> field_8_QHTeT  = {{"Zb4leZ3xwM", 627506223},   {"S2omxTGLLu", 521030644},
                                                 {"6vs6wqu2KG", 1737278059},  {"QpveeT37H1", -1712252165},
                                                 {"Ls8Zn2lgHM", 1382306678},  {"O284LVmvMN", -465171232},
                                                 {"bt0k8hdGzN", -1937216194}, {"NUdqZzKCBQ", -729752605},
                                                 {"m3epufETAq", 118608879},   {"vAWsPJEtDq", -1228871303}};
    std::string field_9_KMboJ                 = "XYvrdgJmdc";
    std::map<std::string, int> field_10_Jx5hn = {{"H6hbsRB5O4", -1315664606}, {"66zPTuE5IO", 282524826},
                                                 {"bL7JM3S5f1", 657691551},   {"FPySmGQxg5", -41110773},
                                                 {"5R75AoFwOF", 273511793},   {"I6kkPEgJ4B", -605806877},
                                                 {"0XoqrTVcI1", -1946103075}, {"XmkXevRq6h", 1683154180},
                                                 {"8Us80BKyjt", 938876682},   {"1rzUOInkgI", -1514440707}};
    double field_11_QxDok                     = -2.8425970885993798e+299;
    std::map<std::string, int> field_12_lmfjr = {{"u140HpgNIs", -1794508440}, {"8NqC69NQeX", -1802555164},
                                                 {"iQvCvxxTAt", -1142833323}, {"goyrKHtOds", -23046121},
                                                 {"mCxDLIP0Jq", 1030448555},  {"pUOu5Sesds", 64731182},
                                                 {"8DCsDJKyXt", -464572007},  {"gzze3p8uSq", -1114890700},
                                                 {"ns33WcZGcY", -959506981},  {"PePlNwnp7W", 981967312}};

    NEKO_SERIALIZER(field_0_crnth, field_1_FnLs0, field_2_HSeNh, field_3_WyVWQ, field_4_NWHng, field_5_NYSvM,
                    field_6_TqIbX, field_7_NboCS, field_8_QHTeT, field_9_KMboJ, field_10_Jx5hn, field_11_QxDok,
                    field_12_lmfjr)
    NEKO_DECLARE_PROTOCOL(TestStruct_13, JsonSerializer)
};

struct TestStruct_14 {
    int field_0_BZmee                         = -983271578;
    bool field_1_l35XQ                        = true;
    std::string field_2_VCFjZ                 = "xeVM2tBYLU";
    std::map<std::string, int> field_3_XLhbM  = {{"aCPkO5k4lM", 1343733612},  {"xDCZpK99Hg", -1393229280},
                                                 {"dBWBKKZQ91", -1948588020}, {"ImPgOtlEZV", -298135640},
                                                 {"J29kVZAuRh", -622348193},  {"Z8f9KhxKc3", 762966231},
                                                 {"irOfZfr5i9", -1318018034}, {"m1U7tPqzIk", 1685367377},
                                                 {"cr9thwJOkw", 215969512},   {"0u2mqrT6fg", -258813703}};
    std::map<std::string, int> field_4_XMQ56  = {{"1JbeZZvk7q", 255070590},   {"lBRGr8elm0", -43461965},
                                                 {"iggLGJPhRR", -1513394753}, {"tpHJ10Lycb", 578835878},
                                                 {"h25aiXk8kn", 372006089},   {"ElFgHElMbi", 1553936069},
                                                 {"paiFsZHEoW", 34061193},    {"UcRmUagFBe", -772447118},
                                                 {"vkrBd1YfyQ", -1726264636}, {"mFKWL46OjT", -122090130}};
    std::string field_5_g6DB1                 = "oKkBWOqEzA";
    bool field_6_V0rIH                        = true;
    std::string field_7_QK2OC                 = "t2xnzrLViM";
    std::vector<int> field_8_GgRJM            = {1444121976, 2137169886, 1560870867, 803804427,   -410679905,
                                                 439984001,  1663147266, 1417987392, -1039114897, 1022022623};
    int field_9_Dm4so                         = -991817995;
    std::map<std::string, int> field_10_8FMP6 = {
        {"aGelr0uaxZ", -2013881596}, {"De8xA7rwnF", -518024946}, {"Uqj2IPoMMO", -929076275}, {"VlMCvCjthJ", -639964566},
        {"G3bUd5B44Y", 1977207245},  {"9FSwzUplDt", 1401040549}, {"DrlaQmLvM5", -924992590}, {"9IeGtJswCf", 990198245},
        {"hPjvnymlTJ", -1949744316}, {"SAMkAqk5Bb", -1336466436}};
    std::map<std::string, int> field_11_7sz04 = {{"itP96DMzpp", -102498432},  {"kwOBrVWASl", -2068383048},
                                                 {"pbsXi7yZ0l", 469094642},   {"codeXQgUN6", -1105405557},
                                                 {"RqSGPBBJAR", 1462644576},  {"216WJiST98", -113158346},
                                                 {"XexPkVmDba", -1980610222}, {"5UxmubgGyS", -564494607},
                                                 {"evYAwzKpYD", 1952811564},  {"e8aFWPK57d", 1246586673}};
    int field_12_j00bw                        = -2073209248;
    std::vector<int> field_13_0K4q0           = {1461495778, -1326166658, -1457386238, -860077324, -2074219673,
                                                 -641212950, 943145450,   899035766,   119361461,  234305824};

    NEKO_SERIALIZER(field_0_BZmee, field_1_l35XQ, field_2_VCFjZ, field_3_XLhbM, field_4_XMQ56, field_5_g6DB1,
                    field_6_V0rIH, field_7_QK2OC, field_8_GgRJM, field_9_Dm4so, field_10_8FMP6, field_11_7sz04,
                    field_12_j00bw, field_13_0K4q0)
    NEKO_DECLARE_PROTOCOL(TestStruct_14, JsonSerializer)
};

struct TestStruct_15 {
    std::map<std::string, int> field_0_PODop = {{"Rtx88VRnKH", 721649196},   {"W80SFog8sy", -984665441},
                                                {"IEnY9T6hlV", -1724957980}, {"jdDtm5faLP", -1028829134},
                                                {"I5fWJTBNWJ", 765317355},   {"T33qRCnoyb", -769822791},
                                                {"vrN4OU90x7", 850621353},   {"IFFTD8Lonq", 608078787},
                                                {"zri2Ffs2bp", -471980093},  {"Qw4g8WuAFp", 1698560570}};
    std::vector<int> field_1_pCElq           = {-663299515, -2017847267, -567560152, -376849519,  1257341503,
                                                95366613,   -679855024,  -733740954, -1023598222, 1005710744};
    bool field_2_8kQqk                       = false;
    double field_3_qsKAo                     = 8.246569859111843e+299;
    std::vector<int> field_4_Ap40I           = {1473678997, -1852616797, 1709409587, 463602058,   173830869,
                                                1558323878, -145079644,  -153423682, -1752683301, -751464476};
    double field_5_Rwomy                     = 3.4355730870353356e+299;
    std::vector<int> field_6_nC1C1           = {236937178,   -1941571069, 2084791379,  1649296648,  -78037929,
                                                -1384989015, 1581197402,  -1555291069, -1300092281, -891466542};
    std::map<std::string, int> field_7_kcav4 = {
        {"WwQ5wOmGaK", 342478752},   {"DolXcma7GJ", 1414349062}, {"7sEcOjKXwu", 507259562},  {"hxNzz7diFI", 1130011101},
        {"r5zvDca7z1", -1930558878}, {"2B8j2MeCGl", 803434452},  {"J96Rlt3fA8", -399963266}, {"5rKQEuxxev", -110750015},
        {"XjoDooGf4L", 794780914},   {"40pZOF6tXK", -1401000474}};
    bool field_8_tEL14                        = false;
    int field_9_Pk7Da                         = 2127479088;
    int field_10_wbt8J                        = 1924121673;
    int field_11_nOc2r                        = -2111956426;
    bool field_12_hKDWp                       = false;
    double field_13_H9D9k                     = -3.96599851349895e+299;
    std::map<std::string, int> field_14_IQyg7 = {{"1jtaSwNWQX", 2127054395},  {"PdLLV3gfMX", 1003473082},
                                                 {"MG9SSx9Kl4", -870917071},  {"2b2BMnYD4q", 703202458},
                                                 {"SzFkSbOk9R", 1684460167},  {"6coquxJ11T", 726747570},
                                                 {"7Gl9QKdZmh", 2107334612},  {"Gp6haV9XaS", -1828347945},
                                                 {"G4CENLB9KX", -1629701910}, {"v42pbU4m0p", -1237037189}};

    NEKO_SERIALIZER(field_0_PODop, field_1_pCElq, field_2_8kQqk, field_3_qsKAo, field_4_Ap40I, field_5_Rwomy,
                    field_6_nC1C1, field_7_kcav4, field_8_tEL14, field_9_Pk7Da, field_10_wbt8J, field_11_nOc2r,
                    field_12_hKDWp, field_13_H9D9k, field_14_IQyg7)
    NEKO_DECLARE_PROTOCOL(TestStruct_15, JsonSerializer)
};

struct TestStruct_16 {
    std::vector<int> field_0_9vhZB           = {502124849, 1860381198,  1693687201, -1812960103, 1532878188,
                                                923311987, -1987168209, 189135350,  -928337349,  -640119193};
    std::map<std::string, int> field_1_bM7Gy = {
        {"CDteKYkESZ", 72168148},   {"9nTfO4SbJs", 681691845},  {"Vcqfom8KB0", 830939497},  {"VLE55UPcAF", -220927810},
        {"RPLyl0Bqun", -73563845},  {"6rJa2jZxuo", -684568497}, {"dm2hc1B5HY", -764790919}, {"6SrCYzsYu5", 1936079389},
        {"0TKqdgheu3", 1595233713}, {"meKsKqHeYG", 101907017}};
    std::map<std::string, int> field_2_vJcFY = {{"1tE2ihSL6t", 218929018},  {"YSHy1l5aYw", -750030716},
                                                {"wzBxwrfnTm", -614297350}, {"ozoulAm4xo", -1154481661},
                                                {"8NgBJXJGff", 588977541},  {"Oek40yqzPM", -1152508248},
                                                {"uPeZzh6TtQ", 1614558104}, {"aTHFJCA84M", -1184829909},
                                                {"LwfT4lCAuK", -945693145}, {"E4nluFWhPT", 1471417554}};
    int field_3_aPcfB                        = 910780630;
    std::string field_4_VcoCS                = "6MT7LRCvh1";
    std::string field_5_3U3Kt                = "eFIkfo2fcZ";
    std::string field_6_Uf0SA                = "8qOCi2TGbm";
    int field_7_GHcW8                        = -1779301584;
    std::map<std::string, int> field_8_fZV6Y = {
        {"i7u0OTHxdE", 816837590},   {"LXx77YisvV", 1035250689}, {"Ek6ue6G0K3", 1186769123}, {"GIHF1ebI3o", 1811643631},
        {"MfwXLkT6lL", -785719414},  {"zNwdnWhcz5", 442218780},  {"IGCMSrUUac", 997559479},  {"tbY8GRdV78", 776006500},
        {"e23ZhzK2Bh", -1182341310}, {"iJmurqKZ41", -934515970}};
    std::vector<int> field_9_m3p3L            = {560298613, -1813758336, 1293788907, -1470175642, 1009672286,
                                                 966491271, -200143759,  -58113533,  746693124,   1608144219};
    int field_10_FCBgZ                        = -414577883;
    bool field_11_BzyK9                       = true;
    std::map<std::string, int> field_12_3q0WP = {{"e7wVPHUJY8", 683186138},  {"YcZXh2erDP", -2018453194},
                                                 {"1cmyBcHqku", 1102140480}, {"PYYdFnxFuo", -1157602078},
                                                 {"DSymF2mNjf", -510968137}, {"aI5HKKTaWx", -387429983},
                                                 {"4CFA1wKqD0", -47077801},  {"TGgIWp8XBR", 199339788},
                                                 {"3N2VnrPi92", 930104177},  {"bkgWzOp9Zb", 316244984}};
    std::string field_13_nBzbe                = "xMJnlIhHki";
    std::string field_14_lNU78                = "LKfFhSVdfh";
    std::vector<int> field_15_V6y2D           = {-368609336, 549943753,  -1183787019, -1596366860, -189369323,
                                                 1557874014, 1587989578, -829162177,  -1670750029, 1489303177};

    NEKO_SERIALIZER(field_0_9vhZB, field_1_bM7Gy, field_2_vJcFY, field_3_aPcfB, field_4_VcoCS, field_5_3U3Kt,
                    field_6_Uf0SA, field_7_GHcW8, field_8_fZV6Y, field_9_m3p3L, field_10_FCBgZ, field_11_BzyK9,
                    field_12_3q0WP, field_13_nBzbe, field_14_lNU78, field_15_V6y2D)
    NEKO_DECLARE_PROTOCOL(TestStruct_16, JsonSerializer)
};

struct TestStruct_17 {
    std::string field_0_Vk5dY                = "NY7qfSQkt7";
    bool field_1_As9i4                       = false;
    double field_2_7THwC                     = 5.014561638365802e+298;
    int field_3_pnOTa                        = 371326658;
    double field_4_Yq5PZ                     = 6.133984801926284e+299;
    std::map<std::string, int> field_5_B4zO3 = {
        {"7u8P1UItLY", 881156667},  {"WAT3upWfMX", 1646576607}, {"wOrKQcOA2K", 1418333920}, {"eKt1hU5bvI", 1095539227},
        {"HJVLp4iU3u", -646649749}, {"gRJrqA5ueL", -310611849}, {"QGPEdCnGOi", 788384332},  {"plAUahITUS", 1452469379},
        {"Q9s1u4CtUY", 1785502873}, {"SutIIqa3J3", 902021793}};
    std::vector<int> field_6_R31OI           = {-1469160926, -854721711,  -1692058696, 1569189080, -1347633582,
                                                1182714499,  -1346258888, -2146021644, 1448907134, -2051865504};
    std::map<std::string, int> field_7_Vc0ES = {
        {"o8xvANU81J", 1501534167}, {"TVEuHtUpOn", 1064216239}, {"mLr4EiSuwp", 876124074},   {"pugOyfkasv", -358349901},
        {"OKnNaOd3iN", 2134402222}, {"C6yxP5jZ1y", 117436308},  {"VHMiBQk5LO", -1313981608}, {"B59iCSidHU", 184855396},
        {"6Y9flKF92E", 1994423886}, {"pNgHdQJ3bT", 2113955930}};
    int field_8_t6Xct                         = -576233360;
    std::map<std::string, int> field_9_S0PaF  = {{"p7EqkLgVgk", -470667733},  {"9sdGSmLtbj", -1610304028},
                                                 {"0eRKH6RpLJ", 2060684821},  {"yKZkW0VqUr", -226568584},
                                                 {"rAMMmFoESQ", 1063502832},  {"DAmOs9pV53", -718574472},
                                                 {"S4y3NkmuVZ", 1093520110},  {"N077Wc9Az1", 2048642132},
                                                 {"gLVM267xNC", -1238500762}, {"RBa5gWPk3x", 221806519}};
    bool field_10_rGyV1                       = false;
    std::map<std::string, int> field_11_aCkWt = {{"9qpsmiDYg9", -1636781888}, {"5V7I1wFuGS", -1448353778},
                                                 {"GustBgpsoo", -798463579},  {"C09hg2VHHK", 1426709393},
                                                 {"zoIwl9tnbD", -971062325},  {"UNtPsLTSPa", -1491145582},
                                                 {"iNh3dCI8IZ", 327846200},   {"kNJE3AZtvW", -1642954410},
                                                 {"gyUu0rblX4", -1679612936}, {"TjJZFqa7RI", -820111512}};
    std::string field_12_I3mWW                = "wHtyT2k6uU";
    std::string field_13_k67IC                = "q0Xy63ZhiQ";
    std::string field_14_jAzgA                = "Z7LFoQHmU2";
    std::map<std::string, int> field_15_0o9jm = {
        {"LMJXL8aa2m", 808211469},   {"9VH0VUeJst", 934009851}, {"2MMeXJURF5", 1838211699}, {"upGCWHYV5B", -626272626},
        {"9FU2K6ol8A", -262527306},  {"ZjGuVm3KXc", 68744215},  {"0c9MUFhmaY", 1915874554}, {"n4pEtVc9RD", -255770193},
        {"vgil1YiLun", -1743583894}, {"jgshWyzl23", -538129648}};
    std::string field_16_a9p7L = "jG9w7rJNRU";

    NEKO_SERIALIZER(field_0_Vk5dY, field_1_As9i4, field_2_7THwC, field_3_pnOTa, field_4_Yq5PZ, field_5_B4zO3,
                    field_6_R31OI, field_7_Vc0ES, field_8_t6Xct, field_9_S0PaF, field_10_rGyV1, field_11_aCkWt,
                    field_12_I3mWW, field_13_k67IC, field_14_jAzgA, field_15_0o9jm, field_16_a9p7L)
    NEKO_DECLARE_PROTOCOL(TestStruct_17, JsonSerializer)
};

struct TestStruct_18 {
    double field_0_4dFfp                     = 8.796392227680598e+299;
    bool field_1_R81Mf                       = false;
    std::string field_2_VRfKe                = "X5Q1IE6Q2i";
    bool field_3_0e1rq                       = false;
    int field_4_G5q8e                        = 638930294;
    std::vector<int> field_5_j56PP           = {1879046659, -2099926999, 1545465879,  1895603498, 1677520061,
                                                418261673,  521261078,   -2046484413, 197842220,  -1149424962};
    double field_6_jCqfr                     = -8.049795184811597e+299;
    std::map<std::string, int> field_7_VAUJc = {
        {"qEvP5ByTcy", 105991868},   {"3syQJqhhX4", -1084251845}, {"e1x79me6S9", -614464154}, {"U1Re21NHqE", 980232978},
        {"x3EJbq4Nkl", -1286920480}, {"we4MozJ3yc", 498197462},   {"BglE9rfeST", 352983213},  {"gDaz5O4UPx", 764594339},
        {"jvLmp2tiAd", 1521639347},  {"9ACQQuhV6S", -1316442394}};
    std::vector<int> field_8_ayKVi           = {1885052645, 631053585, -206733899, -1907781698, -534657711,
                                                1635731737, 963250933, 63739822,   1396321790,  144078041};
    std::map<std::string, int> field_9_IYNJ1 = {{"FelwLGFm9E", -407804807},  {"EKVjDJRGyP", 480319439},
                                                {"GvlW3Wut5I", -1664563031}, {"6Stu5MuQMY", -1139950988},
                                                {"7DQ31shL6q", -619187234},  {"AxwLXYHX0d", -1436761037},
                                                {"iKes9w1TFm", 698202991},   {"LvPGn4OJlu", -1135334399},
                                                {"NLUPRlf79w", 78129412},    {"Y6aNPaUJab", -1242046999}};
    double field_10_nITZc                    = -3.1558283267192525e+299;
    std::vector<int> field_11_JVx2W          = {1412227684,  -826471194, -620960120, -1504500467, 846275591,
                                                -1236260599, -982209456, -97274455,  634568948,   -542755814};
    std::vector<int> field_12_Ca9P2          = {-1766928124, -1050710895, -888238415, -1041223271, 308750114,
                                                1524123572,  303879944,   -606079120, 1264918385,  -1624177776};
    bool field_13_JG3Oa                      = false;
    int field_14_BRz50                       = -1848725646;
    std::string field_15_NIkqR               = "RgkAIisP6T";
    std::vector<int> field_16_zYeHA          = {867558702,  215591064,  -545899175, 1422951659,  -2132416299,
                                                1835904514, -105785390, 474162390,  -1935653846, -1292298308};
    double field_17_DchRr                    = 6.178210949427803e+299;

    NEKO_SERIALIZER(field_0_4dFfp, field_1_R81Mf, field_2_VRfKe, field_3_0e1rq, field_4_G5q8e, field_5_j56PP,
                    field_6_jCqfr, field_7_VAUJc, field_8_ayKVi, field_9_IYNJ1, field_10_nITZc, field_11_JVx2W,
                    field_12_Ca9P2, field_13_JG3Oa, field_14_BRz50, field_15_NIkqR, field_16_zYeHA, field_17_DchRr)
    NEKO_DECLARE_PROTOCOL(TestStruct_18, JsonSerializer)
};

struct TestStruct_19 {
    bool field_0_0lwKQ                       = false;
    int field_1_aZJ3W                        = -78997776;
    bool field_2_qJbdP                       = false;
    int field_3_OdrzP                        = -528267170;
    std::string field_4_gqfNn                = "VbLGZkokgg";
    std::string field_5_NyfJJ                = "f54kgcoY2c";
    double field_6_AZy5l                     = 7.099564009838792e+299;
    std::map<std::string, int> field_7_VuXiT = {
        {"FK5otPQcdw", -1171740264}, {"jCuD9sKqBV", 1244552809}, {"IFvuMlPHiS", 2010034893}, {"goSdoOaa6M", -823204326},
        {"or0CFJUNFJ", -392418516},  {"xaRK5LhLpW", -307797930}, {"d431uWG3BN", 1877245248}, {"LP4kqHRS7y", 1165038220},
        {"tSexI6AbtZ", 1153557090},  {"d8rjvfHRMc", 407666110}};
    std::string field_8_tEGHO       = "C15xgo6sZD";
    double field_9_84gbl            = 4.484269602564921e+299;
    bool field_10_fjD6R             = true;
    bool field_11_nyni4             = false;
    std::string field_12_Y20tI      = "fvVtf3SLWY";
    bool field_13_2vogq             = false;
    double field_14_eXqBg           = 6.700139625165763e+298;
    double field_15_AcgTj           = -2.3879194910123957e+299;
    std::vector<int> field_16_R3CRl = {363267557,   -430587927,  2073602156,  -1297279996, 1283765347,
                                       -1298263059, -1995090881, -1985409024, -806126965,  -1663688891};
    double field_17_svhOB           = 8.432857782920547e+299;
    bool field_18_c2AxJ             = true;

    NEKO_SERIALIZER(field_0_0lwKQ, field_1_aZJ3W, field_2_qJbdP, field_3_OdrzP, field_4_gqfNn, field_5_NyfJJ,
                    field_6_AZy5l, field_7_VuXiT, field_8_tEGHO, field_9_84gbl, field_10_fjD6R, field_11_nyni4,
                    field_12_Y20tI, field_13_2vogq, field_14_eXqBg, field_15_AcgTj, field_16_R3CRl, field_17_svhOB,
                    field_18_c2AxJ)
    NEKO_DECLARE_PROTOCOL(TestStruct_19, JsonSerializer)
};

struct TestStruct_20 {
    std::map<std::string, int> field_0_KTZWz = {
        {"2G7yZB0r2Z", -625665831}, {"d3GKfzGVeq", 1498108226}, {"lnGtLdT0ev", -1810966741}, {"iJkkzQVWNw", 361444060},
        {"9cOgsFPk23", -894997434}, {"QZBnRpVQ8B", -178617731}, {"xBd1fW72qG", -133237643},  {"DWb3wlttjc", 1485783317},
        {"o3tqrAwpui", 771363062},  {"Sv0XeIxfze", -1727444620}};
    bool field_1_Aty0x                        = true;
    std::string field_2_oO4Ko                 = "b73wtS2coT";
    bool field_3_TQaZV                        = true;
    double field_4_cGgOX                      = 2.9858135849814157e+299;
    std::map<std::string, int> field_5_6mobp  = {{"YwVRXJ6z0x", -1389568592}, {"32dRxjTthA", -1852331417},
                                                 {"8lFyB0BBPc", 1746393568},  {"WHvqf9OXru", 1172254542},
                                                 {"j2AOV2VLiI", -163609600},  {"cJNTdPdZWU", -768675763},
                                                 {"liWx8wy7Yf", -922218171},  {"A1kYKpZcKO", -2055866632},
                                                 {"YYi24cbRbP", -575810540},  {"KDVZaF9Ix7", -77361587}};
    std::string field_6_f5Cbg                 = "SKgPPhOIff";
    int field_7_eVSBV                         = -1593796259;
    std::string field_8_7hjI6                 = "ClhcFBbAvv";
    std::vector<int> field_9_osByy            = {1553208990, -1934655667, 1977243992, -1014021622, -1090393519,
                                                 433021655,  1164153061,  -677631827, -437175391,  657304195};
    bool field_10_ksf4X                       = false;
    std::vector<int> field_11_xOiVC           = {1023802797, -1653092807, 642875270,  -141932242, 1490346517,
                                                 -780507387, 633502637,   -558240352, 1993888994, -2034633130};
    std::string field_12_XQg2J                = "84Qu67Hptp";
    std::map<std::string, int> field_13_JDmUg = {{"WqAm7w5xTV", 955874023},   {"DLiXSm0JcM", -114512883},
                                                 {"fq1maOU5XI", -1600401204}, {"53pKrSnhH2", -670267114},
                                                 {"slVgNMqtvV", -1063843661}, {"DonVwH0LZe", -1628754469},
                                                 {"NWskjnMypm", 1205626724},  {"xx6MdMrCMc", 1449176030},
                                                 {"15h3MVCigS", 85038409},    {"wWYKyOUcfb", 852418496}};
    std::map<std::string, int> field_14_TmHUg = {
        {"K2UwGRX2El", -1433250566}, {"unstSrbdhM", 1596386505}, {"F0CyrxiNRF", -16259182}, {"0uwI2tCKh9", 346420108},
        {"f49w30rLaI", 581805156},   {"SiuPGlvYrw", 923512232},  {"RWxS9gEdoO", 653843660}, {"gmduEhxvrG", 340610497},
        {"rXl8Rn27fd", -1001536822}, {"GdX2VyuFFW", -187843216}};
    bool field_15_IJcBN        = true;
    std::string field_16_I4RX1 = "OjkfGpD7gJ";
    bool field_17_FMaDg        = true;
    double field_18_uy3KX      = -8.008131144161175e+299;
    double field_19_0bTpy      = -2.0555281374100117e+299;

    NEKO_SERIALIZER(field_0_KTZWz, field_1_Aty0x, field_2_oO4Ko, field_3_TQaZV, field_4_cGgOX, field_5_6mobp,
                    field_6_f5Cbg, field_7_eVSBV, field_8_7hjI6, field_9_osByy, field_10_ksf4X, field_11_xOiVC,
                    field_12_XQg2J, field_13_JDmUg, field_14_TmHUg, field_15_IJcBN, field_16_I4RX1, field_17_FMaDg,
                    field_18_uy3KX, field_19_0bTpy)
    NEKO_DECLARE_PROTOCOL(TestStruct_20, JsonSerializer)
};

struct StructProto {
    TestStruct_1 a  = {};
    TestStruct_2 b  = {};
    TestStruct_3 c  = {};
    TestStruct_4 d  = {};
    TestStruct_5 e  = {};
    TestStruct_6 f  = {};
    TestStruct_7 g  = {};
    TestStruct_8 h  = {};
    TestStruct_9 i  = {};
    TestStruct_10 j = {};
    TestStruct_11 k = {};
    TestStruct_12 l = {};
    TestStruct_13 m = {};
    TestStruct_14 n = {};
    TestStruct_15 o = {};
    TestStruct_16 p = {};
    TestStruct_17 q = {};
    TestStruct_18 r = {};
    TestStruct_19 s = {};
    TestStruct_20 t = {};

    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)
    NEKO_DECLARE_PROTOCOL(StructProto, JsonSerializer)
};

constexpr static const char* StructProtoData =
    "{\"a\":{\"field_0_VxQU2\":[1,2,3]},\"b\":{\"field_0_5iZxY\":true,\"field_1_Awhr8\":1.8234809459629538e299},"
    "\"c\":{\"field_0_TNIeV\":\"IRXBIWLwpe\",\"field_1_gZcO1\":\"aGEmBxVTN5\",\"field_2_8aFUC\":false},\"d\":{"
    "\"field_0_AcPcF\":1661638798,\"field_1_OhUYJ\":-1193634240,\"field_2_ZYt9z\":[2146707235,2087309939,"
    "1010310152,1653508214,-1085370252,1945350993,-1327495987,627017663,-1156751299,114894873],\"field_3_Belwr\":-"
    "1.851280738575505e299},\"e\":{\"field_0_vTYkM\":\"QM352irEJj\",\"field_1_SNVoB\":-1659854616,\"field_2_"
    "smUks\":9.939052837023292e299,\"field_3_EBttJ\":1634133016,\"field_4_C0rtA\":[1570369841,-1790060393,"
    "1875821487,-370981138,985050100,-1435166299,-158961214,-1965774228,-1862695458,-1295531408]},\"f\":{\"field_0_"
    "b3BJ9\":\"Sc8OzSLE4R\",\"field_1_RTCFt\":false,\"field_2_tJu3L\":{\"0oDJPcZHEg\":1439556402,\"4WOeIkZgJo\":"
    "1915432394,\"62hYbArQGQ\":796157191,\"CLDo3SWrX4\":-1302427362,\"FHnKDkZKwE\":2105295171,\"HwHCIm9Ycb\":-"
    "1525230409,\"WiTZJUgltc\":-1168387995,\"ahHyq4e0FW\":1552399962,\"heydhKPbxA\":55895328,\"yCbrEuwqX7\":"
    "2056298920},\"field_3_FjhPW\":true,\"field_4_3Kr32\":-8.749991922931685e299,\"field_5_fg2X4\":196522747},"
    "\"g\":{\"field_0_3z5T7\":false,\"field_1_D0vpR\":9.334586894821297e299,\"field_2_WRWUG\":{\"0Mpq3oJOUX\":-"
    "1773097592,\"9VCNswi0O2\":-615244861,\"BFPaQJUkOs\":137688080,\"F6Z2vvxu3q\":-1669336129,\"GYlaDrVAPe\":"
    "792689941,\"Np36teywP2\":1445088232,\"YDM9xqwADI\":445206905,\"e4lvL287Os\":-561115363,\"wkn2org39u\":-"
    "1397971640,\"zY4akvnlxX\":-1889204862},\"field_3_Ysxe7\":\"jKpuYjcCkw\",\"field_4_c7I8J\":{\"4JtRq0yDXF\":-"
    "1106774063,\"6EmFXNNfg9\":-1401739684,\"GXJhlXUtez\":975581660,\"KD77jpJo3T\":-1172179429,\"QDCWVXd8l9\":-"
    "1713748885,\"WrbyCoDBDc\":-147987615,\"dwZ8gnwZI1\":67100063,\"etexRTzMdh\":1281692562,\"h3WMVPtiBu\":"
    "1141396250,\"vHqqZ4Touo\":-1443375674},\"field_5_dlac8\":-1180939915,\"field_6_wzhPL\":2.037513968034562e299},"
    "\"h\":{\"field_0_TX0Qu\":\"w6AoPAX6qf\",\"field_1_nzhVL\":[-707140474,431565436,-1551195325,1060328527,"
    "808942624,1826032904,-1885516077,-1223255586,1780000919,831676668],\"field_2_UhFvx\":true,\"field_3_8m2Oo\":[-"
    "2103973190,-1159813699,962782793,1645486993,1384741956,-64164509,-524775054,-833820206,984307700,504147998],"
    "\"field_4_qGnnY\":[-1805159016,-1404005359,2047243918,-600091429,-932362989,1990907740,-893551758,293924709,-"
    "143274695,-1180296799],\"field_5_eGRND\":[1286915394,-1932195953,-2062919627,1427740637,969403866,906072091,-"
    "1025711657,1528566097,-1762693740,445020271],\"field_6_F2bQJ\":-1576878479,\"field_7_gMPgO\":-140460733},"
    "\"i\":{\"field_0_mKGii\":false,\"field_1_2fntF\":1431714376,\"field_2_VqFpW\":[-1462612226,-1038748748,-"
    "1839934088,-1284262904,-1855872667,677903695,-307049640,-281883507,-378661075,1377424481],\"field_3_qrnPp\":"
    "true,\"field_4_wMDg7\":{\"1DLc6fHG13\":2062304569,\"74LCWSnGC1\":-1301373156,\"Bm0dnjE114\":-1594021619,"
    "\"SLhbbpP9Am\":929735946,\"SMK5Aabcy7\":-694750448,\"SQ81yylc4m\":-197558902,\"WWRZ9q6Kd5\":-1952829802,"
    "\"n65UEH7no3\":879076386,\"oSDTOReUuF\":1137541383,\"sPfxnIrFj1\":874968073},\"field_5_kSaZj\":-1847112382,"
    "\"field_6_fQ45q\":[339287397,1959084000,-440055945,-939306983,722227858,591684379,1292113210,-1051930260,"
    "2110201950,-1646296554],\"field_7_ppJwn\":2.181325028499985e299,\"field_8_QJl6W\":-4.944086537154058e299},"
    "\"j\":{\"field_0_KFHZx\":{\"5HfmoqYYNY\":98967938,\"GvtV8R4FBq\":1854200512,\"IJyo6najmC\":462101157,"
    "\"cwEwY6yJH2\":-793597976,\"havsiRomUO\":-1495454041,\"r1hJqBlP15\":1133240743,\"rDaeGG5ZTo\":224879970,"
    "\"smaQGbgFoU\":-1210265489,\"yFZJW4qwg5\":-11241242,\"zpAXAuuQFB\":-2046207857},\"field_1_hhaj8\":[-783213428,"
    "-1064225895,425804589,-590190615,985211111,1081518842,48218918,1703493918,-173322037,-1651004201],\"field_2_"
    "KQlXO\":true,\"field_3_R2kXR\":[-906477422,-645559302,282321310,-1339941021,329414541,-810807126,1184793468,"
    "158989611,-959363502,-45541305],\"field_4_4XdlZ\":-2.0444423872431487e299,\"field_5_evoQC\":true,\"field_6_"
    "Ce1PE\":false,\"field_7_K6fIt\":true,\"field_8_54FI1\":[-252669677,-1494999350,64840047,2145396933,1260546172,"
    "-627961708,-313578823,-621709602,1215021058,-779952205],\"field_9_s2UeV\":2.9372331354867702e299},\"k\":{"
    "\"field_0_Lahct\":\"FC1h6mx4I0\",\"field_1_aw939\":{\"2hy7PwZT31\":-1079767253,\"8SjxLpn7JB\":-1859710014,"
    "\"CRGfTBqPOV\":668198021,\"G6zbzPn1PJ\":915780344,\"K8i23ZGJWl\":-226048837,\"PNxDeral3T\":1312258055,"
    "\"VFgUVbEoI2\":186939967,\"X4mA08zZtT\":613859423,\"hyUVdfy1Pk\":820381214,\"oMXqykemna\":-945526981},\"field_"
    "2_GsXEA\":\"AZlkapiy56\",\"field_3_vq13f\":true,\"field_4_r7FwN\":{\"2eyDm6OvPG\":-133861309,\"40hX7PdaVX\":-"
    "778704261,\"6mxU7K2pdv\":429347904,\"U2QofoD9J4\":1837399168,\"WmyBMPMXAe\":399177191,\"d6XqaDZBFY\":-"
    "1187584538,\"fZ9kpe7V8t\":1672358963,\"hurYtQH7eu\":1632630610,\"i89cSqxoFJ\":-1679613183,\"ucVbsThP2r\":-"
    "90413322},\"field_5_LPdJn\":\"kt3JC7M7rA\",\"field_6_UYN1k\":{\"AqWDRtVrWe\":-1840113418,\"BZG3fCeU5E\":"
    "1017439119,\"K18wvwGpwR\":-952693066,\"RaQlVR9ias\":1717144566,\"SITN77YWiP\":-1645915781,\"THcanti9Er\":"
    "1535913583,\"fLcXGFyNTF\":1950125644,\"pn7hABtyFh\":-2095261745,\"vIGhDnZyo1\":542046729,\"yukpEeTMiE\":-"
    "1388708548},\"field_7_NW2zM\":[2137547146,-636710084,-1371054304,103731950,1594568499,1863769137,1371973805,-"
    "1092710635,127848089,-2129919491],\"field_8_bydWV\":false,\"field_9_TckjT\":-1136967562,\"field_10_3IOUB\":-6."
    "75525271817254e299},\"l\":{\"field_0_XCGro\":-7.450100228285111e299,\"field_1_bys2q\":\"hNV1AjOUe6\",\"field_"
    "2_9SgwO\":false,\"field_3_aGEeN\":[307291796,1483535330,2018477363,-799411059,-1246306780,-44655970,"
    "1395219667,-1614456090,842509812,-733928503],\"field_4_PNKrc\":6.174446445780748e299,\"field_5_Qigly\":{"
    "\"CzHsqxQISG\":1253111289,\"FwY8EuWRwJ\":-2078149448,\"OgXbp1wqyV\":-1298672115,\"SQfyFWve79\":1173603130,"
    "\"eUAzdhjrew\":-777514127,\"f2wrJIvXc3\":-737156435,\"iQk7ZVmBSV\":935267865,\"mCiKgP1iYs\":1323233945,"
    "\"xVot6g6dYL\":805787109,\"xew7vuX4EG\":481650435},\"field_6_17rZJ\":[1692494731,-641583597,334872774,"
    "1974239226,180858666,183676321,1610919088,-2050127153,154241722,-289838027],\"field_7_9nJuF\":1164147709,"
    "\"field_8_YfaBp\":2.0190919606282057e299,\"field_9_W71MG\":489605656,\"field_10_52guk\":{\"4KFTKhUPoz\":"
    "506178390,\"ChrpnHivJr\":889617370,\"GcHD1ZApId\":-1154048722,\"QM5meyJqFf\":1432599828,\"TW87LnsJSm\":"
    "1579141968,\"aHfXUexs0N\":1641939469,\"b9TIpspzr5\":956490052,\"k7DJCfSuRl\":-1757937915,\"mQKBRzt5Rp\":-"
    "957291013,\"nqONrDEweT\":2341009},\"field_11_m1LhZ\":\"feAofChVVD\"},\"m\":{\"field_0_crnth\":4."
    "8155193540237054e299,\"field_1_FnLs0\":[240392687,-2092681892,1109405762,-1220141697,1472084186,-669318899,-"
    "1850688626,-2024362222,298132585,1689397292],\"field_2_HSeNh\":true,\"field_3_WyVWQ\":false,\"field_4_NWHng\":"
    "{\"1GLQQlyYuu\":1471443829,\"1wIZ8DA6WB\":1813233888,\"6DsyP9n1iG\":1922848006,\"Ai3pK0V2Le\":141672047,"
    "\"Au7rsz4GSw\":1538393020,\"Bzi2wCzWi3\":854686066,\"H5wDzgAD3z\":-2030730806,\"Iybu6tb21p\":166122858,"
    "\"aQE0rcmSvD\":1371241446,\"kbW9rBUaSj\":872743700},\"field_5_NYSvM\":[-677321194,-1874250655,1957124263,"
    "1038855721,-981345291,2141714046,-810522595,1134711886,549048405,-515021334],\"field_6_TqIbX\":1032610142,"
    "\"field_7_NboCS\":{\"6RcPahNqwy\":-243119753,\"9r7uM3njQM\":-1785534166,\"L5uauj73LS\":1699688734,"
    "\"MdvoO0xqQ7\":609626628,\"V3RWpZzTb5\":587151661,\"VIywGKlB0E\":-1224721740,\"WqIoynaTxI\":-1942198960,"
    "\"WsJisUzjGr\":650886963,\"rHtQGYKLWa\":1813046762,\"rNjFv1ivFc\":-2037495891},\"field_8_QHTeT\":{"
    "\"6vs6wqu2KG\":1737278059,\"Ls8Zn2lgHM\":1382306678,\"NUdqZzKCBQ\":-729752605,\"O284LVmvMN\":-465171232,"
    "\"QpveeT37H1\":-1712252165,\"S2omxTGLLu\":521030644,\"Zb4leZ3xwM\":627506223,\"bt0k8hdGzN\":-1937216194,"
    "\"m3epufETAq\":118608879,\"vAWsPJEtDq\":-1228871303},\"field_9_KMboJ\":\"XYvrdgJmdc\",\"field_10_Jx5hn\":{"
    "\"0XoqrTVcI1\":-1946103075,\"1rzUOInkgI\":-1514440707,\"5R75AoFwOF\":273511793,\"66zPTuE5IO\":282524826,"
    "\"8Us80BKyjt\":938876682,\"FPySmGQxg5\":-41110773,\"H6hbsRB5O4\":-1315664606,\"I6kkPEgJ4B\":-605806877,"
    "\"XmkXevRq6h\":1683154180,\"bL7JM3S5f1\":657691551},\"field_11_QxDok\":-2.8425970885993798e299,\"field_12_"
    "lmfjr\":{\"8DCsDJKyXt\":-464572007,\"8NqC69NQeX\":-1802555164,\"PePlNwnp7W\":981967312,\"goyrKHtOds\":-"
    "23046121,\"gzze3p8uSq\":-1114890700,\"iQvCvxxTAt\":-1142833323,\"mCxDLIP0Jq\":1030448555,\"ns33WcZGcY\":-"
    "959506981,\"pUOu5Sesds\":64731182,\"u140HpgNIs\":-1794508440}},\"n\":{\"field_0_BZmee\":-983271578,\"field_1_"
    "l35XQ\":true,\"field_2_VCFjZ\":\"xeVM2tBYLU\",\"field_3_XLhbM\":{\"0u2mqrT6fg\":-258813703,\"ImPgOtlEZV\":-"
    "298135640,\"J29kVZAuRh\":-622348193,\"Z8f9KhxKc3\":762966231,\"aCPkO5k4lM\":1343733612,\"cr9thwJOkw\":"
    "215969512,\"dBWBKKZQ91\":-1948588020,\"irOfZfr5i9\":-1318018034,\"m1U7tPqzIk\":1685367377,\"xDCZpK99Hg\":-"
    "1393229280},\"field_4_XMQ56\":{\"1JbeZZvk7q\":255070590,\"ElFgHElMbi\":1553936069,\"UcRmUagFBe\":-772447118,"
    "\"h25aiXk8kn\":372006089,\"iggLGJPhRR\":-1513394753,\"lBRGr8elm0\":-43461965,\"mFKWL46OjT\":-122090130,"
    "\"paiFsZHEoW\":34061193,\"tpHJ10Lycb\":578835878,\"vkrBd1YfyQ\":-1726264636},\"field_5_g6DB1\":\"oKkBWOqEzA\","
    "\"field_6_V0rIH\":true,\"field_7_QK2OC\":\"t2xnzrLViM\",\"field_8_GgRJM\":[1444121976,2137169886,1560870867,"
    "803804427,-410679905,439984001,1663147266,1417987392,-1039114897,1022022623],\"field_9_Dm4so\":-991817995,"
    "\"field_10_8FMP6\":{\"9FSwzUplDt\":1401040549,\"9IeGtJswCf\":990198245,\"De8xA7rwnF\":-518024946,"
    "\"DrlaQmLvM5\":-924992590,\"G3bUd5B44Y\":1977207245,\"SAMkAqk5Bb\":-1336466436,\"Uqj2IPoMMO\":-929076275,"
    "\"VlMCvCjthJ\":-639964566,\"aGelr0uaxZ\":-2013881596,\"hPjvnymlTJ\":-1949744316},\"field_11_7sz04\":{"
    "\"216WJiST98\":-113158346,\"5UxmubgGyS\":-564494607,\"RqSGPBBJAR\":1462644576,\"XexPkVmDba\":-1980610222,"
    "\"codeXQgUN6\":-1105405557,\"e8aFWPK57d\":1246586673,\"evYAwzKpYD\":1952811564,\"itP96DMzpp\":-102498432,"
    "\"kwOBrVWASl\":-2068383048,\"pbsXi7yZ0l\":469094642},\"field_12_j00bw\":-2073209248,\"field_13_0K4q0\":["
    "1461495778,-1326166658,-1457386238,-860077324,-2074219673,-641212950,943145450,899035766,119361461,234305824]}"
    ",\"o\":{\"field_0_PODop\":{\"I5fWJTBNWJ\":765317355,\"IEnY9T6hlV\":-1724957980,\"IFFTD8Lonq\":608078787,"
    "\"Qw4g8WuAFp\":1698560570,\"Rtx88VRnKH\":721649196,\"T33qRCnoyb\":-769822791,\"W80SFog8sy\":-984665441,"
    "\"jdDtm5faLP\":-1028829134,\"vrN4OU90x7\":850621353,\"zri2Ffs2bp\":-471980093},\"field_1_pCElq\":[-663299515,-"
    "2017847267,-567560152,-376849519,1257341503,95366613,-679855024,-733740954,-1023598222,1005710744],\"field_2_"
    "8kQqk\":false,\"field_3_qsKAo\":8.246569859111843e299,\"field_4_Ap40I\":[1473678997,-1852616797,1709409587,"
    "463602058,173830869,1558323878,-145079644,-153423682,-1752683301,-751464476],\"field_5_Rwomy\":3."
    "4355730870353356e299,\"field_6_nC1C1\":[236937178,-1941571069,2084791379,1649296648,-78037929,-1384989015,"
    "1581197402,-1555291069,-1300092281,-891466542],\"field_7_kcav4\":{\"2B8j2MeCGl\":803434452,\"40pZOF6tXK\":-"
    "1401000474,\"5rKQEuxxev\":-110750015,\"7sEcOjKXwu\":507259562,\"DolXcma7GJ\":1414349062,\"J96Rlt3fA8\":-"
    "399963266,\"WwQ5wOmGaK\":342478752,\"XjoDooGf4L\":794780914,\"hxNzz7diFI\":1130011101,\"r5zvDca7z1\":-"
    "1930558878},\"field_8_tEL14\":false,\"field_9_Pk7Da\":2127479088,\"field_10_wbt8J\":1924121673,\"field_11_"
    "nOc2r\":-2111956426,\"field_12_hKDWp\":false,\"field_13_H9D9k\":-3.96599851349895e299,\"field_14_IQyg7\":{"
    "\"1jtaSwNWQX\":2127054395,\"2b2BMnYD4q\":703202458,\"6coquxJ11T\":726747570,\"7Gl9QKdZmh\":2107334612,"
    "\"G4CENLB9KX\":-1629701910,\"Gp6haV9XaS\":-1828347945,\"MG9SSx9Kl4\":-870917071,\"PdLLV3gfMX\":1003473082,"
    "\"SzFkSbOk9R\":1684460167,\"v42pbU4m0p\":-1237037189}},\"p\":{\"field_0_9vhZB\":[502124849,1860381198,"
    "1693687201,-1812960103,1532878188,923311987,-1987168209,189135350,-928337349,-640119193],\"field_1_bM7Gy\":{"
    "\"0TKqdgheu3\":1595233713,\"6SrCYzsYu5\":1936079389,\"6rJa2jZxuo\":-684568497,\"9nTfO4SbJs\":681691845,"
    "\"CDteKYkESZ\":72168148,\"RPLyl0Bqun\":-73563845,\"VLE55UPcAF\":-220927810,\"Vcqfom8KB0\":830939497,"
    "\"dm2hc1B5HY\":-764790919,\"meKsKqHeYG\":101907017},\"field_2_vJcFY\":{\"1tE2ihSL6t\":218929018,"
    "\"8NgBJXJGff\":588977541,\"E4nluFWhPT\":1471417554,\"LwfT4lCAuK\":-945693145,\"Oek40yqzPM\":-1152508248,"
    "\"YSHy1l5aYw\":-750030716,\"aTHFJCA84M\":-1184829909,\"ozoulAm4xo\":-1154481661,\"uPeZzh6TtQ\":1614558104,"
    "\"wzBxwrfnTm\":-614297350},\"field_3_aPcfB\":910780630,\"field_4_VcoCS\":\"6MT7LRCvh1\",\"field_5_3U3Kt\":"
    "\"eFIkfo2fcZ\",\"field_6_Uf0SA\":\"8qOCi2TGbm\",\"field_7_GHcW8\":-1779301584,\"field_8_fZV6Y\":{"
    "\"Ek6ue6G0K3\":1186769123,\"GIHF1ebI3o\":1811643631,\"IGCMSrUUac\":997559479,\"LXx77YisvV\":1035250689,"
    "\"MfwXLkT6lL\":-785719414,\"e23ZhzK2Bh\":-1182341310,\"i7u0OTHxdE\":816837590,\"iJmurqKZ41\":-934515970,"
    "\"tbY8GRdV78\":776006500,\"zNwdnWhcz5\":442218780},\"field_9_m3p3L\":[560298613,-1813758336,1293788907,-"
    "1470175642,1009672286,966491271,-200143759,-58113533,746693124,1608144219],\"field_10_FCBgZ\":-414577883,"
    "\"field_11_BzyK9\":true,\"field_12_3q0WP\":{\"1cmyBcHqku\":1102140480,\"3N2VnrPi92\":930104177,\"4CFA1wKqD0\":"
    "-47077801,\"DSymF2mNjf\":-510968137,\"PYYdFnxFuo\":-1157602078,\"TGgIWp8XBR\":199339788,\"YcZXh2erDP\":-"
    "2018453194,\"aI5HKKTaWx\":-387429983,\"bkgWzOp9Zb\":316244984,\"e7wVPHUJY8\":683186138},\"field_13_nBzbe\":"
    "\"xMJnlIhHki\",\"field_14_lNU78\":\"LKfFhSVdfh\",\"field_15_V6y2D\":[-368609336,549943753,-1183787019,-"
    "1596366860,-189369323,1557874014,1587989578,-829162177,-1670750029,1489303177]},\"q\":{\"field_0_Vk5dY\":"
    "\"NY7qfSQkt7\",\"field_1_As9i4\":false,\"field_2_7THwC\":5.014561638365802e298,\"field_3_pnOTa\":371326658,"
    "\"field_4_Yq5PZ\":6.133984801926284e299,\"field_5_B4zO3\":{\"7u8P1UItLY\":881156667,\"HJVLp4iU3u\":-646649749,"
    "\"Q9s1u4CtUY\":1785502873,\"QGPEdCnGOi\":788384332,\"SutIIqa3J3\":902021793,\"WAT3upWfMX\":1646576607,"
    "\"eKt1hU5bvI\":1095539227,\"gRJrqA5ueL\":-310611849,\"plAUahITUS\":1452469379,\"wOrKQcOA2K\":1418333920},"
    "\"field_6_R31OI\":[-1469160926,-854721711,-1692058696,1569189080,-1347633582,1182714499,-1346258888,-"
    "2146021644,1448907134,-2051865504],\"field_7_Vc0ES\":{\"6Y9flKF92E\":1994423886,\"B59iCSidHU\":184855396,"
    "\"C6yxP5jZ1y\":117436308,\"OKnNaOd3iN\":2134402222,\"TVEuHtUpOn\":1064216239,\"VHMiBQk5LO\":-1313981608,"
    "\"mLr4EiSuwp\":876124074,\"o8xvANU81J\":1501534167,\"pNgHdQJ3bT\":2113955930,\"pugOyfkasv\":-358349901},"
    "\"field_8_t6Xct\":-576233360,\"field_9_S0PaF\":{\"0eRKH6RpLJ\":2060684821,\"9sdGSmLtbj\":-1610304028,"
    "\"DAmOs9pV53\":-718574472,\"N077Wc9Az1\":2048642132,\"RBa5gWPk3x\":221806519,\"S4y3NkmuVZ\":1093520110,"
    "\"gLVM267xNC\":-1238500762,\"p7EqkLgVgk\":-470667733,\"rAMMmFoESQ\":1063502832,\"yKZkW0VqUr\":-226568584},"
    "\"field_10_rGyV1\":false,\"field_11_aCkWt\":{\"5V7I1wFuGS\":-1448353778,\"9qpsmiDYg9\":-1636781888,"
    "\"C09hg2VHHK\":1426709393,\"GustBgpsoo\":-798463579,\"TjJZFqa7RI\":-820111512,\"UNtPsLTSPa\":-1491145582,"
    "\"gyUu0rblX4\":-1679612936,\"iNh3dCI8IZ\":327846200,\"kNJE3AZtvW\":-1642954410,\"zoIwl9tnbD\":-971062325},"
    "\"field_12_I3mWW\":\"wHtyT2k6uU\",\"field_13_k67IC\":\"q0Xy63ZhiQ\",\"field_14_jAzgA\":\"Z7LFoQHmU2\",\"field_"
    "15_0o9jm\":{\"0c9MUFhmaY\":1915874554,\"2MMeXJURF5\":1838211699,\"9FU2K6ol8A\":-262527306,\"9VH0VUeJst\":"
    "934009851,\"LMJXL8aa2m\":808211469,\"ZjGuVm3KXc\":68744215,\"jgshWyzl23\":-538129648,\"n4pEtVc9RD\":-"
    "255770193,\"upGCWHYV5B\":-626272626,\"vgil1YiLun\":-1743583894},\"field_16_a9p7L\":\"jG9w7rJNRU\"},\"r\":{"
    "\"field_0_4dFfp\":8.796392227680598e299,\"field_1_R81Mf\":false,\"field_2_VRfKe\":\"X5Q1IE6Q2i\",\"field_3_"
    "0e1rq\":false,\"field_4_G5q8e\":638930294,\"field_5_j56PP\":[1879046659,-2099926999,1545465879,1895603498,"
    "1677520061,418261673,521261078,-2046484413,197842220,-1149424962],\"field_6_jCqfr\":-8.049795184811597e299,"
    "\"field_7_VAUJc\":{\"3syQJqhhX4\":-1084251845,\"9ACQQuhV6S\":-1316442394,\"BglE9rfeST\":352983213,"
    "\"U1Re21NHqE\":980232978,\"e1x79me6S9\":-614464154,\"gDaz5O4UPx\":764594339,\"jvLmp2tiAd\":1521639347,"
    "\"qEvP5ByTcy\":105991868,\"we4MozJ3yc\":498197462,\"x3EJbq4Nkl\":-1286920480},\"field_8_ayKVi\":[1885052645,"
    "631053585,-206733899,-1907781698,-534657711,1635731737,963250933,63739822,1396321790,144078041],\"field_9_"
    "IYNJ1\":{\"6Stu5MuQMY\":-1139950988,\"7DQ31shL6q\":-619187234,\"AxwLXYHX0d\":-1436761037,\"EKVjDJRGyP\":"
    "480319439,\"FelwLGFm9E\":-407804807,\"GvlW3Wut5I\":-1664563031,\"LvPGn4OJlu\":-1135334399,\"NLUPRlf79w\":"
    "78129412,\"Y6aNPaUJab\":-1242046999,\"iKes9w1TFm\":698202991},\"field_10_nITZc\":-3.1558283267192525e299,"
    "\"field_11_JVx2W\":[1412227684,-826471194,-620960120,-1504500467,846275591,-1236260599,-982209456,-97274455,"
    "634568948,-542755814],\"field_12_Ca9P2\":[-1766928124,-1050710895,-888238415,-1041223271,308750114,1524123572,"
    "303879944,-606079120,1264918385,-1624177776],\"field_13_JG3Oa\":false,\"field_14_BRz50\":-1848725646,\"field_"
    "15_NIkqR\":\"RgkAIisP6T\",\"field_16_zYeHA\":[867558702,215591064,-545899175,1422951659,-2132416299,"
    "1835904514,-105785390,474162390,-1935653846,-1292298308],\"field_17_DchRr\":6.178210949427803e299},\"s\":{"
    "\"field_0_0lwKQ\":false,\"field_1_aZJ3W\":-78997776,\"field_2_qJbdP\":false,\"field_3_OdrzP\":-528267170,"
    "\"field_4_gqfNn\":\"VbLGZkokgg\",\"field_5_NyfJJ\":\"f54kgcoY2c\",\"field_6_AZy5l\":7.099564009838792e299,"
    "\"field_7_VuXiT\":{\"FK5otPQcdw\":-1171740264,\"IFvuMlPHiS\":2010034893,\"LP4kqHRS7y\":1165038220,"
    "\"d431uWG3BN\":1877245248,\"d8rjvfHRMc\":407666110,\"goSdoOaa6M\":-823204326,\"jCuD9sKqBV\":1244552809,"
    "\"or0CFJUNFJ\":-392418516,\"tSexI6AbtZ\":1153557090,\"xaRK5LhLpW\":-307797930},\"field_8_tEGHO\":"
    "\"C15xgo6sZD\",\"field_9_84gbl\":4.484269602564921e299,\"field_10_fjD6R\":true,\"field_11_nyni4\":false,"
    "\"field_12_Y20tI\":\"fvVtf3SLWY\",\"field_13_2vogq\":false,\"field_14_eXqBg\":6.700139625165763e298,\"field_"
    "15_AcgTj\":-2.3879194910123957e299,\"field_16_R3CRl\":[363267557,-430587927,2073602156,-1297279996,1283765347,"
    "-1298263059,-1995090881,-1985409024,-806126965,-1663688891],\"field_17_svhOB\":8.432857782920547e299,\"field_"
    "18_c2AxJ\":true},\"t\":{\"field_0_KTZWz\":{\"2G7yZB0r2Z\":-625665831,\"9cOgsFPk23\":-894997434,\"DWb3wlttjc\":"
    "1485783317,\"QZBnRpVQ8B\":-178617731,\"Sv0XeIxfze\":-1727444620,\"d3GKfzGVeq\":1498108226,\"iJkkzQVWNw\":"
    "361444060,\"lnGtLdT0ev\":-1810966741,\"o3tqrAwpui\":771363062,\"xBd1fW72qG\":-133237643},\"field_1_Aty0x\":"
    "true,\"field_2_oO4Ko\":\"b73wtS2coT\",\"field_3_TQaZV\":true,\"field_4_cGgOX\":2.9858135849814157e299,\"field_"
    "5_6mobp\":{\"32dRxjTthA\":-1852331417,\"8lFyB0BBPc\":1746393568,\"A1kYKpZcKO\":-2055866632,\"KDVZaF9Ix7\":-"
    "77361587,\"WHvqf9OXru\":1172254542,\"YYi24cbRbP\":-575810540,\"YwVRXJ6z0x\":-1389568592,\"cJNTdPdZWU\":-"
    "768675763,\"j2AOV2VLiI\":-163609600,\"liWx8wy7Yf\":-922218171},\"field_6_f5Cbg\":\"SKgPPhOIff\",\"field_7_"
    "eVSBV\":-1593796259,\"field_8_7hjI6\":\"ClhcFBbAvv\",\"field_9_osByy\":[1553208990,-1934655667,1977243992,-"
    "1014021622,-1090393519,433021655,1164153061,-677631827,-437175391,657304195],\"field_10_ksf4X\":false,\"field_"
    "11_xOiVC\":[1023802797,-1653092807,642875270,-141932242,1490346517,-780507387,633502637,-558240352,1993888994,"
    "-2034633130],\"field_12_XQg2J\":\"84Qu67Hptp\",\"field_13_JDmUg\":{\"15h3MVCigS\":85038409,\"53pKrSnhH2\":-"
    "670267114,\"DLiXSm0JcM\":-114512883,\"DonVwH0LZe\":-1628754469,\"NWskjnMypm\":1205626724,\"WqAm7w5xTV\":"
    "955874023,\"fq1maOU5XI\":-1600401204,\"slVgNMqtvV\":-1063843661,\"wWYKyOUcfb\":852418496,\"xx6MdMrCMc\":"
    "1449176030},\"field_14_TmHUg\":{\"0uwI2tCKh9\":346420108,\"F0CyrxiNRF\":-16259182,\"GdX2VyuFFW\":-187843216,"
    "\"K2UwGRX2El\":-1433250566,\"RWxS9gEdoO\":653843660,\"SiuPGlvYrw\":923512232,\"f49w30rLaI\":581805156,"
    "\"gmduEhxvrG\":340610497,\"rXl8Rn27fd\":-1001536822,\"unstSrbdhM\":1596386505},\"field_15_IJcBN\":true,"
    "\"field_16_I4RX1\":\"OjkfGpD7gJ\",\"field_17_FMaDg\":true,\"field_18_uy3KX\":-8.008131144161175e299,\"field_"
    "19_0bTpy\":-2.0555281374100117e299}}";

TEST_F(ProtoTest, StructProto) {
    EXPECT_EQ(factory->proto_type<TestStruct_1>(), 4);
    EXPECT_EQ(factory->proto_type<TestStruct_2>(), 15);
    EXPECT_EQ(factory->proto_type<TestStruct_3>(), 17);
    EXPECT_EQ(factory->proto_type<TestStruct_4>(), 18);
    EXPECT_EQ(factory->proto_type<TestStruct_5>(), 19);
    EXPECT_EQ(factory->proto_type<TestStruct_6>(), 20);
    EXPECT_EQ(factory->proto_type<TestStruct_7>(), 21);
    EXPECT_EQ(factory->proto_type<TestStruct_8>(), 22);
    EXPECT_EQ(factory->proto_type<TestStruct_9>(), 23);
    EXPECT_EQ(factory->proto_type<TestStruct_10>(), 5);
    EXPECT_EQ(factory->proto_type<TestStruct_11>(), 6);
    EXPECT_EQ(factory->proto_type<TestStruct_12>(), 7);
    EXPECT_EQ(factory->proto_type<TestStruct_13>(), 8);
    EXPECT_EQ(factory->proto_type<TestStruct_14>(), 9);
    EXPECT_EQ(factory->proto_type<TestStruct_15>(), 10);
    EXPECT_EQ(factory->proto_type<TestStruct_16>(), 11);
    EXPECT_EQ(factory->proto_type<TestStruct_17>(), 12);
    EXPECT_EQ(factory->proto_type<TestStruct_18>(), 13);
    EXPECT_EQ(factory->proto_type<TestStruct_19>(), 14);
    EXPECT_EQ(factory->proto_type<TestStruct_20>(), 16);
    EXPECT_EQ(factory->proto_type<StructProto>(), 2);

    StructProto proto;
    proto.a.field_0_VxQU2 = {1, 2, 3};
    NEKO_LOG_INFO("{}", SerializableToString(proto));
    auto data = proto.makeProto().toData();
    data.push_back('\0');
    EXPECT_STREQ(data.data(), StructProtoData);

    StructProto proto2;
    proto2.makeProto().formData({StructProtoData, StructProtoData + 18184});
    EXPECT_EQ(proto2.a.field_0_VxQU2, proto.a.field_0_VxQU2);
}

#endif

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::debug);
    return RUN_ALL_TESTS();
}