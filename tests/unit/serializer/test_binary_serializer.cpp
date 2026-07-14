#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

#include <gtest/gtest.h>
#include <bit>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace NEKO_NAMESPACE;

class TestP {
public:
    int8_t a      = 1; // 1 byte
    int16_t b     = 2; // 2 bytes
    int32_t c     = 3; // 4 bytes
    int64_t d     = 4; // 8 bytes
    uint8_t e     = 5; // 1 byte
    uint16_t f    = 6; // 2 bytes
    uint32_t g    = 7; // 4 bytes
    uint64_t h    = 8; // 8 bytes
    std::string i = "hello";
    // 30

    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i)
};

struct TestP1 {
    int a                          = 1;
    std::string b                  = "hello";
    bool c                         = true;
    double d                       = 3.14;
    std::list<int> e               = {1, 2, 3, 4, 5};
    std::map<std::string, int> f   = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::array<int, 5> g           = {1, 2, 3, 4, 5};
    std::tuple<int, std::string> h = {1, "hello"};

    NEKO_SERIALIZER(a, b, c, d, e, f, g, h)
};

struct TestProtocolTableLayout {
    std::uint32_t version = 0;
    std::map<std::uint32_t, std::string> protocols;

    NEKO_SERIALIZER(version, protocols)
};

struct TaggedFixedLength {
    std::uint32_t value = 0;

    struct Neko {
        static constexpr auto value =
            Object("value", make_tags<BinaryTag{.fixed_length = true}>(&TaggedFixedLength::value)); // NOLINT
    };
};

struct InvalidTaggedFixedLength {
    std::uint32_t value = 0;

    struct Neko {
        static constexpr auto value =
            Object("value", make_tags<BinaryTag{.fixed_length = 2}>(&InvalidTaggedFixedLength::value)); // NOLINT
    };
};

struct RawFixedHeader {
    std::uint32_t length = 0;
    std::int32_t data    = 0;
    std::uint16_t type   = 0;

    struct Neko {
        static constexpr auto value = Object(
            "length", make_tags<BinaryTag{.fixed_length = sizeof(std::uint32_t)}>(&RawFixedHeader::length),
            "data", make_tags<BinaryTag{.fixed_length = sizeof(std::int32_t)}>(&RawFixedHeader::data), "type",
            make_tags<BinaryTag{.fixed_length = sizeof(std::uint16_t)}>(&RawFixedHeader::type)); // NOLINT
    };
};

struct FixedFieldEnvelope {
    RawFixedHeader header;
    int tail = 0;

    struct Neko {
        static constexpr auto value =
            Object("header", &FixedFieldEnvelope::header, "tail", &FixedFieldEnvelope::tail); // NOLINT
    };
};

enum class RawHeaderKind : std::uint16_t { Data = 0x1234U };

struct RawTypedHeader {
    bool enabled = false;
    RawHeaderKind kind = RawHeaderKind::Data;

    struct Neko {
        static constexpr auto value = Object(
            "enabled", make_tags<BinaryTag{.fixed_length = sizeof(bool)}>(&RawTypedHeader::enabled), "kind",
            make_tags<BinaryTag{.fixed_length = sizeof(std::uint16_t)}>(&RawTypedHeader::kind)); // NOLINT
    };
};

struct InvalidRawHeader {
    std::string text;

    struct Neko {
        static constexpr auto value =
            Object("text", make_tags<BinaryTag{.fixed_length = 4}>(&InvalidRawHeader::text)); // NOLINT
    };
};

struct OptionalFields {
    std::optional<int> first;
    int value = 0;
    std::optional<std::string> last;

    NEKO_SERIALIZER(first, value, last)
};

struct FlatInner {
    int x = 0;
    std::optional<int> y;

    NEKO_SERIALIZER(x, y)
};

struct FlatOuter {
    int before = 0;
    FlatInner inner;
    int after = 0;

    NEKO_SERIALIZER(before, make_tags<ParserTag{.flat = true}>(inner), after)
};

struct VersionOneObject {
    int first = 0;
    int second = 0;
    std::string extra;

    NEKO_SERIALIZER(first, second, extra)
};

struct VersionTwoObject {
    int second = 0;
    int first = 0;
    std::optional<int> added;

    NEKO_SERIALIZER(second, first, added)
};

enum class BinaryEnum : int {
    Known = 1,
};

struct PublicCustomParserId {
    std::uint64_t value = 0;
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<::BinaryEnum, void> {
    static constexpr auto value = Enumerate("Known", ::BinaryEnum::Known);
};

template <>
struct CustomParser<::PublicCustomParserId> {
    template <typename W, typename Parent, typename Tags>
    static ParserResult write(W& writer, const ::PublicCustomParserId& id, const Parent& parent, const Tags& tags) {
        return parser_write<W>(writer, id.value, parent, tags);
    }

    template <typename R, typename Tags>
    static ParserResult read(typename R::InputValueType in, ::PublicCustomParserId& id, const Tags& tags) {
        return parser_read<R>(in, id.value, tags);
    }

    static parsing::schema::Type toSchema() { return parser_schema<std::uint64_t>(); }
};
NEKO_END_NAMESPACE

TEST(BinarySerializer, Serialize) {
    std::vector<char> buf;
    TestP p;
    p.a = -12;
    p.b = 332;
    p.c = 333;
    p.d = 334;
    p.e = 12;
    p.f = 336;
    p.g = 337;
    p.h = 338;
    p.i = "test string";

    BinarySerializer::OutputSerializer os(buf);
    os(p);

    EXPECT_LE(buf.size(), 64U);
    TestP p2;
    BinarySerializer::InputSerializer input(buf.data(), buf.size());
    input(p2);
    EXPECT_EQ(int(p2.a), -12);
    EXPECT_EQ(p2.b, 332);
    EXPECT_EQ(p2.c, 333);
    EXPECT_EQ(p2.d, 334);
    EXPECT_EQ(p2.e, 12);
    EXPECT_EQ(p2.f, 336);
    EXPECT_EQ(p2.g, 337);
    EXPECT_EQ(p2.h, 338);
    EXPECT_STREQ(p2.i.c_str(), "test string");
}

TEST(BinarySerializer, Serialize2) {
    std::vector<char> buf;
    TestP1 p;
    p.a = -12;
    p.b = "this is a test string";
    p.c = false;
    p.d = 334.114514;
    p.e = {1, 32, 4, 5, 6};
    p.f = {{"ta", 1}, {"tb", 2}, {"tc", 3}};
    p.g = {12, 13, 14, 15, 16};
    p.h = {12, "hello, this is a test string"};

    BinarySerializer::OutputSerializer os(buf);
    os(p);

    EXPECT_LE(buf.size(), 140U);
    TestP1 p2;
    BinarySerializer::InputSerializer input(buf.data(), buf.size());
    input(p2);
    EXPECT_EQ(int(p2.a), -12);
    EXPECT_STREQ(p2.b.c_str(), "this is a test string");
    EXPECT_FALSE(p2.c);
    EXPECT_EQ(p2.d, 334.114514);
    EXPECT_EQ(p2.e, std::list<int>({1, 32, 4, 5, 6}));
    EXPECT_EQ(p2.f.size(), 3);
    for (auto it : p2.f) {
        EXPECT_EQ(it.second, 1 + it.first[1] - 'a');
    }
    EXPECT_EQ(p2.g, (std::array<int, 5>({12, 13, 14, 15, 16})));
    EXPECT_EQ(p2.h, (std::tuple<int, std::string>({12, std::string("hello, this is a test string")})));
    EXPECT_EQ(p2.f, (std::map<std::string, int>({{"ta", 1}, {"tb", 2}, {"tc", 3}})));
    EXPECT_EQ(p2.g, (std::array<int, 5>({12, 13, 14, 15, 16})));
    EXPECT_EQ(p2.h, (std::tuple<int, std::string>({12, std::string("hello, this is a test string")})));
}

TEST(BinarySerializer, NonStringMapKeysRoundTrip) {
    const TestProtocolTableLayout source{
        .version   = 1,
        .protocols = {{0x10203040U, "Message"}, {0x55667788U, "OtherMessage"}},
    };
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    TestProtocolTableLayout decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    const bool parsed = input(decoded);
    ASSERT_TRUE(parsed) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded.version, source.version);
    EXPECT_EQ(decoded.protocols, source.protocols);
}

TEST(BinarySerializer, PairFieldsAreReadSequentially) {
    const std::pair<std::uint32_t, std::string> source{0x55667788U, "Message"};
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    std::pair<std::uint32_t, std::string> decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    const bool parsed = input(decoded);
    ASSERT_TRUE(parsed) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded, source);
}

TEST(BinarySerializer, RawFixedDataPreservesExactWireLayout) {
    std::vector<char> buffer;
    const RawFixedHeader source{
        // A raw segment may begin with the Binary V2 magic. The explicit tag
        // must still force raw parsing rather than treating this as a frame.
        .length = 0x4E500204U,
        .data   = -2,
        .type   = 0x0506U,
    };

    BinarySerializer::OutputSerializer output(buffer);
    EXPECT_TRUE(output(make_tags<BinaryTag{.raw_fixed_data = true}>(source)));
    ASSERT_EQ(buffer.size(), 10U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[0]), 0x4EU);
    EXPECT_EQ(static_cast<unsigned char>(buffer[1]), 0x50U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[2]), 0x02U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[3]), 0x04U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[8]), 0x05U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[9]), 0x06U);

    RawFixedHeader decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    auto taggedDecoded = make_tags<BinaryTag{.raw_fixed_data = true}>(decoded);
    EXPECT_TRUE(input(taggedDecoded));
    EXPECT_EQ(decoded.length, source.length);
    EXPECT_EQ(decoded.data, source.data);
    EXPECT_EQ(decoded.type, source.type);
}

TEST(BinarySerializer, RawFixedDataUsesCanonicalBoolAndEnumWidths) {
    const RawTypedHeader source{.enabled = true, .kind = RawHeaderKind::Data};
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(make_tags<BinaryTag{.raw_fixed_data = true}>(source)));
    ASSERT_EQ(buffer.size(), 3U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[0]), 1U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[1]), 0x12U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[2]), 0x34U);

    RawTypedHeader decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    auto tagged = make_tags<BinaryTag{.raw_fixed_data = true}>(decoded);
    ASSERT_TRUE(input(tagged));
    EXPECT_TRUE(decoded.enabled);
    EXPECT_EQ(decoded.kind, RawHeaderKind::Data);

    std::vector<char> invalidBuffer;
    BinarySerializer::OutputSerializer invalidOutput(invalidBuffer);
    EXPECT_FALSE(invalidOutput(make_tags<BinaryTag{.raw_fixed_data = true}>(InvalidRawHeader{.text = "data"})));
    ASSERT_NE(invalidOutput.error(), nullptr);
    EXPECT_EQ(invalidOutput.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType));

    std::vector<char> scalarBuffer;
    BinarySerializer::OutputSerializer scalarOutput(scalarBuffer);
    EXPECT_FALSE(scalarOutput(make_tags<BinaryTag{.raw_fixed_data = true}>(std::uint32_t{7})));
    ASSERT_NE(scalarOutput.error(), nullptr);
    EXPECT_EQ(scalarOutput.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType));
}

TEST(BinarySerializer, FixedLengthFieldsRemainFramedWithoutRawFixedData) {
    const FixedFieldEnvelope source{
        .header = {.length = 0x01020304U, .data = -2, .type = 0x0506U},
        .tail   = 42,
    };
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    FixedFieldEnvelope decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded));
    EXPECT_EQ(decoded.header.length, source.header.length);
    EXPECT_EQ(decoded.header.data, source.header.data);
    EXPECT_EQ(decoded.header.type, source.header.type);
    EXPECT_EQ(decoded.tail, source.tail);
}

TEST(BinarySerializer, BinaryTagSelectsFixedLengthCapability) {
    const TaggedFixedLength source{0x01020304U};
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    TaggedFixedLength decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded));
    EXPECT_EQ(decoded.value, source.value);
}

TEST(BinarySerializer, FixedLengthTagPassesThroughOptionalPresentAndNull) {
    constexpr auto Fixed = BinaryTag{.fixed_length = true};

    const auto roundTrip = []<typename Optional>(const Optional& source) {
        std::vector<char> buffer;
        BinarySerializer::OutputSerializer output(buffer);
        if (!output(make_tags<Fixed>(source))) {
            ADD_FAILURE() << (output.error() == nullptr ? "" : output.error()->msg);
            return Optional{};
        }

        Optional decoded   = std::uint32_t{0xFFFFFFFFU};
        auto taggedDecoded = make_tags<Fixed>(decoded);
        BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
        if (!input(taggedDecoded)) {
            ADD_FAILURE() << (input.error() == nullptr ? "" : input.error()->msg);
        }
        return decoded;
    };

    const std::optional<std::uint32_t> present = 0x01020304U;
    EXPECT_EQ(roundTrip(present), present);

    const std::optional<std::uint32_t> empty = std::nullopt;
    EXPECT_EQ(roundTrip(empty), empty);
}

TEST(BinarySerializer, PublicCustomParserRoundTripsAndBuildsSchema) {
    const PublicCustomParserId source{0x0102030405060708ULL};
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    PublicCustomParserId decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded));
    EXPECT_EQ(decoded.value, source.value);

    const auto schema   = parser_schema<PublicCustomParserId>();
    const auto expected = parser_schema<std::uint64_t>();
    EXPECT_EQ(schema.value.index(), expected.value.index());
}

TEST(BinarySerializer, BinaryTagIsPreservedInGenericSchema) {
    const auto schema  = parser_schema<TaggedFixedLength>();
    const auto& object = std::get<parsing::schema::Type::Object>(schema.value);
    const auto& field  = object.properties.at("value");
    ASSERT_TRUE(field.fixed_length);
    EXPECT_EQ(*field.fixed_length, sizeof(std::uint32_t));
}

TEST(BinarySerializer, RawFixedDataIsNotImplicitTypeMetadata) {
    const auto schema = parser_schema<RawFixedHeader>();
    EXPECT_FALSE(schema.unframed);
}

TEST(BinarySerializer, InvalidFixedLengthReportsWidthMismatch) {
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    EXPECT_FALSE(output(InvalidTaggedFixedLength{.value = 42}));
    ASSERT_NE(output.error(), nullptr);
    EXPECT_EQ(output.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidLength));
    EXPECT_NE(output.error()->msg.find("requires 4 bytes"), std::string::npos);
}

TEST(BinarySerializer, TruncatedInputReportsParseError) {
    const char data[] = {0x01};
    RawFixedHeader value;
    BinarySerializer::InputSerializer input(data, sizeof(data));
    auto taggedValue = make_tags<BinaryTag{.raw_fixed_data = true}>(value);

    EXPECT_FALSE(input(taggedValue));
    ASSERT_NE(input.error(), nullptr);
    EXPECT_EQ(input.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
    EXPECT_NE(input.error()->msg.find("raw fixed"), std::string::npos);
}

TEST(BinarySerializer, NullAndStringNullHaveDistinctRoundTrips) {
    const std::optional<std::string> source = std::string("null");
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    std::optional<std::string> decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, "null");
}

TEST(BinarySerializer, VariantPreservesActiveIndex) {
    const std::variant<int, std::string> source = std::string("abc");
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    std::variant<int, std::string> decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    ASSERT_EQ(decoded.index(), 1U);
    EXPECT_EQ(std::get<1>(decoded), "abc");
}

TEST(BinarySerializer, UntaggedVariantProbeRestoresReaderAndPreservesPayloadTags) {
    constexpr auto Untagged = UnionTag{.encoding = UnionEncoding::Untagged};
    constexpr auto Fixed    = BinaryTag{.fixed_length = true};
    const std::variant<std::uint32_t, std::string> source = std::uint32_t{0x10203040U};

    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(make_tags<Untagged, Fixed>(source)));

    std::variant<std::uint32_t, std::string> decoded = std::string{"unchanged"};
    auto taggedDecoded = make_tags<Untagged, Fixed>(decoded);
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(taggedDecoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    ASSERT_EQ(decoded.index(), 0U);
    EXPECT_EQ(std::get<0>(decoded), 0x10203040U);
}

TEST(BinarySerializer, EmptyOptionalAndFlatFieldsUseActualMemberCount) {
    const OptionalFields optionalSource{.first = std::nullopt, .value = 42, .last = std::nullopt};
    const FlatOuter flatSource{.before = 1, .inner = {.x = 2, .y = std::nullopt}, .after = 3};

    std::vector<char> optionalBuffer;
    BinarySerializer::OutputSerializer optionalOutput(optionalBuffer);
    ASSERT_TRUE(optionalOutput(optionalSource));
    OptionalFields optionalDecoded;
    BinarySerializer::InputSerializer optionalInput(optionalBuffer.data(), optionalBuffer.size());
    ASSERT_TRUE(optionalInput(optionalDecoded))
        << (optionalInput.error() == nullptr ? "" : optionalInput.error()->msg);
    EXPECT_FALSE(optionalDecoded.first.has_value());
    EXPECT_EQ(optionalDecoded.value, 42);
    EXPECT_FALSE(optionalDecoded.last.has_value());

    std::vector<char> flatBuffer;
    BinarySerializer::OutputSerializer flatOutput(flatBuffer);
    ASSERT_TRUE(flatOutput(flatSource));
    FlatOuter flatDecoded;
    BinarySerializer::InputSerializer flatInput(flatBuffer.data(), flatBuffer.size());
    ASSERT_TRUE(flatInput(flatDecoded)) << (flatInput.error() == nullptr ? "" : flatInput.error()->msg);
    EXPECT_EQ(flatDecoded.before, 1);
    EXPECT_EQ(flatDecoded.inner.x, 2);
    EXPECT_FALSE(flatDecoded.inner.y.has_value());
    EXPECT_EQ(flatDecoded.after, 3);
}

TEST(BinarySerializer, UnknownEnumUnderlyingValueRoundTrips) {
    const auto source = static_cast<BinaryEnum>(7);
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    auto decoded = BinaryEnum::Known;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(static_cast<int>(decoded), 7);
}

TEST(BinarySerializer, ObjectsIgnoreOrderAndUnknownFields) {
    const VersionOneObject source{.first = 11, .second = 22, .extra = "ignored"};
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    VersionTwoObject decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded.first, 11);
    EXPECT_EQ(decoded.second, 22);
    EXPECT_FALSE(decoded.added.has_value());
}

TEST(BinarySerializer, FloatingPointUsesBigEndianGoldenBytes) {
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(1.0));
    ASSERT_EQ(buffer.size(), sizeof(binary::BinaryMagic) + 1U + sizeof(double));
    EXPECT_EQ(static_cast<std::uint8_t>(buffer[3]), static_cast<std::uint8_t>(binary::ValueTag::Float64));
    const std::uint8_t expected[] = {0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (std::size_t ix = 0; ix < sizeof(expected); ++ix) {
        EXPECT_EQ(static_cast<std::uint8_t>(buffer[4 + ix]), expected[ix]);
    }
}

TEST(BinarySerializer, StrictModeRejectsTrailingAndNonCanonicalData) {
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(7));
    buffer.push_back(static_cast<char>(0));
    int target = 99;
    BinarySerializer::InputSerializer trailing(buffer.data(), buffer.size());
    EXPECT_FALSE(trailing(target));
    EXPECT_EQ(target, 99);

    std::vector<char> nonCanonical;
    for (const auto byte : binary::BinaryMagic) nonCanonical.push_back(static_cast<char>(byte));
    nonCanonical.push_back(static_cast<char>(binary::ValueTag::UnsignedInteger));
    nonCanonical.push_back(static_cast<char>(0x80));
    nonCanonical.push_back(static_cast<char>(0x00));
    std::uint32_t value = 5;
    BinarySerializer::InputSerializer canonical(nonCanonical.data(), nonCanonical.size());
    EXPECT_FALSE(canonical(value));
    EXPECT_EQ(value, 5U);
}

TEST(BinarySerializer, ParseLimitsAndNullHandlesFailBeforeAllocation) {
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(std::string(8, 'x')));

    binary::ParseLimits limits;
    limits.max_string_bytes = 4;
    std::string target = "unchanged";
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size(), limits);
    EXPECT_FALSE(input(target));
    EXPECT_EQ(target, "unchanged");

    binary::Reader::InputValue invalid;
    const auto invalidResult = binary::Reader::toBasicType<int>(invalid);
    EXPECT_FALSE(invalidResult);
    EXPECT_NE(invalidResult.error().msg.find("handle is null"), std::string::npos);
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
