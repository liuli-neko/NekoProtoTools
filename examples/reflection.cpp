#include "common/user.hpp"

#include <iostream>
#include <string_view>

int main() {
    User user{7, "Neko"};
    static_assert(nekoproto::Reflect<User>::value_count == 2);
    nekoproto::Reflect<User>::forEach(user, [](const auto& field, std::string_view name) {
        std::cout << name << " = " << field << '\n';
    });
}
