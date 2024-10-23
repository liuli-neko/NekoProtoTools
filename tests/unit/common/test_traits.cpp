#include <gtest/gtest.h>

#include "nekoproto/proto/serializer_base.hpp"
#include "nekoproto/proto/simd_json_serializer.hpp"
#include "nekoproto/proto/types/optional.hpp"
#include "nekoproto/proto/types/struct_unwrap.hpp"
#include "nekoproto/proto/types/vector.hpp"

NEKO_USE_NAMESPACE
struct TestStruct {
    int a;
    std::string b;
    std::optional<std::vector<int>> c;
    std::optional<std::string> d;

    NEKO_SERIALIZER(a, b, c, d)
};

TEST(TraitsTest, test) {
    EXPECT_EQ(NEKO_CPP_PLUS, 17);
    EXPECT_FALSE(traits::is_optional<int>::value);
    EXPECT_TRUE(traits::is_optional<std::optional<int>&>::value);
    EXPECT_TRUE(traits::is_optional<const std::optional<int>&>::value);
    EXPECT_TRUE(traits::is_optional<const std::optional<int>>::value);
    EXPECT_TRUE(detail::can_unwrap_v<TestStruct>);
}

TEST(TraitsTest, simdjson) {
    TestStruct ts;
    ts.a = 1;
    ts.b = "hello";
    ts.c = std::nullopt;
    ts.d = "world";
    std::vector<char> buffer;
    {
        SimdJsonOutputSerializer os(buffer);
        os(ts);
    }
    TestStruct ts2;
    {
        SimdJsonInputSerializer is(buffer.data(), buffer.size());
        is(ts2);
    }
    EXPECT_EQ(ts.a, ts2.a);
    EXPECT_EQ(ts.b, ts2.b);
    EXPECT_EQ(ts.c.has_value(), ts2.c.has_value());
    EXPECT_EQ(ts.d.value(), ts2.d.value());
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}