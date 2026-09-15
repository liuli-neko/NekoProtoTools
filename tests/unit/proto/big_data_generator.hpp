#pragma once

#include <cstddef>
#include <map>
#include <random>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace nekoproto::test {

/**
 * @brief Deterministic pseudo-random generator with fixed seed.
 * Guarantees cross-platform reproducible test data without external files.
 */
struct DataGenerator {
    std::mt19937_64 rng;
    std::uniform_int_distribution<int> int_dist{-2147483648, 2147483647};
    std::uniform_real_distribution<double> double_dist{-1e10, 1e10};

    explicit DataGenerator(uint64_t seed = 42) : rng(seed) {}

    int genInt() { return int_dist(rng); }
    double genDouble() { return double_dist(rng); }
    bool genBool() { return (rng() % 2) == 0; }

    std::string genString(size_t len = 10) {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::string s(len, ' ');
        for (size_t i = 0; i < len; ++i) {
            s[i] = chars[rng() % (sizeof(chars) - 1)];
        }
        return s;
    }

    std::vector<int> genIntVector(size_t len = 10) {
        std::vector<int> vec(len);
        for (auto& v : vec) {
            v = genInt();
        }
        return vec;
    }

    std::map<std::string, int> genStringIntMap(size_t len = 10) {
        std::map<std::string, int> m;
        for (size_t i = 0; i < len; ++i) {
            m[genString()] = genInt();
        }
        return m;
    }
};

template <typename T>
void fillRandom(T& val, DataGenerator& gen);

inline void fillRandom(int& v, DataGenerator& gen) { v = gen.genInt(); }
inline void fillRandom(double& v, DataGenerator& gen) { v = gen.genDouble(); }
inline void fillRandom(bool& v, DataGenerator& gen) { v = gen.genBool(); }
inline void fillRandom(std::string& v, DataGenerator& gen) { v = gen.genString(); }
inline void fillRandom(std::vector<int>& v, DataGenerator& gen) { v = gen.genIntVector(); }
inline void fillRandom(std::map<std::string, int>& v, DataGenerator& gen) { v = gen.genStringIntMap(); }

struct CerealFillArchive {
    DataGenerator& gen;
    template <typename... Args>
    void operator()(Args&&... args) {
        (process(args), ...);
    }
    template <typename Nvp>
    void process(Nvp&& nvp) {
        fillRandom(const_cast<std::remove_reference_t<decltype(nvp.value)>&>(nvp.value), gen);
    }
};

template <typename T>
void fillRandom(T& val, DataGenerator& gen) {
    if constexpr (requires { val.nekoMemberTuple(); }) {
        std::apply([&gen](auto&... fields) {
            (fillRandom(fields, gen), ...);
        }, val.nekoMemberTuple());
    } else if constexpr (requires { val.serialize(std::declval<CerealFillArchive&>()); }) {
        CerealFillArchive ar{gen};
        val.serialize(ar);
    }
}

} // namespace nekoproto::test
