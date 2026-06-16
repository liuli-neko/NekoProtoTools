#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename T>
struct ParserIsStringKey : std::false_type {};

template <typename Traits, typename Alloc>
struct ParserIsStringKey<std::basic_string<char, Traits, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool ParserIsStringKeyV = ParserIsStringKey<std::remove_cvref_t<T>>::value;

template <typename R, typename W, typename K, typename V, bool StringKey = ParserIsStringKeyV<K>>
parsing::schema::Type parser_map_schema() {
    if constexpr (StringKey) {
        parsing::schema::Type::Object object;
        object.additionalProperties =
            std::make_shared<parsing::schema::Type>(parser_schema<R, W, V>());
        return object;
    } else {
        parsing::schema::Type::Object entry;
        entry.properties.emplace("key", parser_schema<R, W, K>());
        entry.properties.emplace("value", parser_schema<R, W, V>());
        entry.required = {"key", "value"};
        parsing::schema::Type::Array array;
        array.items = std::make_shared<parsing::schema::Type>(std::move(entry));
        return array;
    }
}

template <typename R, typename W, typename T, typename ParentType>
ParserResult parser_write_key_value_array(W& writer, const T& values, const ParentType& parent) {
    auto array = parsing::Parent<W>::addArray(writer, values.size(), parent);
    std::size_t index = 0;
    for (const auto& item : values) {
        auto object = parsing::Parent<W>::addObject(writer, 2, typename parsing::Parent<W>::Array{&array});
        auto result =
            parser_write<R, W>(writer, item.first, typename parsing::Parent<W>::Object{"key", &object});
        if (!result) {
            return parser_context(std::move(result),
                                  "Failed to write map entry " + std::to_string(index) + " key: ");
        }
        result = parser_write<R, W>(writer, item.second,
                                    typename parsing::Parent<W>::Object{"value", &object});
        if (!result) {
            return parser_context(std::move(result),
                                  "Failed to write map entry " + std::to_string(index) + " value: ");
        }
        ++index;
    }
    return sa::success();
}

template <typename R, typename W, typename T>
ParserResult parser_read_key_value_array(typename R::InputValueType in, T& values) {
    auto array = R::toArray(in);
    if (!array) {
        return array.error();
    }
    values.clear();
    const auto size = R::arraySize(array.value());
    for (std::size_t i = 0; i < size; ++i) {
        auto object = R::toObject(R::arrayElement(array.value(), i));
        if (!object) {
            return parser_context(object.error(), "Failed to parse map entry " + std::to_string(i) + ": ");
        }
        using Key   = typename T::key_type;
        using Value = typename T::mapped_type;
        Key key{};
        Value value{};
        auto keyField = R::objectField(object.value(), "key");
        if (!keyField) {
            return parser_error(sa::ErrorCode::InvalidField,
                                "Map entry " + std::to_string(i) + " is missing required field 'key'");
        }
        auto result = parser_read<R, W>(keyField.value(), key);
        if (!result) {
            return parser_context(std::move(result),
                                  "Failed to parse map entry " + std::to_string(i) + " key: ");
        }
        auto valField = R::objectField(object.value(), "value");
        if (!valField) {
            return parser_error(sa::ErrorCode::InvalidField,
                                "Map entry " + std::to_string(i) + " is missing required field 'value'");
        }
        result = parser_read<R, W>(valField.value(), value);
        if (!result) {
            return parser_context(std::move(result),
                                  "Failed to parse map entry " + std::to_string(i) + " value: ");
        }
        values.emplace(std::move(key), std::move(value));
    }
    return sa::success();
}

template <typename R, typename W, typename T, typename ParentType>
ParserResult parser_write_string_key_map(W& writer, const T& values, const ParentType& parent) {
    auto object = parsing::Parent<W>::addObject(writer, values.size(), parent);
    for (const auto& item : values) {
        auto result =
            parser_write<R, W>(writer, item.second, typename parsing::Parent<W>::Object{item.first, &object});
        if (!result) {
            return parser_context(std::move(result),
                                  "Failed to write map field '" + std::string(item.first) + "': ");
        }
    }
    return sa::success();
}

template <typename R, typename W, typename T>
ParserResult parser_read_string_key_map(typename R::InputValueType in, T& values) {
    auto object = R::toObject(in);
    if (!object) {
        return object.error();
    }
    values.clear();
    ParserResult result;
    R::forEachObjectMember(object.value(), [&values, &result](std::string_view name, auto field) {
        if (!result) {
            return false;
        }
        typename T::mapped_type value{};
        result = parser_context(parser_read<R, W>(field, value),
                                "Failed to parse map field '" + std::string(name) + "': ");
        if (!result) {
            return false;
        }
        values.emplace(typename T::key_type{name.data(), name.size()}, std::move(value));
        return true;
    });
    return result;
}

template <typename R, typename W, typename K, typename V, typename Compare, typename Alloc>
struct Parser<R, W, std::map<K, V, Compare, Alloc>, void> {
    using Map = std::map<K, V, Compare, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Map& value, const ParentType& parent, const Tags& /*tags*/) {
        if constexpr (ParserIsStringKeyV<K>) {
            return parser_write_string_key_map<R, W>(writer, value, parent);
        } else {
            return parser_write_key_value_array<R, W>(writer, value, parent);
        }
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Map& value, const Tags& /*tags*/) {
        if constexpr (ParserIsStringKeyV<K>) {
            return parser_read_string_key_map<R, W>(in, value);
        } else {
            return parser_read_key_value_array<R, W>(in, value);
        }
    }

    static parsing::schema::Type toSchema() { return parser_map_schema<R, W, K, V>(); }
};

template <typename R, typename W, typename K, typename V, typename Compare, typename Alloc>
struct Parser<R, W, std::multimap<K, V, Compare, Alloc>, void> {
    using Map = std::multimap<K, V, Compare, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Map& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_key_value_array<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Map& value, const Tags& /*tags*/) {
        return parser_read_key_value_array<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_map_schema<R, W, K, V, false>(); }
};

template <typename R, typename W, typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct Parser<R, W, std::unordered_map<K, V, Hash, Eq, Alloc>, void> {
    using Map = std::unordered_map<K, V, Hash, Eq, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Map& value, const ParentType& parent, const Tags& /*tags*/) {
        if constexpr (ParserIsStringKeyV<K>) {
            return parser_write_string_key_map<R, W>(writer, value, parent);
        } else {
            return parser_write_key_value_array<R, W>(writer, value, parent);
        }
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Map& value, const Tags& /*tags*/) {
        if constexpr (ParserIsStringKeyV<K>) {
            return parser_read_string_key_map<R, W>(in, value);
        } else {
            return parser_read_key_value_array<R, W>(in, value);
        }
    }

    static parsing::schema::Type toSchema() { return parser_map_schema<R, W, K, V>(); }
};

template <typename R, typename W, typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct Parser<R, W, std::unordered_multimap<K, V, Hash, Eq, Alloc>, void> {
    using Map = std::unordered_multimap<K, V, Hash, Eq, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Map& value, const ParentType& parent, const Tags& /*tags*/) {
        return parser_write_key_value_array<R, W>(writer, value, parent);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Map& value, const Tags& /*tags*/) {
        return parser_read_key_value_array<R, W>(in, value);
    }

    static parsing::schema::Type toSchema() { return parser_map_schema<R, W, K, V, false>(); }
};

} // namespace detail
NEKO_END_NAMESPACE
