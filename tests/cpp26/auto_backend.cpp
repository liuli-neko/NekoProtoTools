#include <nekoproto/reflect.hpp>
struct NativeOnly { int value; explicit NativeOnly(int v) : value(v) {} };
enum class Sparse { Small=1, Large=10000 };
static_assert(nekoproto::Reflectable<NativeOnly>);
static_assert(nekoproto::detail::ReflectProvider<NativeOnly>::provider_kind == nekoproto::detail::ReflectProviderKind::NativeReflection);
// auto migrates object discovery while preserving the legacy enum scan contract.
static_assert(nekoproto::Reflect<Sparse>::size() == 1);
int main() { NativeOnly v{3}; return nekoproto::Reflect<NativeOnly>::value<0>(v)==3 ? 0 : 1; }
