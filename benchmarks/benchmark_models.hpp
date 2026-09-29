#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Shared business types. Competitor adapters add metadata without changing fields.
struct Address {
    std::string city;
    std::string street;
    int zip = 0;
    auto operator==(const Address&) const -> bool = default;
};

struct User {
    std::uint64_t id = 0;
    std::string name;
    std::vector<double> scores;
    std::optional<std::string> email;
    Address address;
    auto operator==(const User&) const -> bool = default;
};

inline auto next_random(std::uint64_t& state) -> std::uint64_t {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 0x2545F4914F6CDD1DULL;
}

inline auto make_user(std::uint64_t seed, std::size_t score_count) -> User {
    User user;
    user.id = next_random(seed);
    user.name = "user-" + std::to_string(next_random(seed) % 100000);
    user.address = {"city-" + std::to_string(next_random(seed) % 1000),
                    "street-" + std::to_string(next_random(seed) % 10000),
                    static_cast<int>(next_random(seed) % 100000)};
    if ((next_random(seed) & 1U) != 0) user.email = user.name + "@example.test";
    user.scores.reserve(score_count);
    for (std::size_t index = 0; index < score_count; ++index) {
        user.scores.push_back(static_cast<double>(next_random(seed) % 400) / 4.0);
    }
    return user;
}
