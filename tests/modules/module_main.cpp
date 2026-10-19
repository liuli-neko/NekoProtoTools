#include <string>
#include <tuple>
#include <type_traits>
import nekoproto.reflect;
#include <nekoproto/reflection/tag_macros.hpp>
#include "module_shared.hpp"

struct Imported { int id; };
template <> struct nekoproto::Meta<Imported> {
    static constexpr auto value = nekoproto::Object("imported_id", &Imported::id);
};
namespace custom_property {
NEKO_DEFINE_TAG_PROPERTY(bool, enabled, Enabled)
}
struct CustomTag { bool enabled; };
static_assert(nekoproto::Reflect<Imported>::names()[0] == "imported_id");
static_assert(nekoproto::Reflect<Imported>::indexOf<&Imported::id>() == 0);
static_assert(std::tuple_size_v<SharedTags> == 1);
static_assert(nekoproto::tag_query::get<nekoproto::tag_property::Name>(std::get<0>(SharedTags{})) == "shared_value");
static_assert(nekoproto::tag_query::get<custom_property::Enabled>(CustomTag{true}));
#ifdef NEKO_TEST_NATIVE
struct ImportedAnnotation { int value; };
struct AnnotatedAfterImport {
    [[=ImportedAnnotation{1}, =42, =nekoproto::ParserTag{.skippable=false}]] int id;
};
constexpr auto imported_tags = nekoproto::Reflect<AnnotatedAfterImport>::tagOf<&AnnotatedAfterImport::id>();
static_assert(imported_tags.size() == 3);
static_assert(imported_tags.get<0>().value == 1);
static_assert(imported_tags.get<1>() == 42);
static_assert(!nekoproto::tag_query::get<nekoproto::tag_property::Skippable>(imported_tags));
struct Annotated { [[=nekoproto::rename_tag<"native_id">]] int id; };
static_assert(nekoproto::FieldDescriptor<Annotated, 0>::name == "native_id");
enum class ImportedEnum { Large=10000, Alias=Large };
#if NEKO_PROTO_REFLECTION_BACKEND == 1
static_assert(nekoproto::Reflect<ImportedEnum>::size() == 2);
static_assert(nekoproto::Reflect<ImportedEnum>::values()[1] == ImportedEnum::Large);
#endif
#endif
int main() {
    Shared shared{3};
    SharedTags tags;
    auto [tag] = tags;
    if (nekoproto::tag_query::get<nekoproto::tag_property::Name>(tag) != "shared_value") return 1;
    if (header_increment(shared, tags) != 4 || shared.value != 4) return 2;
    Imported value{7};
    nekoproto::Reflect<Imported>::forEach(value, [](int& id) { ++id; });
    return value.id == 8 ? 0 : 3;
}
