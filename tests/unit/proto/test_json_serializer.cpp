#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <locale>

#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
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
    auto ret = sa(make_size_tag(size));
    if (size != 8) {
        NEKO_LOG_ERROR("unit test", "struct size mismatch: json obejct size {} != struct size 8", size);
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
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    using WriterType = detail::JsonWriter<>;
    using ValueType  = detail::JsonValue;
#endif
    using InputSerializer  = JsonSerializer::InputSerializer;
    using OutputSerializer = JsonSerializer::OutputSerializer;
    JsonSerializerTest() : mBuffer(), mOutput(mBuffer) {}

protected:
    virtual void SetUp() {}
    virtual void TearDown() {}
    std::vector<char> mBuffer;
    JsonSerializer::OutputSerializer mOutput;
};

TEST_F(JsonSerializerTest, Int) {
    int num = 1;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", num)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":1}");

    int num1 = 0;
    InputSerializer input(mBuffer.data(), mBuffer.size());
    EXPECT_TRUE(input.startNode());
    EXPECT_TRUE(input(make_name_value_pair("a", num1)));
    EXPECT_TRUE(input.finishNode());
    EXPECT_EQ(num1, num);
}

TEST_F(JsonSerializerTest, String) {
    std::string str1 = "hello";
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("str1", str1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"str1\":\"hello\"}");

    std::string str2; // = "hello";
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("str1", str2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(str2.c_str(), str1.c_str());
}
TEST_F(JsonSerializerTest, EmptyString) {
    // empty string test
    std::string str1;
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("str1", str1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"str1\":\"\"}");

    std::string str2 = "hello";
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("str1", str2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(str2.c_str(), str1.c_str());
}

TEST_F(JsonSerializerTest, Bool) {
    bool bool1 = true;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("bool1", bool1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"bool1\":true}");

    bool bool2 = false;
    InputSerializer input(mBuffer.data(), mBuffer.size());
    EXPECT_TRUE(input.startNode());
    EXPECT_TRUE(input(make_name_value_pair("bool1", bool2)));
    EXPECT_TRUE(input.finishNode());
    EXPECT_EQ(bool2, bool1);
}

TEST_F(JsonSerializerTest, Double) {
    double double1 = 3.14;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("double1", double1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"double1\":3.14}");

    double double2 = 0;
    InputSerializer input(mBuffer.data(), mBuffer.size());
    EXPECT_TRUE(input.startNode());
    EXPECT_TRUE(input(make_name_value_pair("double1", double2)));
    EXPECT_TRUE(input.finishNode());
    EXPECT_DOUBLE_EQ(double2, double1);
}

TEST_F(JsonSerializerTest, Vector) {
    std::vector<int> double1 = {1, 2, 3, 4, 5};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("double1", double1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"double1\":[1,2,3,4,5]}");

    std::vector<int> a1;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("double1", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, double1);
}
TEST_F(JsonSerializerTest, EmptyVector) {
    // test with empty vector
    std::vector<int> vec1 = {};
    std::vector<int> vec2 = {1, 2, 3};
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("vec1", vec1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"vec1\":[]}");

    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("vec1", vec2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(vec2, vec1);
}

TEST_F(JsonSerializerTest, List) {
    std::list<int> list1 = {1, 2, 3, 4, 5};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("list1", list1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"list1\":[1,2,3,4,5]}");

    std::list<int> list2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("list1", list2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(list2, list1);
    EXPECT_EQ(list2.size(), 5);
}
TEST_F(JsonSerializerTest, EmptyList) {
    // test with empty list
    std::list<int> list1 = {};
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("list1", list1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"list1\":[]}");

    std::list<int> list2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("list1", list2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(list2, list1);
}

TEST_F(JsonSerializerTest, Map) {
    std::map<std::string, int> map1 = {{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("map1", map1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"map1\":{\"a\":1,\"b\":2,\"c\":3}}");

    std::map<std::string, int> map2;
    {
        InputSerializer input(str, strlen(str));
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("map1", map2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(map2, map1);
}
TEST_F(JsonSerializerTest, EmptyMap) {
    // test with empty map
    std::map<std::string, int> map1 = {};
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("map1", map1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"map1\":{}}");

    std::map<std::string, int> map2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("map1", map2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(map2, map1);
}

TEST_F(JsonSerializerTest, Map1) {
    std::map<double, int> map1 = {{1.1, 1}, {2.2, 2}, {3.3, 3}};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", map1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"a\":[{\"key\":1.1,\"value\":1},{\"key\":2.2,\"value\":2},{\"key\":3.3,\"value\":3}]}");

    std::map<double, int> map2;
    {
        InputSerializer input(str, strlen(str));
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", map2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(map2, map1);
}
TEST_F(JsonSerializerTest, EmptyMap1) {
    // test with empty map
    std::map<std::string, std::vector<int>> map1;
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("map1", map1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"map1\":{}}");

    std::map<std::string, std::vector<int>> map2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("map1", map2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(map2, map1);
}

TEST_F(JsonSerializerTest, UnorderedMap) {
    std::unordered_map<std::string, int> umap1 = {{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());

    std::unordered_map<std::string, int> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}
TEST_F(JsonSerializerTest, EmptyUnorderedMap) {
    // test with empty map
    std::unordered_map<std::string, std::vector<int>> umap1;
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":{}}");

    std::unordered_map<std::string, std::vector<int>> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}

TEST_F(JsonSerializerTest, UnorderedMap1) {
    std::unordered_map<double, int> umap1 = {{1.1, 1}, {2.2, 2}, {3.3, 3}};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());

    std::unordered_map<double, int> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}

TEST_F(JsonSerializerTest, EmptyUnorderedMap1) {
    // test with empty map
    std::unordered_map<double, double> umap1;
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":[]}");

    std::unordered_map<double, double> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}

TEST_F(JsonSerializerTest, MultiMap) {
    std::unordered_map<std::string, int> umap1 = {{"a", 1}, {"a", 2}, {"c", 3}, {"c", 4}};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());

    std::unordered_map<std::string, int> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}
TEST_F(JsonSerializerTest, EmptyMultiMap) {
    // test with empty map
    std::unordered_map<std::string, int> umap1;
    mBuffer.clear();
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":{}}");

    std::unordered_map<std::string, int> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}

TEST_F(JsonSerializerTest, UnorderedMultiMap) {
    std::unordered_map<std::string, int> umap1 = {{"a", 1}, {"a", 2}, {"c", 3}, {"c", 4}};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());

    std::unordered_map<std::string, int> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}

TEST_F(JsonSerializerTest, EmptyUnorderedMultiMap) {
    std::unordered_map<std::string, int> umap1;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", umap1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"a\":{}}");

    std::unordered_map<std::string, int> umap2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", umap2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umap2, umap1);
}

TEST_F(JsonSerializerTest, Set) {
    std::set<int> set1 = {1, 2, 3, 4, 5};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", set1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");

    std::set<int> set2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", set2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(set2, set1);
}

TEST_F(JsonSerializerTest, EmptySet) {
    std::set<int> set1;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", set1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":[]}");

    std::set<int> set2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", set2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(set2, set1);
}

TEST_F(JsonSerializerTest, UnorderedSet) {
    std::unordered_set<int> set1 = {1, 2, 3, 4, 5};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("set1", set1)));
    EXPECT_TRUE(mOutput.end());

    std::unordered_set<int> set2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("set1", set2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(set2, set1);
}

TEST_F(JsonSerializerTest, MultiSet) {
    std::multiset<int> mset1 = {1, 1, 1, 1, 2, 2, 2, 3};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("mset1", mset1)));
    EXPECT_TRUE(mOutput.end());

    std::multiset<int> mset2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("mset1", mset2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(mset2, mset1);
}

TEST_F(JsonSerializerTest, UnorderedMultiSet) {
    std::unordered_multiset<int> umset1 = {1, 1, 1, 1, 2, 2, 2, 3};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("umset1", umset1)));
    EXPECT_TRUE(mOutput.end());

    std::unordered_multiset<int> umset2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("umset1", umset2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(umset2, umset1);
}

TEST_F(JsonSerializerTest, Atomic) {
    std::atomic<int> anum1{5};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("anum1", anum1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"anum1\":5}");

    std::atomic<int> anum2{0};
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("anum1", anum2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(anum2, anum1);
}

TEST_F(JsonSerializerTest, BinaryData) {
    std::string data = "hello world";
    BinaryData<const char> base64(data.data(), data.size());
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("base64", base64)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"base64\":\"aGVsbG8gd29ybGQ=\"}");

    std::vector<char> data1;
    data1.resize(data.size());
    BinaryData<char> str2(data1.data(), data1.size());
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("base64", str2)));
        EXPECT_TRUE(input.finishNode());
    }
    data1.push_back('\0');
    EXPECT_STREQ(data1.data(), data.c_str());
}

TEST_F(JsonSerializerTest, Bitset) {
    std::bitset<5> bitset1 = 0b10101;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("bitset1", bitset1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"bitset1\":\"10101\"}");

    std::bitset<5> bitset2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("bitset1", bitset2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(bitset2.to_string().c_str(), "10101");
}

TEST_F(JsonSerializerTest, Pair) {
    std::pair<int, std::string> pair1 = {1, "hello world"};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("pair1", pair1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"pair1\":{\"first\":1,\"second\":\"hello world\"}}");

    std::pair<int, std::string> pair2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("pair1", pair2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(pair2.second.c_str(), "hello world");
}

TEST_F(JsonSerializerTest, SharedPtr) {
    std::shared_ptr<int> ptr1 = std::make_shared<int>(42);
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("ptr1", ptr1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"ptr1\":42}");

    std::shared_ptr<int> ptr2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("ptr1", ptr2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(*ptr2, 42);
}

TEST_F(JsonSerializerTest, EmptySharedPtr) {
    std::shared_ptr<int> ptr1;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("ptr1", ptr1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"ptr1\":null}");
    EXPECT_FALSE(ptr1);

    std::shared_ptr<int> ptr2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("ptr1", ptr2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_FALSE(ptr2);
}

TEST_F(JsonSerializerTest, UniquePtr) {
    std::unique_ptr<int> ptr1(new int{42});
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("ptr1", ptr1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"ptr1\":42}");

    std::unique_ptr<int> ptr2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("ptr1", ptr2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(*ptr2, 42);
}

TEST_F(JsonSerializerTest, EmptyUniquePtr) {
    std::unique_ptr<int> ptr1;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("ptr1", ptr1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"ptr1\":null}");
    EXPECT_FALSE(ptr1);

    std::unique_ptr<int> ptr2;
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("ptr1", ptr2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_FALSE(ptr2);
}

TEST_F(JsonSerializerTest, Array) {
    std::array<int, 5> arr1 = {1, 2, 3, 4, 5};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("arr1", arr1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, "{\"arr1\":[1,2,3,4,5]}");

    std::array<int, 5> arr2;
    {
        InputSerializer input(str, strlen(str));
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("arr1", arr2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(arr2, arr1);
}

#if NEKO_CPP_PLUS >= 17
TEST_F(JsonSerializerTest, AnyStruct) {
    struct {
        int a;
        std::string b;
        bool c;
    } testp{3, "Struct test", true}, testp1;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", testp)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":{\"a\":3,\"b\":\"Struct test\",\"c\":true}}");

    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", testp1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(testp.a, testp1.a);
    EXPECT_STREQ(testp.b.c_str(), testp1.b.c_str());
    EXPECT_EQ(testp.c, testp1.c);
}

TEST_F(JsonSerializerTest, Variant) {
    std::variant<int, std::string> var1{3};
    std::variant<int, std::string> var2;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", var1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":3}");

    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", var2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(std::get<0>(var1), std::get<0>(var2));
}

TEST_F(JsonSerializerTest, Variant1) {
    std::variant<int, std::string> var1{"test"};
    std::variant<int, std::string> var2;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", var1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":\"test\"}");

    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", var2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(std::get<1>(var1).c_str(), std::get<1>(var2).c_str());
}

TEST_F(JsonSerializerTest, Optional) {
    std::optional<int> var1{3};
    std::optional<int> var2;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", var1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":3}");

    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", var2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(var1.value(), var2.value());
}

TEST_F(JsonSerializerTest, Optional1) {
    // null
    std::optional<int> var1;
    std::optional<int> var2{3};
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", var1)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{}");

    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", var2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(var2.has_value(), var1.has_value());
}

#endif

#if NEKO_CPP_PLUS >= 20
TEST_F(JsonSerializerTest, U8string) {
    std::u8string v = u8"这是一个测试; this is a test.";
    std::u8string v1;
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", v)));
    EXPECT_TRUE(mOutput.end());
    mBuffer.push_back('\0');
    EXPECT_STREQ(mBuffer.data(), "{\"a\":\"这是一个测试; this is a test.\"}");
    {
        InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", v1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_TRUE(v1 == v);
}
#endif

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
    EXPECT_TRUE(mOutput.startObject(1));
    EXPECT_TRUE(mOutput(make_name_value_pair("a", testp)));
    EXPECT_TRUE(mOutput.end());
    const char* answer =
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_A\","
        "\"i\":{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,"
        "3,0,0],\"h\":\"TEnum_A\"},\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5],\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},{"
        "\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0],\"c\":{\"a\":1221,\"b\":\"this is a test for "
        "optional\"}},{\"a\":[{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":"
        "12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]}]}]}}}";
#else
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_"
        "A\",\"i\":{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":["
        "1,2,3,0,0],\"h\":\"TEnum_A\"},\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5],\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},"
        "{\"a\":12.9,\"b\":[1,2,3,4,5],\"c\":{\"a\":1221,\"b\":\"this is a test for "
        "optional\"}},{\"a\":[{\"a\":12.9,\"b\":[1,2,3,4,5]},{\"a\":12.9,\"b\":[1,2,3,4,5]},{\"a\":"
        "12.9,\"b\":[1,2,3,4,5]}]}]}}}";
#endif
    mBuffer.push_back('\0');
    const char* str = mBuffer.data();
    EXPECT_STREQ(str, answer);

    TestP testp2;
    {
        JsonSerializer::InputSerializer input(mBuffer.data(), mBuffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", testp2)));
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
    NEKO_LOG_DEBUG("unit test", "{}", serializable_to_string(testp));
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    {
        std::vector<char> buffer;
        RapidJsonOutputSerializer<detail::PrettyJsonWriter<>> output(buffer, JsonOutputFormatOptions::Default());
        EXPECT_TRUE(output.startObject(1));
        EXPECT_TRUE(output(make_name_value_pair("a", testp)));
        EXPECT_TRUE(output.end());
        buffer.push_back('\0');
        const char* str1 = buffer.data();
        NEKO_LOG_DEBUG("unit test", "{}", str1);
    }
#endif
}

TEST_F(JsonSerializerTest, IOStreamTest) {
    std::string filepath = "test" + std::to_string(rand()) + ".json";
    std::ofstream ofs(filepath);
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

    {
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
        RapidJsonOutputSerializer<detail::PrettyJsonWriter<std::ofstream>> output(ofs);
#elif defined(NEKO_PROTO_ENABLE_SIMDJSON)
        SimdJsonOutputSerializer<std::ofstream> output(ofs);
#endif
        output(testp);
    }
    ofs.close();

    std::ifstream ifs(filepath);
    TestP testp2;
    {
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
        RapidJsonInputSerializer<std::ifstream> input(ifs);
#elif defined(NEKO_PROTO_ENABLE_SIMDJSON)
        std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        SimdJsonInputSerializer input(str.data(), str.size());
#endif
        input(testp2);
    }
    ifs.close();

    EXPECT_EQ(testp2.a, testp.a);
    EXPECT_STREQ(testp2.b.c_str(), testp.b.c_str());
    EXPECT_EQ(testp2.c, testp.c);
    EXPECT_EQ(testp2.d, testp.d);
    EXPECT_EQ(testp2.e, testp.e);
    EXPECT_EQ(testp2.f, testp.f);
    EXPECT_EQ(testp2.g, testp.g);
    EXPECT_EQ(testp2.h, testp.h);
    EXPECT_EQ(testp2.i.a, testp.i.a);
    EXPECT_STREQ(testp2.i.b.c_str(), testp.i.b.c_str());
    EXPECT_EQ(testp2.i.c, testp.i.c);
    EXPECT_EQ(testp2.i.d, testp.i.d);
    EXPECT_EQ(testp2.i.e, testp.i.e);
    EXPECT_EQ(testp2.i.f, testp.i.f);
    EXPECT_EQ(testp2.i.g, testp.i.g);
    EXPECT_EQ(testp2.i.h, testp.i.h);
    EXPECT_EQ(testp2.j, testp.j);
    EXPECT_EQ(testp2.k, testp.k);
#if NEKO_CPP_PLUS >= 17
    EXPECT_EQ(std::get<1>(testp2.l.a).c->a, std::get<1>(testp.l.a).c->a);
    EXPECT_STREQ(std::get<1>(testp2.l.a).c->b.c_str(), std::get<1>(testp.l.a).c->b.c_str());
#endif
}

struct ZTypeTest1 {
    std::unordered_map<int, int> a;
    std::unordered_map<std::string, std::string> b;
    std::unordered_set<int> c;
    std::unordered_multimap<double, int> d;
    std::unordered_multiset<double> e;
    std::unordered_map<std::string, std::shared_ptr<std::string>> f;
    std::unique_ptr<std::string> g;
    std::atomic<int> i;

    NEKO_SERIALIZER(a, b, c, d, e, f, g, i)
};

TEST_F(JsonSerializerTest, SerializableTest) {
    ZTypeTest1 proto;
    proto.a = {{1, 1}, {2, 2}};
    proto.b = {{"hello", "world"}};
    proto.c = {1, 2, 3};
    proto.d = {{1.1, 1}, {2.2, 2}};
    proto.e = {1.1, 2.2, 3.3};
    proto.f = {{"hello", std::make_shared<std::string>("world")}, {"nullptr", nullptr}};
    proto.g = std::make_unique<std::string>("hello");
    proto.i = 123;
    NEKO_LOG_DEBUG("unit test", "{}", serializable_to_string(proto));
    EXPECT_TRUE(mOutput(proto));
    mBuffer.push_back('\0');

    ZTypeTest1 proto2;
    {
        JsonSerializer::InputSerializer input(mBuffer.data(), mBuffer.size());
        input(proto2);
    }
    EXPECT_EQ(proto.a, proto2.a);
    EXPECT_EQ(proto.b, proto2.b);
    EXPECT_EQ(proto.c, proto2.c);
    EXPECT_EQ(proto.d, proto2.d);
    EXPECT_EQ(proto.e, proto2.e);
    EXPECT_EQ(*proto.g, *proto2.g);
    EXPECT_EQ(*proto.f["hello"], *proto2.f["hello"]);
    EXPECT_EQ(proto.f["nullptr"], proto2.f["nullptr"]);
    EXPECT_EQ(proto.i.load(), proto2.i.load());
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);
    std::setlocale(LC_ALL, "en_US.UTF-8");
    testing::InitGoogleTest(&argc, argv);
    ProtoFactory factor(1, 0, 0);
    return RUN_ALL_TESTS();
}