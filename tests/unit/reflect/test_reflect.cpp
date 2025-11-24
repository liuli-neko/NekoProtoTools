#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflect.hpp"

#include <gtest/gtest.h>

NEKO_USE_NAMESPACE

struct this_is_a_test_struct {
    int a;
    float b;
    double c;
};

namespace {
struct a_struct_in_inline_namespace {
    int a;
    float b;
    double c;
};
} // namespace

namespace test_namespace {
struct a_struct_in_namespace {
    int a;
    float b;
    double c;
};
} // namespace test_namespace

namespace a::b::c::d::e::f {
struct a_struct_in_nested_namespace {
    int a;
    float b;
    double c;
};
} // namespace a::b::c::d::e::f

TEST(ReflectTest, TestReflect) {
    EXPECT_STREQ(detail::class_nameof<this_is_a_test_struct>.data(), "this_is_a_test_struct");
    EXPECT_STREQ(detail::class_nameof<test_namespace::a_struct_in_namespace>.data(), "a_struct_in_namespace");
    EXPECT_STREQ(detail::class_nameof<a::b::c::d::e::f::a_struct_in_nested_namespace>.data(),
                 "a_struct_in_nested_namespace");
}

#include "../common/common_main.cpp.in" // IWYU pragma: export