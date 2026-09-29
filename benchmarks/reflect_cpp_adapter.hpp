#pragma once

#include "benchmark_models.hpp"
#include <rfl/json.hpp>

#include <string>

// reflect-cpp extracts the field names directly from these shared aggregate types.
struct BenchmarkAdapter {
    using Buffer = std::string;
    static constexpr const char* library = "reflect-cpp";
    static constexpr const char* backend = "yyjson";

    static auto encode(const User& value, Buffer& bytes) -> bool {
        bytes = rfl::json::write(value);
        return true;
    }
    static auto decode(const Buffer& bytes, User& value) -> bool {
        auto result = rfl::json::read<User>(bytes);
        if (!result) return false;
        value = std::move(result.value());
        return true;
    }
};
