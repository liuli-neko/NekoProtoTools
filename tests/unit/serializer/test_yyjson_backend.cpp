#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "nekoproto/global/global.hpp"
#include <gtest/gtest.h>

#ifdef NEKO_PROTO_ENABLE_YYJSON

#include "nekoproto/serialization/json/yyjson_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

using namespace nekoproto;

namespace {

struct YySmoke {
    int id = 0;
    std::string text;
    std::vector<int> values;
    std::optional<int> optional;

    NEKO_SERIALIZER(id, text, values, optional)
};

struct RawYyField {
    std::string payload;

    struct Neko {
        static constexpr auto value =
            Object("payload", makeTags<JsonTag{.raw_string = true}>(&RawYyField::payload));
    };
};

struct YyNested {
    std::string name;
    YySmoke smoke;

    NEKO_SERIALIZER(name, smoke)
};

template <typename T>
std::string writeJson(const T& value) {
    std::vector<char> buffer;
    YyJsonSerializer::OutputSerializer output(buffer);
    EXPECT_TRUE(output(value)) << (output.error() == nullptr ? "" : output.error()->msg);
    EXPECT_TRUE(output.end()) << (output.error() == nullptr ? "" : output.error()->msg);
    return {buffer.begin(), buffer.end()};
}

template <typename T>
bool readJson(std::string_view json, T& value) {
    YyJsonSerializer::InputSerializer input(json.data(), json.size());
    const auto result = input(value);
    EXPECT_TRUE(result) << (input.error() == nullptr ? "" : input.error()->msg);
    return static_cast<bool>(result);
}

} // namespace

TEST(YyJsonBackend, RoundTripsReflectionAndContainers) {
    const YySmoke source{.id = 7, .text = "hello", .values = {1, 2, 3}, .optional = 9};
    const auto json = writeJson(source);
    EXPECT_EQ(json, R"({"id":7,"text":"hello","values":[1,2,3],"optional":9})");

    YySmoke decoded;
    ASSERT_TRUE(readJson(json, decoded));
    EXPECT_EQ(decoded.id, source.id);
    EXPECT_EQ(decoded.text, source.text);
    EXPECT_EQ(decoded.values, source.values);
    EXPECT_EQ(decoded.optional, source.optional);
}

TEST(YyJsonBackend, HandlesNestedStructures) {
    const YyNested source{.name = "outer", .smoke = {.id = 42, .text = "inner", .values = {10, 20}, .optional = std::nullopt}};
    const auto json = writeJson(source);

    YyNested decoded;
    ASSERT_TRUE(readJson(json, decoded));
    EXPECT_EQ(decoded.name, source.name);
    EXPECT_EQ(decoded.smoke.id, source.smoke.id);
    EXPECT_EQ(decoded.smoke.text, source.smoke.text);
    EXPECT_EQ(decoded.smoke.values, source.smoke.values);
    EXPECT_FALSE(decoded.smoke.optional.has_value());
}

TEST(YyJsonBackend, RawStringRoundTrip) {
    const RawYyField source{.payload = R"({"enabled":true,"count":10})"};
    const auto json = writeJson(source);
    EXPECT_EQ(json, R"({"payload":{"enabled":true,"count":10}})");

    RawYyField decoded;
    ASSERT_TRUE(readJson(json, decoded));
    EXPECT_EQ(decoded.payload, R"({"enabled":true,"count":10})");
}

TEST(YyJsonBackend, YyJsonValueDOM) {
    const std::string json = R"({"title":"Neko","numbers":[10,20,30],"nested":{"flag":true}})";
    YyJsonSerializer::InputSerializer input(json.data(), json.size());
    YyJsonSerializer::JsonValue dom;
    ASSERT_TRUE(input(dom));
    EXPECT_TRUE(dom.isObject());
    EXPECT_EQ(dom.size(), 3u);

    auto title = dom["title"];
    EXPECT_TRUE(title.isString());
    std::string title_str;
    EXPECT_TRUE(title.value(title_str));
    EXPECT_EQ(title_str, "Neko");

    auto numbers = dom["numbers"];
    EXPECT_TRUE(numbers.isArray());
    EXPECT_EQ(numbers.size(), 3u);
    int num1 = 0;
    EXPECT_TRUE(numbers[1].value(num1));
    EXPECT_EQ(num1, 20);

    auto nested = dom["nested"];
    EXPECT_TRUE(nested.isObject());
    bool flag = false;
    EXPECT_TRUE(nested["flag"].value(flag));
    EXPECT_TRUE(flag);
}

TEST(YyJsonBackend, HandlesPrettyOutput) {
    const YySmoke source{.id = 1, .text = "test", .values = {1}, .optional = std::nullopt};
    std::vector<char> buffer;
    YyJsonSerializer::PrettyOutputSerializer output(buffer);
    ASSERT_TRUE(output(source));
    ASSERT_TRUE(output.end());
    std::string pretty(buffer.begin(), buffer.end());
    EXPECT_NE(pretty.find('\n'), std::string::npos);
}

#endif
