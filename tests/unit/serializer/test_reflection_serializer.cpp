#include <string>

#include <gtest/gtest.h>

#include "nekoproto/proto/private/reflection_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_USE_NAMESPACE

namespace {
struct ReflectedValue {
    int id           = 1;
    std::string name = "before";

    NEKO_SERIALIZER(id, name)
};
} // namespace

TEST(ReflectionSerializer, BindsFieldsFromReflectionMetadata) {
    ReflectedValue value;
    auto serializer = ReflectionSerializer::reflection(value);
    auto* object    = serializer.getObject();

    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->getField("id", -1), 1);
    EXPECT_EQ(object->getField("name", std::string{}), "before");
    EXPECT_TRUE(object->setField("id", 9));
    EXPECT_TRUE(object->setField("name", std::string{"after"}));
    EXPECT_EQ(value.id, 9);
    EXPECT_EQ(value.name, "after");
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
