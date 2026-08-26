/**
 * @file serializer_base.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief serializer base
 *
 * @mainpage NekoProtoTools
 *
 * @section intro_sec Introduction
 * Parser-based serialization backends consume reflection metadata. Prefer the
 * non-intrusive `template<> struct Meta<T>` form for hand-written metadata; this
 * header also provides the `NEKO_SERIALIZER` convenience macro.
 *
 * @section usage_sec Usage
 * Define the data type normally, then provide `Meta<T>` metadata in the
 * NekoProto namespace.
 *
 * @section example_sec Example
 * @code {.c++}
 *
 * class MyClass {
 * public:
 *     std::string name;
 *     int age;
 *     std::string address;
 * };
 *
 * namespace nekoproto {
 * template <>
 * struct Meta<::MyClass> {
 *     constexpr static auto value =
 *         Object("name", &::MyClass::name,
 *                "age", &::MyClass::age,
 *                "address", &::MyClass::address);
 * };
 * } // namespace nekoproto
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
 *  MIT License
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
#include "nekoproto/global/reflection_tags.hpp"

namespace nekoproto {
namespace detail {

constexpr auto isSpace(char c) noexcept -> bool { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

constexpr auto trim(std::string_view s) noexcept -> std::string_view {
    while (!s.empty() && isSpace(s.front())) {
        s.remove_prefix(1);
    }
    while (!s.empty() && isSpace(s.back())) {
        s.remove_suffix(1);
    }
    return s;
}

constexpr auto startsWith(std::string_view s, std::string_view prefix) noexcept -> bool {
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

constexpr auto findOuterCallOpen(std::string_view s) noexcept -> std::size_t {
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

constexpr auto findMatchingParen(std::string_view s, std::size_t open) noexcept -> std::size_t {
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

constexpr auto stripEnclosingParens(std::string_view token) noexcept -> std::string_view {
    token = trim(token);
    while (token.size() >= 2 && token.front() == '(') {
        const auto close = findMatchingParen(token, 0);
        if (close != token.size() - 1) {
            break;
        }
        token = trim(token.substr(1, token.size() - 2));
    }
    return token;
}

constexpr auto serializerArgName(std::string_view token) noexcept -> std::string_view {
    token = stripEnclosingParens(token);

    // makeTags<...>(code) 的元数据名仍然取 code
    if (startsWith(token, "makeTags")) {
        const auto open = findOuterCallOpen(token);
        if (open != std::string_view::npos) {
            const auto close = findMatchingParen(token, open);
            if (close != std::string_view::npos) {
                return serializerArgName(token.substr(open + 1, close - open - 1));
            }
        }
    }

    return token;
}

template <int N>
inline constexpr auto parseNames(std::string_view names) noexcept -> std::array<std::string_view, N> {
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
                    result[index++] = serializerArgName(names.substr(begin, i - begin));
                    begin           = i + 1;
                }
                break;
            default:
                break;
            }
        }

        result[index] = serializerArgName(names.substr(begin));
        return result;
    }
}

template <ConstexprString NamesStr, size_t N>
struct MakeNamesImpl {
    constexpr static std::array names = parseNames<N>(NamesStr.view());
};

template <typename... Args>
constexpr auto serializerMemberTuple(Args&&... args) noexcept {
    return std::forward_as_tuple(nekoproto::fieldAccessor(std::forward<Args>(args))...);
}

template <std::size_t I>
struct NekoMemberIndexAccessor {
    template <typename Self>
    constexpr auto operator()(Self&& self) const -> decltype(auto) {
        return std::get<I>(std::forward<Self>(self)._nekoMemberTuple());
    }
};

template <typename Spec>
constexpr auto serializerMakeTags() noexcept {
    return nekoproto::field_tags_v<Spec>;
}
} // namespace detail

} // namespace nekoproto
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
    constexpr auto _nekoMemberTuple() noexcept { return nekoproto::detail::serializerMemberTuple(__VA_ARGS__); }       \
    constexpr auto _nekoMemberTuple() const noexcept { return nekoproto::detail::serializerMemberTuple(__VA_ARGS__); } \
    struct NekoSerializerArgsHelper {                                                                                  \
        using tuple = decltype(std::forward_as_tuple(__VA_ARGS__));                                                    \
    };                                                                                                                 \
    struct Neko {                                                                                                      \
        using NekoSerializerArgsTuple = typename NekoSerializerArgsHelper::tuple;                                      \
        constexpr static std::array names =                                                                            \
            nekoproto::detail::MakeNamesImpl<#__VA_ARGS__, NEKO_VA_ARGS_SIZE(__VA_ARGS__)>::names;                     \
        constexpr static auto values = []<std::size_t... Is>(std::index_sequence<Is...>) {                             \
            return std::tuple{nekoproto::detail::NekoMemberIndexAccessor<Is>{}...};                                    \
        }(std::make_index_sequence<NEKO_VA_ARGS_SIZE(__VA_ARGS__)>{});                                                 \
        constexpr static auto field_tags = []<std::size_t... Is>(std::index_sequence<Is...>) {                         \
            return std::tuple{                                                                                         \
                nekoproto::detail::serializerMakeTags<std::tuple_element_t<Is, NekoSerializerArgsTuple>>()...};        \
        }(std::make_index_sequence<NEKO_VA_ARGS_SIZE(__VA_ARGS__)>{});                                                 \
    };
