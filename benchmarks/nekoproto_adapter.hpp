#pragma once

#include "benchmark_models.hpp"
#include <nekoproto/reflect.hpp>

template <>
struct nekoproto::Meta<Address> {
    static constexpr auto value = nekoproto::Object(
        "city", &Address::city, "street", &Address::street, "zip", &Address::zip);
};

template <>
struct nekoproto::Meta<User> {
    static constexpr auto value = nekoproto::Object(
        "id", &User::id, "name", &User::name, "scores", &User::scores,
        "email", &User::email, "address", &User::address);
};

#include <nekoproto/serialization/json_serializer.hpp>

#include <vector>

struct BenchmarkAdapter {
    using Buffer = std::vector<char>;
    static constexpr const char* library = "NekoProtoTools";
    static constexpr const char* backend = "RapidJSON";

    static auto encode(const User& value, Buffer& bytes) -> bool {
        bytes.clear();
        nekoproto::JsonSerializer::OutputSerializer output(bytes);
        return static_cast<bool>(output(value)) && static_cast<bool>(output.end());
    }
    static auto decode(const Buffer& bytes, User& value) -> bool {
        nekoproto::JsonSerializer::InputSerializer input(bytes.data(), bytes.size());
        return static_cast<bool>(input(value));
    }
};
