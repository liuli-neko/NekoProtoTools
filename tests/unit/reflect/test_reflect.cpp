#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflect.hpp"

#include <gtest/gtest.h>

using namespace nekoproto;

struct ThisIsATestStruct {
    int a;
    float b;
    double c;
};

namespace {
struct AStructInInlineNamespace {
    int a;
    float b;
    double c;
};
} // namespace

namespace test_namespace {
struct AStructInNamespace {
    int a;
    float b;
    double c;
};
} // namespace test_namespace

namespace a::b::c::d::e::f {
struct AStructInNestedNamespace {
    int a;
    float b;
    double c;
};
} // namespace a::b::c::d::e::f

TEST(ReflectTest, TestReflect) {
    EXPECT_STREQ(detail::class_nameof<ThisIsATestStruct>.data(), "ThisIsATestStruct");
    EXPECT_STREQ(detail::class_nameof<test_namespace::AStructInNamespace>.data(), "AStructInNamespace");
    EXPECT_STREQ(detail::class_nameof<a::b::c::d::e::f::AStructInNestedNamespace>.data(),
                 "AStructInNestedNamespace");
}

#include "../common/common_main.cpp.in" // IWYU pragma: export