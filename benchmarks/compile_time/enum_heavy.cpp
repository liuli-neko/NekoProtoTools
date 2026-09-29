#include "nekoproto/serialization/reflection.hpp"

#include <array>

namespace neko_compile_bench {

#define NEKO_BENCH_ENUM(N)                                                                                             \
    enum class Enum##N { Zero, One, Two, Three, Four, Five, Six, Seven }
NEKO_BENCH_ENUM(0);
NEKO_BENCH_ENUM(1);
NEKO_BENCH_ENUM(2);
NEKO_BENCH_ENUM(3);
NEKO_BENCH_ENUM(4);
NEKO_BENCH_ENUM(5);
NEKO_BENCH_ENUM(6);
NEKO_BENCH_ENUM(7);
#undef NEKO_BENCH_ENUM

enum class ExplicitEnum0 { Zero, One, Two, Three };
enum class ExplicitEnum1 { Zero, One, Two, Three };

} // namespace neko_compile_bench

namespace nekoproto {
template <>
struct Meta<neko_compile_bench::ExplicitEnum0> {
    using T                     = neko_compile_bench::ExplicitEnum0;
    static constexpr auto value = Enumerate{"Zero", T::Zero, "One", T::One, "Two", T::Two, "Three", T::Three};
};
template <>
struct Meta<neko_compile_bench::ExplicitEnum1> {
    using T                     = neko_compile_bench::ExplicitEnum1;
    static constexpr auto value = Enumerate{"Zero", T::Zero, "One", T::One, "Two", T::Two, "Three", T::Three};
};
} // namespace nekoproto

template <typename T>
constexpr std::size_t reflect_enum() {
    return nekoproto::Reflect<T>::names().size() + nekoproto::Reflect<T>::values().size();
}

static_assert(reflect_enum<neko_compile_bench::Enum0>() == 16);
static_assert(reflect_enum<neko_compile_bench::Enum1>() == 16);
static_assert(reflect_enum<neko_compile_bench::Enum2>() == 16);
static_assert(reflect_enum<neko_compile_bench::Enum3>() == 16);
static_assert(reflect_enum<neko_compile_bench::Enum4>() == 16);
static_assert(reflect_enum<neko_compile_bench::Enum5>() == 16);
static_assert(reflect_enum<neko_compile_bench::Enum6>() == 16);
static_assert(reflect_enum<neko_compile_bench::Enum7>() == 16);
static_assert(reflect_enum<neko_compile_bench::ExplicitEnum0>() == 8);
static_assert(reflect_enum<neko_compile_bench::ExplicitEnum1>() == 8);
