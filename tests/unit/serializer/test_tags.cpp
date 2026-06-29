#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_USE_NAMESPACE

struct TypeLevelFlatTagInner {
    int code = 0;
    std::string label;

    NEKO_SERIALIZER(make_tags<rename_tag<"wire_code">>(code), label)
};

struct TypeLevelFlatTagOwner {
    int before = 0;
    TypeLevelFlatTagInner inner;
    int after = 0;

    NEKO_SERIALIZER(before, inner, after)
};

struct TypeLevelUnframedHeader {
    std::uint16_t version = 0;
    std::uint16_t type = 0;

    NEKO_SERIALIZER(version, type)
};

struct TypeLevelUnframedEnvelope {
    TypeLevelUnframedHeader header;
    std::uint16_t tail = 0;

    NEKO_SERIALIZER(header, tail)
};

inline constexpr auto RawStringTag = JsonTags{.rawString = true};
inline constexpr auto RenamedSkipableTag =
    comment_tag<"doc keeps parser depth", rename_tag<"wire(alias)", JsonTags{.skipable = true}>>;
inline constexpr auto SkipableTag = JsonTags{.skipable = true};
inline constexpr auto FlattenedDocsTag = comment_tag<"flattened docs", JsonTags{.flat = true}>;

struct MacroParsedTaggedObject {
    std::string raw;
    int renamed = 0;
    std::optional<int> optional;
    TypeLevelFlatTagInner nested;

    NEKO_SERIALIZER(make_tags<RawStringTag>(raw), make_tags<RenamedSkipableTag>(renamed),
                    make_tags<SkipableTag>(optional), make_tags<FlattenedDocsTag>(nested))
};

struct SchemaTaggedObject {
    int required = 0;
    int renamed = 0;
    int retained = 0;
    std::optional<int> optional;
    TypeLevelFlatTagInner flat;
    std::uint32_t fixed = 0;

    struct Neko {
        static constexpr auto value =
            Object("required", &SchemaTaggedObject::required, "renamed",
                   make_tags<rename_tag<"wire_required">>(&SchemaTaggedObject::renamed), "retained",
                   make_tags<JsonTags{.skipable = true}>(&SchemaTaggedObject::retained), "optional",
                   &SchemaTaggedObject::optional, "flat",
                   make_tags<JsonTags{.flat = true}>(&SchemaTaggedObject::flat), "fixed",
                   make_tags<BinaryTags{.fixedLength = true}>(&SchemaTaggedObject::fixed)); // NOLINT
    };
};

NEKO_BEGIN_NAMESPACE
template <>
struct is_flat_tag<TypeLevelFlatTagInner> : std::true_type {};

template <>
struct is_unframed_tag<TypeLevelUnframedHeader> : std::true_type {};
NEKO_END_NAMESPACE

namespace {

template <typename T>
std::string writeJson(const T& value) {
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer out(buffer);
    EXPECT_TRUE(out(value));
    EXPECT_TRUE(out.end());
    return {buffer.begin(), buffer.end()};
}

template <typename T>
bool readJson(std::string_view json, T& value) {
    JsonSerializer::InputSerializer in(json.data(), json.size());
    const bool parsed = in(value);
    return parsed && static_cast<bool>(in);
}

template <typename T>
parsing::schema::Type::Object objectSchema() {
    const auto schema = detail::parser_schema<typename JsonSerializer::Reader, typename JsonSerializer::Writer, T>();
    return std::get<parsing::schema::Type::Object>(schema.value);
}

bool contains(const std::vector<std::string>& values, std::string_view expected) {
    return std::find(values.begin(), values.end(), expected) != values.end();
}

} // namespace

TEST(SerializationTags, AccessorsResolveDirectAndNestedTagFlags) {
    constexpr auto Tags =
        comment_tag<"outer comment", rename_tag<"wire_name", JsonTags{.flat = true, .skipable = true}>>;

    static_assert(tag_query::has_comment(Tags));
    static_assert(tag_query::comment(Tags) == "outer comment");
    static_assert(tag_query::has_name(Tags));
    static_assert(tag_query::name(Tags) == "wire_name");
    static_assert(tag_query::is_flat<TypeLevelFlatTagInner>(Tags));
    static_assert(tag_query::is_skipable(Tags));
}

TEST(SerializationTags, TypeLevelTagsAreVisibleThroughNoTags) {
    static_assert(tag_query::is_flat<TypeLevelFlatTagInner>(NoTags{}));
    static_assert(!tag_query::is_flat<int>(NoTags{}));
    static_assert(tag_query::is_unframed<TypeLevelUnframedHeader>(NoTags{}));
    static_assert(!tag_query::is_unframed<std::uint16_t>(NoTags{}));
    static_assert(tag_query::fixed_length<std::uint32_t>(BinaryTags{.fixedLength = true}) == sizeof(std::uint32_t));
    static_assert(tag_query::fixed_length<std::uint32_t>(BinaryTags{.fixedLength = 2}) == 2U);
}

TEST(SerializationTags, MakeTagsExposeAccessorAndWrappedTag) {
    auto spec = make_tags<rename_tag<"wire_code", JsonTags{.skipable = true}>>(&TypeLevelFlatTagInner::code);

    static_assert(is_field_spec_v<decltype(spec)>);
    static_assert(std::is_same_v<detail::resolve_without_context_t<decltype(&TypeLevelFlatTagInner::code)>, int>);
    static_assert(std::is_same_v<resolve_member_type_t<field_accessor_t<decltype(spec)>, TypeLevelFlatTagInner>, int>);
    static_assert(tag_query::name(field_tags_v<decltype(spec)>) == "wire_code");
    static_assert(tag_query::is_skipable(field_tags_v<decltype(spec)>));
}

TEST(SerializationTags, SerializerMacroStripsMakeTagsWhenBuildingReflectionNames) {
    constexpr auto names = Reflect<MacroParsedTaggedObject>::names();
    static_assert(names.size() == 4);
    static_assert(names[0] == "raw");
    static_assert(names[1] == "renamed");
    static_assert(names[2] == "optional");
    static_assert(names[3] == "nested");

    constexpr auto rawTags = std::get<0>(Reflect<MacroParsedTaggedObject>::field_tags);
    constexpr auto renamedTags = std::get<1>(Reflect<MacroParsedTaggedObject>::field_tags);
    constexpr auto optionalTags = std::get<2>(Reflect<MacroParsedTaggedObject>::field_tags);
    constexpr auto nestedTags = std::get<3>(Reflect<MacroParsedTaggedObject>::field_tags);

    static_assert(tag_query::is_raw_string(rawTags));
    static_assert(tag_query::comment(renamedTags) == "doc keeps parser depth");
    static_assert(tag_query::name(renamedTags) == "wire(alias)");
    static_assert(tag_query::is_skipable(renamedTags));
    static_assert(tag_query::is_skipable(optionalTags));
    static_assert(tag_query::comment(nestedTags) == "flattened docs");
    static_assert(tag_query::is_flat<TypeLevelFlatTagInner>(nestedTags));
}

TEST(SerializationTagIntegration, JsonParserAppliesRawRenameSkipableAndFlatTags) {
    const MacroParsedTaggedObject source{
        .raw = R"({"enabled":true})",
        .renamed = 11,
        .optional = std::nullopt,
        .nested = {.code = 7, .label = "seven"},
    };

    EXPECT_EQ(writeJson(source),
              R"json({"raw":{"enabled":true},"wire(alias)":11,"wire_code":7,"label":"seven"})json");

    MacroParsedTaggedObject decoded;
    decoded.renamed = 99;
    decoded.optional = 42;
    ASSERT_TRUE(readJson(R"({"raw":{"enabled":false},"wire_code":8,"label":"eight"})", decoded));
    EXPECT_EQ(decoded.raw, R"({"enabled":false})");
    EXPECT_EQ(decoded.renamed, 99);
    ASSERT_TRUE(decoded.optional.has_value());
    EXPECT_EQ(decoded.optional.value(), 42);
    EXPECT_EQ(decoded.nested.code, 8);
    EXPECT_EQ(decoded.nested.label, "eight");
}

TEST(SerializationTagIntegration, TypeLevelFlatTagFlattensJsonObjectsAndSchema) {
    const TypeLevelFlatTagOwner source{
        .before = 1,
        .inner = {.code = 2, .label = "two"},
        .after = 3,
    };

    EXPECT_EQ(writeJson(source), R"({"before":1,"wire_code":2,"label":"two","after":3})");

    TypeLevelFlatTagOwner decoded;
    ASSERT_TRUE(readJson(R"({"before":4,"wire_code":5,"label":"five","after":6})", decoded));
    EXPECT_EQ(decoded.before, 4);
    EXPECT_EQ(decoded.inner.code, 5);
    EXPECT_EQ(decoded.inner.label, "five");
    EXPECT_EQ(decoded.after, 6);

    const auto object = objectSchema<TypeLevelFlatTagOwner>();
    EXPECT_TRUE(object.properties.contains("before"));
    EXPECT_TRUE(object.properties.contains("wire_code"));
    EXPECT_TRUE(object.properties.contains("label"));
    EXPECT_TRUE(object.properties.contains("after"));
    EXPECT_FALSE(object.properties.contains("inner"));
    EXPECT_TRUE(contains(object.required, "wire_code"));
    EXPECT_TRUE(contains(object.required, "label"));
}

TEST(SerializationTagIntegration, SchemaUsesFlatSkipableOptionalAndFixedLengthTags) {
    const auto object = objectSchema<SchemaTaggedObject>();

    EXPECT_TRUE(object.properties.contains("required"));
    EXPECT_TRUE(object.properties.contains("wire_required"));
    EXPECT_TRUE(object.properties.contains("retained"));
    EXPECT_TRUE(object.properties.contains("optional"));
    EXPECT_TRUE(object.properties.contains("wire_code"));
    EXPECT_TRUE(object.properties.contains("label"));
    EXPECT_TRUE(object.properties.contains("fixed"));
    EXPECT_FALSE(object.properties.contains("renamed"));
    EXPECT_FALSE(object.properties.contains("flat"));

    EXPECT_TRUE(contains(object.required, "required"));
    EXPECT_TRUE(contains(object.required, "wire_required"));
    EXPECT_TRUE(contains(object.required, "wire_code"));
    EXPECT_TRUE(contains(object.required, "label"));
    EXPECT_TRUE(contains(object.required, "fixed"));
    EXPECT_FALSE(contains(object.required, "retained"));
    EXPECT_FALSE(contains(object.required, "optional"));

    const auto& fixed = object.properties.at("fixed");
    ASSERT_TRUE(fixed.fixedLength.has_value());
    EXPECT_EQ(fixed.fixedLength.value(), sizeof(std::uint32_t));
}

TEST(SerializationTagIntegration, TypeLevelUnframedTagRoundTripsBinaryLayout) {
    const TypeLevelUnframedEnvelope source{
        .header = {.version = 1, .type = 2},
        .tail = 3,
    };

    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    TypeLevelUnframedEnvelope decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded.header.version, source.header.version);
    EXPECT_EQ(decoded.header.type, source.header.type);
    EXPECT_EQ(decoded.tail, source.tail);
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
