#include <nekoproto/reflect.hpp>
#include <array>
#include <string>
#include <tuple>
#include <type_traits>

struct User {
    [[=nekoproto::rename_tag<"user_id">, =nekoproto::ParserTag{.skippable = false}]] int id;
    std::string name;
    User(int value, std::string text) : id(value), name(text) {}
};
struct Empty {};
struct ArrayField { int values[3]; };
struct ReferenceField { int& value; };
struct ConstField { const int value; };
struct ExplicitArray { int values[3]; };
template <> struct nekoproto::Meta<ExplicitArray> {
    static constexpr auto value = nekoproto::Object("values", &ExplicitArray::values);
};
static_assert(std::is_same_v<nekoproto::detail::NativeReflectionProvider<ArrayField>::field_type<0>, int*>);
static_assert(std::is_same_v<nekoproto::detail::LegacyReflectionProvider<ExplicitArray>::field_type<0>, int*>);
static_assert(std::is_same_v<decltype(nekoproto::Reflect<ArrayField>::value<0>(std::declval<ArrayField&>())), int (&)[3]>);
static_assert(std::is_same_v<nekoproto::detail::NativeReflectionProvider<ConstField>::field_type<0>, int>);
static_assert(std::is_same_v<decltype(nekoproto::Reflect<ConstField>::value<0>(std::declval<ConstField&>())), const int&>);
struct ZeroTag { [[=nekoproto::BinaryTag{.fixed_length=0}]] int value; };
static_assert(nekoproto::tag_query::has<nekoproto::tag_property::FixedLength<void>>(nekoproto::Reflect<ZeroTag>::tagOf<&ZeroTag::value>()));
static_assert(nekoproto::tag_query::get<nekoproto::tag_property::FixedLength<int>>(nekoproto::Reflect<ZeroTag>::tagOf<&ZeroTag::value>()) == 0);
static_assert(nekoproto::Reflect<ArrayField>::indexOf<&ArrayField::values>() == 0);
struct CustomAnnotation { int value; };
struct AnnotatedValues {
    [[=42, =nekoproto::ConstexprString{"custom"}, =CustomAnnotation{1},
      =nekoproto::ParserTag{.skippable=false}]] int value;
};
constexpr auto custom_tags = nekoproto::Reflect<AnnotatedValues>::tagOf<&AnnotatedValues::value>();
static_assert(custom_tags.size() == 4);
static_assert(custom_tags.get<0>() == 42);
static_assert(custom_tags.get<1>().view() == "custom");
static_assert(custom_tags.get<2>().value == 1);
static_assert(nekoproto::tag_query::has<nekoproto::tag_property::Skippable>(custom_tags));
static_assert(!nekoproto::tag_query::get<nekoproto::tag_property::Skippable>(custom_tags));
struct Private { private: int value; };
struct Base { int value; };
struct Derived : Base { int other; };
struct Bits { unsigned bit : 1; };
union Union { int value; };
struct AnonymousUnion { union { int value; float other; }; int named; };
static_assert(!nekoproto::detail::nativeObjectEligible<Private>());
static_assert(!nekoproto::detail::nativeObjectEligible<Derived>());
static_assert(!nekoproto::detail::nativeObjectEligible<Bits>());
static_assert(!nekoproto::detail::nativeObjectEligible<Union>());
static_assert(!nekoproto::detail::nativeObjectEligible<AnonymousUnion>());
struct Explicit {
    [[=nekoproto::rename_tag<"ignored_native_name">]] int value;
};
template <> struct nekoproto::Meta<Explicit> {
    static constexpr auto value = nekoproto::Object("custom", &Explicit::value);
};
static_assert(nekoproto::Reflectable<User> && nekoproto::NamedReflectable<User>);
static_assert(nekoproto::Reflect<User>::value_count == 2);
static_assert(nekoproto::Reflect<User>::indexOf<&User::id>() == 0);
static_assert(nekoproto::Reflect<User>::nameOf<&User::name>() == "name");
static_assert(nekoproto::FieldDescriptor<User, 0>::name == "user_id");
static_assert(nekoproto::tag_query::has<nekoproto::tag_property::Skippable>(nekoproto::Reflect<User>::tagOf<&User::id>()));
static_assert(!nekoproto::tag_query::get<nekoproto::tag_property::Skippable>(nekoproto::Reflect<User>::tagOf<&User::id>()));
static_assert(nekoproto::Reflect<Empty>::value_count == 0);
static_assert(nekoproto::Reflect<ArrayField>::value_count == 1);
static_assert(nekoproto::Reflect<ReferenceField>::value_count == 1);
static_assert(nekoproto::Reflect<Explicit>::names()[0] == "custom");
static_assert(!nekoproto::tag_query::has<nekoproto::tag_property::Name>(nekoproto::Reflect<Explicit>::tagOf<&Explicit::value>()));
static_assert(std::is_same_v<decltype(nekoproto::Reflect<User>::value<0>(std::declval<const User&>())), const int&>);

int main() {
    Empty empty;
    bool called = false;
    if (!nekoproto::Reflect<Empty>::forEachWhile(empty, [&](auto&&...) { called = true; return false; }) || called) return 4;
    User user{3, "Neko"};
    int count = 0;
    bool completed = nekoproto::Reflect<User>::forEachWhile(user,
        [&](auto, auto&, auto, const auto&) { ++count; return false; });
    if (completed || count != 1) return 1;
    auto result = nekoproto::Reflect<User>::forEachNamed(user, [](auto& value, auto name) { return name.size(); });
    if (std::get<0>(result) != 2 || std::get<1>(result) != 4) return 2;
    nekoproto::Reflect<User>::value<0>(user) = 7;
    int value = 1;
    ReferenceField reference{value};
    nekoproto::Reflect<ReferenceField>::value<0>(reference) = 4;
    return user.id == 7 && value == 4 ? 0 : 3;
}
