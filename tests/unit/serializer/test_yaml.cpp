#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nekoproto/serialization/private/tags.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/yaml_serializer.hpp"

NEKO_USE_NAMESPACE

#if !defined(NEKO_PROTO_NO_YAML_SERIALIZER)

namespace {

struct YamlNested {
    int code = 0;
    std::string label;

    NEKO_SERIALIZER((make_tags<rename_tag<"wire_code">>(code)), label)
};

struct Config {
    std::string key;
    std::variant<std::monostate, int, std::string, double> values;
};

struct YamlDocument {
    std::string title;
    std::vector<int> values;
    std::optional<std::string> maybe;
    YamlNested nested;
    std::vector<Config> configs;

    NEKO_SERIALIZER((make_tags<yaml_tag<"!title">, yaml_scalar_style_tag<YamlScalarStyle::DoubleQuoted>>(title)),
                    (make_tags<yaml_collection_style_tag<YamlCollectionStyle::Flow>>(values)), maybe,
                    (make_tags<ParserTag{.flat = true}>(nested)), configs)
};

static_assert(traits::is_scalar_like_v<std::string>);
static_assert(traits::is_scalar_like_v<std::optional<int>>);
static_assert(!traits::is_scalar_like_v<std::vector<int>>);
static_assert(traits::is_collection_like_v<std::vector<int>>);
static_assert(traits::is_collection_like_v<YamlDocument>);
static_assert(!traits::is_collection_like_v<int>);
static_assert(tag_detail::yaml_tag_impl<"tag:example.com,2000:app/title">::yaml_tag ==
              std::string_view{"tag:example.com,2000:app/title"});

template <typename Serializer, typename T>
std::string writeYaml(const T& value) {
    std::vector<char> buffer;
    typename Serializer::OutputSerializer out(buffer);
    EXPECT_TRUE(out(value)) << (out.error() == nullptr ? "" : out.error()->msg);
    EXPECT_TRUE(out.end()) << (out.error() == nullptr ? "" : out.error()->msg);
    return {buffer.begin(), buffer.end()};
}

template <typename T>
std::string writeYaml(const T& value) {
    return writeYaml<YamlSerializer>(value);
}

std::size_t countOccurrences(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    for (std::size_t pos = text.find(needle); pos != std::string_view::npos;
         pos             = text.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

} // namespace

TEST(YamlSerialization, RoundTripsReflectionTagsAndYamlMetadata) {
    const YamlDocument source{.title   = "hello",
                              .values  = {1, 2, 3},
                              .maybe   = std::nullopt,
                              .nested  = {.code = 7, .label = "seven"},
                              .configs = {{.key = "config1", .values = 14},
                                          {.key = "config2", .values = "multi-line\nstring"},
                                          {.key = "config3", .values = 3.14}}};

    const auto output =
        writeYaml(make_tags<yaml_tag<"!config">, yaml_collection_style_tag<YamlCollectionStyle::Block>>(source));
    EXPECT_NE(output.find("!config"), std::string::npos) << output;
    EXPECT_NE(output.find("!title"), std::string::npos) << output;
    EXPECT_NE(output.find("values:"), std::string::npos) << output;
    EXPECT_NE(output.find("values: ["), std::string::npos) << output;
    EXPECT_NE(output.find("wire_code"), std::string::npos);
    EXPECT_EQ(output.find("nested:"), std::string::npos);
    NEKO_LOG_INFO("test", "{}", std::string_view{output});
    YamlDocument decoded;
    YamlSerializer::InputSerializer input(output.data(), output.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded.title, source.title);
    EXPECT_EQ(decoded.values, source.values);
    EXPECT_FALSE(decoded.maybe.has_value());
    EXPECT_EQ(decoded.nested.code, source.nested.code);
    EXPECT_EQ(decoded.nested.label, source.nested.label);
}

TEST(YamlSerialization, ParseErrorsAreReportedAsSerializationErrors) {
    YamlDocument decoded;
    YamlSerializer::InputSerializer input("value: [", 8);
    ASSERT_FALSE(input(decoded));
    ASSERT_NE(input.error(), nullptr);
    EXPECT_EQ(input.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
}

TEST(YamlSerialization, CollectionTagsDoNotPropagateToElements) {
    const std::array<int, 3> values{1, 2, 3};
    const auto output =
        writeYaml(make_tags<yaml_tag<"!numbers">, yaml_collection_style_tag<YamlCollectionStyle::Flow>>(values));

    EXPECT_EQ(countOccurrences(output, "!numbers"), 1U) << output;
    EXPECT_NE(output.find("["), std::string::npos) << output;
}

TEST(YamlSerialization, WriterErrorsAreReportedAsSerializationErrors) {
    std::vector<char> buffer;
    YamlSerializer::OutputSerializer output(buffer);
    ASSERT_FALSE(output(std::numeric_limits<double>::infinity()));
    ASSERT_NE(output.error(), nullptr);
    EXPECT_EQ(output.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType));
}

#if defined(NEKO_PROTO_ENABLE_YAMLCPP)
TEST(YamlSerialization, YamlCppBackendRoundTripsObjects) {
    const YamlDocument source{.title   = "yaml-cpp",
                              .values  = {4, 5, 6},
                              .maybe   = "present",
                              .nested  = {.code = 9, .label = "nine"},
                              .configs = {{.key = "config", .values = 42}}};

    const auto output = writeYaml<YamlCppSerializer>(
        make_tags<yaml_tag<"!config">, yaml_collection_style_tag<YamlCollectionStyle::Block>>(source));
    EXPECT_NE(output.find("!config"), std::string::npos) << output;
    EXPECT_NE(output.find("values: ["), std::string::npos) << output;

    YamlDocument decoded;
    YamlCppSerializer::InputSerializer input(output.data(), output.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded.title, source.title);
    EXPECT_EQ(decoded.values, source.values);
    ASSERT_TRUE(decoded.maybe.has_value());
    EXPECT_EQ(*decoded.maybe, *source.maybe);
    EXPECT_EQ(decoded.nested.code, source.nested.code);
    EXPECT_EQ(decoded.nested.label, source.nested.label);
}
#endif

#endif

#include "../common/common_main.cpp.in" // IWYU pragma: export
