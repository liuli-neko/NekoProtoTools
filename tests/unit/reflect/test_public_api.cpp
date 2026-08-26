#include <gtest/gtest.h>

#include "nekoproto/reflect.hpp"
#include "nekoproto/tags.hpp"
#include "nekoproto/serializer.hpp"
#include "nekoproto/to_string.hpp"
#include "nekoproto/string_literal.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace nekoproto;

// 1. Test Structs
struct AggregateUser {
    int id = 1;
    std::string name = "Alice";
    double score = 98.5;
};

struct ExplicitUser {
    int id = 2;
    std::string name = "Bob";
    bool active = true;

    struct Neko {
        constexpr static auto value = Object(
            "id", &ExplicitUser::id,
            "name", makeTags<rename_tag<"user_name">, serialization_ignore_tag>(&ExplicitUser::name),
            "active", &ExplicitUser::active
        );
    };
};

enum class Status {
    Pending,
    Active,
    Disabled
};

// Custom Tag definition using public macro
struct CustomSqlTag {
    bool primary_key = false;
    bool auto_increment = false;
};

namespace custom_tag_property {
NEKO_DEFINE_TAG_PROPERTY(bool, primary_key, PrimaryKey)
NEKO_DEFINE_TAG_PROPERTY(bool, auto_increment, AutoIncrement)
} // namespace custom_tag_property

struct TaggedUser {
    int id = 100;
    std::string name = "Admin";

    struct Neko {
        constexpr static auto value = Object(
            "id", makeTags<CustomSqlTag{.primary_key = true, .auto_increment = true}>(&TaggedUser::id),
            "name", &TaggedUser::name
        );
    };
};

// Compile-time static assertions for Concepts & Traits
static_assert(Reflectable<AggregateUser>);
static_assert(Reflectable<ExplicitUser>);
static_assert(Reflectable<TaggedUser>);
static_assert(Reflectable<Status>);
static_assert(!Reflectable<int*>);

static_assert(NamedReflectable<AggregateUser>);
static_assert(NamedReflectable<ExplicitUser>);
static_assert(NamedReflectable<TaggedUser>);
static_assert(!NamedReflectable<Status>);
static_assert(!NamedReflectable<std::tuple<int, float>>);

static_assert(TupleLike<std::tuple<int, std::string>>);
static_assert(!TupleLike<AggregateUser>);
static_assert(!TupleLike<int>);

static_assert(OptionalLike<std::optional<int>>);
static_assert(OptionalLike<std::optional<std::string>>);
static_assert(std::is_same_v<OptionalValueT<std::optional<int>>, int>);
static_assert(!OptionalLike<int>);

// Compile-time Member Pointer Reflection checks for Explicit Metadata
static_assert(Reflect<ExplicitUser>::indexOf<&ExplicitUser::id>() == 0);
static_assert(Reflect<ExplicitUser>::indexOf<&ExplicitUser::name>() == 1);
static_assert(Reflect<ExplicitUser>::indexOf<&ExplicitUser::active>() == 2);
static_assert(Reflect<ExplicitUser>::hasMember<&ExplicitUser::id>());
static_assert(Reflect<ExplicitUser>::hasMember<&ExplicitUser::name>());
static_assert(Reflect<ExplicitUser>::hasMember<&ExplicitUser::active>());
static_assert(!Reflect<ExplicitUser>::hasMember<&AggregateUser::id>());

static_assert(Reflect<ExplicitUser>::nameOf<&ExplicitUser::id>() == "id");
static_assert(Reflect<ExplicitUser>::nameOf<&ExplicitUser::name>() == "name");
static_assert(Reflect<ExplicitUser>::nameOf<&ExplicitUser::active>() == "active");

static_assert(std::is_same_v<Reflect<ExplicitUser>::FieldType<&ExplicitUser::id>, int>);
static_assert(std::is_same_v<Reflect<ExplicitUser>::FieldType<&ExplicitUser::name>, std::string>);
static_assert(std::is_same_v<Reflect<ExplicitUser>::FieldType<&ExplicitUser::active>, bool>);

// Compile-time Member Pointer Reflection checks for Aggregate Structs
static_assert(Reflect<AggregateUser>::indexOf<&AggregateUser::id>() == 0);
static_assert(Reflect<AggregateUser>::indexOf<&AggregateUser::name>() == 1);
static_assert(Reflect<AggregateUser>::indexOf<&AggregateUser::score>() == 2);
static_assert(Reflect<AggregateUser>::hasMember<&AggregateUser::id>());
static_assert(Reflect<AggregateUser>::hasMember<&AggregateUser::score>());
static_assert(!Reflect<AggregateUser>::hasMember<&ExplicitUser::id>());

static_assert(Reflect<AggregateUser>::nameOf<&AggregateUser::id>() == "id");
static_assert(Reflect<AggregateUser>::nameOf<&AggregateUser::name>() == "name");
static_assert(Reflect<AggregateUser>::nameOf<&AggregateUser::score>() == "score");

TEST(PublicApiTest, ConceptsAndTraits) {
    EXPECT_TRUE(is_reflectable_v<AggregateUser>);
    EXPECT_TRUE(has_named_reflection_v<AggregateUser>);
    EXPECT_FALSE(has_named_reflection_v<Status>);
    EXPECT_TRUE((TupleLike<std::tuple<int, double>>));
    EXPECT_FALSE(TupleLike<AggregateUser>);
    EXPECT_TRUE(is_optional_v<std::optional<int>>);
    EXPECT_FALSE(is_optional_v<int>);
}

TEST(PublicApiTest, MemberPointerReflection) {
    ExplicitUser u;
    EXPECT_EQ(Reflect<ExplicitUser>::nameOf<&ExplicitUser::id>(), "id");
    EXPECT_EQ(Reflect<ExplicitUser>::nameOf<&ExplicitUser::name>(), "name");
    EXPECT_EQ(Reflect<ExplicitUser>::nameOf<&ExplicitUser::active>(), "active");

    constexpr auto tag_name = Reflect<ExplicitUser>::tagOf<&ExplicitUser::name>();
    EXPECT_TRUE((tag_query::has<tag_property::Name>(tag_name)));
    EXPECT_EQ((tag_query::get<tag_property::Name>(tag_name)), "user_name");
    EXPECT_TRUE((tag_query::get<tag_property::Ignore>(tag_name)));

    constexpr auto tag_id = Reflect<ExplicitUser>::tagOf<&ExplicitUser::id>();
    EXPECT_FALSE((tag_query::has<tag_property::Name>(tag_id)));
}

TEST(PublicApiTest, TagVisitationAndCustomTags) {
    constexpr auto tags = Reflect<TaggedUser>::tagOf<&TaggedUser::id>();
    EXPECT_TRUE((tag_query::has<custom_tag_property::PrimaryKey>(tags)));
    EXPECT_TRUE((tag_query::get<custom_tag_property::PrimaryKey>(tags)));
    EXPECT_TRUE((tag_query::get<custom_tag_property::AutoIncrement>(tags)));
    EXPECT_TRUE((tag_query::hasTag<CustomSqlTag>(tags)));

    // Test forEachTag on TagList
    int visited_count = 0;
    forEachTag(tags, [&](const auto& tag) {
        visited_count++;
        using TagT = std::decay_t<decltype(tag)>;
        if constexpr (std::is_same_v<TagT, CustomSqlTag>) {
            EXPECT_TRUE(tag.primary_key);
            EXPECT_TRUE(tag.auto_increment);
        }
    });
    EXPECT_EQ(visited_count, 1);

    // Test forEachTag on single tag
    CustomSqlTag single_tag{.primary_key = true};
    int single_count = 0;
    forEachTag(single_tag, [&](const auto& tag) {
        single_count++;
        EXPECT_TRUE(tag.primary_key);
    });
    EXPECT_EQ(single_count, 1);

    // Test forEachTag on NoTags
    int no_tag_count = 0;
    forEachTag(NoTags{}, [&](const auto&) {
        no_tag_count++;
    });
    EXPECT_EQ(no_tag_count, 0);
}

TEST(PublicApiTest, ParserErrorAndSuccess) {
    auto err1 = makeParserError(sa::ErrorCode::InvalidType, "Test error");
    EXPECT_FALSE(err1);
    EXPECT_EQ(err1.error().msg, "Test error");

    auto ok1 = makeParserSuccess();
    EXPECT_TRUE(ok1);

    auto err2 = parsing::error(sa::ErrorCode::InvalidLength, "Length mismatch");
    EXPECT_FALSE(err2);
    EXPECT_EQ(err2.error().msg, "Length mismatch");

    auto ok2 = parsing::success();
    EXPECT_TRUE(ok2);
}

// Custom Dummy Backend to verify public OutputSerializerAdapter / InputSerializerAdapter
struct DummyBackend {
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = std::string;

    template <typename BufferT>
    struct OutputState {
        BufferT& buf;
        explicit OutputState(BufferT& b) : buf(b) {}
    };

    template <typename SourceT>
    struct InputState {
        SourceT src;
        explicit InputState(SourceT s) : src(std::move(s)) {}
    };

    template <typename BufferT, typename T>
    static auto write(OutputState<BufferT>& state, const T& val) -> sa::Result<void> {
        state.buf.push_back('X');
        return sa::success();
    }

    template <typename SourceT, typename T>
    static auto read(InputState<SourceT>& state, T& val) -> sa::Result<void> {
        return sa::success();
    }

    template <typename BufferT>
    static auto finish(OutputState<BufferT>&) -> sa::Result<void> {
        return sa::success();
    }

    template <typename SourceT>
    static auto finish(InputState<SourceT>&) -> sa::Result<void> {
        return sa::success();
    }
};

TEST(PublicApiTest, SerializerAdapters) {
    std::vector<char> out_buf;
    OutputSerializerAdapter<DummyBackend> out_serializer(out_buf);
    EXPECT_TRUE(out_serializer(123));
    EXPECT_TRUE(out_serializer.end());
    EXPECT_EQ(out_buf.size(), 1);
    EXPECT_EQ(out_buf[0], 'X');

    InputSerializerAdapter<DummyBackend> in_serializer(std::string("hello"));
    int val = 0;
    EXPECT_TRUE(in_serializer(val));
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
