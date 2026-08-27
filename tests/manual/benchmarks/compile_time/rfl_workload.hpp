#pragma once

#include <rfl.hpp>
#include <optional>
#include <string>
#include <vector>

#ifndef NEKO_BENCH_FIELD_COUNT
#define NEKO_BENCH_FIELD_COUNT 64
#endif

namespace rfl_compile_bench {

enum class BenchEnum { Zero, One, Two, Three };

struct Nested {
    int code{};
    std::string text;
};

#if NEKO_BENCH_FIELD_COUNT == 4
struct BenchmarkStruct {
    int f00{};
    double f01{};
    std::string f02;
    std::optional<int> f03;
};
#elif NEKO_BENCH_FIELD_COUNT == 16
struct BenchmarkStruct {
    int f00{}; double f01{}; std::string f02; std::optional<int> f03;
    std::vector<int> f04; BenchEnum f05{}; Nested f06; int f07{};
    double f08{}; std::string f09; std::optional<int> f10; std::vector<int> f11;
    BenchEnum f12{}; Nested f13; int f14{}; double f15{};
};
#elif NEKO_BENCH_FIELD_COUNT == 32
struct BenchmarkStruct {
    int f00{}; double f01{}; std::string f02; std::optional<int> f03;
    std::vector<int> f04; BenchEnum f05{}; Nested f06; int f07{};
    double f08{}; std::string f09; std::optional<int> f10; std::vector<int> f11;
    BenchEnum f12{}; Nested f13; int f14{}; double f15{};
    std::string f16; std::optional<int> f17; std::vector<int> f18; BenchEnum f19{};
    Nested f20; int f21{}; double f22{}; std::string f23;
    std::optional<int> f24; std::vector<int> f25; BenchEnum f26{}; Nested f27;
    int f28{}; double f29{}; std::string f30; std::optional<int> f31;
};
#elif NEKO_BENCH_FIELD_COUNT == 64
struct BenchmarkStruct {
    int f00{}; double f01{}; std::string f02; std::optional<int> f03;
    std::vector<int> f04; BenchEnum f05{}; Nested f06; int f07{};
    double f08{}; std::string f09; std::optional<int> f10; std::vector<int> f11;
    BenchEnum f12{}; Nested f13; int f14{}; double f15{};
    std::string f16; std::optional<int> f17; std::vector<int> f18; BenchEnum f19{};
    Nested f20; int f21{}; double f22{}; std::string f23;
    std::optional<int> f24; std::vector<int> f25; BenchEnum f26{}; Nested f27;
    int f28{}; double f29{}; std::string f30; std::optional<int> f31;
    std::vector<int> f32; BenchEnum f33{}; Nested f34; int f35{};
    double f36{}; std::string f37; std::optional<int> f38; std::vector<int> f39;
    BenchEnum f40{}; Nested f41; int f42{}; double f43{};
    std::string f44; std::optional<int> f45; std::vector<int> f46; BenchEnum f47{};
    Nested f48; int f49{}; double f50{}; std::string f51;
    std::optional<int> f52; std::vector<int> f53; BenchEnum f54{}; Nested f55;
    int f56{}; double f57{}; std::string f58; std::optional<int> f59;
    std::vector<int> f60; BenchEnum f61{}; Nested f62; int f63{};
};
#else
#error "NEKO_BENCH_FIELD_COUNT must be 4, 16, 32, or 64"
#endif

} // namespace rfl_compile_bench

