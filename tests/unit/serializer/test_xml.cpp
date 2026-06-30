#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "nekoproto/serialization/parsing/supports_comments.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

#ifdef NEKO_PROTO_ENABLE_PUGIXML
#include <pugixml.hpp>

#include "nekoproto/serialization/xml_serializer.hpp"

NEKO_USE_NAMESPACE

namespace {

struct XmlChild {
    std::string name;
    int age = 0;

    NEKO_SERIALIZER(name, age)

    bool operator==(const XmlChild&) const = default;
};

struct XmlDocument {
    int id = 0;
    std::string title;
    std::vector<int> values;
    std::vector<XmlChild> children;
    std::vector<std::vector<int>> matrix;
    std::vector<int> empty;
    std::optional<std::string> note;
    std::unique_ptr<int> nullable;

    NEKO_SERIALIZER(id, title, values, children, matrix, empty, note, nullable)
};

struct XmlAttributes {
    int id = 0;
    std::string name;

    NEKO_SERIALIZER(id, name)
};

struct XmlText {
    std::string xml_content;
    int id = 0;

    NEKO_SERIALIZER(xml_content, id)
};

struct XmlCommentedDocument {
    std::vector<int> values;
    std::optional<int> maybe;
    std::vector<int> empty;

    struct Neko {
        constexpr static auto value =
            Object("values", make_tags<comment_tag<"values comment">>(&XmlCommentedDocument::values), "maybe",
                   make_tags<comment_tag<"optional comment">>(&XmlCommentedDocument::maybe), "empty",
                   make_tags<comment_tag<"empty comment">>(&XmlCommentedDocument::empty)); // NOLINT
    };
};

struct XmlPositionalComments {
    int first  = 0;
    int second = 0;

    struct Neko {
        constexpr static auto value =
            Array(make_tags<comment_tag<"first element">>(&XmlPositionalComments::first),
                  make_tags<comment_tag<"second element">>(&XmlPositionalComments::second)); // NOLINT
    };
};

std::string asString(const std::vector<char>& buffer) { return {buffer.begin(), buffer.end()}; }

std::size_t countOccurrences(std::string_view input, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos   = 0;
    while ((pos = input.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

TEST(PugiXmlBackend, RoundTripsObjectsArraysEmptyContainersAndNull) {
    XmlDocument source;
    source.id       = 7;
    source.title    = "XML <roundtrip>";
    source.values   = {1, 2, 3};
    source.children = {{"Alice", 8}, {"Bob", 10}};
    source.matrix   = {{1, 2}, {}, {3, 4}};

    std::vector<char> buffer;
    PugiXmlOutputSerializer out(buffer, "document");
    ASSERT_TRUE(out(source));
    ASSERT_TRUE(out.end());

    pugi::xml_document document;
    ASSERT_TRUE(document.load_buffer(buffer.data(), buffer.size()));
    const auto root = document.child("document");
    ASSERT_TRUE(root);
    EXPECT_EQ(std::distance(root.children("values").begin(), root.children("values").end()), 3);
    EXPECT_EQ(std::distance(root.children("children").begin(), root.children("children").end()), 2);
    EXPECT_STREQ(root.child("empty").attribute("neko-array").value(), "empty");
    EXPECT_STREQ(root.child("nullable").attribute("nil").value(), "true");

    XmlDocument decoded;
    decoded.note     = "old";
    decoded.nullable = std::make_unique<int>(42);
    PugiXmlInputSerializer in(buffer.data(), buffer.size());
    ASSERT_TRUE(in(decoded)) << (in.error() == nullptr ? "" : in.error()->msg);
    EXPECT_EQ(decoded.id, source.id);
    EXPECT_EQ(decoded.title, source.title);
    EXPECT_EQ(decoded.values, source.values);
    EXPECT_EQ(decoded.children, source.children);
    EXPECT_EQ(decoded.matrix, source.matrix);
    EXPECT_TRUE(decoded.empty.empty());
    EXPECT_FALSE(decoded.note.has_value());
    EXPECT_FALSE(decoded.nullable);
}

TEST(PugiXmlBackend, ReadsObjectFieldsFromAttributes) {
    const std::string input = R"(<person id="7"><name>Alice</name></person>)";
    XmlAttributes decoded;
    PugiXmlInputSerializer in(input.data(), input.size());

    ASSERT_TRUE(in(decoded)) << (in.error() == nullptr ? "" : in.error()->msg);
    EXPECT_EQ(decoded.id, 7);
    EXPECT_EQ(decoded.name, "Alice");
}

TEST(PugiXmlBackend, MapsXmlContentFieldToNodeText) {
    const XmlText source{.xml_content = "hello", .id = 9};
    std::vector<char> buffer;
    PugiXmlOutputSerializer out(buffer, "message");

    ASSERT_TRUE(out(source));
    ASSERT_TRUE(out.end());
    EXPECT_NE(asString(buffer).find("<message>hello"), std::string::npos);

    XmlText decoded;
    PugiXmlInputSerializer in(buffer.data(), buffer.size());
    ASSERT_TRUE(in(decoded)) << (in.error() == nullptr ? "" : in.error()->msg);
    EXPECT_EQ(decoded.xml_content, source.xml_content);
    EXPECT_EQ(decoded.id, source.id);
}

TEST(PugiXmlBackend, WritesCommentTagsForObjectsAndArrays) {
    static_assert(parsing::supports_comments<xml::Writer>);
    static_assert(tag_query::get<tag_prop::comment>(TagList<comment_tag<"values comment">>{}) ==
                  std::string_view{"values comment"});
    static_assert(
        tag_query::get<tag_prop::skipable>(TagList<comment_tag<"skip comment">, JsonTags{.skipable = true}>{}));
    static_assert(tag_query::has<tag_prop::name>(TagList<rename_tag<"value">, comment_tag<"inner comment">>{}));
    static_assert(tag_query::has<tag_prop::comment>(TagList<rename_tag<"value">, comment_tag<"inner comment">>{}));

    XmlCommentedDocument source;
    source.values = {1, 2};
    source.maybe  = 7;

    std::vector<char> buffer;
    PugiXmlOutputSerializer out(buffer, "document");
    ASSERT_TRUE(out(source));
    ASSERT_TRUE(out.end());

    const auto output = asString(buffer);
    EXPECT_EQ(countOccurrences(output, "<!--values comment-->"), 1U);
    EXPECT_EQ(countOccurrences(output, "<!--optional comment-->"), 1U);
    EXPECT_EQ(countOccurrences(output, "<!--empty comment-->"), 1U);

    pugi::xml_document document;
    ASSERT_TRUE(document.load_buffer(output.data(), output.size(), pugi::parse_default | pugi::parse_comments));
    const auto root = document.child("document");
    ASSERT_TRUE(root);

    bool foundValuesComment = false;
    bool foundEmptyMarker   = false;
    for (auto child = root.first_child(); child; child = child.next_sibling()) {
        if (child.type() == pugi::node_comment && std::string_view{child.value()} == "values comment") {
            foundValuesComment = true;
        }
        if (child.type() == pugi::node_element && std::string_view{child.name()} == "empty" &&
            std::string_view{child.attribute("neko-array").value()} == "empty") {
            foundEmptyMarker = true;
        }
    }
    EXPECT_TRUE(foundValuesComment);
    EXPECT_TRUE(foundEmptyMarker);

    XmlCommentedDocument decoded;
    PugiXmlInputSerializer in(output.data(), output.size());
    ASSERT_TRUE(in(decoded)) << (in.error() == nullptr ? "" : in.error()->msg);
    EXPECT_EQ(decoded.values, source.values);
    ASSERT_TRUE(decoded.maybe.has_value());
    EXPECT_EQ(decoded.maybe.value(), source.maybe.value());
    EXPECT_TRUE(decoded.empty.empty());

    const XmlPositionalComments positional{.first = 3, .second = 4};
    std::vector<char> arrayBuffer;
    PugiXmlOutputSerializer arrayOut(arrayBuffer, "array");
    ASSERT_TRUE(arrayOut(positional));
    ASSERT_TRUE(arrayOut.end());

    const auto arrayOutput = asString(arrayBuffer);
    EXPECT_EQ(countOccurrences(arrayOutput, "<!--first element-->"), 1U);
    EXPECT_EQ(countOccurrences(arrayOutput, "<!--second element-->"), 1U);

    XmlPositionalComments positionalDecoded;
    PugiXmlInputSerializer arrayIn(arrayOutput.data(), arrayOutput.size());
    ASSERT_TRUE(arrayIn(positionalDecoded)) << (arrayIn.error() == nullptr ? "" : arrayIn.error()->msg);
    EXPECT_EQ(positionalDecoded.first, positional.first);
    EXPECT_EQ(positionalDecoded.second, positional.second);
}

struct Source {
    int source = 5;
    NEKO_SERIALIZER((make_tags<rename_tag<"value">, comment_tag<"inner comment">>(source)));
};

struct OuterSource { // same as Source, tag order will not effect the output
    int source = 5;
    NEKO_SERIALIZER((make_tags<comment_tag<"outer comment">, rename_tag<"value">>(source)));
};

TEST(PugiXmlBackend, WriteModifierTagsCanBeLayered) {
    Source tagged;

    std::vector<char> buffer;
    PugiXmlOutputSerializer out(buffer, "root");
    ASSERT_TRUE(out(tagged));
    ASSERT_TRUE(out.end());

    const auto output = asString(buffer);
    NEKO_LOG_INFO("test", "xml: {}", output);
    EXPECT_EQ(countOccurrences(output, "<!--inner comment-->"), 1U);
    EXPECT_NE(output.find("<value>5</value>"), std::string::npos);

    pugi::xml_document document;
    ASSERT_TRUE(document.load_buffer(output.data(), output.size(), pugi::parse_default | pugi::parse_comments));
    const auto root = document.child("root");
    ASSERT_TRUE(root);
    ASSERT_TRUE(root.child("value"));
    EXPECT_STREQ(root.child("value").child_value(), "5");
    ASSERT_EQ(root.child("value").next_sibling().type(), pugi::node_comment);
    EXPECT_STREQ(root.child("value").next_sibling().value(), "inner comment");

    Source target;
    PugiXmlInputSerializer in(output.data(), output.size());
    ASSERT_TRUE(in(target)) << (in.error() == nullptr ? "" : in.error()->msg);
    EXPECT_EQ(target.source, tagged.source);

    OuterSource outerComment;
    std::vector<char> outerBuffer;
    PugiXmlOutputSerializer outerOut(outerBuffer, "root");
    ASSERT_TRUE(outerOut(outerComment));
    ASSERT_TRUE(outerOut.end());

    pugi::xml_document outerDocument;
    const auto outerOutput = asString(outerBuffer);
    NEKO_LOG_INFO("test", "outerOutput: {}", outerOutput);
    ASSERT_TRUE(
        outerDocument.load_buffer(outerOutput.data(), outerOutput.size(), pugi::parse_default | pugi::parse_comments));
    const auto outerRoot = outerDocument.child("root");
    ASSERT_TRUE(outerRoot.child("value"));
    EXPECT_STREQ(outerRoot.child("value").child_value(), "5");
    ASSERT_EQ(outerRoot.child("value").next_sibling().type(), pugi::node_comment);
    EXPECT_STREQ(outerRoot.child("value").next_sibling().value(), "outer comment");
}

TEST(PugiXmlBackend, ReportsParseAndFieldErrors) {
    {
        const std::string malformed = "<document>";
        PugiXmlInputSerializer in(malformed.data(), malformed.size());
        EXPECT_FALSE(in);
        ASSERT_NE(in.error(), nullptr);
        EXPECT_EQ(in.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
    }
    {
        const std::string missing = R"(<person><name>Alice</name></person>)";
        XmlAttributes decoded;
        PugiXmlInputSerializer in(missing.data(), missing.size());
        EXPECT_FALSE(in(decoded));
        ASSERT_NE(in.error(), nullptr);
        EXPECT_EQ(in.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidField));
        EXPECT_NE(in.error()->msg.find("Required field 'id' is missing"), std::string::npos);
    }
}

#endif

#include "../common/common_main.cpp.in" // IWYU pragma: export
