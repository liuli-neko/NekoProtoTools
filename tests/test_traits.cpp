#include <gtest/gtest.h>

#include "../core/simd_json_serializer.hpp"
#include "../core/types/struct_unwrap.hpp"

NEKO_USE_NAMESPACE
struct TestStruct {
    int a;
    std::string b;
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
    std::vector<char> buffer;
    SimdJsonOutputSerializer serializer(buffer);
    simdjson::padded_string json_str = "{\"a\": 1, \"b\": \"hello\"}"_padded;
    simdjson::dom::parser parser;
    simdjson::dom::element el;
    simdjson::dom::object obj;
    std::cout << "1111111111" << std::endl;
    std::cout << (el == simdjson::dom::element()) << std::endl;
    std::cout << "22222222" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::debug);
    return RUN_ALL_TESTS();
}