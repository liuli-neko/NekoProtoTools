#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nekoproto/serialization/private/tags.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/toml_serializer.hpp"

NEKO_USE_NAMESPACE

#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)

namespace {

struct TomlNested {
    int code = 0;
    std::string label;

    NEKO_SERIALIZER((make_tags<rename_tag<"wire_code">>(code)), label)
};

struct TomlDocument {
    std::string title;
    int count = 0;
    std::vector<int> values;
    std::optional<std::string> missing;
    TomlNested nested;

    NEKO_SERIALIZER(title, count, values, missing, (make_tags<TomlTag{.inline_table = true}>(nested)))
};

struct BadNumber {
    double value = std::numeric_limits<double>::infinity();

    NEKO_SERIALIZER(value)
};

template <typename T>
std::string writeToml(const T& value) {
    std::vector<char> buffer;
    TomlSerializer::OutputSerializer out(buffer);
    EXPECT_TRUE(out(value)) << (out.error() == nullptr ? "" : out.error()->msg);
    EXPECT_TRUE(out.end()) << (out.error() == nullptr ? "" : out.error()->msg);
    return {buffer.begin(), buffer.end()};
}

} // namespace

TEST(TomlSerialization, RoundTripsReflectionAndTomlInlineTableTag) {
    const TomlDocument source{.title  = "hello",
                              .count  = 3,
                              .values = {1, 2, 3},
                              .missing = std::nullopt,
                              .nested = {.code = 7, .label = "seven"}};

    const auto output = writeToml(source);
    EXPECT_NE(output.find("title"), std::string::npos) << output;
    EXPECT_NE(output.find("values"), std::string::npos) << output;
    EXPECT_NE(output.find("nested = {"), std::string::npos) << output;
    EXPECT_NE(output.find("wire_code"), std::string::npos) << output;
    EXPECT_EQ(output.find("missing"), std::string::npos) << output;
    NEKO_LOG_INFO("test", "{}", output);
    TomlDocument decoded;
    TomlSerializer::InputSerializer input(output.data(), output.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded.title, source.title);
    EXPECT_EQ(decoded.count, source.count);
    EXPECT_EQ(decoded.values, source.values);
    EXPECT_FALSE(decoded.missing.has_value());
    EXPECT_EQ(decoded.nested.code, source.nested.code);
    EXPECT_EQ(decoded.nested.label, source.nested.label);
}

TEST(TomlSerialization, RejectsUnsupportedRootAndNullValues) {
    {
        std::vector<char> buffer;
        TomlSerializer::OutputSerializer output(buffer);
        ASSERT_FALSE(output(42));
        ASSERT_NE(output.error(), nullptr);
        EXPECT_EQ(output.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType));
    }

    {
        std::vector<char> buffer;
        TomlSerializer::OutputSerializer output(buffer);
        const std::optional<int> empty;
        ASSERT_FALSE(output(empty));
        ASSERT_NE(output.error(), nullptr);
        EXPECT_EQ(output.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType));
    }
}

TEST(TomlSerialization, ParseErrorsAreReportedAsSerializationErrors) {
    TomlDocument decoded;
    TomlSerializer::InputSerializer input("value = [", 9);
    ASSERT_FALSE(input(decoded));
    ASSERT_NE(input.error(), nullptr);
    EXPECT_EQ(input.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
}

TEST(TomlSerialization, WriterErrorsAreReportedAsSerializationErrors) {
    std::vector<char> buffer;
    TomlSerializer::OutputSerializer output(buffer);
    ASSERT_FALSE(output(BadNumber{}));
    ASSERT_NE(output.error(), nullptr);
    EXPECT_EQ(output.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType));
}

#endif

#include "../common/common_main.cpp.in" // IWYU pragma: export
