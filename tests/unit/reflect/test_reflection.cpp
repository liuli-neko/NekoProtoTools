#include <gtest/gtest.h>

#include "nekoproto/serialization/private/tags.hpp"
#include "nekoproto/serialization/private/traits.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/types/struct_unwrap.hpp"

#include <iostream>
NEKO_USE_NAMESPACE

struct Test1 {
    int member1 = 213;
    int member2 = 125;
    int member3 = 462;
};

TEST(Reflection, Test) {
    EXPECT_FALSE(traits::can_be_serializable<int*>);
    EXPECT_TRUE(traits::can_be_serializable<Test1>);
    EXPECT_EQ((detail::member_nameof<0, Test1>), "member1");
    EXPECT_EQ((detail::member_nameof<1, Test1>), "member2");
    EXPECT_EQ((detail::member_nameof<2, Test1>), "member3");
    EXPECT_TRUE(detail::can_unwrap_v<Test1>);
    static_assert(detail::can_unwrap_v<Test1>, "Test1 must can unwrap");
    static_assert(!detail::is_local_ref_values<Test1> && !detail::is_local_names<Test1>,
                  "Test1 must not be local_ref_values");
    static_assert(!detail::is_local_ref_object<Test1>, "Test1 must not be local_ref_object");
    static_assert(!detail::is_local_ref_array<Test1>, "Test1 must not be local_ref_array");
    static_assert(!detail::is_local_ref_value<Test1>, "Test1 must not be local_ref_value");
    Test1 test{.member1 = 23, .member2 = 12, .member3 = 45};
    Reflect<Test1>::forEach(test, [](auto& field) {
        std::cout << field << std::endl;
        field += 10;
    });
    Reflect<Test1>::forEach(
        test, [](auto& field, std::string_view name) { std::cout << name << ": " << field << std::endl; });
    EXPECT_EQ(test.member1, 33);
    EXPECT_EQ(test.member2, 22);
    EXPECT_EQ(test.member3, 55);
}

struct Test2 {
    int member1 = 213;
    int member2 = 125;
    int member3 = 462;
    int member4 = 546;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object(
                "member1", &Test2::member1, "member2", &Test2::member2, "member3",
                [](auto&& self) -> auto& { return self.member3; }, "member4", &Test2::member4);
    };
};

TEST(Reflection, RefObjectValue) {
    static_assert(!detail::is_local_ref_values<Test2> && !detail::is_local_names<Test2>,
                  "Test2 must not be local_ref_values");
    static_assert(detail::is_local_ref_object<Test2>, "Test2 must be local_ref_object");
    static_assert(!detail::is_local_ref_array<Test2>, "Test2 must not be local_ref_array");
    static_assert(!detail::is_local_ref_value<Test2>, "Test2 must not be local_ref_value");
    static_assert(detail::has_value_function<Test2>, "Test2 must have value function");
    using FuncType = std::decay_t<decltype(std::get<2>(Test2::Neko::value.values))>;
    static_assert(detail::is_member_ref_function<FuncType, Test2>, "");
    Test2 test{.member1 = 23, .member2 = 12, .member3 = 45, .member4 = 56};
    Reflect<Test2>::forEach(test, [](auto& field) {
        std::cout << field << std::endl;
        field += 10;
    });
    Reflect<Test2>::forEach(
        test, [](auto& field, std::string_view name) { std::cout << name << ": " << field << std::endl; });
    EXPECT_EQ(test.member1, 33);
    EXPECT_EQ(test.member2, 22);
    EXPECT_EQ(test.member3, 55);
    EXPECT_EQ(test.member4, 66);

    auto names = Reflect<Test2>::names();
    EXPECT_EQ(names.size(), 4);
    EXPECT_EQ(names[0], "member1");
    EXPECT_EQ(names[1], "member2");
    EXPECT_EQ(names[2], "member3");
    EXPECT_EQ(names[3], "member4");
}

struct Test3 {
    struct Private {
        int member1 = 213;
    } prr;
    std::string member2 = "3453";

    struct Neko {
        constexpr static auto value = // NOLINT
            Array([](auto&& self) -> auto& { return self.prr.member1; }, &Test3::member2);
    };
};

TEST(Reflection, RefObjectArray) {
    static_assert(!detail::is_local_ref_values<Test3> && !detail::is_local_names<Test3>,
                  "Test3 must not be local_ref_values");
    static_assert(!detail::is_local_ref_object<Test3>, "Test3 must not be local_ref_object");
    static_assert(detail::is_local_ref_array<Test3>, "Test3 must be local_ref_array");
    static_assert(!detail::is_local_ref_value<Test3>, "Test3 must not be local_ref_value");
    Test3 test{.prr = {.member1 = 234}, .member2 = "3453"};
    Reflect<Test3>::forEach(test, []<typename T>(T& field) {
        std::cout << field << std::endl;
        if constexpr (std::is_arithmetic_v<T>) {
            field += 10;
        } else {
            field += "123";
        }
    });
    EXPECT_STREQ(test.member2.c_str(), "3453123");
    EXPECT_EQ(test.prr.member1, 244);
}

struct Test4 {
    int member1 = 213;
    int member2 = 125;

    struct Neko {
        constexpr static auto value = &Test4::member1; // NOLINT
    };
};

struct Test5 {
    int member1 = 213;
    int member2 = 125;

    struct Neko {
        constexpr static std::array names = {"member2"};                                       // NOLINT
        constexpr static auto value       = [](auto&& self) -> auto& { return self.member2; }; // NOLINT
    };
};

TEST(Reflection, RefObjectSingle) {
    static_assert(!detail::is_local_ref_values<Test4> && !detail::is_local_names<Test4>,
                  "Test4 must not be local_ref_values");
    static_assert(!detail::is_local_ref_object<Test4>, "Test4 must not be local_ref_object");
    static_assert(!detail::is_local_ref_array<Test4>, "Test4 must not be local_ref_array");
    static_assert(detail::is_local_ref_value<Test4>, "Test4 must be local_ref_value");
    static_assert(!detail::is_local_ref_values<Test5> && detail::is_local_names<Test5>,
                  "Test5 must not be local_ref_values");
    static_assert(!detail::is_local_ref_object<Test5>, "Test5 must not be local_ref_object");
    static_assert(!detail::is_local_ref_array<Test5>, "Test5 must not be local_ref_array");
    static_assert(detail::is_local_ref_value<Test5>, "Test5 must be local_ref_value");
    Test4 test{.member1 = 23, .member2 = 12};
    Reflect<Test4>::forEach(test, [](auto& field) {
        std::cout << field << std::endl;
        field += 10;
    });
    EXPECT_EQ(test.member1, 33);

    Test5 test2{.member1 = 23, .member2 = 12};
    Reflect<Test5>::forEach(test2, [](auto& field, std::string_view name) {
        std::cout << name << ": " << field << std::endl;
        field += 10;
    });
    EXPECT_EQ(test2.member2, 22);
}

struct Test6 {
    int member1 = 213;
    int member2 = 125;
};

template <>
struct Meta<Test6, void> {
    constexpr static auto value = // NOLINT
        Object("the_member1", [](auto&& self) -> auto& { return self.member1; }, "the_member2", &Test6::member2);
};

TEST(Reflection, Local) {
    static_assert(!detail::is_local_ref_values<Test6>, "Test6 must be local_ref_values");
    static_assert(!detail::is_local_names<Test6>, "Test6 must be local_names");
    static_assert(!detail::is_local_ref_object<Test6>, "Test6 must not be local_ref_object");
    static_assert(!detail::is_local_ref_array<Test6>, "Test6 must not be local_ref_array");
    static_assert(!detail::is_local_ref_value<Test6>, "Test6 must not be local_ref_value");
    Test6 test{.member1 = 23, .member2 = 12};
    Reflect<Test6>::forEach(test, [](auto& field, std::string_view name) {
        std::cout << name << ": " << field << std::endl;
        field += 10;
    });
    EXPECT_EQ(test.member1, 33);
    EXPECT_EQ(test.member2, 22);
}

TEST(Reflection, ConstObject) {
    const Test1 test = {
        .member1 = 1,
        .member2 = 2,
        .member3 = 3,
    };
    Reflect<Test1>::forEach(test, [](auto&& field) { std::cout << field << std::endl; });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);
    return RUN_ALL_TESTS();
}