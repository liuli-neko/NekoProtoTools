#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#ifdef NEKO_TEST_TYPE_DETECTION_MODULE
import nekoproto.reflect;
#else
#include <nekoproto/reflect.hpp>
#endif

struct Incomplete;
union IncompleteUnion;
union Union { int integer; double real; };
enum class Enum { Value };
enum class OpaqueEnum : unsigned;
struct Aggregate { int id; double score; static void function(); void method(); };
struct Empty {};
struct NonAggregate { explicit NonAggregate(int); int value; };
struct Private { private: int value; };
struct Abstract { virtual void call() = 0; };
struct DeletedConstructor { DeletedConstructor() = delete; int value; };
struct ReferenceMember { int& value; };
struct ArrayMember { int values[2]; };
struct BitField { unsigned value : 1; };
struct Derived : Aggregate { int value; };
struct AnonymousUnion { union { int integer; float real; }; };
struct InvalidLocal { struct Neko {}; int value; };
struct InvalidMeta { int value; };
template <> struct nekoproto::Meta<InvalidMeta> { static constexpr int value = 0; };
struct Explicit { int value; };
template <> struct nekoproto::Meta<Explicit> {
    static constexpr auto value = nekoproto::Object("value", &Explicit::value);
};
struct Local { int value; struct Neko {
    static constexpr auto value = nekoproto::Object("value", &Local::value);
}; };

// Each entry checks both the concept and its public variable-template spelling.
// A regression is a compile failure even when the expected result is false.
template <typename T, bool Reflects = false, bool Named = false, bool Tuple = false, bool Optional = false>
consteval bool check() {
    static_assert(nekoproto::Reflectable<T> == Reflects);
    static_assert(nekoproto::is_reflectable_v<T> == Reflects);
    static_assert(nekoproto::NamedReflectable<T> == Named);
    static_assert(nekoproto::has_named_reflection_v<T> == Named);
    static_assert(nekoproto::TupleLike<T> == Tuple);
    static_assert(nekoproto::is_tuple_v<T> == Tuple);
    static_assert(nekoproto::OptionalLike<T> == Optional);
    static_assert(nekoproto::is_optional_v<T> == Optional);
    static_assert(nekoproto::traits::OptionalLikeType<T>::value == Optional);
    static_assert(!nekoproto::is_tag_list_v<T>);
    static_assert(!nekoproto::is_tagged_field_v<T>);
    static_assert(!nekoproto::IsFlatTag<T>::value);
    static_assert(!nekoproto::IsUnframedTag<T>::value);
    // Exercise the common traits as well, including on incomplete/function types.
    [[maybe_unused]] constexpr bool common_queries[] = {
        nekoproto::traits::is_string_like_v<T>, nekoproto::traits::is_scalar_like_v<T>,
        nekoproto::traits::is_collection_like_v<T>, nekoproto::traits::is_known_collection_v<T>,
        nekoproto::traits::HasMonostate<std::remove_cvref_t<T>>::value,
        nekoproto::detail::can_unwrap_v<T>};
    return true;
}

template <typename T, bool Reflects = false, bool Named = false, bool Tuple = false, bool Optional = false>
consteval bool qualified() {
    return check<T, Reflects, Named, Tuple, Optional>() &&
           check<const T, Reflects, Named, Tuple, Optional>() &&
           check<volatile T, Reflects, Named, Tuple, Optional>() &&
           check<const volatile T, Reflects, Named, Tuple, Optional>() &&
           check<std::add_lvalue_reference_t<T>, Reflects, Named, Tuple, Optional>() &&
           check<std::add_rvalue_reference_t<T>, Reflects, Named, Tuple, Optional>() &&
           check<std::add_lvalue_reference_t<const T>, Reflects, Named, Tuple, Optional>() &&
           check<std::add_lvalue_reference_t<volatile T>, Reflects, Named, Tuple, Optional>() &&
           check<std::add_lvalue_reference_t<const volatile T>, Reflects, Named, Tuple, Optional>() &&
           check<std::add_rvalue_reference_t<const T>, Reflects, Named, Tuple, Optional>() &&
           check<std::add_rvalue_reference_t<volatile T>, Reflects, Named, Tuple, Optional>() &&
           check<std::add_rvalue_reference_t<const volatile T>, Reflects, Named, Tuple, Optional>();
}

template <typename... Ts>
consteval bool rejected() { return (qualified<Ts>() && ...); }

static_assert(rejected<void, bool, char, signed char, unsigned char, wchar_t, char8_t, char16_t, char32_t,
                       short, unsigned short, int, unsigned int, long, unsigned long, long long,
                       unsigned long long, float, double, long double, std::nullptr_t>());
static_assert(rejected<void*, const void*, int*, const int*, Incomplete*, IncompleteUnion*,
                       int Aggregate::*, void (Aggregate::*)(), int (Aggregate::*)() const & noexcept>());
static_assert(rejected<void(), int(int), void(...) noexcept, int(double, ...) noexcept,
                       void(*)(), int(*)(int) noexcept, void() const, int() volatile && noexcept>());
static_assert(rejected<int[3], const int[2][3], int[], Incomplete[], Union, Incomplete, IncompleteUnion,
                       std::array<int, 0>, std::array<int, 3>, std::vector<int>, std::string,
                       Private, InvalidLocal, InvalidMeta>());
static_assert(qualified<Enum, true>() && qualified<OpaqueEnum, true>());
static_assert(qualified<Aggregate, true, true>() && qualified<Empty, true, true>());
static_assert(qualified<Explicit, true, true>() && qualified<Local, true, true>());
static_assert(qualified<std::tuple<>, false, false, true>());
static_assert(qualified<std::tuple<int, std::string>, false, false, true>());
static_assert(qualified<std::optional<int>, false, false, false, true>());
static_assert(qualified<std::optional<std::string>, false, false, false, true>());
static_assert(std::is_same_v<nekoproto::OptionalValueT<void>, void>);
static_assert(std::is_same_v<nekoproto::OptionalValueT<const std::optional<int>&>, int>);
static_assert(!nekoproto::traits::is_string_like_v<Incomplete>);
static_assert(!nekoproto::traits::is_string_like_v<IncompleteUnion>);
static_assert(!nekoproto::traits::is_scalar_like_v<void>);
static_assert(!nekoproto::traits::is_collection_like_v<void>);
static_assert(nekoproto::traits::is_string_like_v<const char*>);
static_assert(nekoproto::traits::is_scalar_like_v<const std::optional<int>&>);
static_assert(nekoproto::traits::is_known_collection_v<const std::vector<int>&>);

// Complex aggregates must remain safe to probe without instantiating legacy
// structured bindings (which cannot decompose every aggregate shape).
template <typename... Ts>
consteval bool probe() {
    return ((nekoproto::Reflectable<Ts> == nekoproto::is_reflectable_v<Ts> &&
             nekoproto::NamedReflectable<Ts> == nekoproto::has_named_reflection_v<Ts>) && ...);
}
static_assert(probe<NonAggregate, DeletedConstructor, Abstract, ReferenceMember, ArrayMember, BitField, Derived, AnonymousUnion>());

// The detectors must compose with overload resolution and requires expressions.
template <nekoproto::NamedReflectable T> constexpr bool dispatch() { return true; }
template <typename T> constexpr bool dispatch() { return false; }
static_assert(!dispatch<void>() && !dispatch<Incomplete>() && dispatch<Explicit>());

static_assert(!nekoproto::Reflect<void>::hasMember<&Aggregate::id>());
static_assert(!nekoproto::Reflect<int>::hasMember<&Aggregate::id>());
static_assert(nekoproto::Reflect<void>::size() == 0);
static_assert(nekoproto::Reflect<void>::names().empty());
static_assert(!nekoproto::Reflect<Explicit>::hasMember<0>());
static_assert(!nekoproto::Reflect<Explicit>::hasMember<nullptr>());
static_assert(!nekoproto::Reflect<Explicit>::hasMember<&Aggregate::function>());
static_assert(!nekoproto::Reflect<Explicit>::hasMember<&Aggregate::method>());
static_assert(!nekoproto::Reflect<Explicit>::hasMember<&Aggregate::id>());
static_assert(nekoproto::Reflect<Explicit>::hasMember<&Explicit::value>());
static_assert(!nekoproto::Reflect<Aggregate>::hasMember<0>());
static_assert(!nekoproto::Reflect<Aggregate>::hasMember<nullptr>());
static_assert(!nekoproto::Reflect<Aggregate>::hasMember<&Aggregate::function>());
static_assert(!nekoproto::Reflect<Aggregate>::hasMember<&Aggregate::method>());
static_assert(!nekoproto::Reflect<Aggregate>::hasMember<&Explicit::value>());
static_assert(nekoproto::Reflect<Aggregate>::hasMember<&Aggregate::id>());

using Tags = nekoproto::TagList<nekoproto::rename_tag<"field">>;
using Tagged = decltype(nekoproto::makeTags<nekoproto::rename_tag<"field">>(&Aggregate::id));
static_assert(nekoproto::is_tag_list_v<Tags> && nekoproto::IsTagList<Tags>::value);
static_assert(nekoproto::is_tagged_field_v<Tagged> && nekoproto::is_tagged_field_v<const Tagged&>);
static_assert(nekoproto::is_optional_v<std::variant<std::monostate, int>>);
static_assert(!nekoproto::OptionalLike<std::variant<std::monostate, int>>);

int main() {}
