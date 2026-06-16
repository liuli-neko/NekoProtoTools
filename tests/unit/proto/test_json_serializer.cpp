#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/serialization/json/schema.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/types/binary_data.hpp"

NEKO_USE_NAMESPACE

enum class TEnum : uint32_t { TEnum_A = 1, TEnum_B = 2, TEnum_C = 3 };

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<TEnum> {
    using T                     = TEnum;
    static constexpr auto value = Enumerate{
        "TEnum_A", TEnum::TEnum_A, "TEnum_B", TEnum::TEnum_B, "TEnum_C", TEnum::TEnum_C}; // NOLINT
};
NEKO_END_NAMESPACE

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
    TEnum h                      = TEnum::TEnum_A;
    StructA i = {1, "hello", true, 3.14, {1, 2, 3, 4, 5}, {{"a", 1}, {"b", 2}, {"c", 3}},
                 {1, 2, 3, 4, 5}, TEnum::TEnum_A};
    std::tuple<int, std::string> j = {1, "hello"};
    std::vector<int> k             = {1, 2, 3, 4, 5};
    TestD l;

    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l)
    NEKO_DECLARE_PROTOCOL(TestP, JsonSerializer)
};

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

struct ZTypeTest2 {
    std::optional<int> a = {2};
    std::variant<std::monostate, std::string> b;

    NEKO_SERIALIZER(a, b)
};

struct SchemaNested {
    int nested = 0;

    NEKO_SERIALIZER(nested)
};

struct SchemaPolicy {
    int required = 0;
    int retained = 0;
    std::optional<int> optional;
    SchemaNested flat;
    std::variant<std::monostate, std::string> choice;
    std::map<std::string, int> named;
    std::map<int, std::string> indexed;
    std::tuple<int, std::string> tuple;
    std::array<int, 2> fixed{};

    struct Neko {
        constexpr static auto value =
            Object("required", &SchemaPolicy::required, "retained",
                   make_tags<JsonTags{.skipable = true}>(&SchemaPolicy::retained), "optional",
                   &SchemaPolicy::optional, "flat",
                   make_tags<JsonTags{.flat = true}>(&SchemaPolicy::flat), "choice",
                   &SchemaPolicy::choice, "named", &SchemaPolicy::named, "indexed",
                   &SchemaPolicy::indexed, "tuple", &SchemaPolicy::tuple, "fixed",
                   &SchemaPolicy::fixed); // NOLINT
    };
};

namespace {

std::string serializer_error(const sa::Error* error) {
    return error == nullptr ? "no error detail" : error->msg;
}

template <typename T>
std::string write_json(const T& value) {
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer output(buffer);
    const bool wrote = output(value);
    EXPECT_TRUE(wrote) << serializer_error(output.error());
    if (!wrote) {
        return {};
    }
    EXPECT_TRUE(output.end()) << serializer_error(output.error());
    return {buffer.begin(), buffer.end()};
}

template <typename T>
bool read_json(std::string_view json, T& value) {
    JsonSerializer::InputSerializer input(json.data(), json.size());
    const bool read = input(value);
    EXPECT_TRUE(read) << serializer_error(input.error());
    return read;
}

template <typename T>
std::string write_named(std::string_view name, const T& value) {
    auto field = make_name_value_pair(name, value);
    return write_json(field);
}

template <typename T>
bool read_named(std::string_view json, std::string_view name, T& value) {
    auto field = make_name_value_pair(name, value);
    return read_json(json, field);
}

TestP make_test_object() {
    TestP value;
    value.a = 3;
    value.b = "Struct test";
    value.c = true;
    value.d = 3.141592654;
    value.e = {1, 2, 3};
    value.f = {{"a", 1}, {"b", 2}};
    value.g = {1, 2, 3};
    value.h = TEnum::TEnum_A;
    value.i = {1, "hello", true, 3.141592654, {1, 2, 3}, {{"a", 1}, {"b", 2}},
               {1, 2, 3}, TEnum::TEnum_A};
    value.j = {1, "hello"};
    value.k = {1, 2, 3, 4, 5};
#if NEKO_CPP_PLUS >= 17
    std::get<1>(value.l.a).c = TestA{1221, "this is a test for optional"};
#endif
    return value;
}

void expect_test_object_eq(const TestP& actual, const TestP& expected) {
    EXPECT_EQ(actual.a, expected.a);
    EXPECT_EQ(actual.b, expected.b);
    EXPECT_EQ(actual.c, expected.c);
    EXPECT_DOUBLE_EQ(actual.d, expected.d);
    EXPECT_EQ(actual.e, expected.e);
    EXPECT_EQ(actual.f, expected.f);
    EXPECT_EQ(actual.g, expected.g);
    EXPECT_EQ(actual.h, expected.h);
    EXPECT_EQ(actual.i.a, expected.i.a);
    EXPECT_EQ(actual.i.b, expected.i.b);
    EXPECT_EQ(actual.i.c, expected.i.c);
    EXPECT_DOUBLE_EQ(actual.i.d, expected.i.d);
    EXPECT_EQ(actual.i.e, expected.i.e);
    EXPECT_EQ(actual.i.f, expected.i.f);
    EXPECT_EQ(actual.i.g, expected.i.g);
    EXPECT_EQ(actual.i.h, expected.i.h);
    EXPECT_EQ(actual.j, expected.j);
    EXPECT_EQ(actual.k, expected.k);
#if NEKO_CPP_PLUS >= 17
    ASSERT_TRUE(std::get<1>(actual.l.a).c.has_value());
    ASSERT_TRUE(std::get<1>(expected.l.a).c.has_value());
    EXPECT_EQ(std::get<1>(actual.l.a).c->a, std::get<1>(expected.l.a).c->a);
    EXPECT_EQ(std::get<1>(actual.l.a).c->b, std::get<1>(expected.l.a).c->b);
#endif
}

} // namespace

TEST(JsonSerializerTest, BasicNamedValuesUseGenericParser) {
    int integer = 1;
    EXPECT_EQ(write_named("a", integer), R"({"a":1})");
    int decodedInteger = 0;
    ASSERT_TRUE(read_named(R"({"a":1})", "a", decodedInteger));
    EXPECT_EQ(decodedInteger, integer);

    std::string string = "hello";
    EXPECT_EQ(write_named("value", string), R"({"value":"hello"})");
    std::string decodedString;
    ASSERT_TRUE(read_named(R"({"value":"hello"})", "value", decodedString));
    EXPECT_EQ(decodedString, string);

    std::string empty;
    EXPECT_EQ(write_named("value", empty), R"({"value":""})");
    std::string decodedEmpty = "not empty";
    ASSERT_TRUE(read_named(R"({"value":""})", "value", decodedEmpty));
    EXPECT_TRUE(decodedEmpty.empty());

    bool boolean = true;
    EXPECT_EQ(write_named("value", boolean), R"({"value":true})");
    bool decodedBoolean = false;
    ASSERT_TRUE(read_named(R"({"value":true})", "value", decodedBoolean));
    EXPECT_TRUE(decodedBoolean);

    double number = 3.14;
    EXPECT_EQ(write_named("value", number), R"({"value":3.14})");
    double decodedNumber = 0.0;
    ASSERT_TRUE(read_named(R"({"value":3.14})", "value", decodedNumber));
    EXPECT_DOUBLE_EQ(decodedNumber, number);
}

TEST(JsonSerializerTest, SequenceContainersRoundTrip) {
    const std::vector<int> vector{1, 2, 3, 4, 5};
    EXPECT_EQ(write_named("value", vector), R"({"value":[1,2,3,4,5]})");
    std::vector<int> decodedVector;
    ASSERT_TRUE(read_named(write_named("value", vector), "value", decodedVector));
    EXPECT_EQ(decodedVector, vector);

    const std::vector<int> emptyVector;
    EXPECT_EQ(write_named("value", emptyVector), R"({"value":[]})");
    decodedVector = {1, 2, 3};
    ASSERT_TRUE(read_named(R"({"value":[]})", "value", decodedVector));
    EXPECT_TRUE(decodedVector.empty());

    const std::list<int> list{1, 2, 3, 4, 5};
    EXPECT_EQ(write_named("value", list), R"({"value":[1,2,3,4,5]})");
    std::list<int> decodedList;
    ASSERT_TRUE(read_named(write_named("value", list), "value", decodedList));
    EXPECT_EQ(decodedList, list);

    const std::array<int, 5> array{1, 2, 3, 4, 5};
    EXPECT_EQ(write_named("value", array), R"({"value":[1,2,3,4,5]})");
    std::array<int, 5> decodedArray{};
    ASSERT_TRUE(read_named(write_named("value", array), "value", decodedArray));
    EXPECT_EQ(decodedArray, array);
}

TEST(JsonSerializerTest, OrderedMapsRoundTripWithFormatSpecificShapes) {
    const std::map<std::string, int> stringMap{{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_EQ(write_named("value", stringMap), R"({"value":{"a":1,"b":2,"c":3}})");
    std::map<std::string, int> decodedStringMap;
    ASSERT_TRUE(read_named(write_named("value", stringMap), "value", decodedStringMap));
    EXPECT_EQ(decodedStringMap, stringMap);

    const std::map<double, int> numericMap{{1.1, 1}, {2.2, 2}, {3.3, 3}};
    EXPECT_EQ(write_named("value", numericMap),
              R"({"value":[{"key":1.1,"value":1},{"key":2.2,"value":2},{"key":3.3,"value":3}]})");
    std::map<double, int> decodedNumericMap;
    ASSERT_TRUE(read_named(write_named("value", numericMap), "value", decodedNumericMap));
    EXPECT_EQ(decodedNumericMap, numericMap);

    const std::map<std::string, std::vector<int>> emptyMap;
    EXPECT_EQ(write_named("value", emptyMap), R"({"value":{}})");
    std::map<std::string, std::vector<int>> decodedEmptyMap{{"old", {1}}};
    ASSERT_TRUE(read_named(R"({"value":{}})", "value", decodedEmptyMap));
    EXPECT_TRUE(decodedEmptyMap.empty());
}

TEST(JsonSerializerTest, UnorderedMapsRoundTrip) {
    const std::unordered_map<std::string, int> stringMap{{"a", 1}, {"b", 2}, {"c", 3}};
    const auto stringJson = write_named("value", stringMap);
    std::unordered_map<std::string, int> decodedStringMap;
    ASSERT_TRUE(read_named(stringJson, "value", decodedStringMap));
    EXPECT_EQ(decodedStringMap, stringMap);

    const std::unordered_map<double, int> numericMap{{1.1, 1}, {2.2, 2}, {3.3, 3}};
    const auto numericJson = write_named("value", numericMap);
    std::unordered_map<double, int> decodedNumericMap;
    ASSERT_TRUE(read_named(numericJson, "value", decodedNumericMap));
    EXPECT_EQ(decodedNumericMap, numericMap);

    const std::unordered_multimap<double, int> multiMap{{1.1, 1}, {1.1, 2}, {2.2, 3}};
    const auto multiJson = write_named("value", multiMap);
    std::unordered_multimap<double, int> decodedMultiMap;
    ASSERT_TRUE(read_named(multiJson, "value", decodedMultiMap));
    EXPECT_EQ(decodedMultiMap, multiMap);

    const std::unordered_map<double, double> emptyMap;
    EXPECT_EQ(write_named("value", emptyMap), R"({"value":[]})");
}

TEST(JsonSerializerTest, SetContainersRoundTrip) {
    const std::set<int> ordered{1, 2, 3, 4, 5};
    EXPECT_EQ(write_named("value", ordered), R"({"value":[1,2,3,4,5]})");
    std::set<int> decodedOrdered;
    ASSERT_TRUE(read_named(write_named("value", ordered), "value", decodedOrdered));
    EXPECT_EQ(decodedOrdered, ordered);

    const std::unordered_set<int> unordered{1, 2, 3, 4, 5};
    const auto unorderedJson = write_named("value", unordered);
    std::unordered_set<int> decodedUnordered;
    ASSERT_TRUE(read_named(unorderedJson, "value", decodedUnordered));
    EXPECT_EQ(decodedUnordered, unordered);

    const std::multiset<int> multi{1, 1, 1, 2, 2, 3};
    const auto multiJson = write_named("value", multi);
    std::multiset<int> decodedMulti;
    ASSERT_TRUE(read_named(multiJson, "value", decodedMulti));
    EXPECT_EQ(decodedMulti, multi);

    const std::unordered_multiset<int> unorderedMulti{1, 1, 1, 2, 2, 3};
    const auto unorderedMultiJson = write_named("value", unorderedMulti);
    std::unordered_multiset<int> decodedUnorderedMulti;
    ASSERT_TRUE(read_named(unorderedMultiJson, "value", decodedUnorderedMulti));
    EXPECT_EQ(decodedUnorderedMulti, unorderedMulti);
}

TEST(JsonSerializerTest, AtomicBinaryAndBitsetRoundTrip) {
    std::atomic<int> atomic{5};
    EXPECT_EQ(write_named("value", atomic), R"({"value":5})");
    std::atomic<int> decodedAtomic{0};
    ASSERT_TRUE(read_named(R"({"value":5})", "value", decodedAtomic));
    EXPECT_EQ(decodedAtomic.load(), atomic.load());

    const std::string data = "hello world";
    BinaryData<const char> encoded(data.data(), data.size());
    EXPECT_EQ(write_named("value", encoded), R"({"value":"aGVsbG8gd29ybGQ="})");
    std::vector<char> decodedData(data.size());
    BinaryData<char> decodedBinary(decodedData.data(), decodedData.size());
    ASSERT_TRUE(read_named(R"({"value":"aGVsbG8gd29ybGQ="})", "value", decodedBinary));
    EXPECT_EQ(std::string(decodedData.begin(), decodedData.end()), data);

    const std::bitset<5> bitset{0b10101};
    EXPECT_EQ(write_named("value", bitset), R"({"value":"10101"})");
    std::bitset<5> decodedBitset;
    ASSERT_TRUE(read_named(R"({"value":"10101"})", "value", decodedBitset));
    EXPECT_EQ(decodedBitset, bitset);
}

TEST(JsonSerializerTest, PairAndPointersRoundTrip) {
    const std::pair<int, std::string> pair{1, "hello world"};
    EXPECT_EQ(write_named("value", pair), R"({"value":{"first":1,"second":"hello world"}})");
    std::pair<int, std::string> decodedPair;
    ASSERT_TRUE(read_named(write_named("value", pair), "value", decodedPair));
    EXPECT_EQ(decodedPair, pair);

    const auto shared = std::make_shared<int>(42);
    EXPECT_EQ(write_named("value", shared), R"({"value":42})");
    std::shared_ptr<int> decodedShared;
    ASSERT_TRUE(read_named(R"({"value":42})", "value", decodedShared));
    ASSERT_TRUE(decodedShared);
    EXPECT_EQ(*decodedShared, *shared);

    const std::shared_ptr<int> emptyShared;
    EXPECT_EQ(write_named("value", emptyShared), R"({"value":null})");
    decodedShared = std::make_shared<int>(7);
    ASSERT_TRUE(read_named(R"({"value":null})", "value", decodedShared));
    EXPECT_FALSE(decodedShared);

    const std::unique_ptr<int> unique = std::make_unique<int>(42);
    EXPECT_EQ(write_named("value", unique), R"({"value":42})");
    std::unique_ptr<int> decodedUnique;
    ASSERT_TRUE(read_named(R"({"value":42})", "value", decodedUnique));
    ASSERT_TRUE(decodedUnique);
    EXPECT_EQ(*decodedUnique, *unique);

    const std::unique_ptr<int> emptyUnique;
    EXPECT_EQ(write_named("value", emptyUnique), R"({"value":null})");
    decodedUnique = std::make_unique<int>(7);
    ASSERT_TRUE(read_named(R"({"value":null})", "value", decodedUnique));
    EXPECT_FALSE(decodedUnique);
}

#if NEKO_CPP_PLUS >= 17
TEST(JsonSerializerTest, AggregateVariantAndOptionalRoundTrip) {
    struct Aggregate {
        int a;
        std::string b;
        bool c;
    };

    const Aggregate aggregate{3, "Struct test", true};
    EXPECT_EQ(write_named("value", aggregate), R"({"value":{"a":3,"b":"Struct test","c":true}})");
    Aggregate decodedAggregate{};
    ASSERT_TRUE(read_named(write_named("value", aggregate), "value", decodedAggregate));
    EXPECT_EQ(decodedAggregate.a, aggregate.a);
    EXPECT_EQ(decodedAggregate.b, aggregate.b);
    EXPECT_EQ(decodedAggregate.c, aggregate.c);

    const std::variant<int, std::string> integerVariant{3};
    EXPECT_EQ(write_named("value", integerVariant), R"({"value":3})");
    std::variant<int, std::string> decodedIntegerVariant;
    ASSERT_TRUE(read_named(R"({"value":3})", "value", decodedIntegerVariant));
    EXPECT_EQ(decodedIntegerVariant, integerVariant);

    const std::variant<int, std::string> stringVariant{"test"};
    EXPECT_EQ(write_named("value", stringVariant), R"({"value":"test"})");
    std::variant<int, std::string> decodedStringVariant;
    ASSERT_TRUE(read_named(R"({"value":"test"})", "value", decodedStringVariant));
    EXPECT_EQ(decodedStringVariant, stringVariant);

    const std::optional<int> optional{3};
    EXPECT_EQ(write_named("value", optional), R"({"value":3})");
    std::optional<int> decodedOptional;
    ASSERT_TRUE(read_named(R"({"value":3})", "value", decodedOptional));
    EXPECT_EQ(decodedOptional, optional);

    const std::optional<int> emptyOptional;
    EXPECT_EQ(write_named("value", emptyOptional), R"({"value":null})");
    decodedOptional = 3;
    ASSERT_TRUE(read_named(R"({"value":null})", "value", decodedOptional));
    EXPECT_FALSE(decodedOptional.has_value());
}
#endif

#if NEKO_CPP_PLUS >= 20
TEST(JsonSerializerTest, U8StringRoundTrip) {
    const std::u8string value = u8"这是一个测试; this is a test.";
    const auto json            = write_named("value", value);
    EXPECT_EQ(json, R"({"value":"这是一个测试; this is a test."})");

    std::u8string decoded;
    ASSERT_TRUE(read_named(json, "value", decoded));
    EXPECT_TRUE(decoded == value);
}
#endif

TEST(JsonSerializerTest, ReflectedObjectRoundTripsThroughPublicEntry) {
    const TestP source = make_test_object();
    const auto json    = write_json(source);
    EXPECT_NE(json.find(R"("h":"TEnum_A")"), std::string::npos);
    EXPECT_NE(json.find(R"("j":[1,"hello"])"), std::string::npos);
    EXPECT_NE(json.find(R"("c":{"a":1221,"b":"this is a test for optional"})"), std::string::npos);

    TestP decoded;
    ASSERT_TRUE(read_json(json, decoded));
    expect_test_object_eq(decoded, source);

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    std::vector<char> prettyBuffer;
    RapidJsonOutputSerializer<detail::PrettyJsonWriter<>> pretty(prettyBuffer, JsonOutputFormatOptions::Default());
    ASSERT_TRUE(pretty(source)) << serializer_error(pretty.error());
    ASSERT_TRUE(pretty.end()) << serializer_error(pretty.error());
    const std::string prettyJson(prettyBuffer.begin(), prettyBuffer.end());
    EXPECT_NE(prettyJson.find('\n'), std::string::npos);
#endif
}

TEST(JsonSerializerTest, IOStreamRoundTripUsesTheSameParserPath) {
    const TestP source = make_test_object();
    std::stringstream stream;

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    {
        RapidJsonOutputSerializer<detail::PrettyJsonWriter<std::stringstream>> output(stream);
        ASSERT_TRUE(output(source)) << serializer_error(output.error());
        ASSERT_TRUE(output.end()) << serializer_error(output.error());
    }

    stream.seekg(0);
    TestP decoded;
    {
        RapidJsonInputSerializer<std::stringstream> input(stream);
        ASSERT_TRUE(input(decoded)) << serializer_error(input.error());
    }
    expect_test_object_eq(decoded, source);
#endif
}

TEST(JsonSerializerTest, CompositeUnorderedTypesRoundTrip) {
    ZTypeTest1 source;
    source.a = {{1, 1}, {2, 2}};
    source.b = {{"hello", "world"}};
    source.c = {1, 2, 3};
    source.d = {{1.1, 1}, {2.2, 2}};
    source.e = {1.1, 2.2, 3.3};
    source.f = {{"hello", std::make_shared<std::string>("world")}, {"nullptr", nullptr}};
    source.g = std::make_unique<std::string>("hello");
    source.i = 123;

    const auto json = write_json(source);
    ZTypeTest1 decoded;
    ASSERT_TRUE(read_json(json, decoded));
    EXPECT_EQ(decoded.a, source.a);
    EXPECT_EQ(decoded.b, source.b);
    EXPECT_EQ(decoded.c, source.c);
    EXPECT_EQ(decoded.d, source.d);
    EXPECT_EQ(decoded.e, source.e);
    ASSERT_TRUE(decoded.g);
    EXPECT_EQ(*decoded.g, *source.g);
    ASSERT_TRUE(decoded.f["hello"]);
    EXPECT_EQ(*decoded.f["hello"], *source.f["hello"]);
    EXPECT_EQ(decoded.f["nullptr"], source.f["nullptr"]);
    EXPECT_EQ(decoded.i.load(), source.i.load());
}

TEST(JsonSerializerTest, OptionalVariantObjectRoundTrip) {
    ZTypeTest2 source;
    source.b = "hello";

    const auto json = write_json(source);
    ZTypeTest2 decoded;
    ASSERT_TRUE(read_json(json, decoded));
    EXPECT_EQ(decoded.a, source.a);
    EXPECT_EQ(decoded.b, source.b);
}

TEST(JsonSerializerTest, JsonSchemaUsesGenericReflectionParser) {
    static_assert(detail::has_values_meta<TestP>);
    static_assert(detail::has_values_meta<JsonSchema>);

    const TestP value = make_test_object();
    JsonSchema schema;
    ASSERT_TRUE(generate_schema<TestP>(value, schema));

    const auto json = write_json(schema);
    EXPECT_NE(json.find(R"("$schema":"http://json-schema.org/draft-07/schema#")"), std::string::npos);
    EXPECT_NE(json.find(R"("type":"object")"), std::string::npos);
    EXPECT_NE(json.find(R"("properties":)"), std::string::npos);
    NEKO_LOG_INFO("test", "{}", json);
}

TEST(JsonSerializerTest, JsonSchemaFollowsGenericParserShapes) {
    JsonSchema schema;
    ASSERT_TRUE(generate_schema<SchemaPolicy>(schema));
    ASSERT_EQ(schema.type, "object");
    ASSERT_TRUE(schema.properties);

    const auto& properties = *schema.properties;
    EXPECT_TRUE(properties.contains("required"));
    EXPECT_TRUE(properties.contains("retained"));
    EXPECT_TRUE(properties.contains("optional"));
    EXPECT_TRUE(properties.contains("nested"));
    EXPECT_FALSE(properties.contains("flat"));

    ASSERT_TRUE(schema.required);
    const auto isRequired = [&schema](std::string_view name) {
        return std::find(schema.required->begin(), schema.required->end(), name) !=
               schema.required->end();
    };
    EXPECT_TRUE(isRequired("required"));
    EXPECT_TRUE(isRequired("nested"));
    EXPECT_TRUE(isRequired("named"));
    EXPECT_TRUE(isRequired("indexed"));
    EXPECT_TRUE(isRequired("tuple"));
    EXPECT_TRUE(isRequired("fixed"));
    EXPECT_FALSE(isRequired("retained"));
    EXPECT_FALSE(isRequired("optional"));
    EXPECT_FALSE(isRequired("choice"));

    ASSERT_TRUE(properties.at("optional").oneOf);
    ASSERT_EQ(properties.at("optional").oneOf->size(), 2U);
    EXPECT_EQ((*properties.at("optional").oneOf)[0].type, "integer");
    EXPECT_EQ((*properties.at("optional").oneOf)[1].type, "null");

    ASSERT_TRUE(properties.at("choice").oneOf);
    ASSERT_EQ(properties.at("choice").oneOf->size(), 2U);
    EXPECT_EQ((*properties.at("choice").oneOf)[0].type, "null");
    EXPECT_EQ((*properties.at("choice").oneOf)[1].type, "string");

    const auto& named = properties.at("named");
    EXPECT_EQ(named.type, "object");
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<JsonSchema>>(named.additionalProperties));
    EXPECT_EQ(std::get<std::unique_ptr<JsonSchema>>(named.additionalProperties)->type, "integer");

    const auto& indexed = properties.at("indexed");
    EXPECT_EQ(indexed.type, "array");
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<JsonSchema>>(indexed.items));
    const auto& mapEntry = *std::get<std::unique_ptr<JsonSchema>>(indexed.items);
    ASSERT_TRUE(mapEntry.required);
    EXPECT_EQ(*mapEntry.required, (std::vector<std::string>{"key", "value"}));

    const auto& tuple = properties.at("tuple");
    EXPECT_EQ(tuple.minItems, 2U);
    EXPECT_EQ(tuple.maxItems, 2U);
    EXPECT_EQ(tuple.additionalItems, false);
    ASSERT_TRUE(std::holds_alternative<std::vector<JsonSchema>>(tuple.items));
    EXPECT_EQ(std::get<std::vector<JsonSchema>>(tuple.items).size(), 2U);

    const auto& fixed = properties.at("fixed");
    EXPECT_EQ(fixed.minItems, 2U);
    EXPECT_EQ(fixed.maxItems, 2U);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<JsonSchema>>(fixed.items));
    EXPECT_EQ(std::get<std::unique_ptr<JsonSchema>>(fixed.items)->type, "integer");
}

#define CUSTOM_MAIN ProtoFactory factor(1, 0, 0);

#include "../common/common_main.cpp.in" // IWYU pragma: export
