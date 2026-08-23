#pragma once

#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

#include <optional>
#include <string>
#include <vector>

#ifndef NEKO_BENCH_FIELD_COUNT
#define NEKO_BENCH_FIELD_COUNT 64
#endif

#ifndef NEKO_BENCH_TAG_DENSITY
#define NEKO_BENCH_TAG_DENSITY 0
#endif

namespace neko_compile_bench {

struct CompileTagA {
    template <typename T, auto Tags>
    static consteval bool constexprCheck() {
        return true;
    }
};

struct CompileTagB {
    template <typename T, auto Tags>
    static consteval bool constexprCheck() {
        return true;
    }
};

struct CompileTagC {
    template <typename T, auto Tags>
    static consteval bool constexprCheck() {
        return true;
    }
};

struct CompileTagD {
    template <typename T, auto Tags>
    static consteval bool constexprCheck() {
        return true;
    }
};

enum class BenchEnum { Zero, One, Two, Three };

struct Nested {
    int code{};
    std::string text;

    NEKO_SERIALIZER(code, text)
};

} // namespace neko_compile_bench

namespace nekoproto {
template <>
struct Meta<neko_compile_bench::BenchEnum> {
    using T                     = neko_compile_bench::BenchEnum;
    static constexpr auto value = Enumerate{"Zero", T::Zero, "One", T::One, "Two", T::Two, "Three", T::Three};
};
} // namespace nekoproto

namespace neko_compile_bench {

#if NEKO_BENCH_TAG_DENSITY == 0
#define NEKO_BENCH_FIELD(FIELD) FIELD
#elif NEKO_BENCH_TAG_DENSITY == 1
#define NEKO_BENCH_FIELD(FIELD) (nekoproto::makeTags<CompileTagA{}>(FIELD))
#elif NEKO_BENCH_TAG_DENSITY == 2
#define NEKO_BENCH_FIELD(FIELD)                                                                                        \
    (nekoproto::makeTags<CompileTagA{}, CompileTagB{}, CompileTagC{}, CompileTagD{}>(FIELD))
#else
#error "NEKO_BENCH_TAG_DENSITY must be 0, 1, or 2"
#endif

#define NEKO_BENCH_ARGS_4 NEKO_BENCH_FIELD(f00), NEKO_BENCH_FIELD(f01), NEKO_BENCH_FIELD(f02), NEKO_BENCH_FIELD(f03)

#define NEKO_BENCH_ARGS_16                                                                                             \
    NEKO_BENCH_ARGS_4, NEKO_BENCH_FIELD(f04), NEKO_BENCH_FIELD(f05), NEKO_BENCH_FIELD(f06), NEKO_BENCH_FIELD(f07),     \
        NEKO_BENCH_FIELD(f08), NEKO_BENCH_FIELD(f09), NEKO_BENCH_FIELD(f10), NEKO_BENCH_FIELD(f11),                    \
        NEKO_BENCH_FIELD(f12), NEKO_BENCH_FIELD(f13), NEKO_BENCH_FIELD(f14), NEKO_BENCH_FIELD(f15)

#define NEKO_BENCH_ARGS_32                                                                                             \
    NEKO_BENCH_ARGS_16, NEKO_BENCH_FIELD(f16), NEKO_BENCH_FIELD(f17), NEKO_BENCH_FIELD(f18), NEKO_BENCH_FIELD(f19),    \
        NEKO_BENCH_FIELD(f20), NEKO_BENCH_FIELD(f21), NEKO_BENCH_FIELD(f22), NEKO_BENCH_FIELD(f23),                    \
        NEKO_BENCH_FIELD(f24), NEKO_BENCH_FIELD(f25), NEKO_BENCH_FIELD(f26), NEKO_BENCH_FIELD(f27),                    \
        NEKO_BENCH_FIELD(f28), NEKO_BENCH_FIELD(f29), NEKO_BENCH_FIELD(f30), NEKO_BENCH_FIELD(f31)

#define NEKO_BENCH_ARGS_64                                                                                             \
    NEKO_BENCH_ARGS_32, NEKO_BENCH_FIELD(f32), NEKO_BENCH_FIELD(f33), NEKO_BENCH_FIELD(f34), NEKO_BENCH_FIELD(f35),    \
        NEKO_BENCH_FIELD(f36), NEKO_BENCH_FIELD(f37), NEKO_BENCH_FIELD(f38), NEKO_BENCH_FIELD(f39),                    \
        NEKO_BENCH_FIELD(f40), NEKO_BENCH_FIELD(f41), NEKO_BENCH_FIELD(f42), NEKO_BENCH_FIELD(f43),                    \
        NEKO_BENCH_FIELD(f44), NEKO_BENCH_FIELD(f45), NEKO_BENCH_FIELD(f46), NEKO_BENCH_FIELD(f47),                    \
        NEKO_BENCH_FIELD(f48), NEKO_BENCH_FIELD(f49), NEKO_BENCH_FIELD(f50), NEKO_BENCH_FIELD(f51),                    \
        NEKO_BENCH_FIELD(f52), NEKO_BENCH_FIELD(f53), NEKO_BENCH_FIELD(f54), NEKO_BENCH_FIELD(f55),                    \
        NEKO_BENCH_FIELD(f56), NEKO_BENCH_FIELD(f57), NEKO_BENCH_FIELD(f58), NEKO_BENCH_FIELD(f59),                    \
        NEKO_BENCH_FIELD(f60), NEKO_BENCH_FIELD(f61), NEKO_BENCH_FIELD(f62), NEKO_BENCH_FIELD(f63)

#define NEKO_BENCH_APPLY_SERIALIZER(...) NEKO_SERIALIZER(__VA_ARGS__)

struct BenchmarkStruct {
    int f00{};
    double f01{};
    std::string f02;
    std::optional<int> f03;
    std::vector<int> f04;
    BenchEnum f05{};
    Nested f06;
    int f07{};
    double f08{};
    std::string f09;
    std::optional<int> f10;
    std::vector<int> f11;
    BenchEnum f12{};
    Nested f13;
    int f14{};
    double f15{};
    std::string f16;
    std::optional<int> f17;
    std::vector<int> f18;
    BenchEnum f19{};
    Nested f20;
    int f21{};
    double f22{};
    std::string f23;
    std::optional<int> f24;
    std::vector<int> f25;
    BenchEnum f26{};
    Nested f27;
    int f28{};
    double f29{};
    std::string f30;
    std::optional<int> f31;
    std::vector<int> f32;
    BenchEnum f33{};
    Nested f34;
    int f35{};
    double f36{};
    std::string f37;
    std::optional<int> f38;
    std::vector<int> f39;
    BenchEnum f40{};
    Nested f41;
    int f42{};
    double f43{};
    std::string f44;
    std::optional<int> f45;
    std::vector<int> f46;
    BenchEnum f47{};
    Nested f48;
    int f49{};
    double f50{};
    std::string f51;
    std::optional<int> f52;
    std::vector<int> f53;
    BenchEnum f54{};
    Nested f55;
    int f56{};
    double f57{};
    std::string f58;
    std::optional<int> f59;
    std::vector<int> f60;
    BenchEnum f61{};
    Nested f62;
    int f63{};

#if NEKO_BENCH_FIELD_COUNT == 4
    NEKO_BENCH_APPLY_SERIALIZER(NEKO_BENCH_ARGS_4)
#elif NEKO_BENCH_FIELD_COUNT == 16
    NEKO_BENCH_APPLY_SERIALIZER(NEKO_BENCH_ARGS_16)
#elif NEKO_BENCH_FIELD_COUNT == 32
    NEKO_BENCH_APPLY_SERIALIZER(NEKO_BENCH_ARGS_32)
#elif NEKO_BENCH_FIELD_COUNT == 64
    NEKO_BENCH_APPLY_SERIALIZER(NEKO_BENCH_ARGS_64)
#else
#error "NEKO_BENCH_FIELD_COUNT must be 4, 16, 32, or 64"
#endif
};

#undef NEKO_BENCH_APPLY_SERIALIZER
#undef NEKO_BENCH_ARGS_64
#undef NEKO_BENCH_ARGS_32
#undef NEKO_BENCH_ARGS_16
#undef NEKO_BENCH_ARGS_4
#undef NEKO_BENCH_FIELD

} // namespace neko_compile_bench
