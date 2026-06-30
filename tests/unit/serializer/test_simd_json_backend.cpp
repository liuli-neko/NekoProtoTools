#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#ifdef NEKO_PROTO_ENABLE_SIMDJSON

#include "nekoproto/serialization/json/simd_json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_USE_NAMESPACE

namespace {

struct SimdSmoke {
    int id = 0;
    std::string text;
    std::vector<int> values;
    std::optional<int> optional;

    NEKO_SERIALIZER(id, text, values, optional)
};

struct RawSimdField {
    std::string payload;

    struct Neko {
        static constexpr auto value =
            Object("payload", make_tags<JsonTag{.raw_string = true}>(&RawSimdField::payload)); // NOLINT
    };
};

struct SimdRequiredField {
    int value = 0;

    NEKO_SERIALIZER(value)
};

template <typename T>
std::string write_json(const T& value) {
    std::vector<char> buffer;
    SimdJsonSerializer::OutputSerializer output(buffer);
    EXPECT_TRUE(output(value)) << (output.error() == nullptr ? "" : output.error()->msg);
    EXPECT_TRUE(output.end()) << (output.error() == nullptr ? "" : output.error()->msg);
    return {buffer.begin(), buffer.end()};
}

template <typename T>
bool read_json(std::string_view json, T& value) {
    SimdJsonSerializer::InputSerializer input(json.data(), json.size());
    const auto result = input(value);
    EXPECT_TRUE(result) << (input.error() == nullptr ? "" : input.error()->msg);
    return result;
}

} // namespace

TEST(SimdJsonBackend, RoundTripsReflectionAndContainers) {
    const SimdSmoke source{.id = 7, .text = "hello", .values = {1, 2, 3}, .optional = 9};
    const auto json = write_json(source);
    EXPECT_EQ(json, R"({"id":7,"text":"hello","values":[1,2,3],"optional":9})");

    SimdSmoke decoded;
    ASSERT_TRUE(read_json(json, decoded));
    EXPECT_EQ(decoded.id, source.id);
    EXPECT_EQ(decoded.text, source.text);
    EXPECT_EQ(decoded.values, source.values);
    EXPECT_EQ(decoded.optional, source.optional);
}

TEST(SimdJsonBackend, TextWriterEscapesStrings) {
    const std::string source = "quote=\" slash=\\ newline=\n";
    const auto json = write_json(source);
    EXPECT_EQ(json, R"("quote=\" slash=\\ newline=\n")");

    std::string decoded;
    ASSERT_TRUE(read_json(json, decoded));
    EXPECT_EQ(decoded, source);
}

TEST(SimdJsonBackend, RawStringUsesSimdjsonValidation) {
    const RawSimdField source{.payload = R"({"enabled":true,"items":[1,2]})"};
    const auto json = write_json(source);
    EXPECT_EQ(json, R"({"payload":{"enabled":true,"items":[1,2]}})");

    RawSimdField decoded;
    ASSERT_TRUE(read_json(json, decoded));
    EXPECT_EQ(decoded.payload, source.payload);

    RawSimdField invalid{.payload = R"({"broken":)"};
    std::vector<char> buffer;
    SimdJsonSerializer::OutputSerializer output(buffer);
    EXPECT_FALSE(output(invalid));
    ASSERT_NE(output.error(), nullptr);
    EXPECT_EQ(output.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
}

TEST(SimdJsonBackend, StringKeyMapUsesJsonObject) {
    const std::map<std::string, int> source{{"a", 1}, {"b", 2}};
    const auto json = write_json(source);
    EXPECT_EQ(json, R"({"a":1,"b":2})");

    std::map<std::string, int> decoded;
    ASSERT_TRUE(read_json(json, decoded));
    EXPECT_EQ(decoded, source);
}

TEST(SimdJsonBackend, MissingRequiredFieldReportsContext) {
    const std::string json = R"({})";
    SimdRequiredField decoded;
    SimdJsonSerializer::InputSerializer input(json.data(), json.size());

    EXPECT_FALSE(input(decoded));
    ASSERT_NE(input.error(), nullptr);
    EXPECT_EQ(input.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidField));
    EXPECT_NE(input.error()->msg.find("Required field 'value' is missing"), std::string::npos);
}

TEST(SimdJsonBackend, NativeValueKeepsParserAlive) {
    detail::simd::SimdJsonValue value;
    {
        const std::string json = R"({"nested":{"value":42}})";
        SimdJsonSerializer::InputSerializer input(json.data(), json.size());
        ASSERT_TRUE(input(value));
    }

    ASSERT_TRUE(value.hasValue());
    ASSERT_TRUE(value["nested"].isObject());
    int decoded = 0;
    EXPECT_TRUE(value["nested"]["value"].value(decoded));
    EXPECT_EQ(decoded, 42);

    const auto json = write_json(value);
    EXPECT_EQ(json, R"({"nested":{"value":42}})");
}

TEST(SimdJsonBackend, WritesToOStream) {
    std::ostringstream stream;
    SimdJsonOutputSerializer<std::ostringstream> output(stream);
    ASSERT_TRUE(output(std::vector<int>{1, 2, 3}));
    ASSERT_TRUE(output.end());
    EXPECT_EQ(stream.str(), "[1,2,3]");
}

TEST(SimdJsonBackend, InvalidJsonReturnsParseError) {
    const std::string json = R"({"value":)";
    SimdRequiredField decoded;
    SimdJsonSerializer::InputSerializer input(json.data(), json.size());

    EXPECT_FALSE(input(decoded));
    ASSERT_NE(input.error(), nullptr);
    EXPECT_EQ(input.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
    EXPECT_NE(input.error()->msg.find("simdjson parse error"), std::string::npos);
}

#endif

#include "../common/common_main.cpp.in" // IWYU pragma: export
