#pragma once

#include "benchmark_models.hpp"
#include <glaze/glaze.hpp>

#include <string>

template <>
struct glz::meta<Address> {
    using T = Address;
    static constexpr auto value = object("city", &T::city, "street", &T::street, "zip", &T::zip);
};

template <>
struct glz::meta<User> {
    using T = User;
    static constexpr auto value = object("id", &T::id, "name", &T::name, "scores", &T::scores,
                                         "email", &T::email, "address", &T::address);
};

struct BenchmarkAdapter {
    using Buffer = std::string;
    static constexpr const char* library = "Glaze";
    static constexpr const char* backend = "Glaze JSON";

    static auto encode(const User& value, Buffer& bytes) -> bool {
        bytes.clear();
        return !glz::write_json(value, bytes);
    }
    static auto decode(const Buffer& bytes, User& value) -> bool {
        return !glz::read_json(value, bytes);
    }
};
