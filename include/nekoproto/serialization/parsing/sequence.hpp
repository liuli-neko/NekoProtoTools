#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <array>
#include <cstddef>
#include <deque>
#include <list>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename>
inline constexpr bool ParserSequenceDependentFalseV = false;

template <typename R, typename W, typename T>
parsing::schema::Type parser_sequence_schema(bool uniqueItems = false) {
    parsing::schema::Type::Array schema;
    schema.items = std::make_shared<parsing::schema::Type>(parser_schema<R, W, T>());
    if (uniqueItems) {
        schema.uniqueItems = true;
    }
    return schema;
}

template <typename T>
bool parser_insert_sequence_value(T& values, typename T::value_type&& value) {
    if constexpr (requires { values.push_back(std::move(value)); }) {
        values.push_back(std::move(value));
    } else if constexpr (requires { values.emplace(std::move(value)); }) {
        values.emplace(std::move(value));
    } else if constexpr (requires { values.insert(std::move(value)); }) {
        values.insert(std::move(value));
    } else {
        static_assert(ParserSequenceDependentFalseV<T>, "Unsupported sequence container insertion");
    }
    return true;
}

template <typename R, typename W, typename T, typename ParentType>
ParserResult parser_write_sequence(W& writer, const T& values, const ParentType& parent) {
    auto array = parsing::Parent<W>::addArray(writer, values.size(), parent);
    std::size_t index = 0;
    for (const auto& value : values) {
        auto result = parser_write<R, W>(writer, value, typename parsing::Parent<W>::Array{&array});
        if (!result) {
            return parser_context(std::move(result), "Failed to write sequence element " + std::to_string(index) +
                                                         ": ");
        }
        ++index;
    }
    return sa::success();
}

template <typename R, typename W, typename T>
ParserResult parser_read_sequence(typename R::InputValueType in, T& values) {
    auto array = R::toArray(in);
    if (!array) {
        return array.error();
    }
    values.clear();
    const auto size = R::arraySize(array.value());
    for (std::size_t i = 0; i < size; ++i) {
        typename T::value_type item{};
        auto result = parser_read<R, W>(R::arrayElement(array.value(), i), item);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse sequence element " + std::to_string(i) + ": ");
        }
        parser_insert_sequence_value(values, std::move(item));
    }
    return sa::success();
}

template <typename R, typename W, typename T, typename Alloc>
struct Parser<R, W, std::vector<T, Alloc>, void> {
    using Vector = std::vector<T, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Vector& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_sequence<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Vector& value, const Tags& /*tags*/) {
        return parser_read_sequence<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, T>(); }
};

template <typename R, typename W, typename Alloc>
struct Parser<R, W, std::vector<bool, Alloc>, void> {
    using Vector = std::vector<bool, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Vector& value, const ParentType& parent, const Tags& /*tags*/) {
        auto array = parsing::Parent<W>::addArray(writer, value.size(), parent);
        std::size_t index = 0;
        for (bool item : value) {
            auto result = parser_write<R, W>(writer, item, typename parsing::Parent<W>::Array{&array});
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to write vector<bool> element " + std::to_string(index) + ": ");
            }
            ++index;
        }
        return sa::success();
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Vector& value, const Tags& /*tags*/) {
        auto array = R::toArray(in);
        if (!array) {
            return array.error();
        }
        value.clear();
        value.reserve(R::arraySize(array.value()));
        const auto size = R::arraySize(array.value());
        for (std::size_t i = 0; i < size; ++i) {
            bool item = false;
            auto result = parser_read<R, W>(R::arrayElement(array.value(), i), item);
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to parse vector<bool> element " + std::to_string(i) + ": ");
            }
            value.push_back(item);
        }
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, bool>(); }
};

template <typename R, typename W, typename T, typename Alloc>
struct Parser<R, W, std::deque<T, Alloc>, void> {
    using Deque = std::deque<T, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Deque& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_sequence<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Deque& value, const Tags& /*tags*/) {
        return parser_read_sequence<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, T>(); }
};

template <typename R, typename W, typename T, typename Alloc>
struct Parser<R, W, std::list<T, Alloc>, void> {
    using List = std::list<T, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const List& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_sequence<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, List& value, const Tags& /*tags*/) {
        return parser_read_sequence<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, T>(); }
};

template <typename R, typename W, typename T, std::size_t N>
struct Parser<R, W, std::array<T, N>, void> {
    using Array = std::array<T, N>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Array& value, const ParentType& parent, const Tags& /*tags*/) {
        auto array = parsing::Parent<W>::addArray(writer, N, parent);
        for (std::size_t i = 0; i < N; ++i) {
            auto result = parser_write<R, W>(writer, value[i], typename parsing::Parent<W>::Array{&array});
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to write fixed array element " + std::to_string(i) + ": ");
            }
        }
        return sa::success();
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Array& value, const Tags& /*tags*/) {
        auto array = R::toArray(in);
        if (!array) {
            return array.error();
        }
        const auto actualSize = R::arraySize(array.value());
        if (actualSize != N) {
            return parser_error(sa::ErrorCode::InvalidLength,
                                "Expected fixed array with " + std::to_string(N) + " elements, got " +
                                    std::to_string(actualSize));
        }
        for (std::size_t i = 0; i < N; ++i) {
            auto result = parser_read<R, W>(R::arrayElement(array.value(), i), value[i]);
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to parse fixed array element " + std::to_string(i) + ": ");
            }
        }
        return sa::success();
    }

    static parsing::schema::Type toSchema() {
        auto schema     = parser_sequence_schema<R, W, T>();
        auto& array     = std::get<parsing::schema::Type::Array>(schema.value);
        array.minItems  = N;
        array.maxItems  = N;
        return schema;
    }
};

template <typename R, typename W, typename T, typename Compare, typename Alloc>
struct Parser<R, W, std::set<T, Compare, Alloc>, void> {
    using Set = std::set<T, Compare, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Set& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_sequence<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Set& value, const Tags& /*tags*/) {
        return parser_read_sequence<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, T>(true); }
};

template <typename R, typename W, typename T, typename Compare, typename Alloc>
struct Parser<R, W, std::multiset<T, Compare, Alloc>, void> {
    using Set = std::multiset<T, Compare, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Set& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_sequence<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Set& value, const Tags& /*tags*/) {
        return parser_read_sequence<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, T>(); }
};

template <typename R, typename W, typename T, typename Hash, typename Eq, typename Alloc>
struct Parser<R, W, std::unordered_set<T, Hash, Eq, Alloc>, void> {
    using Set = std::unordered_set<T, Hash, Eq, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Set& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_sequence<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Set& value, const Tags& /*tags*/) {
        return parser_read_sequence<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, T>(true); }
};

template <typename R, typename W, typename T, typename Hash, typename Eq, typename Alloc>
struct Parser<R, W, std::unordered_multiset<T, Hash, Eq, Alloc>, void> {
    using Set = std::unordered_multiset<T, Hash, Eq, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Set& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_sequence<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Set& value, const Tags& /*tags*/) {
        return parser_read_sequence<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_sequence_schema<R, W, T>(); }
};

} // namespace detail
NEKO_END_NAMESPACE
