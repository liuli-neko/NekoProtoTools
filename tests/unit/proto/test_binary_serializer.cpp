#include "nekoproto/proto/binary_serializer.hpp"
#include "nekoproto/proto/serializer_base.hpp"

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

    EXPECT_EQ(buf.size(), 50);
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
    // EXPECT_STREQ(p2.i.c_str(), "test string");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}