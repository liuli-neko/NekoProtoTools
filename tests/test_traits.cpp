#include <gtest/gtest.h>

#include "../core/binary_serializer.hpp"
#include "../core/json_serializer.hpp"
#include "../core/proto_base.hpp"
#include "../core/serializer_base.hpp"
#include "../core/to_string.hpp"
#include "../core/types/array.hpp"
#include "../core/types/enum.hpp"
#include "../core/types/list.hpp"
#include "../core/types/map.hpp"
#include "../core/types/struct_unwrap.hpp"
#include "../core/types/tuple.hpp"
#include "../core/types/variant.hpp"
#include "../core/types/vector.hpp"

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

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::debug);
    return RUN_ALL_TESTS();
}