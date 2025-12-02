#include <gtest/gtest.h>

#include "nekoproto/global/config.h"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/serialization/private/tags.hpp"
#include "nekoproto/serialization/private/traits.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/types/optional.hpp"
#include "nekoproto/serialization/types/struct_unwrap.hpp"
#include "nekoproto/serialization/types/vector.hpp"

#include <tuple>
#include <utility>
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
    static_assert(std::is_same_v<Reflect<Test1>::value_types, std::tuple<int, int, int>>, "Test1 must have 3 members");
    Test1 test{.member1 = 23, .member2 = 12, .member3 = 45};
    Reflect<Test1>::forEach(
        test, [](auto& field, [[maybe_unused]] const JsonTags& tags) { // default tags can convert to any tags
            NEKO_LOG_INFO("test", "field: {}", field);
            field += 10;
        });
    Reflect<Test1>::forEach(test,
                            [](auto& field, std::string_view name) { NEKO_LOG_INFO("test", "{}: {}", name, field); });
    EXPECT_EQ(test.member1, 33);
    EXPECT_EQ(test.member2, 22);
    EXPECT_EQ(test.member3, 55);
}

struct CustomTags {
    bool tag1 = false;
    bool tag2 = false;
    bool tag3 = false;
};

struct Test2 {
    int member1 = 213;
    int member2 = 125;
    int member3 = 462;
    int member4 = 546;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("member1", make_tags<CustomTags{.tag1 = false, .tag2 = true, .tag3 = false}>(&Test2::member1),
                   "member2", make_tags<JsonTags{.flat = true, .skipable = false, .rawString = true}>(&Test2::member2),
                   "member3",
                   make_tags<BinaryTags{.fixedLength = true}>([](auto&& self) -> auto& { return self.member3; }),
                   "member4", &Test2::member4);
    };
};

TEST(Reflection, RefObjectValue) {
    static_assert(!detail::is_local_ref_values<Test2> && !detail::is_local_names<Test2>,
                  "Test2 must not be local_ref_values");
    static_assert(detail::is_local_ref_object<Test2>, "Test2 must be local_ref_object");
    static_assert(!detail::is_local_ref_array<Test2>, "Test2 must not be local_ref_array");
    static_assert(!detail::is_local_ref_value<Test2>, "Test2 must not be local_ref_value");
    static_assert(detail::has_value_function<Test2>, "Test2 must have value function");
    using FuncType = unwrap_tags_t<std::decay_t<decltype(std::get<2>(Test2::Neko::value.values))>>;
    static_assert(detail::is_member_ref_function<FuncType, Test2>, "");
    Test2 test{.member1 = 23, .member2 = 12, .member3 = 45, .member4 = 56};
    Reflect<Test2>::forEach(test, [](auto& field) {
        NEKO_LOG_INFO("test", "field: {}", field);
        field += 10;
    });
    static_assert(std::is_same_v<Reflect<Test2>::value_types, std::tuple<int, int, int, int>>,
                  "Test2 must have 4 members");
    static_assert(is_tagged_value_v<std::tuple_element_t<
                      0, std::decay_t<decltype(detail::ReflectHelper<Test2>::getValues(std::declval<Test2&>()))>>>,
                  "has tag");
    Reflect<Test2>::forEach(test, [](auto& field, std::string_view name, const auto& tags) {
        NEKO_LOG_INFO("test", "{}: {}", name, field);
        if constexpr (std::is_same_v<std::decay_t<decltype(tags)>, CustomTags>) {
            NEKO_LOG_INFO("test", "custom tags: tag1={} tag2={} tag3={}", tags.tag1, tags.tag2, tags.tag3);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(tags)>, JsonTags>) {
            NEKO_LOG_INFO("test", "json tags: flat={} skipable={} rawString={}", tags.flat, tags.skipable,
                          tags.rawString);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(tags)>, BinaryTags>) {
            NEKO_LOG_INFO("test", "binary tags: fixedLength={}", tags.fixedLength);
        } else {
            NEKO_LOG_INFO("test", "not tags");
        }
    });
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
            Array(make_tags<BinaryTags{.fixedLength = true}>([](auto&& self) -> auto& { return self.prr.member1; }),
                  make_tags<JsonTags{.rawString = true}>(&Test3::member2));
    };
};

TEST(Reflection, RefObjectArray) {
    static_assert(!detail::is_local_ref_values<Test3> && !detail::is_local_names<Test3>,
                  "Test3 must not be local_ref_values");
    static_assert(!detail::is_local_ref_object<Test3>, "Test3 must not be local_ref_object");
    static_assert(detail::is_local_ref_array<Test3>, "Test3 must be local_ref_array");
    static_assert(!detail::is_local_ref_value<Test3>, "Test3 must not be local_ref_value");
    static_assert(std::is_same_v<Reflect<Test3>::value_types, std::tuple<int, std::string>>,
                  "Test3 must have 2 members");
    Test3 test{.prr = {.member1 = 234}, .member2 = "3453"};
    Reflect<Test3>::forEach(test, []<typename T>(T& field, const auto& tags) {
        NEKO_LOG_INFO("test", "field: {}", field);
        if constexpr (std::is_same_v<std::decay_t<decltype(tags)>, BinaryTags>) {
            NEKO_LOG_INFO("test", "binary tags: fixedLength={}", tags.fixedLength);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(tags)>, JsonTags>) {
            NEKO_LOG_INFO("test", "json tags: flat={} skipable={} rawString={}", tags.flat, tags.skipable,
                          tags.rawString);
        }
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
        constexpr static auto value = make_tags<JsonTags{.flat = true}>(&Test4::member1); // NOLINT
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
    static_assert(std::is_same_v<Reflect<Test4>::value_types, std::tuple<int>>, "Test4 must have 1 members");
    static_assert(std::is_same_v<Reflect<Test5>::value_types, std::tuple<int>>, "Test5 must have 1 members");

    Test4 test{.member1 = 23, .member2 = 12};
    Reflect<Test4>::forEach(test, [](auto& field) {
        NEKO_LOG_INFO("test", "field: {}", field);
        field += 10;
    });
    EXPECT_EQ(test.member1, 33);

    Test5 test2{.member1 = 23, .member2 = 12};
    Reflect<Test5>::forEach(test2, [](auto& field, std::string_view name) {
        NEKO_LOG_INFO("test", "{}: {}", name, field);
        field += 10;
    });
    EXPECT_EQ(test2.member2, 22);
}

struct Test6 {
    int member1 = 213;
    int member2 = 125;
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<Test6, void> {
    constexpr static auto value = // NOLINT
        Object("the_member1", [](auto&& self) -> auto& { return self.member1; }, "the_member2", &Test6::member2);
};
NEKO_END_NAMESPACE

TEST(Reflection, Local) {
    static_assert(!detail::is_local_ref_values<Test6>, "Test6 must be local_ref_values");
    static_assert(!detail::is_local_names<Test6>, "Test6 must be local_names");
    static_assert(!detail::is_local_ref_object<Test6>, "Test6 must not be local_ref_object");
    static_assert(!detail::is_local_ref_array<Test6>, "Test6 must not be local_ref_array");
    static_assert(!detail::is_local_ref_value<Test6>, "Test6 must not be local_ref_value");
    Test6 test{.member1 = 23, .member2 = 12};
    static_assert(std::is_same_v<Reflect<Test6>::value_types, std::tuple<int, int>>, "Test6 must have 2 members");
    Reflect<Test6>::forEach(test, [](auto& field, std::string_view name) {
        NEKO_LOG_INFO("test", "{}: {}", name, field);
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
    Reflect<Test1>::forEach(test, [](auto&& field) { NEKO_LOG_INFO("test", "field: {}", field); });
}

struct Test7 {
    std::optional<int> member1;
    int member2;
    std::string member3;
    std::string member4;
    int member5; // msvc has bug with optional
};

TEST(Reflection, Optional) {
    Test7 test{.member1 = 1, .member2 = 2, .member3 = "3", .member4 = "5", .member5 = 6};
    // clang-format off
    static_assert(std::is_same_v<Reflect<Test7>::value_types, std::tuple<std::optional<int>, int, std::string, std::string, int>>, "Test7 must have 5 members");
    Reflect<Test7>::forEach(test, Overloads{
        [](std::optional<int>& field, std::string_view name) {
            if (field.has_value()) {
                NEKO_LOG_INFO("test", "{}: {}", name, field.value());
                *field += 10;
            } else {
                NEKO_LOG_INFO("test", "{}: null", name);
            }
        },
        [](int& field, std::string_view name) {
            NEKO_LOG_INFO("test", "{}: {}", name, field);
            field += 10;
        },
        [](std::string& field, std::string_view name) {
            NEKO_LOG_INFO("test", "{}: {}", name, field);
            field += "5";
        },
        [](std::optional<std::string>& field, std::string_view name) {
            if (field.has_value()) {
                NEKO_LOG_INFO("test", "{}: {}", name, field.value());
                *field += "5";
            } else {
                NEKO_LOG_INFO("test", "{}: null", name);
            }
        }});
    // clang-format on
    EXPECT_EQ(*test.member1, 11);
    EXPECT_EQ(test.member2, 12);
    EXPECT_EQ(test.member3, "35");
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
