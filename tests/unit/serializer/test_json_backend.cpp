#include <atomic>
#include <array>
#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/parsing/atomic.hpp"
#include "nekoproto/serialization/parsing/optional.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_USE_NAMESPACE

enum class ParserBackendEnum { Ready = 1, Stopped = 2 };

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<ParserBackendEnum> {
    using T                     = ParserBackendEnum;
    static constexpr auto value = Enumerate{"Ready", T::Ready, "Stopped", T::Stopped}; // NOLINT
};
NEKO_END_NAMESPACE

namespace {
struct ParserBackendSmoke {
    int id = 0;
    std::optional<std::string> label;
    std::atomic<int> count{0};

    NEKO_SERIALIZER(id, label, count)
};

struct RawJsonField {
    std::string payload;

    struct Neko {
        constexpr static auto value =
            Object("payload", make_tags<JsonTag{.raw_string = true}>(&RawJsonField::payload)); // NOLINT
    };
};

struct FlatJsonValue {
    int code = 0;

    NEKO_SERIALIZER(make_tags<rename_tag<"accode">>(code))
};

struct FlatJsonObject {
    int id = 0;
    FlatJsonValue nested;

    NEKO_SERIALIZER(id, make_tags<JsonTag{.flat = true}>(nested))
};

struct MissingFieldPolicy {
    int required = 1;
    int retained = 2;
    std::optional<int> optional = 3;

    struct Neko {
        constexpr static auto value =
            Object("required", &MissingFieldPolicy::required, "retained",
                   make_tags<JsonTag{.skippable = true}>(&MissingFieldPolicy::retained), "optional",
                   &MissingFieldPolicy::optional); // NOLINT
    };
};

struct FixedLengthJsonField {
    int value = 0;

    struct Neko {
        constexpr static auto value =
            Object("value", make_tags<BinaryTag{.fixed_length = 2}>(&FixedLengthJsonField::value)); // NOLINT
    };
};

struct UnframedBinaryLayout {
    int value = 0;

    struct Neko {
        static constexpr auto value =
            Object("value", &UnframedBinaryLayout::value); // NOLINT
    };
};

template <typename T>
std::vector<char> write_json(const T& value) {
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer out(buffer);
    EXPECT_TRUE(out(value));
    EXPECT_TRUE(out.end());
    return buffer;
}

template <typename T>
void read_json(const std::vector<char>& buffer, T& value) {
    JsonSerializer::InputSerializer in(buffer.data(), buffer.size());
    EXPECT_TRUE(in(value));
    EXPECT_TRUE(in);
}

std::string as_string(const std::vector<char>& buffer) {
    return {buffer.begin(), buffer.end()};
}

std::string as_string(const std::vector<std::byte>& buffer) {
    std::string text;
    text.reserve(buffer.size());
    for (auto byte : buffer) {
        text.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
    }
    return text;
}
} // namespace

TEST(RapidJsonBackendParser, RoundTripsBasicRootThroughParserEntry) {
    const auto buffer = write_json(42);
    EXPECT_EQ(as_string(buffer), "42");

    int decoded = 0;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, 42);
}

TEST(RapidJsonBackendParser, RoundTripsStringRootThroughGenericParser) {
    const auto buffer = write_json(std::string{"ready"});
    EXPECT_EQ(as_string(buffer), "\"ready\"");

    std::string decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, "ready");
}

TEST(RapidJsonBackendParser, WritesByteOutputBuffer) {
    std::vector<std::byte> buffer;
    JsonSerializer::ByteOutputSerializer out(buffer);
    ASSERT_TRUE(out(std::tuple{7, std::string{"byte"}}));
    ASSERT_TRUE(out.end());
    EXPECT_EQ(as_string(buffer), R"([7,"byte"])");

    std::tuple<int, std::string> decoded;
    JsonSerializer::InputSerializer in(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    ASSERT_TRUE(in(decoded));
    EXPECT_EQ(decoded, std::make_tuple(7, std::string{"byte"}));
}

TEST(RapidJsonBackendParser, WritesNullRootThroughGenericParser) {
    const auto buffer = write_json(nullptr);
    EXPECT_EQ(as_string(buffer), "null");
}

TEST(RapidJsonBackendParser, RoundTripsEnumRootThroughGenericParser) {
    const auto buffer = write_json(ParserBackendEnum::Ready);
    EXPECT_EQ(as_string(buffer), "\"Ready\"");

    ParserBackendEnum decoded = ParserBackendEnum::Stopped;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, ParserBackendEnum::Ready);
}

TEST(RapidJsonBackendParser, RoundTripsOptionalRootThroughGenericParser) {
    const std::optional<std::string> source;
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "null");

    std::optional<std::string> decoded = "value";
    read_json(buffer, decoded);
    EXPECT_FALSE(decoded.has_value());
}

TEST(RapidJsonBackendParser, RoundTripsVectorRootThroughGenericSequenceParser) {
    const std::vector<int> source{1, 2, 3};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "[1,2,3]");

    std::vector<int> decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsListRootThroughGenericSequenceParser) {
    const std::list<std::string> source{"a", "b"};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "[\"a\",\"b\"]");

    std::list<std::string> decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsArrayRootThroughGenericSequenceParser) {
    const std::array<int, 3> source{4, 5, 6};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "[4,5,6]");

    std::array<int, 3> decoded{};
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsSetRootThroughGenericSequenceParser) {
    const std::set<int> source{3, 1, 2};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "[1,2,3]");

    std::set<int> decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsStringKeyMapRootThroughGenericMapParser) {
    const std::map<std::string, int> source{{"a", 1}, {"b", 2}};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "{\"a\":1,\"b\":2}");

    std::map<std::string, int> decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsNonStringKeyMapRootThroughGenericMapParser) {
    const std::map<int, std::string> source{{1, "one"}, {2, "two"}};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "[{\"key\":1,\"value\":\"one\"},{\"key\":2,\"value\":\"two\"}]");

    std::map<int, std::string> decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsPairRootThroughGenericTupleParser) {
    const std::pair<int, std::string> source{1, "one"};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "{\"first\":1,\"second\":\"one\"}");

    std::pair<int, std::string> decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsTupleRootThroughGenericTupleParser) {
    const std::tuple<int, std::string, bool> source{1, "one", true};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "[1,\"one\",true]");

    std::tuple<int, std::string, bool> decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded, source);
}

TEST(RapidJsonBackendParser, RoundTripsSharedPtrRootThroughGenericPointerParser) {
    const auto source = std::make_shared<std::string>("owned");
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "\"owned\"");

    std::shared_ptr<std::string> decoded;
    read_json(buffer, decoded);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(*decoded, *source);
}

TEST(RapidJsonBackendParser, RoundTripsUniquePtrNullRootThroughGenericPointerParser) {
    const std::unique_ptr<int> source;
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "null");

    auto decoded = std::make_unique<int>(7);
    read_json(buffer, decoded);
    EXPECT_FALSE(decoded);
}

TEST(RapidJsonBackendParser, RoundTripsVariantRootThroughGenericVariantParser) {
    const std::variant<int, std::string> source = std::string{"variant"};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "\"variant\"");

    std::variant<int, std::string> decoded;
    read_json(buffer, decoded);
    ASSERT_TRUE(std::holds_alternative<std::string>(decoded));
    EXPECT_EQ(std::get<std::string>(decoded), std::get<std::string>(source));
}

TEST(RapidJsonBackendParser, RoundTripsMonostateVariantRootThroughGenericVariantParser) {
    const std::variant<std::monostate, int> source = std::monostate{};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "null");

    std::variant<std::monostate, int> decoded = 7;
    read_json(buffer, decoded);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(decoded));
}

TEST(RapidJsonBackendParser, RoundTripsAtomicRootThroughGenericParser) {
    std::atomic<int> source{11};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), "11");

    std::atomic<int> decoded{0};
    read_json(buffer, decoded);
    EXPECT_EQ(decoded.load(), 11);
}

TEST(RapidJsonBackendParser, RoundTripsReflectObjectThroughParserEntry) {
    ParserBackendSmoke source;
    source.id    = 7;
    source.label = "ready";
    source.count.store(3);

    const auto buffer = write_json(source);

    ParserBackendSmoke decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded.id, source.id);
    ASSERT_TRUE(decoded.label.has_value());
    EXPECT_EQ(*decoded.label, *source.label);
    EXPECT_EQ(decoded.count.load(), source.count.load());
}

TEST(RapidJsonBackendParser, RawStringTagIsHandledByJsonStringParser) {
    const RawJsonField source{.payload = R"({"enabled":true,"count":2})"};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), R"({"payload":{"enabled":true,"count":2}})");

    RawJsonField decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded.payload, source.payload);
}

TEST(RapidJsonBackendParser, FlatTagIsHandledByReflectObjectParser) {
    const FlatJsonObject source{.id = 7, .nested = {.code = 9}};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), R"({"id":7,"accode":9})");

    FlatJsonObject decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded.id, 7);
    EXPECT_EQ(decoded.nested.code, 9);
}

TEST(RapidJsonBackendParser, MissingFieldPolicyPreservesExplicitlySkippableValue) {
    const std::string json = R"({"required":5})";
    MissingFieldPolicy decoded{.required = 1, .retained = 22, .optional = 33};
    JsonSerializer::InputSerializer in(json.data(), json.size());

    EXPECT_TRUE(in(decoded));
    EXPECT_EQ(decoded.required, 5);
    EXPECT_EQ(decoded.retained, 22);
    EXPECT_FALSE(decoded.optional.has_value());
}

TEST(RapidJsonBackendParser, MissingRequiredFieldAndNullScalarFail) {
    {
        const std::string json = R"({"retained":5})";
        MissingFieldPolicy decoded;
        JsonSerializer::InputSerializer in(json.data(), json.size());
        EXPECT_FALSE(in(decoded));
        ASSERT_NE(in.error(), nullptr);
        EXPECT_EQ(in.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidField));
        EXPECT_NE(in.error()->msg.find("Required field 'required' is missing"), std::string::npos);
    }
    {
        const std::string json = R"({"required":5,"retained":null})";
        MissingFieldPolicy decoded;
        JsonSerializer::InputSerializer in(json.data(), json.size());
        EXPECT_FALSE(in(decoded));
        ASSERT_NE(in.error(), nullptr);
        EXPECT_EQ(in.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType));
        EXPECT_NE(in.error()->msg.find("Failed to parse field 'retained'"), std::string::npos);
        EXPECT_NE(in.error()->msg.find("Expected integer"), std::string::npos);
    }
}

TEST(RapidJsonBackendParser, BinaryFixedLengthTagIsIgnoredByJsonParser) {
    const FixedLengthJsonField source{.value = 42};
    const auto buffer = write_json(source);
    EXPECT_EQ(as_string(buffer), R"({"value":42})");

    FixedLengthJsonField decoded;
    read_json(buffer, decoded);
    EXPECT_EQ(decoded.value, 42);
}

TEST(RapidJsonBackendParser, BinaryLayoutTagIsIgnoredByJsonParser) {
    const UnframedBinaryLayout source{.value = 42};
    const auto buffer = write_json(make_tags<BinaryTag{.unframed = true}>(source));
    EXPECT_EQ(as_string(buffer), R"({"value":42})");

    UnframedBinaryLayout decoded;
    auto taggedDecoded = make_tags<BinaryTag{.unframed = true}>(decoded);
    read_json(buffer, taggedDecoded);
    EXPECT_EQ(decoded.value, 42);
}

TEST(RapidJsonBackendParser, TupleLengthFailureReportsExpectedAndActualSize) {
    const std::string json = R"([1])";
    std::tuple<int, std::string> decoded;
    JsonSerializer::InputSerializer in(json.data(), json.size());

    EXPECT_FALSE(in(decoded));
    ASSERT_NE(in.error(), nullptr);
    EXPECT_EQ(in.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidLength));
    EXPECT_NE(in.error()->msg.find("Expected tuple with 2 elements, got 1"), std::string::npos);
}

TEST(RapidJsonBackendParser, InvalidJsonReportsParseErrorAndOffset) {
    const std::string json = R"({"value":)";
    MissingFieldPolicy decoded;
    JsonSerializer::InputSerializer in(json.data(), json.size());

    EXPECT_FALSE(in(decoded));
    ASSERT_NE(in.error(), nullptr);
    EXPECT_EQ(in.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
#ifdef NEKO_PROTO_ENABLE_RAPIDJSON
    EXPECT_NE(in.error()->msg.find("RapidJSON parse error at offset"), std::string::npos) << in.error()->msg;
#elif defined(NEKO_PROTO_ENABLE_SIMDJSON)
    EXPECT_NE(in.error()->msg.find("The JSON document has an improper structure: missing or superfluous commas, braces, missing keys, etc."), std::string::npos) << in.error()->msg;
#endif
}

TEST(RapidJsonBackendParser, InvalidRawStringReportsParseError) {
    RawJsonField source{.payload = R"({"broken":)"};
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer out(buffer);

    EXPECT_FALSE(out(source));
    ASSERT_NE(out.error(), nullptr);
    EXPECT_EQ(out.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
    EXPECT_NE(out.error()->msg.find("Failed to write field 'payload'"), std::string::npos);
    EXPECT_NE(out.error()->msg.find("Invalid raw value"), std::string::npos);
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
