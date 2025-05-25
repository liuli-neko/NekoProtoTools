/**
 * @file serializer_base.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief serializer base
 *
 * @mainpage NekoProtoTools
 *
 * @section intro_sec Introduction
 * A simple c++ header to generate serializer and deserializer function for class.
 * This file provides the macro NEKO_SERIALIZER to generation, and this function is
 * a template function, can't be used directly. you need provide compliant requirements
 * Serializer. a default JsonSerializer is provided in proto_json_serializer.hpp.
 *
 * @section usage_sec Usage
 * you only need to use NEKO_SERIALIZER in your class, and specify the members you want to
 * serialize.
 *
 * @section example_sec Example
 * @code {.c++}
 *
 * class MyClass {
 *     std::string name;
 *     int age;
 *     std::string address;
 *
 *     NEKO_SERIALIZER(name, age, address)
 * };
 *
 * int main()
 *     MyClass obj;
 *     obj.name = "Alice";
 *     obj.age = 18;
 *     obj.address = "Zh";
 *     std::vector<char> data;
 *     {
 *          JsonSerializer::OutputSerializer out(data);
 *          out(obj);
 *     }
 *     data.push_back(0);
 *     std::cout << data.data() << std::endl;
 *
 *     return 0;
 * }
 * @endcode
 *
 *
 * @par license
 *  GPL-2.0 license
 *
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024 by llhsdmd
 *
 */
#pragma once

#include <array>
#include <string>
#include <vector>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "private/helpers.hpp"
#include "types/struct_unwrap.hpp"

NEKO_BEGIN_NAMESPACE
// NOLINTBEGIN
namespace detail {
inline constexpr int _members_size() NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
    return 0;
}
inline constexpr int _members_size(std::string_view names) NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
    if (names.empty()) {
        return 0;
    }
    int count         = 0;
    std::size_t begin = 0;
    std::size_t end   = names.find_first_of(',', begin + 1);
    while (end != std::string::npos) {
        begin = end + 1;
        end   = names.find_first_of(',', begin + 1);
        count++;
    }
    if (begin != names.size()) {
        count++;
    }
    return count;
}
template <int N>
inline constexpr std::array<std::string_view, N>
_parse_names(std::string_view names) NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
    if constexpr (N == 0) {
        return {};
    } else {
        std::array<std::string_view, N> namesVec;
        std::size_t begin = 0;
        std::size_t end   = names.find_first_of(',', begin + 1);
        int icount        = 0;
        while (end != std::string_view::npos) {
            std::size_t bbegin     = names.find_first_not_of(' ', begin);
            std::size_t eend       = names.find_last_not_of(' ', end);
            std::string_view token = names.substr(bbegin, eend - bbegin);
            namesVec[icount++]     = token;
            begin                  = end + 1;
            end                    = names.find_first_of(',', begin + 1);
        }
        if (begin != names.size()) {
            std::size_t bbegin     = names.find_first_not_of(' ', begin);
            std::size_t eend       = names.back() == ' ' ? names.find_last_not_of(' ') : names.size();
            std::string_view token = names.substr(bbegin, eend - bbegin);
            namesVec[N - 1]        = token;
        }
        return namesVec;
    }
}

template <ConstexprString NamesStr>
struct _make_names_impl {
    constexpr static int size         = _members_size(NamesStr.view());
    constexpr static std::array names = _parse_names<size>(NamesStr.view());
};
} // namespace detail

NEKO_END_NAMESPACE
// NOLINTEND

/**
 * @brief generate serialize and deserialize functions for a class.
 *
 * Give all member variables that require serialization and deserialization support as parameters to the macro,
 * this macro will generate the serialize and deserialize functions for the class to process all given members.
 *
 * @param ...Args
 * member variables that require serialization and deserialization support.
 *
 * @note
 * Don't use this macro duplicate times. because it will generate same functions for class.
 * This function cannot directly serialize members and needs to be used in conjunction with supported serializers.
 * Please refer to the default JSON serializer implementation for the implementation specifications of the serializer.
 *
 * @example
 * class MyClass {
 *  ...
 *  NEKO_SERIALIZER(a, b, c)
 * private:
 *  int a;
 *  std::string b;
 *  std::vector<int> c;
 * };
 */
#define NEKO_SERIALIZER(...)                                                                                           \
public:                                                                                                                \
    template <int N>                                                                                                   \
    auto& _neko_get_n_member_reference() {                                                                             \
        auto tuple = std::tie(__VA_ARGS__);                                                                            \
        return std::get<N>(tuple);                                                                                     \
    }                                                                                                                  \
    template <int N>                                                                                                   \
    const auto& _neko_get_n_member_reference() const {                                                                 \
        auto tuple = std::tie(__VA_ARGS__);                                                                            \
        return std::get<N>(tuple);                                                                                     \
    }                                                                                                                  \
    friend struct Neko;                                                                                                \
                                                                                                                       \
    struct Neko {                                                                                                      \
        constexpr static std::array names = NEKO_NAMESPACE::detail::_make_names_impl<#__VA_ARGS__>::names;             \
        constexpr static auto values      = []<std::size_t... Is>(std::index_sequence<Is...>) {                        \
            return std::tuple{                                                                                    \
                ([](auto&& self) -> auto& { return self.template _neko_get_n_member_reference<Is>(); })...};      \
        }(std::make_index_sequence<NEKO_NAMESPACE::detail::_members_size(#__VA_ARGS__)>{});                            \
    };