#pragma once

#include <nekoproto/reflect.hpp>

#include <string>

struct User {
    int id = 0;
    std::string name;
};

template <>
struct nekoproto::Meta<User> {
    static constexpr auto value = nekoproto::Object("id", &User::id, "name", &User::name);
};
