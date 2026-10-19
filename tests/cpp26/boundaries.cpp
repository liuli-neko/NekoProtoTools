#ifdef NEKO_TEST_GENERATED_BINDING
#undef NEKO_USE_STRUCTURED_BINDING_PACKS
#undef NEKO_USE_NATIVE_REFLECTION
#define NEKO_USE_STRUCTURED_BINDING_PACKS 0
#define NEKO_USE_NATIVE_REFLECTION 0
#endif
#include <nekoproto/reflect.hpp>
#define F4(P) int P##a=0; int P##b=0; int P##c=0; int P##d=0;
#define F16(P) F4(P##a) F4(P##b) F4(P##c) F4(P##d)
#define F64(P) F16(P##a) F16(P##b) F16(P##c) F16(P##d)
#ifndef NEKO_TEST_FIELD_COUNT
#define NEKO_TEST_FIELD_COUNT 64
#endif
struct Boundary {
    F64(first)
#if NEKO_TEST_FIELD_COUNT >= 128
    F64(second)
#endif
#if NEKO_TEST_FIELD_COUNT == 65 || NEKO_TEST_FIELD_COUNT == 129
    int last=0;
#endif
};
using R = nekoproto::Reflect<Boundary>;
static_assert(R::value_count == NEKO_TEST_FIELD_COUNT);
static_assert(std::tuple_size_v<R::value_types> == NEKO_TEST_FIELD_COUNT);
static_assert(std::is_same_v<decltype(R::value<0>(std::declval<const Boundary&>())), const int&>);
int main() {
    Boundary b;
    int count=0;
    bool done=R::forEachWhile(b, [&](auto i, int& v, auto name, auto) {
        ++count; v=int(i)+1; return !name.empty();
    });
    if (!done || count != NEKO_TEST_FIELD_COUNT || R::value<NEKO_TEST_FIELD_COUNT-1>(b) != count) return 1;
    count=0;
    done=R::forEachWhile(b, [&](auto i, auto&, auto, auto) { ++count; return i<63; });
    return !done && count==64 ? 0 : 2;
}
