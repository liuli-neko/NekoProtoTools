#include "nekoproto/proto/binary_serializer.hpp"
#include "nekoproto/proto/serializer_base.hpp"
#include "nekoproto/proto/types/array.hpp"
#include "nekoproto/proto/types/list.hpp"
#include "nekoproto/proto/types/map.hpp"
#include "nekoproto/proto/types/tuple.hpp"
#include "nekoproto/proto/types/vector.hpp"

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
    // EXPECT_STREQ(p2.i.c_str(), "test string");
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
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}