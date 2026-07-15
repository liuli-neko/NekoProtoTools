#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

#include <gtest/gtest.h>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
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

struct MapEntryWireValue {
    int key = 0;
    int value = 0;

    NEKO_SERIALIZER(key, value)
};

struct ArbitraryBinaryValue {
    std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
    std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    std::uint64_t unsignedMaximum = std::numeric_limits<std::uint64_t>::max();
    float negativeInfinity = -std::numeric_limits<float>::infinity();
    double notANumber = std::numeric_limits<double>::quiet_NaN();
    std::string opaque;

    NEKO_SERIALIZER(minimum, maximum, unsignedMaximum, negativeInfinity, notANumber, opaque)
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

    const auto roundTrip = [Fixed]<typename Optional>(const Optional& source) {
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

TEST(BinarySerializer, NullStringAndVariantGoldenVectorsRemainStable) {
    auto bytesOf = [](const auto& value) {
        std::vector<char> buffer;
        BinarySerializer::OutputSerializer output(buffer);
        EXPECT_TRUE(output(value));
        return std::vector<std::uint8_t>(buffer.begin(), buffer.end());
    };

    const std::vector<std::uint8_t> nullGolden{0x4eU, 0x50U, 0x02U, 0x00U};
    EXPECT_EQ(bytesOf(std::optional<std::string>{}), nullGolden);

    const std::vector<std::uint8_t> stringGolden{
        0x4eU, 0x50U, 0x02U, 0x07U, 0x04U,
        static_cast<std::uint8_t>('n'), static_cast<std::uint8_t>('u'),
        static_cast<std::uint8_t>('l'), static_cast<std::uint8_t>('l')};
    EXPECT_EQ(bytesOf(std::string{"null"}), stringGolden);

    const std::vector<std::uint8_t> variantGolden{
        0x4eU, 0x50U, 0x02U, 0x08U, 0x02U, 0x04U, 0x01U, 0x07U, 0x01U,
        static_cast<std::uint8_t>('A')};
    EXPECT_EQ(bytesOf(std::variant<int, std::string>{std::in_place_index<1>, "A"}), variantGolden);
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

TEST(BinarySerializer, ContainerDepthInputAndAllocationBudgetsAreIndependent) {
    const std::vector<std::vector<int>> source{{1}, {2}, {3}};
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    auto expectRejectedWithoutMutation = [&](binary::ParseLimits limits) {
        std::vector<std::vector<int>> target{{99}};
        BinarySerializer::InputSerializer input(buffer.data(), buffer.size(), limits);
        EXPECT_FALSE(input(target));
        EXPECT_EQ(target, std::vector<std::vector<int>>{{99}});
    };

    binary::ParseLimits containerLimit;
    containerLimit.max_container_elements = 2U;
    expectRejectedWithoutMutation(containerLimit);

    binary::ParseLimits depthLimit;
    depthLimit.max_depth = 0U;
    expectRejectedWithoutMutation(depthLimit);

    binary::ParseLimits inputLimit;
    inputLimit.max_input_bytes = buffer.size() - 1U;
    expectRejectedWithoutMutation(inputLimit);

    binary::ParseLimits allocationLimit;
    allocationLimit.max_total_allocated_bytes = 0U;
    expectRejectedWithoutMutation(allocationLimit);
}

TEST(BinarySerializer, DuplicateUniqueKeysElementsAndWireFieldsAreRejectedTransactionally) {
    const std::vector<MapEntryWireValue> duplicateMapEntries{{.key = 7, .value = 1}, {.key = 7, .value = 2}};
    std::vector<char> mapBuffer;
    BinarySerializer::OutputSerializer mapOutput(mapBuffer);
    ASSERT_TRUE(mapOutput(duplicateMapEntries));

    std::map<int, int> mapTarget{{99, 99}};
    BinarySerializer::InputSerializer mapInput(mapBuffer.data(), mapBuffer.size());
    EXPECT_FALSE(mapInput(mapTarget));
    EXPECT_EQ(mapTarget, (std::map<int, int>{{99, 99}}));

    const std::vector<int> duplicateElements{5, 5};
    std::vector<char> setBuffer;
    BinarySerializer::OutputSerializer setOutput(setBuffer);
    ASSERT_TRUE(setOutput(duplicateElements));

    std::set<int> setTarget{99};
    BinarySerializer::InputSerializer setInput(setBuffer.data(), setBuffer.size());
    EXPECT_FALSE(setInput(setTarget));
    EXPECT_EQ(setTarget, (std::set<int>{99}));

    std::vector<char> duplicateFieldBuffer;
    for (const auto byte : binary::BinaryMagic) {
        duplicateFieldBuffer.push_back(static_cast<char>(byte));
    }
    const std::uint8_t duplicateFieldWire[] = {
        static_cast<std::uint8_t>(binary::ValueTag::NamedObject), 0x02U,
        0x01U, static_cast<std::uint8_t>('a'), static_cast<std::uint8_t>(binary::ValueTag::UnsignedInteger), 0x01U,
        0x01U, static_cast<std::uint8_t>('a'), static_cast<std::uint8_t>(binary::ValueTag::UnsignedInteger), 0x02U,
    };
    for (const auto byte : duplicateFieldWire) {
        duplicateFieldBuffer.push_back(static_cast<char>(byte));
    }

    std::map<std::string, int> fieldTarget{{"unchanged", 9}};
    BinarySerializer::InputSerializer fieldInput(duplicateFieldBuffer.data(), duplicateFieldBuffer.size());
    EXPECT_FALSE(fieldInput(fieldTarget));
    EXPECT_EQ(fieldTarget, (std::map<std::string, int>{{"unchanged", 9}}));
}

TEST(BinarySerializer, ParseLimitBoundariesAcceptExactValuesAndRejectOnePastThem) {
    auto serialize = [](const auto& value) {
        std::vector<char> buffer;
        BinarySerializer::OutputSerializer output(buffer);
        EXPECT_TRUE(output(value));
        return buffer;
    };

    const auto emptyBuffer = serialize(std::vector<int>{});
    binary::ParseLimits zeroContainerLimit;
    zeroContainerLimit.max_container_elements = 0U;
    std::vector<int> emptyTarget{99};
    BinarySerializer::InputSerializer emptyInput(emptyBuffer.data(), emptyBuffer.size(), zeroContainerLimit);
    ASSERT_TRUE(emptyInput(emptyTarget));
    EXPECT_TRUE(emptyTarget.empty());

    const std::vector<int> threeValues{1, 2, 3};
    const auto threeBuffer = serialize(threeValues);
    binary::ParseLimits exactContainerLimit;
    exactContainerLimit.max_container_elements = threeValues.size();
    std::vector<int> exactTarget;
    BinarySerializer::InputSerializer exactInput(threeBuffer.data(), threeBuffer.size(), exactContainerLimit);
    ASSERT_TRUE(exactInput(exactTarget));
    EXPECT_EQ(exactTarget, threeValues);

    binary::ParseLimits onePastContainerLimit;
    onePastContainerLimit.max_container_elements = threeValues.size() - 1U;
    std::vector<int> rejectedTarget{99};
    BinarySerializer::InputSerializer rejectedInput(threeBuffer.data(), threeBuffer.size(), onePastContainerLimit);
    EXPECT_FALSE(rejectedInput(rejectedTarget));
    EXPECT_EQ(rejectedTarget, std::vector<int>{99});

    const std::string eightBytes("a\0b\0c\0d\0", 8U);
    const auto stringBuffer = serialize(eightBytes);
    binary::ParseLimits exactStringLimit;
    exactStringLimit.max_string_bytes = eightBytes.size();
    std::string stringTarget;
    BinarySerializer::InputSerializer stringInput(stringBuffer.data(), stringBuffer.size(), exactStringLimit);
    ASSERT_TRUE(stringInput(stringTarget));
    EXPECT_EQ(stringTarget, eightBytes);

    binary::ParseLimits shortStringLimit;
    shortStringLimit.max_string_bytes = eightBytes.size() - 1U;
    std::string unchanged = "unchanged";
    BinarySerializer::InputSerializer shortStringInput(stringBuffer.data(), stringBuffer.size(), shortStringLimit);
    EXPECT_FALSE(shortStringInput(unchanged));
    EXPECT_EQ(unchanged, "unchanged");

    const VersionOneObject object{.first = 1, .second = 2, .extra = "three"};
    const auto objectBuffer = serialize(object);
    binary::ParseLimits exactObjectLimit;
    exactObjectLimit.max_object_fields = 3U;
    VersionOneObject objectTarget;
    BinarySerializer::InputSerializer objectInput(objectBuffer.data(), objectBuffer.size(), exactObjectLimit);
    ASSERT_TRUE(objectInput(objectTarget));
    EXPECT_EQ(objectTarget.first, 1);
    EXPECT_EQ(objectTarget.second, 2);
    EXPECT_EQ(objectTarget.extra, "three");

    binary::ParseLimits shortObjectLimit;
    shortObjectLimit.max_object_fields = 2U;
    objectTarget = {.first = 99, .second = 99, .extra = "unchanged"};
    BinarySerializer::InputSerializer shortObjectInput(objectBuffer.data(), objectBuffer.size(), shortObjectLimit);
    EXPECT_FALSE(shortObjectInput(objectTarget));
    EXPECT_EQ(objectTarget.first, 99);
    EXPECT_EQ(objectTarget.second, 99);
    EXPECT_EQ(objectTarget.extra, "unchanged");

    const std::vector<std::vector<int>> nested{{1}};
    const auto nestedBuffer = serialize(nested);
    binary::ParseLimits exactDepth;
    exactDepth.max_depth = 2U;
    std::vector<std::vector<int>> nestedTarget;
    BinarySerializer::InputSerializer exactDepthInput(nestedBuffer.data(), nestedBuffer.size(), exactDepth);
    ASSERT_TRUE(exactDepthInput(nestedTarget));
    EXPECT_EQ(nestedTarget, nested);

    binary::ParseLimits shallowDepth;
    shallowDepth.max_depth = 1U;
    nestedTarget = {{99}};
    BinarySerializer::InputSerializer shallowInput(nestedBuffer.data(), nestedBuffer.size(), shallowDepth);
    EXPECT_FALSE(shallowInput(nestedTarget));
    EXPECT_EQ(nestedTarget, std::vector<std::vector<int>>{{99}});

    binary::ParseLimits exactInputSize;
    exactInputSize.max_input_bytes = nestedBuffer.size();
    nestedTarget.clear();
    BinarySerializer::InputSerializer exactSizeInput(nestedBuffer.data(), nestedBuffer.size(), exactInputSize);
    EXPECT_TRUE(exactSizeInput(nestedTarget));
    exactInputSize.max_input_bytes = nestedBuffer.size() - 1U;
    nestedTarget = {{99}};
    BinarySerializer::InputSerializer shortSizeInput(nestedBuffer.data(), nestedBuffer.size(), exactInputSize);
    EXPECT_FALSE(shortSizeInput(nestedTarget));
    EXPECT_EQ(nestedTarget, std::vector<std::vector<int>>{{99}});
}

TEST(BinarySerializer, FixedSeedRandomMiddleValuesRoundTrip) {
    std::mt19937_64 generator(0x4E505632ULL);
    std::uniform_int_distribution<std::int64_t> integers(-1'000'000'000LL, 1'000'000'000LL);
    std::uniform_int_distribution<unsigned> lengths(0U, 64U);
    std::uniform_int_distribution<unsigned> bytes(0U, 255U);

    for (unsigned sample = 0; sample < 64U; ++sample) {
        std::vector<std::int64_t> source(lengths(generator));
        for (auto& value : source) {
            value = integers(generator);
        }
        std::string opaque(lengths(generator), '\0');
        for (auto& value : opaque) {
            value = static_cast<char>(bytes(generator));
        }
        auto aggregate = std::make_pair(source, opaque);

        std::vector<char> buffer;
        BinarySerializer::OutputSerializer output(buffer);
        ASSERT_TRUE(output(aggregate)) << "sample=" << sample;
        decltype(aggregate) decoded;
        BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
        ASSERT_TRUE(input(decoded)) << "sample=" << sample
                                    << " error=" << (input.error() == nullptr ? "" : input.error()->msg);
        EXPECT_EQ(decoded, aggregate) << "sample=" << sample;
    }
}

TEST(BinarySerializer, RandomInvalidAndTruncatedWireAlwaysReportsAndPreservesTarget) {
    std::mt19937 generator(0xBAD4E50U);
    std::uniform_int_distribution<unsigned> invalidTags(
        static_cast<unsigned>(binary::ValueTag::FixedUnsigned64) + 1U, 0xffU);
    for (unsigned sample = 0; sample < 64U; ++sample) {
        std::vector<char> buffer;
        for (const auto byte : binary::BinaryMagic) {
            buffer.push_back(static_cast<char>(byte));
        }
        buffer.push_back(static_cast<char>(invalidTags(generator)));
        int target = 99;
        BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
        EXPECT_FALSE(input(target)) << "sample=" << sample;
        EXPECT_EQ(target, 99) << "sample=" << sample;
        ASSERT_NE(input.error(), nullptr) << "sample=" << sample;
        EXPECT_EQ(input.error()->ec, sa::make_error_code(sa::ErrorCode::InvalidType)) << "sample=" << sample;
    }

    std::vector<char> valid;
    BinarySerializer::OutputSerializer output(valid);
    ASSERT_TRUE(output(std::vector<std::string>{"alpha", "beta", "gamma"}));
    for (std::size_t length = 0; length < valid.size(); ++length) {
        std::vector<std::string> target{"unchanged"};
        BinarySerializer::InputSerializer input(valid.data(), length);
        EXPECT_FALSE(input(target)) << "truncated_length=" << length;
        EXPECT_EQ(target, std::vector<std::string>{"unchanged"}) << "truncated_length=" << length;
        EXPECT_NE(input.error(), nullptr) << "truncated_length=" << length;
    }
}

TEST(BinarySerializer, LegitimateUnusualScalarAndOpaqueValuesRoundTrip) {
    ArbitraryBinaryValue source;
    source.opaque = std::string{"\0\x01\x7f\x80\xff", 5U};
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    ArbitraryBinaryValue decoded;
    decoded.minimum = 0;
    decoded.maximum = 0;
    decoded.unsignedMaximum = 0;
    decoded.negativeInfinity = 0;
    decoded.notANumber = 0;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    ASSERT_TRUE(input(decoded)) << (input.error() == nullptr ? "" : input.error()->msg);
    EXPECT_EQ(decoded.minimum, std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(decoded.maximum, std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(decoded.unsignedMaximum, std::numeric_limits<std::uint64_t>::max());
    EXPECT_TRUE(std::isinf(decoded.negativeInfinity));
    EXPECT_LT(decoded.negativeInfinity, 0.0F);
    EXPECT_TRUE(std::isnan(decoded.notANumber));
    EXPECT_EQ(decoded.opaque, source.opaque);
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
