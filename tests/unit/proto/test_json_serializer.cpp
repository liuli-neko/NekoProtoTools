#include <gtest/gtest.h>
#include <iostream>
#include <locale>
#include <fstream>

#include "nekoproto/proto/binary_serializer.hpp"
#include "nekoproto/proto/json_serializer.hpp"
#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/proto/serializer_base.hpp"
#include "nekoproto/proto/to_string.hpp"
#include "nekoproto/proto/types/types.hpp"

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
    using WriterType       = detail::JsonWriter<>;
    using ValueType        = detail::JsonValue;
    using InputSerializer  = JsonSerializer::InputSerializer;
    using OutputSerializer = JsonSerializer::OutputSerializer;
    JsonSerializerTest() : buffer(), output(buffer) {}

protected:
    virtual void SetUp() {}
    virtual void TearDown() {}
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer output;
};

TEST_F(JsonSerializerTest, Int) {
    int a = 1;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":1}");

    int a1 = 0;
    InputSerializer input(buffer.data(), buffer.size());
    EXPECT_TRUE(input.startNode());
    EXPECT_TRUE(input(make_name_value_pair("a", a1)));
    EXPECT_TRUE(input.finishNode());
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, String) {
    std::string a = "hello";
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":\"hello\"}");

    std::string a1; // = "hello";
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(a1.c_str(), a.c_str());
}
TEST_F(JsonSerializerTest, EmptyString) {
    // empty string test
    std::string a = "";
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":\"\"}");

    std::string a1 = "hello";
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(a1.c_str(), a.c_str());
}

TEST_F(JsonSerializerTest, Bool) {
    bool a = true;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":true}");

    bool a1 = false;
    InputSerializer input(buffer.data(), buffer.size());
    EXPECT_TRUE(input.startNode());
    EXPECT_TRUE(input(make_name_value_pair("a", a1)));
    EXPECT_TRUE(input.finishNode());
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, Double) {
    double a = 3.14;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":3.14}");

    double a1 = 0;
    InputSerializer input(buffer.data(), buffer.size());
    EXPECT_TRUE(input.startNode());
    EXPECT_TRUE(input(make_name_value_pair("a", a1)));
    EXPECT_TRUE(input.finishNode());
    EXPECT_DOUBLE_EQ(a1, a);
}

TEST_F(JsonSerializerTest, Vector) {
    std::vector<int> a = {1, 2, 3, 4, 5};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");

    std::vector<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}
TEST_F(JsonSerializerTest, EmptyVector) {
    // test with empty vector
    std::vector<int> a = {}, a1 = {1, 2, 3};
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":[]}");

    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, List) {
    std::list<int> a = {1, 2, 3, 4, 5};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");

    std::list<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
    EXPECT_EQ(a1.size(), 5);
}
TEST_F(JsonSerializerTest, EmptyList) {
    // test with empty list
    std::list<int> a = {}, a1 = {1, 2, 3};
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":[]}");

    std::list<int> a2;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a2)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a2, a);
}

TEST_F(JsonSerializerTest, Map) {
    std::map<std::string, int> a = {{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":{\"a\":1,\"b\":2,\"c\":3}}");

    std::map<std::string, int> a1;
    {
        InputSerializer input(str, strlen(str));
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}
TEST_F(JsonSerializerTest, EmptyMap) {
    // test with empty map
    std::map<std::string, int> a = {};
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":{}}");

    std::map<std::string, int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, Map1) {
    std::map<double, int> a = {{1.1, 1}, {2.2, 2}, {3.3, 3}};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":[{\"key\":1.1,\"value\":1},{\"key\":2.2,\"value\":2},{\"key\":3.3,\"value\":3}]}");

    std::map<double, int> a1;
    {
        InputSerializer input(str, strlen(str));
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}
TEST_F(JsonSerializerTest, EmptyMap1) {
    // test with empty map
    std::map<std::string, std::vector<int>> a;
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":{}}");

    std::map<std::string, std::vector<int>> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, UnorderedMap) {
    std::unordered_map<std::string, int> a = {{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());

    std::unordered_map<std::string, int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}
TEST_F(JsonSerializerTest, EmptyUnorderedMap) {
    // test with empty map
    std::unordered_map<std::string, std::vector<int>> a;
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":{}}");

    std::unordered_map<std::string, std::vector<int>> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, UnorderedMap1) {
    std::unordered_map<double, int> a = {{1.1, 1}, {2.2, 2}, {3.3, 3}};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());

    std::unordered_map<double, int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, EmptyUnorderedMap1) {
    // test with empty map
    std::unordered_map<double, double> a;
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":[]}");

    std::unordered_map<double, double> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, MultiMap) {
    std::unordered_map<std::string, int> a = {{"a", 1}, {"a", 2}, {"c", 3}, {"c", 4}};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());

    std::unordered_map<std::string, int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}
TEST_F(JsonSerializerTest, EmptyMultiMap) {
    // test with empty map
    std::unordered_map<std::string, int> a;
    buffer.clear();
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":{}}");

    std::unordered_map<std::string, int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, UnorderedMultiMap) {
    std::unordered_map<std::string, int> a = {{"a", 1}, {"a", 2}, {"c", 3}, {"c", 4}};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());

    std::unordered_map<std::string, int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, EmptyUnorderedMultiMap) {
    std::unordered_map<std::string, int> a;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":{}}");

    std::unordered_map<std::string, int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, Set) {
    std::set<int> a = {1, 2, 3, 4, 5};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");

    std::set<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, EmptySet) {
    std::set<int> a;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":[]}");

    std::set<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, UnorderedSet) {
    std::unordered_set<int> a = {1, 2, 3, 4, 5};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());

    std::unordered_set<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, MultiSet) {
    std::multiset<int> a = {1, 1, 1, 1, 2, 2, 2, 3};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());

    std::multiset<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, UnorderedMultiSet) {
    std::unordered_multiset<int> a = {1, 1, 1, 1, 2, 2, 2, 3};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());

    std::unordered_multiset<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, Atomic) {
    std::atomic<int> a{5};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":5}");

    std::atomic<int> a1{0};
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

TEST_F(JsonSerializerTest, BinaryData) {
    std::string data = "hello world";
    BinaryData<const char> a(data.data(), data.size());
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":\"aGVsbG8gd29ybGQ=\"}");

    std::vector<char> data1;
    data1.resize(data.size());
    BinaryData<char> a1(data1.data(), data1.size());
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    data1.push_back('\0');
    EXPECT_STREQ(data1.data(), data.c_str());
}

TEST_F(JsonSerializerTest, Bitset) {
    std::bitset<5> a = 0b10101;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":\"10101\"}");

    std::bitset<5> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(a1.to_string().c_str(), "10101");
}

TEST_F(JsonSerializerTest, Pair) {
    std::pair<int, std::string> a = {1, "hello world"};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":{\"first\":1,\"second\":\"hello world\"}}");

    std::pair<int, std::string> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(a1.second.c_str(), "hello world");
}

TEST_F(JsonSerializerTest, SharedPtr) {
    std::shared_ptr<int> a = std::make_shared<int>(42);
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":42}");

    std::shared_ptr<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(*a1, 42);
}

TEST_F(JsonSerializerTest, EmptySharedPtr) {
    std::shared_ptr<int> a;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":null}");
    EXPECT_FALSE(a);

    std::shared_ptr<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_FALSE(a1);
}

TEST_F(JsonSerializerTest, UniquePtr) {
    std::unique_ptr<int> a(new int{42});
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":42}");

    std::unique_ptr<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(*a1, 42);
}

TEST_F(JsonSerializerTest, EmptyUniquePtr) {
    std::unique_ptr<int> a;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":null}");
    EXPECT_FALSE(a);

    std::unique_ptr<int> a1;
    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_FALSE(a1);
}

TEST_F(JsonSerializerTest, Array) {
    std::array<int, 5> a = {1, 2, 3, 4, 5};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", a)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, "{\"a\":[1,2,3,4,5]}");

    std::array<int, 5> a1;
    {
        InputSerializer input(str, strlen(str));
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", a1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(a1, a);
}

#if NEKO_CPP_PLUS >= 17
TEST_F(JsonSerializerTest, AnyStruct) {
    struct {
        int a;
        std::string b;
        bool c;
    } testp{3, "Struct test", true}, testp1;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", testp)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":[3,\"Struct test\",true]}");

    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", testp1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(testp.a, testp1.a);
    EXPECT_STREQ(testp.b.c_str(), testp1.b.c_str());
    EXPECT_EQ(testp.c, testp1.c);
}

TEST_F(JsonSerializerTest, Variant) {
    std::variant<int, std::string> v{3}, v1;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", v)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":3}");

    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", v1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(std::get<0>(v), std::get<0>(v1));
}

TEST_F(JsonSerializerTest, Variant1) {
    std::variant<int, std::string> v{"test"}, v1;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", v)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":\"test\"}");

    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", v1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_STREQ(std::get<1>(v).c_str(), std::get<1>(v1).c_str());
}

TEST_F(JsonSerializerTest, Optional) {
    std::optional<int> v{3}, v1;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", v)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":3}");

    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", v1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(v.value(), v1.value());
}

TEST_F(JsonSerializerTest, Optional1) {
    // null
    std::optional<int> v, v1{3};
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", v)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{}");

    {
        InputSerializer input(buffer.data(), buffer.size());
        EXPECT_TRUE(input.startNode());
        EXPECT_TRUE(input(make_name_value_pair("a", v1)));
        EXPECT_TRUE(input.finishNode());
    }
    EXPECT_EQ(v1.has_value(), v.has_value());
}

#endif

#if NEKO_CPP_PLUS >= 20
TEST_F(JsonSerializerTest, U8string) {
    std::u8string v = u8"这是一个测试; this is a test.";
    std::u8string v1;
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", v)));
    EXPECT_TRUE(output.end());
    buffer.push_back('\0');
    EXPECT_STREQ(buffer.data(), "{\"a\":\"这是一个测试; this is a test.\"}");
    {
        InputSerializer input(buffer.data(), buffer.size());
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
    EXPECT_TRUE(output.startObject(1));
    EXPECT_TRUE(output(make_name_value_pair("a", testp)));
    EXPECT_TRUE(output.end());
#if NEKO_CPP_PLUS >= 17
    const char* answer =
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":\"TEnum_"
        "A(1)"
        "\",\"i\":[1,\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],\"TEnum_A(1)\"],\"j\":[1,"
        "\"hello\"],\"k\":[1,2,3,4,5],\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,"
        "5.0]"
        ",\"c\":{\"a\":1221,\"b\":\"this is a test for "
        "optional\"}},{\"a\":[{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{"
        "\"a\":"
        "12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]}]}]}}}";
#else
    const char* answer =
        "{\"a\":{\"a\":3,\"b\":\"Struct "
        "test\",\"c\":true,\"d\":3.141592654,\"e\":[1,2,3],\"f\":{\"a\":1,\"b\":2},\"g\":[1,2,3,0,0],\"h\":1,\"i\":"
        "[1,"
        "\"hello\",true,3.141592654,[1,2,3],{\"a\":1,\"b\":2},[1,2,3,0,0],1],\"j\":[1,\"hello\"],\"k\":[1,2,3,4,5],"
        "\"l\":{\"a\":[{\"a\":1,\"b\":\"dsadfsd\"},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":[{\"a\":12.9,"
        "\"b\":["
        "1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]},{\"a\":12.9,\"b\":[1.0,2.0,3.0,4.0,5.0]}]}]"
        "}}}";
#endif
    buffer.push_back('\0');
    const char* str = buffer.data();
    EXPECT_STREQ(str, answer);

    TestP testp2;
    {
        JsonSerializer::InputSerializer input(buffer.data(), buffer.size());
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
    NEKO_LOG_DEBUG("unit test", "{}", SerializableToString(testp));
    {
        std::vector<char> buffer;
        RapidJsonOutputSerializer<detail::PrettyJsonWriter<>> output(buffer, JsonOutputFormatOptions::Default());
        EXPECT_TRUE(output.startObject(1));
        EXPECT_TRUE(output(make_name_value_pair("a", testp)));
        EXPECT_TRUE(output.end());
        buffer.push_back('\0');
        const char* str = buffer.data();
        NEKO_LOG_DEBUG("unit test", "{}", str);
    }
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
        RapidJsonOutputSerializer<detail::PrettyJsonWriter<std::ofstream>> output(ofs);
        output(testp);
    }
    ofs.close();

    std::ifstream ifs(filepath);
    TestP testp2;
    {
        RapidJsonInputSerializer<std::ifstream> input(ifs);
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

struct zTypeTest1 {
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
    zTypeTest1 t;
    t.a = {{1, 1}, {2, 2}};
    t.b = {{"hello", "world"}};
    t.c = {1, 2, 3};
    t.d = {{1.1, 1}, {2.2, 2}};
    t.e = {1.1, 2.2, 3.3};
    t.f = {{"hello", std::make_shared<std::string>("world")}, {"nullptr", nullptr}};
    t.g = std::make_unique<std::string>("hello");
    t.i = 123;
    NEKO_LOG_DEBUG("unit test", "{}", SerializableToString(t));
    EXPECT_TRUE(output(t));
    buffer.push_back('\0');

    zTypeTest1 t1;
    {
        JsonSerializer::InputSerializer input(buffer.data(), buffer.size());
        input(t1);
    }
    EXPECT_EQ(t.a, t1.a);
    EXPECT_EQ(t.b, t1.b);
    EXPECT_EQ(t.c, t1.c);
    EXPECT_EQ(t.d, t1.d);
    EXPECT_EQ(t.e, t1.e);
    EXPECT_EQ(*t.g, *t1.g);
    EXPECT_EQ(*t.f["hello"], *t1.f["hello"]);
    EXPECT_EQ(t.f["nullptr"], t1.f["nullptr"]);
    EXPECT_EQ(t.i.load(), t1.i.load());
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