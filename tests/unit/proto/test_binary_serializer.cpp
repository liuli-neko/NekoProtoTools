#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

#include <gtest/gtest.h>
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
            Object("value", make_tags<BinaryTags{.fixedLength = true}>(&TaggedFixedLength::value)); // NOLINT
    };
};

struct InvalidTaggedFixedLength {
    std::uint32_t value = 0;

    struct Neko {
        static constexpr auto value =
            Object("value",
                   make_tags<BinaryTags{.fixedLength = 2}>(
                       &InvalidTaggedFixedLength::value)); // NOLINT
    };
};

struct TaggedUnframedHeader {
    std::uint32_t length = 0;
    std::int32_t data = 0;
    std::uint16_t type = 0;

    struct Neko {
        static constexpr auto value =
            Object("length",
                   make_tags<BinaryTags{.fixedLength = sizeof(std::uint32_t)}>(
                       &TaggedUnframedHeader::length),
                   "data",
                   make_tags<BinaryTags{.fixedLength = sizeof(std::int32_t)}>(
                       &TaggedUnframedHeader::data),
                   "type",
                   make_tags<BinaryTags{.fixedLength = sizeof(std::uint16_t)}>(
                       &TaggedUnframedHeader::type)); // NOLINT
    };
};

struct TaggedUnframedEnvelope {
    TaggedUnframedHeader header;
    int tail = 0;

    struct Neko {
        static constexpr auto value =
            Object("header", make_tags<BinaryTags{.unframed = true}>(&TaggedUnframedEnvelope::header),
                   "tail", &TaggedUnframedEnvelope::tail); // NOLINT
    };
};

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

    EXPECT_EQ(buf.size(), 51);
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

    EXPECT_EQ(buf.size(), 105);
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

TEST(BinarySerializer, TaggedFieldsPreserveUnframedWireLayout) {
    std::vector<char> buffer;
    const TaggedUnframedHeader source{
        .length = 0x01020304U,
        .data   = -2,
        .type   = 0x0506U,
    };

    BinarySerializer::OutputSerializer output(buffer);
    EXPECT_TRUE(output(make_tags<BinaryTags{.unframed = true}>(source)));
    ASSERT_EQ(buffer.size(), 10U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[0]), 0x01U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[1]), 0x02U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[2]), 0x03U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[3]), 0x04U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[8]), 0x05U);
    EXPECT_EQ(static_cast<unsigned char>(buffer[9]), 0x06U);

    TaggedUnframedHeader decoded;
    BinarySerializer::InputSerializer input(buffer.data(), buffer.size());
    auto taggedDecoded = make_tags<BinaryTags{.unframed = true}>(decoded);
    EXPECT_TRUE(input(taggedDecoded));
    EXPECT_EQ(decoded.length, source.length);
    EXPECT_EQ(decoded.data, source.data);
    EXPECT_EQ(decoded.type, source.type);
}

TEST(BinarySerializer, UnframedLayoutCanBeNestedInNamedObject) {
    const TaggedUnframedEnvelope source{
        .header = {.length = 0x01020304U, .data = -2, .type = 0x0506U},
        .tail   = 42,
    };
    std::vector<char> buffer;
    BinarySerializer::OutputSerializer output(buffer);
    ASSERT_TRUE(output(source));

    TaggedUnframedEnvelope decoded;
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

TEST(BinarySerializer, BinaryTagIsPreservedInGenericSchema) {
    const auto schema =
        detail::parser_schema<binary::Reader, binary::Writer, TaggedFixedLength>();
    const auto& object = std::get<parsing::schema::Type::Object>(schema.value);
    const auto& field = object.properties.at("value");
    ASSERT_TRUE(field.fixedLength);
    EXPECT_EQ(*field.fixedLength, sizeof(std::uint32_t));
}

TEST(BinarySerializer, UnframedIsNotStoredAsTypeSchemaMetadata) {
    const auto schema =
        detail::parser_schema<binary::Reader, binary::Writer, TaggedUnframedHeader>();
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
    TaggedUnframedHeader value;
    BinarySerializer::InputSerializer input(data, sizeof(data));
    auto taggedValue = make_tags<BinaryTags{.unframed = true}>(value);

    EXPECT_FALSE(input(taggedValue));
    ASSERT_NE(input.error(), nullptr);
    EXPECT_EQ(input.error()->ec, sa::make_error_code(sa::ErrorCode::ParseError));
    EXPECT_NE(input.error()->msg.find("fixed-length"), std::string::npos);
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
