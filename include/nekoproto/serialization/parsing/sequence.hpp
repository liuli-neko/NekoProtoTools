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

template <typename T>
parsing::schema::Type parser_sequence_schema(bool uniqueItems = false) {
    parsing::schema::Type::Array schema;
    schema.items = std::make_shared<parsing::schema::Type>(parser_schema<T>());
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

template <typename W, typename T, typename ParentType, typename Tags>
ParserResult parser_write_sequence(W& writer, const T& values, const ParentType& parent, const Tags& tags) {
    auto array        = parsing::Parent<W>::addArray(writer, values.size(), parent, tags);
    std::size_t index = 0;
    for (const auto& value : values) {
        auto result = parser_write<W>(writer, value, typename parsing::Parent<W>::Array{&array});
        if (!result) {
            return parser_context(std::move(result),
                                  "Failed to write sequence element " + std::to_string(index) + ": ");
        }
        ++index;
    }
    return sa::success();
}

template <typename R, typename T>
ParserResult parser_read_sequence(typename R::InputValueType in, T& values) {
    auto array = R::toArray(in);
    if (!array) {
        return array.error();
    }
    values.clear();
    const auto size = R::arraySize(array.value());
    for (std::size_t i = 0; i < size; ++i) {
        typename T::value_type item{};
        auto result = parser_read<R>(R::arrayElement(array.value(), i), item);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse sequence element " + std::to_string(i) + ": ");
        }
        parser_insert_sequence_value(values, std::move(item));
    }
    return sa::success();
}

template <typename W, typename Sequence>
struct SequenceWriteParser {
    using Container = Sequence;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Container& value, const ParentType& parent, const Tags& tags) {
        return parser_write_sequence<W>(writer, value, parent, tags);
    }
};

template <typename R, typename Sequence>
struct SequenceReadParser {
    using Container = Sequence;

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Container& value, const Tags& /*tags*/) {
        return parser_read_sequence<R>(in, value);
    }
};

template <typename T, bool UniqueItems = false>
struct SequenceSchemaParser {
    static parsing::schema::Type toSchema() { return parser_sequence_schema<T>(UniqueItems); }
};

template <typename W, typename T, typename Alloc>
struct WriteParser<W, std::vector<T, Alloc>, void> : SequenceWriteParser<W, std::vector<T, Alloc>> {};

template <typename R, typename T, typename Alloc>
struct ReadParser<R, std::vector<T, Alloc>, void> : SequenceReadParser<R, std::vector<T, Alloc>> {};

template <typename T, typename Alloc>
struct SchemaParser<std::vector<T, Alloc>, void> : SequenceSchemaParser<T> {};

template <typename W, typename Alloc>
struct WriteParser<W, std::vector<bool, Alloc>, void> {
    using Vector = std::vector<bool, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Vector& value, const ParentType& parent, const Tags& tags) {
        auto array        = parsing::Parent<W>::addArray(writer, value.size(), parent, tags);
        std::size_t index = 0;
        for (bool item : value) {
            auto result = parser_write<W>(writer, item, typename parsing::Parent<W>::Array{&array});
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to write vector<bool> element " + std::to_string(index) + ": ");
            }
            ++index;
        }
        return sa::success();
    }
};

template <typename R, typename Alloc>
struct ReadParser<R, std::vector<bool, Alloc>, void> {
    using Vector = std::vector<bool, Alloc>;

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
            bool item   = false;
            auto result = parser_read<R>(R::arrayElement(array.value(), i), item);
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to parse vector<bool> element " + std::to_string(i) + ": ");
            }
            value.push_back(item);
        }
        return sa::success();
    }
};

template <typename Alloc>
struct SchemaParser<std::vector<bool, Alloc>, void> {
    static parsing::schema::Type toSchema() { return parser_sequence_schema<bool>(); }
};

template <typename W, typename T, typename Alloc>
struct WriteParser<W, std::deque<T, Alloc>, void> : SequenceWriteParser<W, std::deque<T, Alloc>> {};

template <typename R, typename T, typename Alloc>
struct ReadParser<R, std::deque<T, Alloc>, void> : SequenceReadParser<R, std::deque<T, Alloc>> {};

template <typename T, typename Alloc>
struct SchemaParser<std::deque<T, Alloc>, void> : SequenceSchemaParser<T> {};

template <typename W, typename T, typename Alloc>
struct WriteParser<W, std::list<T, Alloc>, void> : SequenceWriteParser<W, std::list<T, Alloc>> {};

template <typename R, typename T, typename Alloc>
struct ReadParser<R, std::list<T, Alloc>, void> : SequenceReadParser<R, std::list<T, Alloc>> {};

template <typename T, typename Alloc>
struct SchemaParser<std::list<T, Alloc>, void> : SequenceSchemaParser<T> {};

template <typename W, typename T, std::size_t N>
struct WriteParser<W, std::array<T, N>, void> {
    using Array = std::array<T, N>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Array& value, const ParentType& parent, const Tags& tags) {
        auto array = parsing::Parent<W>::addArray(writer, N, parent, tags);
        for (std::size_t i = 0; i < N; ++i) {
            auto result = parser_write<W>(writer, value[i], typename parsing::Parent<W>::Array{&array});
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to write fixed array element " + std::to_string(i) + ": ");
            }
        }
        return sa::success();
    }
};

template <typename R, typename T, std::size_t N>
struct ReadParser<R, std::array<T, N>, void> {
    using Array = std::array<T, N>;

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Array& value, const Tags& /*tags*/) {
        auto array = R::toArray(in);
        if (!array) {
            return array.error();
        }
        const auto actualSize = R::arraySize(array.value());
        if (actualSize != N) {
            return parser_error(sa::ErrorCode::InvalidLength, "Expected fixed array with " + std::to_string(N) +
                                                                  " elements, got " + std::to_string(actualSize));
        }
        for (std::size_t i = 0; i < N; ++i) {
            auto result = parser_read<R>(R::arrayElement(array.value(), i), value[i]);
            if (!result) {
                return parser_context(std::move(result),
                                      "Failed to parse fixed array element " + std::to_string(i) + ": ");
            }
        }
        return sa::success();
    }
};

template <typename T, std::size_t N>
struct SchemaParser<std::array<T, N>, void> {
    static parsing::schema::Type toSchema() {
        auto schema    = parser_sequence_schema<T>();
        auto& array    = std::get<parsing::schema::Type::Array>(schema.value);
        array.minItems = N;
        array.maxItems = N;
        return schema;
    }
};

template <typename W, typename T, typename Compare, typename Alloc>
struct WriteParser<W, std::set<T, Compare, Alloc>, void> : SequenceWriteParser<W, std::set<T, Compare, Alloc>> {};

template <typename R, typename T, typename Compare, typename Alloc>
struct ReadParser<R, std::set<T, Compare, Alloc>, void> : SequenceReadParser<R, std::set<T, Compare, Alloc>> {};

template <typename T, typename Compare, typename Alloc>
struct SchemaParser<std::set<T, Compare, Alloc>, void> : SequenceSchemaParser<T, true> {};

template <typename W, typename T, typename Compare, typename Alloc>
struct WriteParser<W, std::multiset<T, Compare, Alloc>, void>
    : SequenceWriteParser<W, std::multiset<T, Compare, Alloc>> {};

template <typename R, typename T, typename Compare, typename Alloc>
struct ReadParser<R, std::multiset<T, Compare, Alloc>, void> : SequenceReadParser<R, std::multiset<T, Compare, Alloc>> {
};

template <typename T, typename Compare, typename Alloc>
struct SchemaParser<std::multiset<T, Compare, Alloc>, void> : SequenceSchemaParser<T> {};

template <typename W, typename T, typename Hash, typename Eq, typename Alloc>
struct WriteParser<W, std::unordered_set<T, Hash, Eq, Alloc>, void>
    : SequenceWriteParser<W, std::unordered_set<T, Hash, Eq, Alloc>> {};

template <typename R, typename T, typename Hash, typename Eq, typename Alloc>
struct ReadParser<R, std::unordered_set<T, Hash, Eq, Alloc>, void>
    : SequenceReadParser<R, std::unordered_set<T, Hash, Eq, Alloc>> {};

template <typename T, typename Hash, typename Eq, typename Alloc>
struct SchemaParser<std::unordered_set<T, Hash, Eq, Alloc>, void> : SequenceSchemaParser<T, true> {};

template <typename W, typename T, typename Hash, typename Eq, typename Alloc>
struct WriteParser<W, std::unordered_multiset<T, Hash, Eq, Alloc>, void>
    : SequenceWriteParser<W, std::unordered_multiset<T, Hash, Eq, Alloc>> {};

template <typename R, typename T, typename Hash, typename Eq, typename Alloc>
struct ReadParser<R, std::unordered_multiset<T, Hash, Eq, Alloc>, void>
    : SequenceReadParser<R, std::unordered_multiset<T, Hash, Eq, Alloc>> {};

template <typename T, typename Hash, typename Eq, typename Alloc>
struct SchemaParser<std::unordered_multiset<T, Hash, Eq, Alloc>, void> : SequenceSchemaParser<T> {};

} // namespace detail
NEKO_END_NAMESPACE
