#include <nekoproto/reflect.hpp>
#include <string>
#include <type_traits>
#include <utility>

struct Aggregate { int id; double score; };
struct Explicit { int id; std::string name; };
template <> struct nekoproto::Meta<Explicit> {
    static constexpr auto value = nekoproto::Object(
        "key", &Explicit::id,
        "label", nekoproto::makeTags<nekoproto::rename_tag<"display_name">>(&Explicit::name));
};
static_assert(nekoproto::Reflectable<Aggregate>);
static_assert(nekoproto::NamedReflectable<Explicit>);
static_assert(nekoproto::Reflect<Aggregate>::value_count == 2);
static_assert(nekoproto::Reflect<Aggregate>::names()[0] == "id");
static_assert(nekoproto::Reflect<Aggregate>::indexOf<&Aggregate::score>() == 1);
static_assert(nekoproto::Reflect<Explicit>::nameOf<&Explicit::id>() == "key");
static_assert(nekoproto::FieldDescriptor<Explicit, 1>::name == "display_name");
static_assert(std::is_same_v<nekoproto::Reflect<Explicit>::FieldType<&Explicit::name>, std::string>);
static_assert(nekoproto::detail::ReflectProvider<Explicit>::provider_kind ==
              nekoproto::detail::ReflectProviderKind::ExplicitMetadata);
int main() {
    Aggregate object{3, 1.0};
    int calls = 0;
    bool complete = nekoproto::Reflect<Aggregate>::forEachWhile(
        object, [&](auto, auto&, auto, const auto&) { ++calls; return false; });
    if (complete || calls != 1) return 1;
    Explicit explicit_object{5, "Neko"};
    nekoproto::Reflect<Explicit>::forEach(explicit_object, [](auto& field) {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(field)>, int>) ++field;
    });
    return explicit_object.id == 6 && explicit_object.name == "Neko" ? 0 : 2;
}
