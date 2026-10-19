#include <nekoproto/reflect.hpp>

struct FakeNative {
    int value;
    int other = 10;
    explicit FakeNative(int v) : value(v) {}
    FakeNative(const FakeNative&) = delete;
};
namespace nekoproto::detail {
template <> struct NativeReflectionProvider<FakeNative> {
    using context_type = FakeNative;
    static constexpr bool available = true, has_values = true, has_names = true;
    static constexpr auto provider_kind = ReflectProviderKind::NativeReflection;
    static constexpr std::size_t value_count = 2;
    static inline int bind_count = 0;
    template <std::size_t> using field_type = int;
    static constexpr auto names() { return std::array<std::string_view, 2>{"value", "other"}; }
    template <std::size_t I> static constexpr auto name() { return names()[I]; }
    template <std::size_t I> static constexpr auto tag() {
        if constexpr (I == 0) return normalize_tags_v<rename_tag<"fake_value">>;
        else return NoTags{};
    }
    static constexpr auto tags() { return std::tuple{tag<0>(), tag<1>()}; }
    template <typename U> struct Bound { U* object; };
    template <typename U> static auto bind(U& object) { ++bind_count; return Bound<U>{&object}; }
    template <std::size_t I, typename U> static auto getBound(Bound<U>& b) -> decltype(auto) {
        if constexpr (I == 0) return (b.object->value);
        else return (b.object->other);
    }
    template <std::size_t I, typename U> static auto get(U& object) -> decltype(auto) { auto b = bind(object); return getBound<I>(b); }
    template <auto Pointer> static consteval std::size_t memberIndex() {
        if constexpr (std::is_same_v<decltype(Pointer), int FakeNative::*>) {
            if (Pointer == &FakeNative::value) return 0;
            if (Pointer == &FakeNative::other) return 1;
        }
        return static_cast<std::size_t>(-1);
    }
};
}
static_assert(nekoproto::Reflectable<FakeNative> && nekoproto::NamedReflectable<FakeNative>);
static_assert(nekoproto::Reflect<FakeNative>::value_count == 2);
static_assert(nekoproto::FieldDescriptor<FakeNative, 0>::name == "fake_value");
static_assert(nekoproto::Reflect<FakeNative>::indexOf<&FakeNative::value>() == 0);
static_assert(std::is_same_v<nekoproto::Reflect<FakeNative>::FieldType<&FakeNative::value>, int>);
int main() {
    FakeNative object{7};
    nekoproto::Reflect<FakeNative>::forEach(object, [](int& v) { ++v; });
    if (nekoproto::detail::NativeReflectionProvider<FakeNative>::bind_count != 1) return 1;
    const FakeNative& read_only = object;
    int result = 0;
    nekoproto::Reflect<FakeNative>::forEach(read_only, [&](const int& v) { result += v; });
    if (result != 19 || nekoproto::detail::NativeReflectionProvider<FakeNative>::bind_count != 2) return 2;
    int visited = 0;
    auto completed = nekoproto::Reflect<FakeNative>::forEachWhile(object, [&](auto, int&, auto, auto) { ++visited; return false; });
    return !completed && visited == 1 && nekoproto::detail::NativeReflectionProvider<FakeNative>::bind_count == 3 ? 0 : 3;
}
