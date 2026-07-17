#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
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

    NEKO_SERIALIZER(
        (make_tags<comment_tag<"doc keeps parser depth">, rename_tag<"wire_code">, ParserTag{.flat = true}>(code)),
        label)
};

struct TypeLevelFlatTagOwner {
    int before = 0;
    TypeLevelFlatTagInner inner;
    int after = 0;

    NEKO_SERIALIZER(before, inner, after)
};

struct TypeLevelUnframedHeader {
    std::uint16_t version = 0;
    std::uint16_t type    = 0;

    NEKO_SERIALIZER(version, type)
};

struct TypeLevelUnframedEnvelope {
    TypeLevelUnframedHeader header;
    std::uint16_t tail = 0;

    NEKO_SERIALIZER(header, tail)
};

inline constexpr auto RawStringTag     = JsonTag{.raw_string = true};
inline constexpr auto SkippableTag      = ParserTag{.skippable = true};
inline constexpr auto FlattenedDocsTag = TagList<comment_tag<"flattened docs">, ParserTag{.flat = true}>{};

struct MacroParsedTaggedObject {
    std::string raw;
    int renamed = 0;
    std::optional<int> optional;
    TypeLevelFlatTagInner nested;
};

// clang-format off
template <>
struct NEKO_NAMESPACE::Meta<MacroParsedTaggedObject> {
    static constexpr auto value = Object(
        "raw", make_tags<RawStringTag>(&MacroParsedTaggedObject::raw),
        "renamed", make_tags<comment_tag<"doc keeps parser depth">, rename_tag<"wire(alias)">, JsonTag{.skippable = true}>(&MacroParsedTaggedObject::renamed),
        "optional", make_tags<SkippableTag>(&MacroParsedTaggedObject::optional),
        "nested", make_tags<FlattenedDocsTag>(&MacroParsedTaggedObject::nested)
    );
};
// clang-format on

struct SchemaTaggedObject {
    int required = 0;
    int renamed  = 0;
    int retained = 0;
    std::optional<int> optional;
    TypeLevelFlatTagInner flat;
    std::uint32_t fixed = 0;

    struct Neko {
        static constexpr auto value =
            Object("required", &SchemaTaggedObject::required, "renamed",
                   make_tags<rename_tag<"wire_required">>(&SchemaTaggedObject::renamed), "retained",
                   make_tags<ParserTag{.skippable = true}>(&SchemaTaggedObject::retained), "optional",
                   &SchemaTaggedObject::optional, "flat", make_tags<ParserTag{.flat = true}>(&SchemaTaggedObject::flat),
                   "fixed", make_tags<BinaryTag{.fixed_length = true}>(&SchemaTaggedObject::fixed)); // NOLINT
    };
};

struct SerializationIgnoredObject {
    int id        = 0;
    int transient = 0;
    int value     = 0;

    struct Neko {
        static constexpr auto value =
            Object("id", &SerializationIgnoredObject::id, "transient",
                   make_tags<serialization_ignore_tag>(&SerializationIgnoredObject::transient), "value",
                   &SerializationIgnoredObject::value); // NOLINT
    };
};

struct SerializationIgnoredArray {
    int first     = 0;
    int transient = 0;
    int last      = 0;

    struct Neko {
        static constexpr auto value =
            Array(&SerializationIgnoredArray::first,
                  make_tags<serialization_ignore_tag>(&SerializationIgnoredArray::transient),
                  &SerializationIgnoredArray::last); // NOLINT
    };
};

struct ReaderProbeTag {
    int marker = 0;

    template <typename T, auto /*Tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }

    constexpr bool operator==(const ReaderProbeTag&) const = default;
};

inline constexpr auto ReaderNodeTag = ReaderProbeTag{.marker = 7};

struct ReaderProbeObject {
    int value = 0;

    NEKO_SERIALIZER((make_tags<ReaderNodeTag>(value)))
};

NEKO_BEGIN_NAMESPACE
template <>
struct is_flat_tag<TypeLevelFlatTagInner> : std::true_type {};

template <>
struct is_unframed_tag<TypeLevelUnframedHeader> : std::true_type {};
NEKO_END_NAMESPACE

namespace {

struct ReaderProbeNode {
    enum class Kind : std::uint8_t {
        Null,
        Integer,
        Array,
        Object,
    };

    Kind kind = Kind::Null;
    int integer = 0;
    std::vector<ReaderProbeNode> elements;
    std::vector<std::string> fieldNames;
    std::vector<ReaderProbeNode> fieldValues;

    static ReaderProbeNode integerValue(int value) {
        ReaderProbeNode node;
        node.kind    = Kind::Integer;
        node.integer = value;
        return node;
    }

    static ReaderProbeNode array(std::initializer_list<ReaderProbeNode> values) {
        ReaderProbeNode node;
        node.kind     = Kind::Array;
        node.elements = values;
        return node;
    }

    static ReaderProbeNode object(std::initializer_list<std::pair<std::string, ReaderProbeNode>> values);
};

ReaderProbeNode ReaderProbeNode::object(std::initializer_list<std::pair<std::string, ReaderProbeNode>> values) {
    ReaderProbeNode node;
    node.kind = Kind::Object;
    node.fieldNames.reserve(values.size());
    node.fieldValues.reserve(values.size());
    for (const auto& [name, value] : values) {
        node.fieldNames.push_back(name);
        node.fieldValues.push_back(value);
    }
    return node;
}

struct TagAwareProbeReader {
    using InputValueType = const ReaderProbeNode*;
    using InputArrayType = const ReaderProbeNode*;
    using InputObjectType = const ReaderProbeNode*;

    static inline int taggedArrayCalls = 0;
    static inline int legacyArrayCalls = 0;
    static inline int arrayCallsWithProbe = 0;
    static inline int taggedObjectCalls = 0;
    static inline int legacyObjectCalls = 0;
    static inline int objectCallsWithProbe = 0;
    static inline int taggedFieldCalls = 0;
    static inline int legacyFieldCalls = 0;
    static inline int fieldCallsWithProbe = 0;
    static inline int taggedEmptyCalls = 0;
    static inline int legacyEmptyCalls = 0;
    static inline int emptyCallsWithProbe = 0;
    static inline int taggedBasicCalls = 0;
    static inline int legacyBasicCalls = 0;
    static inline int basicCallsWithProbe = 0;

    static void reset() {
        taggedArrayCalls = 0;
        legacyArrayCalls = 0;
        arrayCallsWithProbe = 0;
        taggedObjectCalls = 0;
        legacyObjectCalls = 0;
        objectCallsWithProbe = 0;
        taggedFieldCalls = 0;
        legacyFieldCalls = 0;
        fieldCallsWithProbe = 0;
        taggedEmptyCalls = 0;
        legacyEmptyCalls = 0;
        emptyCallsWithProbe = 0;
        taggedBasicCalls = 0;
        legacyBasicCalls = 0;
        basicCallsWithProbe = 0;
    }

    template <typename Tags>
    static sa::Result<InputArrayType> toArray(InputValueType input, const Tags& tags) {
        ++taggedArrayCalls;
        if (tag_query::has_tag<ReaderProbeTag>(tags)) {
            ++arrayCallsWithProbe;
        }
        return toArrayImpl(input);
    }

    static sa::Result<InputArrayType> toArray(InputValueType input) {
        ++legacyArrayCalls;
        return toArrayImpl(input);
    }

    static std::size_t arraySize(const InputArrayType& array) { return array->elements.size(); }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) {
        return &array->elements[index];
    }

    template <typename Tags>
    static sa::Result<InputObjectType> toObject(InputValueType input, const Tags& tags) {
        ++taggedObjectCalls;
        if (tag_query::has_tag<ReaderProbeTag>(tags)) {
            ++objectCallsWithProbe;
        }
        return toObjectImpl(input);
    }

    static sa::Result<InputObjectType> toObject(InputValueType input) {
        ++legacyObjectCalls;
        return toObjectImpl(input);
    }

    template <typename Tags>
    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name,
                                                  const Tags& tags) {
        ++taggedFieldCalls;
        if (tag_query::has_tag<ReaderProbeTag>(tags)) {
            ++fieldCallsWithProbe;
        }
        return objectFieldImpl(object, name);
    }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) {
        ++legacyFieldCalls;
        return objectFieldImpl(object, name);
    }

    template <typename Tags>
    static bool isEmpty(InputValueType input, const Tags& tags) {
        ++taggedEmptyCalls;
        if (tag_query::has_tag<ReaderProbeTag>(tags)) {
            ++emptyCallsWithProbe;
        }
        return input->kind == ReaderProbeNode::Kind::Null;
    }

    static bool isEmpty(InputValueType input) {
        ++legacyEmptyCalls;
        return input->kind == ReaderProbeNode::Kind::Null;
    }

    template <typename T, typename Tags>
    static sa::Result<T> toBasicType(InputValueType input, const Tags& tags) {
        ++taggedBasicCalls;
        if (tag_query::has_tag<ReaderProbeTag>(tags)) {
            ++basicCallsWithProbe;
        }
        return toBasicTypeImpl<T>(input);
    }

    template <typename T>
    static sa::Result<T> toBasicType(InputValueType input) {
        ++legacyBasicCalls;
        return toBasicTypeImpl<T>(input);
    }

private:
    static sa::Result<InputArrayType> toArrayImpl(InputValueType input) {
        if (input->kind != ReaderProbeNode::Kind::Array) {
            return sa::Err(sa::ErrorCode::InvalidType, "probe input is not an array");
        }
        return input;
    }

    static sa::Result<InputObjectType> toObjectImpl(InputValueType input) {
        if (input->kind != ReaderProbeNode::Kind::Object) {
            return sa::Err(sa::ErrorCode::InvalidType, "probe input is not an object");
        }
        return input;
    }

    static sa::Result<InputValueType> objectFieldImpl(const InputObjectType& object, std::string_view name) {
        for (std::size_t i = 0; i < object->fieldNames.size(); ++i) {
            if (object->fieldNames[i] == name) {
                return &object->fieldValues[i];
            }
        }
        return sa::Err(sa::ErrorCode::InvalidField, "probe field is missing");
    }

    template <typename T>
    static sa::Result<T> toBasicTypeImpl(InputValueType input) {
        static_assert(std::is_integral_v<T>, "probe reader only implements integral scalar conversion");
        if (input->kind != ReaderProbeNode::Kind::Integer) {
            return sa::Err(sa::ErrorCode::InvalidType, "probe input is not an integer");
        }
        return static_cast<T>(input->integer);
    }
};

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
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
#endif

template <typename T>
parsing::schema::Type::Object objectSchema() {
    const auto schema = parser_schema<T>();
    return std::get<parsing::schema::Type::Object>(schema.value);
}

bool contains(const std::vector<std::string>& values, std::string_view expected) {
    return std::find(values.begin(), values.end(), expected) != values.end();
}

} // namespace

TEST(SerializationTags, AccessorsResolveDirectAndNestedTagFlags) {
    constexpr auto Tags =
        TagList<comment_tag<"outer comment">, rename_tag<"wire_name">, ParserTag{.flat = true, .skippable = true}>{};

    static_assert(tag_query::has<tag_property::comment>(Tags));
    static_assert(tag_query::get<tag_property::comment>(Tags) == "outer comment");
    static_assert(tag_query::has<tag_property::name>(Tags));
    static_assert(tag_query::get<tag_property::name>(Tags) == "wire_name");
    static_assert(tag_query::get<tag_property::flat<TypeLevelFlatTagInner>>(Tags));
    static_assert(tag_query::get<tag_property::skippable>(Tags));
    static_assert(tag_query::get<tag_property::skippable>(JsonTag{.skippable = true}));
}

TEST(SerializationTags, TypeLevelTagsAreVisibleThroughNoTags) {
    static_assert(tag_query::get<tag_property::flat<TypeLevelFlatTagInner>>(NoTags{}));
    static_assert(!tag_query::get<tag_property::flat<int>>(NoTags{}));
    static_assert(tag_query::get<tag_property::unframed<TypeLevelUnframedHeader>>(NoTags{}));
    static_assert(!tag_query::get<tag_property::unframed<std::uint16_t>>(NoTags{}));
    static_assert(tag_query::get<tag_property::fixed_length<std::uint32_t>>(BinaryTag{.fixed_length = true}) ==
                  sizeof(std::uint32_t));
    static_assert(tag_query::get<tag_property::fixed_length<std::uint32_t>>(BinaryTag{.fixed_length = 2}) == 2U);
    static_assert(tag_query::get<tag_property::union_encoding>(NoTags{}) == UnionEncoding::TaggedArray);
    static_assert(tag_query::get<tag_property::union_encoding>(
                      UnionTag{.encoding = UnionEncoding::Untagged}) == UnionEncoding::Untagged);
}

TEST(SerializationTags, VariantSchemaFollowsUnionEncoding) {
    using Variant = std::variant<int, std::string>;

    const auto taggedSchema = parser_schema<Variant>();
    const auto* array = std::get_if<parsing::schema::Type::Array>(&taggedSchema.value);
    ASSERT_NE(array, nullptr);
    ASSERT_EQ(array->prefixItems.size(), 2U);
    EXPECT_EQ(array->minItems, 2U);
    EXPECT_EQ(array->maxItems, 2U);
    EXPECT_EQ(array->additionalItems, false);
    EXPECT_TRUE(std::holds_alternative<parsing::schema::Type::Integer>(array->prefixItems[0].value));
    EXPECT_TRUE(std::holds_alternative<parsing::schema::Type::AnyOf>(array->prefixItems[1].value));

    constexpr auto Untagged = UnionTag{.encoding = UnionEncoding::Untagged};
    const auto untaggedSchema = parser_schema<Variant>(Untagged);
    const auto* alternatives = std::get_if<parsing::schema::Type::AnyOf>(&untaggedSchema.value);
    ASSERT_NE(alternatives, nullptr);
    EXPECT_EQ(alternatives->types.size(), 2U);

    Variant value;
    auto taggedValue = make_tags<Untagged>(value);
    const auto taggedFieldSchema = parser_schema<decltype(taggedValue)>();
    EXPECT_TRUE(std::holds_alternative<parsing::schema::Type::AnyOf>(taggedFieldSchema.value));
}

TEST(SerializationTags, MakeTagsExposeAccessorAndWrappedTag) {
    auto spec = make_tags<rename_tag<"wire_code">, JsonTag{.skippable = true}>(&TypeLevelFlatTagInner::code);

    static_assert(is_tagged_field_v<decltype(spec)>);
    static_assert(std::is_same_v<detail::resolve_without_context_t<decltype(&TypeLevelFlatTagInner::code)>, int>);
    static_assert(std::is_same_v<resolve_member_type_t<field_accessor_t<decltype(spec)>, TypeLevelFlatTagInner>, int>);
    static_assert(tag_query::get<tag_property::name>(field_tags_v<decltype(spec)>) == "wire_code");
    static_assert(tag_query::get<tag_property::skippable>(field_tags_v<decltype(spec)>));
}

TEST(SerializationTags, SerializerMacroStripsMakeTagsWhenBuildingReflectionNames) {
    constexpr auto names = Reflect<MacroParsedTaggedObject>::names();
    static_assert(names.size() == 4);
    static_assert(names[0] == "raw");
    static_assert(names[1] == "renamed");
    static_assert(names[2] == "optional");
    static_assert(names[3] == "nested");

    constexpr auto rawTags      = std::get<0>(Reflect<MacroParsedTaggedObject>::field_tags);
    constexpr auto renamedTags  = std::get<1>(Reflect<MacroParsedTaggedObject>::field_tags);
    constexpr auto optionalTags = std::get<2>(Reflect<MacroParsedTaggedObject>::field_tags);
    constexpr auto nestedTags   = std::get<3>(Reflect<MacroParsedTaggedObject>::field_tags);

    static_assert(tag_query::get<tag_property::raw_string>(rawTags));
    static_assert(tag_query::get<tag_property::comment>(renamedTags) == "doc keeps parser depth");
    static_assert(tag_query::get<tag_property::name>(renamedTags) == "wire(alias)");
    static_assert(tag_query::get<tag_property::skippable>(renamedTags));
    static_assert(tag_query::get<tag_property::skippable>(optionalTags));
    static_assert(tag_query::get<tag_property::comment>(nestedTags) == "flattened docs");
    static_assert(tag_query::get<tag_property::flat<TypeLevelFlatTagInner>>(nestedTags));
    constexpr auto ignoredSpec = make_tags<serialization_ignore_tag>(&SerializationIgnoredObject::transient);
    static_assert(tag_query::get<tag_property::ignore>(field_tags_v<decltype(ignoredSpec)>));

    constexpr auto innerNames = Reflect<TypeLevelFlatTagInner>::names();
    static_assert(innerNames.size() == 2);
    static_assert(innerNames[0] == "code");
    static_assert(innerNames[1] == "label");
}

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
TEST(SerializationTagIntegration, JsonParserAppliesRawRenameSkippableAndFlatTags) {
    const MacroParsedTaggedObject source{
        .raw      = R"({"enabled":true})",
        .renamed  = 11,
        .optional = std::nullopt,
        .nested   = {.code = 7, .label = "seven"},
    };

    EXPECT_EQ(writeJson(source), R"json({"raw":{"enabled":true},"wire(alias)":11,"wire_code":7,"label":"seven"})json");

    MacroParsedTaggedObject decoded;
    decoded.renamed  = 99;
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
        .inner  = {.code = 2, .label = "two"},
        .after  = 3,
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
#endif

TEST(SerializationTagIntegration, SchemaUsesFlatSkippableOptionalAndFixedLengthTags) {
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
    ASSERT_TRUE(fixed.fixed_length.has_value());
    EXPECT_EQ(fixed.fixed_length.value(), sizeof(std::uint32_t));
}

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
TEST(SerializationTagIntegration, SerializationIgnoreTagOmitsNamedFieldFromJsonReadAndSchema) {
    const SerializationIgnoredObject source{
        .id        = 1,
        .transient = 99,
        .value     = 2,
    };
    EXPECT_EQ(writeJson(source), R"({"id":1,"value":2})");

    SerializationIgnoredObject decoded;
    decoded.transient = 55;
    ASSERT_TRUE(readJson(R"({"id":3,"transient":77,"value":4})", decoded));
    EXPECT_EQ(decoded.id, 3);
    EXPECT_EQ(decoded.transient, 55);
    EXPECT_EQ(decoded.value, 4);

    const auto object = objectSchema<SerializationIgnoredObject>();
    EXPECT_TRUE(object.properties.contains("id"));
    EXPECT_FALSE(object.properties.contains("transient"));
    EXPECT_TRUE(object.properties.contains("value"));
    EXPECT_TRUE(contains(object.required, "id"));
    EXPECT_FALSE(contains(object.required, "transient"));
    EXPECT_TRUE(contains(object.required, "value"));
}

TEST(SerializationTagIntegration, SerializationIgnoreTagRemovesPositionalFieldFromJsonLayout) {
    const SerializationIgnoredArray source{
        .first     = 1,
        .transient = 99,
        .last      = 2,
    };
    EXPECT_EQ(writeJson(source), R"([1,2])");

    SerializationIgnoredArray decoded;
    decoded.transient = 55;
    ASSERT_TRUE(readJson(R"([3,4])", decoded));
    EXPECT_EQ(decoded.first, 3);
    EXPECT_EQ(decoded.transient, 55);
    EXPECT_EQ(decoded.last, 4);

    const auto schema = parser_schema<SerializationIgnoredArray>();
    const auto array  = std::get<parsing::schema::Type::Array>(schema.value);
    ASSERT_TRUE(array.minItems.has_value());
    ASSERT_TRUE(array.maxItems.has_value());
    EXPECT_EQ(array.minItems.value(), 2U);
    EXPECT_EQ(array.maxItems.value(), 2U);
    EXPECT_EQ(array.prefixItems.size(), 2U);
}
#endif

TEST(SerializationTagIntegration, TypeLevelUnframedTagRoundTripsBinaryLayout) {
    const TypeLevelUnframedEnvelope source{
        .header = {.version = 1, .type = 2},
        .tail   = 3,
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

TEST(SerializationTagPropagation, TagAwareReaderGetsContainerTagButElementsDoNotInheritIt) {
    const auto input = ReaderProbeNode::array(
        {ReaderProbeNode::integerValue(10), ReaderProbeNode::integerValue(20)});
    std::vector<int> decoded;

    TagAwareProbeReader::reset();
    ASSERT_TRUE(parser_read<TagAwareProbeReader>(&input, decoded, TagList<ReaderNodeTag>{}));
    EXPECT_EQ(decoded, (std::vector<int>{10, 20}));
    EXPECT_EQ(TagAwareProbeReader::taggedArrayCalls, 1);
    EXPECT_EQ(TagAwareProbeReader::arrayCallsWithProbe, 1);
    EXPECT_EQ(TagAwareProbeReader::taggedBasicCalls, 2);
    EXPECT_EQ(TagAwareProbeReader::basicCallsWithProbe, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyArrayCalls, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyBasicCalls, 0);
}

TEST(SerializationTagPropagation, OptionalPassesTagToItsNonNullPayloadNode) {
    const auto input = ReaderProbeNode::array(
        {ReaderProbeNode::integerValue(30), ReaderProbeNode::integerValue(40)});
    std::optional<std::vector<int>> decoded;

    TagAwareProbeReader::reset();
    ASSERT_TRUE(parser_read<TagAwareProbeReader>(&input, decoded, TagList<ReaderNodeTag>{}));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), (std::vector<int>{30, 40}));
    EXPECT_EQ(TagAwareProbeReader::taggedEmptyCalls, 1);
    EXPECT_EQ(TagAwareProbeReader::emptyCallsWithProbe, 1);
    EXPECT_EQ(TagAwareProbeReader::taggedArrayCalls, 1);
    EXPECT_EQ(TagAwareProbeReader::arrayCallsWithProbe, 1);
    EXPECT_EQ(TagAwareProbeReader::basicCallsWithProbe, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyEmptyCalls, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyArrayCalls, 0);
}

TEST(SerializationTagPropagation, ReflectionDispatchesFieldTagToLookupAndValueNode) {
    const auto input = ReaderProbeNode::object({{"value", ReaderProbeNode::integerValue(55)}});
    ReaderProbeObject decoded;

    TagAwareProbeReader::reset();
    ASSERT_TRUE(parser_read<TagAwareProbeReader>(&input, decoded));
    EXPECT_EQ(decoded.value, 55);
    EXPECT_EQ(TagAwareProbeReader::fieldCallsWithProbe, 1);
    EXPECT_EQ(TagAwareProbeReader::emptyCallsWithProbe, 1);
    EXPECT_EQ(TagAwareProbeReader::basicCallsWithProbe, 1);
    EXPECT_EQ(TagAwareProbeReader::objectCallsWithProbe, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyObjectCalls, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyFieldCalls, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyEmptyCalls, 0);
    EXPECT_EQ(TagAwareProbeReader::legacyBasicCalls, 0);
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
