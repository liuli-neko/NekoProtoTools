#if defined(NEKO_BENCH_NEKO)
#include "nekoproto_adapter.hpp"
#elif defined(NEKO_BENCH_NEKO_SIMDJSON)
#include "nekoproto_simdjson_adapter.hpp"
#elif defined(NEKO_BENCH_RAPIDJSON_RAW)
#include "rapidjson_raw_adapter.hpp"
#elif defined(NEKO_BENCH_REFLECT_CPP)
#include "reflect_cpp_adapter.hpp"
#elif defined(NEKO_BENCH_GLAZE)
#include "glaze_adapter.hpp"
#else
#error Select exactly one JSON benchmark adapter
#endif

#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

using Clock = std::chrono::steady_clock;

struct BenchmarkCase {
    std::string_view name;
    std::size_t scores;
    std::uint64_t seed;
};

constexpr std::uint64_t base_seed = 0x4e454b4f20260400ULL;
constexpr std::array<BenchmarkCase, 4> cases{{
    {"small", 2, base_seed},
    {"medium", 32, base_seed},
    {"large", 256, base_seed},
    {"small_null_email", 2, base_seed + 4}
}};

auto find_case(std::string_view name) -> const BenchmarkCase* {
    for (const auto& item : cases) {
        if (item.name == name) return &item;
    }
    return nullptr;
}

auto load_json(const std::string& filename, BenchmarkAdapter::Buffer& bytes) -> bool {
    std::ifstream input(filename, std::ios::binary);
    if (!input) return false;
    const std::string raw(std::istreambuf_iterator<char>{input}, {});
    bytes.assign(raw.begin(), raw.end());
    return !bytes.empty();
}

auto run_case(const BenchmarkCase& item, int iterations, std::string_view fixture_dir) -> bool {
    const User source = make_user(item.seed, item.scores);
    BenchmarkAdapter::Buffer bytes;
    User decoded;
    if (!BenchmarkAdapter::encode(source, bytes) ||
        !BenchmarkAdapter::decode(bytes, decoded) || decoded != source) return false;

    BenchmarkAdapter::Buffer input_bytes = bytes;
    if (!fixture_dir.empty()) {
        const std::string filename = std::string(fixture_dir) + "/" + std::string(item.name) + ".json";
        if (!load_json(filename, input_bytes)) return false;
    }
    if (!BenchmarkAdapter::decode(input_bytes, decoded) || decoded != source) return false;

    for (int index = 0; index < 100; ++index) {
        BenchmarkAdapter::Buffer warm_bytes;
        User warm;
        if (!BenchmarkAdapter::encode(source, warm_bytes) ||
            !BenchmarkAdapter::decode(input_bytes, warm) || warm != source) return false;
    }

    std::uint64_t checksum = 0;
    const auto write_start = Clock::now();
    for (int index = 0; index < iterations; ++index) {
        BenchmarkAdapter::Buffer result;
        if (!BenchmarkAdapter::encode(source, result)) return false;
        checksum += result.size();
    }
    const auto write_end = Clock::now();
    if (!BenchmarkAdapter::decode(bytes, decoded) || decoded != source) return false;

    const auto read_start = Clock::now();
    for (int index = 0; index < iterations; ++index) {
        User value;
        if (!BenchmarkAdapter::decode(input_bytes, value)) return false;
        checksum += value.id;
    }
    const auto read_end = Clock::now();
    if (!BenchmarkAdapter::decode(input_bytes, decoded) || decoded != source) return false;

    const auto write_ns = std::chrono::duration<double, std::nano>(write_end - write_start).count();
    const auto read_ns = std::chrono::duration<double, std::nano>(read_end - read_start).count();
    const auto write_mb_s = static_cast<double>(bytes.size()) * iterations * 1000.0 / write_ns;
    const auto read_mb_s = static_cast<double>(input_bytes.size()) * iterations * 1000.0 / read_ns;
    std::cout << item.name << ',' << BenchmarkAdapter::library << ',' << BenchmarkAdapter::backend
              << ",serialize," << iterations << ',' << bytes.size() << ','
              << write_ns / iterations << ',' << write_mb_s << '\n';
    std::cout << item.name << ',' << BenchmarkAdapter::library << ',' << BenchmarkAdapter::backend
              << ",deserialize," << iterations << ',' << input_bytes.size() << ','
              << read_ns / iterations << ',' << read_mb_s << '\n';
    std::cerr << item.name << ' ' << BenchmarkAdapter::library << " checksum=" << checksum << '\n';
    return true;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string_view(argv[1]) == "--emit") {
        if (argc != 3) return 2;
        const auto* item = find_case(argv[2]);
        if (!item) return 2;
        BenchmarkAdapter::Buffer bytes;
        if (!BenchmarkAdapter::encode(make_user(item->seed, item->scores), bytes)) return 1;
        std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return 0;
    }
    if (argc >= 2 && std::string_view(argv[1]) == "--check-json") {
        if (argc != 4) return 2;
        const auto* item = find_case(argv[2]);
        if (!item) return 2;
        BenchmarkAdapter::Buffer bytes;
        User decoded;
        return load_json(argv[3], bytes) && BenchmarkAdapter::decode(bytes, decoded) &&
                       decoded == make_user(item->seed, item->scores) ? 0 : 1;
    }

    int iterations = 10000;
    std::string_view fixture_dir;
    if (argc >= 2) {
        iterations = std::atoi(argv[1]);
        if (iterations <= 0) return 2;
    }
    if (argc == 3) fixture_dir = argv[2];
    else if (argc > 3) return 2;

    std::cout << "case,library,backend,operation,iterations,json_bytes,ns_per_op,mb_per_second\n";
    for (const auto& item : cases) {
        if (!run_case(item, iterations, fixture_dir)) {
            std::cerr << "Correctness check failed; no performance result is valid.\n";
            return 1;
        }
    }
}
