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

struct TomlFormatNetwork {
    std::string host;
    int port = 0;

    NEKO_SERIALIZER(host, port)
};

struct TomlFormatParams {
    std::string root;
    TomlFormatNetwork network;

    NEKO_SERIALIZER(root, network)
};

struct TomlFormatDocument {
    std::string command;
    TomlFormatParams params;

    NEKO_SERIALIZER(command, params)
};

template <typename T>
std::string writeToml(const T& value) {
    std::vector<char> buffer;
    TomlSerializer::OutputSerializer out(buffer);
    EXPECT_TRUE(out(value)) << (out.error() == nullptr ? "" : out.error()->msg);
    EXPECT_TRUE(out.end()) << (out.error() == nullptr ? "" : out.error()->msg);
    return {buffer.begin(), buffer.end()};
}

template <typename T>
std::string writeToml(const T& value, const TomlOutputFormatOptions& options) {
    std::vector<char> buffer;
    TomlSerializer::OutputSerializer out(buffer, options);
    EXPECT_TRUE(out(value)) << (out.error() == nullptr ? "" : out.error()->msg);
    EXPECT_TRUE(out.end()) << (out.error() == nullptr ? "" : out.error()->msg);
    return {buffer.begin(), buffer.end()};
}

} // namespace

TEST(TomlSerialization, RoundTripsReflectionAndTomlInlineTableTag) {
    const TomlDocument source{.title   = "hello",
                              .count   = 3,
                              .values  = {1, 2, 3},
                              .missing = std::nullopt,
                              .nested  = {.code = 7, .label = "seven"}};

    const auto output = writeToml(source);
    EXPECT_NE(output.find("title"), std::string::npos) << output;
    EXPECT_NE(output.find("values"), std::string::npos) << output;
    EXPECT_NE(output.find("nested={"), std::string::npos) << output;
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

TEST(TomlSerialization, DefaultOutputIsCompactAndPrettyOutputIsOptional) {
    const TomlFormatDocument source{.command = "serve",
                                    .params  = {.root = "this/is/root",
                                                .network = {.host = "a.com", .port = 1234}}};

    const auto compact = writeToml(source);
    EXPECT_NE(compact.find("command='serve'"), std::string::npos) << compact;
    EXPECT_NE(compact.find("[params.network]"), std::string::npos) << compact;
    EXPECT_NE(compact.find("host='a.com'"), std::string::npos) << compact;
    EXPECT_EQ(compact.find("command = 'serve'"), std::string::npos) << compact;
    EXPECT_EQ(compact.find("    [params.network]"), std::string::npos) << compact;
    EXPECT_EQ(compact.find("\n\n"), std::string::npos) << compact;

    const auto pretty = writeToml(source, TomlOutputFormatOptions::Pretty());
    EXPECT_NE(pretty.find("command = 'serve'"), std::string::npos) << pretty;
    EXPECT_NE(pretty.find("    [params.network]"), std::string::npos) << pretty;
    EXPECT_NE(pretty.find("host = 'a.com'"), std::string::npos) << pretty;
    EXPECT_NE(pretty.find("\n\n"), std::string::npos) << pretty;
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

// Add tests for all types
struct IIIType {
    std::string ixx = "iii";
    int i           = 3;
};
struct IItype {
    std::string ixx = "ii";
    int i           = 2;
    IIIType iii;
};
struct InlineTypes {
    int i         = 1;
    float f       = 1.1;
    double d      = 1.2;
    std::string s = "str";
    IItype ii;
};
struct AllTypes {
    int i;
    float f;
    double d;
    std::string s;
    std::vector<int> vi;
    std::vector<std::string> vs;
    std::vector<std::vector<int>> vvi;
    std::vector<std::vector<std::string>> vvs;
    std::optional<int> oi;
    std::optional<std::string> os;
    InlineTypes it;
    std::vector<InlineTypes> itlist;
};

TEST(TomlSerialization, AllTypes) {
    AllTypes all;
    all.i         = 1;
    all.f         = 2.0;
    all.d         = 3.0;
    all.s         = "4";
    all.vi        = {5, 6};
    all.vs        = {"7", "8"};
    all.vvi       = {{9, 10}, {11, 12}};
    all.vvs       = {{"13", "14"}, {"15", "16"}};
    all.oi        = 17;
    all.os        = "18";
    all.it.i      = 19;
    all.it.f      = 20.0;
    all.it.d      = 21.0;
    all.it.s      = "22";
    all.it.ii.i   = 23;
    all.it.ii.ixx = "24";
    all.itlist       = {all.it, all.it};

    std::vector<char> buffer;
    TomlSerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(all));
    output.end();
    ASSERT_EQ(output.error(), nullptr);

    std::string output_str(buffer.begin(), buffer.end());
    NEKO_LOG_INFO("test", "output_str: {}", output_str);

    TomlSerializer::InputSerializer input(buffer.data(), buffer.size());
    AllTypes all2;
    ASSERT_TRUE(input(all2));
    ASSERT_EQ(input.error(), nullptr);
    ASSERT_EQ(all.i, all2.i);
    ASSERT_EQ(all.f, all2.f);
    ASSERT_EQ(all.d, all2.d);
}

#endif

#include "../common/common_main.cpp.in" // IWYU pragma: export
