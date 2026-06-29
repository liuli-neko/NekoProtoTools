/**
 * @file serializer_base.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief serializer base
 *
 * @mainpage NekoProtoTools
 *
 * @section intro_sec Introduction
 * Provides NEKO_SERIALIZER, which exposes class members as reflection metadata
 * consumed by the parser-based serialization backends.
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

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/string_literal.hpp"
#include "nekoproto/serialization/private/tags.hpp"

NEKO_BEGIN_NAMESPACE
// NOLINTBEGIN
namespace detail {

constexpr bool _is_space(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

constexpr std::string_view _trim(std::string_view s) noexcept {
    while (!s.empty() && _is_space(s.front())) {
        s.remove_prefix(1);
    }
    while (!s.empty() && _is_space(s.back())) {
        s.remove_suffix(1);
    }
    return s;
}

constexpr bool _starts_with(std::string_view s, std::string_view prefix) noexcept {
    if (s.size() < prefix.size()) {
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (s[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

constexpr std::size_t _find_outer_call_open(std::string_view s) noexcept {
    int angle   = 0;
    int brace   = 0;
    int bracket = 0;
    char quote  = 0;
    bool escape = false;

    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];

        if (quote != 0) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == quote) {
                quote = 0;
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }

        switch (c) {
        case '<':
            ++angle;
            break;
        case '>':
            if (angle > 0) {
                --angle;
            }
            break;
        case '{':
            ++brace;
            break;
        case '}':
            if (brace > 0) {
                --brace;
            }
            break;
        case '[':
            ++bracket;
            break;
        case ']':
            if (bracket > 0) {
                --bracket;
            }
            break;
        case '(':
            if (angle == 0 && brace == 0 && bracket == 0) {
                return i;
            }
            break;
        default:
            break;
        }
    }

    return std::string_view::npos;
}

constexpr std::size_t _find_matching_paren(std::string_view s, std::size_t open) noexcept {
    int paren   = 0;
    char quote  = 0;
    bool escape = false;

    for (std::size_t i = open; i < s.size(); ++i) {
        const char c = s[i];

        if (quote != 0) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == quote) {
                quote = 0;
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }

        if (c == '(') {
            ++paren;
        } else if (c == ')') {
            --paren;
            if (paren == 0) {
                return i;
            }
        }
    }

    return std::string_view::npos;
}

constexpr std::string_view _strip_enclosing_parens(std::string_view token) noexcept {
    token = _trim(token);
    while (token.size() >= 2 && token.front() == '(') {
        const auto close = _find_matching_paren(token, 0);
        if (close != token.size() - 1) {
            break;
        }
        token = _trim(token.substr(1, token.size() - 2));
    }
    return token;
}

constexpr std::string_view _serializer_arg_name(std::string_view token) noexcept {
    token = _strip_enclosing_parens(token);

    // make_tags<...>(code) 的元数据名仍然取 code
    if (_starts_with(token, "make_tags")) {
        const auto open = _find_outer_call_open(token);
        if (open != std::string_view::npos) {
            const auto close = _find_matching_paren(token, open);
            if (close != std::string_view::npos) {
                return _serializer_arg_name(token.substr(open + 1, close - open - 1));
            }
        }
    }

    return token;
}

template <int N>
inline constexpr std::array<std::string_view, N> _parse_names(std::string_view names) NEKO_NOEXCEPT {
    std::array<std::string_view, N> result{};

    if constexpr (N == 0) {
        return result;
    } else {
        std::size_t begin = 0;
        std::size_t index = 0;

        int angle   = 0;
        int paren   = 0;
        int brace   = 0;
        int bracket = 0;
        char quote  = 0;
        bool escape = false;

        for (std::size_t i = 0; i < names.size(); ++i) {
            const char c = names[i];

            if (quote != 0) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == quote) {
                    quote = 0;
                }
                continue;
            }

            if (c == '\'' || c == '"') {
                quote = c;
                continue;
            }

            switch (c) {
            case '<':
                ++angle;
                break;
            case '>':
                if (angle > 0) {
                    --angle;
                }
                break;
            case '(':
                ++paren;
                break;
            case ')':
                if (paren > 0) {
                    --paren;
                }
                break;
            case '{':
                ++brace;
                break;
            case '}':
                if (brace > 0) {
                    --brace;
                }
                break;
            case '[':
                ++bracket;
                break;
            case ']':
                if (bracket > 0) {
                    --bracket;
                }
                break;
            case ',':
                if (angle == 0 && paren == 0 && brace == 0 && bracket == 0) {
                    result[index++] = _serializer_arg_name(names.substr(begin, i - begin));
                    begin           = i + 1;
                }
                break;
            default:
                break;
            }
        }

        result[index] = _serializer_arg_name(names.substr(begin));
        return result;
    }
}

template <ConstexprString NamesStr, size_t N>
struct _make_names_impl {
    constexpr static std::array names = _parse_names<N>(NamesStr.view());
};

template <typename T>
constexpr decltype(auto) _serializer_unwrap_member_ref(T&& value) noexcept {
    return NEKO_NAMESPACE::field_accessor(std::forward<T>(value));
}

template <std::size_t N, typename... Args>
constexpr decltype(auto) _serializer_get_n_member_reference(Args&&... args) noexcept {
    auto tuple = std::forward_as_tuple(
        _serializer_unwrap_member_ref(std::forward<Args>(args))...
    );
    return std::get<N>(tuple);
}

template <typename Spec, std::size_t /*I*/, typename Accessor>
constexpr auto _serializer_make_accessor(Accessor accessor) noexcept {
    return accessor;
}

template <typename Spec>
constexpr auto _serializer_make_tags() noexcept {
    return NEKO_NAMESPACE::field_tags_v<Spec>;
}
} // namespace detail

NEKO_END_NAMESPACE
// NOLINTEND

/**
 * @brief Generate reflection metadata for a class.
 *
 * Give all member variables that require serialization support as parameters.
 * Parser backends use the generated names and member accessors for reading and writing.
 *
 * @param ...Args
 * member variables that require serialization and deserialization support.
 *
 * @note
 * Do not use this macro more than once in the same class.
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
    decltype(auto) _neko_get_n_member_reference() noexcept {                                                           \
        return NEKO_NAMESPACE::detail::_serializer_get_n_member_reference<N>(__VA_ARGS__);                             \
    }                                                                                                                  \
    template <int N>                                                                                                   \
    decltype(auto) _neko_get_n_member_reference() const noexcept {                                                     \
        return NEKO_NAMESPACE::detail::_serializer_get_n_member_reference<N>(__VA_ARGS__);                             \
    }                                                                                                                  \
    struct _neko_serializer_args_helper {                                                                              \
        using tuple = decltype(std::forward_as_tuple(__VA_ARGS__));                                                    \
    };                                                                                                                 \
    struct Neko {                                                                                                      \
        using _neko_serializer_args_tuple = typename _neko_serializer_args_helper::tuple;                              \
        constexpr static std::array names =                                                                            \
            NEKO_NAMESPACE::detail::_make_names_impl<#__VA_ARGS__, NEKO_VA_ARGS_SIZE(__VA_ARGS__)>::names;             \
        constexpr static auto values = []<std::size_t... Is>(std::index_sequence<Is...>) {                             \
            return std::tuple{NEKO_NAMESPACE::detail::_serializer_make_accessor<                                       \
                std::tuple_element_t<Is, _neko_serializer_args_tuple>, Is>(                                            \
                [](auto&& self) -> decltype(auto) { return self.template _neko_get_n_member_reference<Is>(); })...};   \
        }(std::make_index_sequence<NEKO_VA_ARGS_SIZE(__VA_ARGS__)>{});                                                 \
        constexpr static auto field_tags = []<std::size_t... Is>(std::index_sequence<Is...>) {                         \
            return std::tuple{NEKO_NAMESPACE::detail::_serializer_make_tags<                                           \
                std::tuple_element_t<Is, _neko_serializer_args_tuple>>()...};                                          \
        }(std::make_index_sequence<NEKO_VA_ARGS_SIZE(__VA_ARGS__)>{});                                                 \
    };
