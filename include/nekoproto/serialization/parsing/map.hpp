#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace nekoproto {
namespace detail {

template <typename T>
struct ParserIsStringKey : std::false_type {};

template <typename Traits, typename Alloc>
struct ParserIsStringKey<std::basic_string<char, Traits, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool ParserIsStringKeyV = ParserIsStringKey<std::remove_cvref_t<T>>::value;

template <typename K, typename V, bool StringKey = ParserIsStringKeyV<K>>
auto parserMapSchema() -> parsing::schema::Type {
    if constexpr (StringKey) {
        parsing::schema::Type::Object object;
        object.additionalProperties = std::make_shared<parsing::schema::Type>(parserSchema<V>());
        return object;
    } else {
        parsing::schema::Type::Object entry;
        entry.properties.emplace("key", parserSchema<K>());
        entry.properties.emplace("value", parserSchema<V>());
        entry.required = {"key", "value"};
        parsing::schema::Type::Array array;
        array.items = std::make_shared<parsing::schema::Type>(std::move(entry));
        return array;
    }
}

template <typename W, typename T, typename ParentType, typename Tags>
auto parserWriteKeyValueArray(W& writer, const T& values, const ParentType& parent, const Tags& tags) -> ParserResult {
    auto array        = parsing::Parent<W>::addArray(writer, values.size(), parent, tags);
    std::size_t index = 0;
    for (const auto& item : values) {
        auto object = parsing::Parent<W>::addObject(writer, 2, typename parsing::Parent<W>::Array{&array});
        auto result = parserWrite<W>(writer, item.first, typename parsing::Parent<W>::Object{"key", &object});
        if (!result) {
            return parserContext(std::move(result), "Failed to write map entry " + std::to_string(index) + " key: ");
        }
        result = parserWrite<W>(writer, item.second, typename parsing::Parent<W>::Object{"value", &object});
        if (!result) {
            return parserContext(std::move(result), "Failed to write map entry " + std::to_string(index) + " value: ");
        }
        ++index;
    }
    return sa::success();
}

template <typename R, typename T, typename Tags>
auto parserReadKeyValueArray(typename R::InputValueType in, T& values, const Tags& tags) -> ParserResult {
    auto array = parsing::readerToArray<R>(in, tags);
    if (!array) {
        return array.error();
    }
    T parsed = parserEmptyContainerLike(values);
    const auto size = R::arraySize(array.value());
    for (std::size_t i = 0; i < size; ++i) {
        auto object = parsing::readerToObject<R>(R::arrayElement(array.value(), i), NoTags{});
        if (!object) {
            return parserContext(object.error(), "Failed to parse map entry " + std::to_string(i) + ": ");
        }
        using Key   = typename T::key_type;
        using Value = typename T::mapped_type;
        Key key{};
        Value value{};
        auto keyField = parsing::readerObjectField<R>(object.value(), "key", NoTags{});
        if (!keyField) {
            return makeParserError(sa::ErrorCode::InvalidField,
                                "Map entry " + std::to_string(i) + " is missing required field 'key'");
        }
        auto result = parserRead<R>(keyField.value(), key);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse map entry " + std::to_string(i) + " key: ");
        }
        auto valField = parsing::readerObjectField<R>(object.value(), "value", NoTags{});
        if (!valField) {
            return makeParserError(sa::ErrorCode::InvalidField,
                                "Map entry " + std::to_string(i) + " is missing required field 'value'");
        }
        result = parserRead<R>(valField.value(), value);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse map entry " + std::to_string(i) + " value: ");
        }
        auto inserted = parsed.emplace(std::move(key), std::move(value));
        if constexpr (requires { inserted.second; }) {
            if (!inserted.second) {
                return makeParserError(sa::ErrorCode::InvalidField,
                                    "Map entry " + std::to_string(i) + " contains a duplicate key");
            }
        }
    }
    values = std::move(parsed);
    return sa::success();
}

template <typename W, typename T, typename ParentType, typename Tags>
auto parserWriteStringKeyMap(W& writer, const T& values, const ParentType& parent, const Tags& tags) -> ParserResult {
    auto object = parsing::Parent<W>::addObject(writer, values.size(), parent, tags);
    for (const auto& item : values) {
        auto result = parserWrite<W>(writer, item.second, typename parsing::Parent<W>::Object{item.first, &object});
        if (!result) {
            return parserContext(std::move(result), "Failed to write map field '" + std::string(item.first) + "': ");
        }
    }
    return sa::success();
}

template <typename R, typename T, typename Tags>
auto parserReadStringKeyMap(typename R::InputValueType in, T& values, const Tags& tags) -> ParserResult {
    auto object = parsing::readerToObject<R>(in, tags);
    if (!object) {
        return object.error();
    }
    T parsed = parserEmptyContainerLike(values);
    ParserResult result;
    parsing::readerForEachObjectMember<R>(
        object.value(),
        [&parsed, &result](std::string_view name, auto field) {
            if (!result) {
                return false;
            }
            typename T::mapped_type value{};
            result =
                parserContext(parserRead<R>(field, value), "Failed to parse map field '" + std::string(name) + "': ");
            if (!result) {
                return false;
            }
            auto inserted = parsed.emplace(typename T::key_type{name.data(), name.size()}, std::move(value));
            if constexpr (requires { inserted.second; }) {
                if (!inserted.second) {
                    result = makeParserError(sa::ErrorCode::InvalidField,
                                          "Map contains duplicate key '" + std::string(name) + "'");
                    return false;
                }
            }
            return true;
        },
        tags);
    if (result) {
        values = std::move(parsed);
    }
    return result;
}

template <typename W, typename Map, bool StringKey = ParserIsStringKeyV<typename Map::key_type>>
struct MapWriteParser;

template <typename W, typename Map>
struct MapWriteParser<W, Map, true> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Map& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return parserWriteStringKeyMap<W>(writer, value, parent, tags);
    }
};

template <typename W, typename Map>
struct MapWriteParser<W, Map, false> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Map& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return parserWriteKeyValueArray<W>(writer, value, parent, tags);
    }
};

template <typename R, typename Map, bool StringKey = ParserIsStringKeyV<typename Map::key_type>>
struct MapReadParser;

template <typename R, typename Map>
struct MapReadParser<R, Map, true> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, Map& value, const Tags& tags) -> ParserResult {
        return parserReadStringKeyMap<R>(in, value, tags);
    }
};

template <typename R, typename Map>
struct MapReadParser<R, Map, false> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, Map& value, const Tags& tags) -> ParserResult {
        return parserReadKeyValueArray<R>(in, value, tags);
    }
};

template <typename K, typename V, bool StringKey = ParserIsStringKeyV<K>>
struct MapSchemaParser {
    static auto toSchema() -> parsing::schema::Type { return parserMapSchema<K, V, StringKey>(); }
};

template <typename W, typename K, typename V, typename Compare, typename Alloc>
struct WriteParser<W, std::map<K, V, Compare, Alloc>, void> : MapWriteParser<W, std::map<K, V, Compare, Alloc>> {};

template <typename R, typename K, typename V, typename Compare, typename Alloc>
struct ReadParser<R, std::map<K, V, Compare, Alloc>, void> : MapReadParser<R, std::map<K, V, Compare, Alloc>> {};

template <typename K, typename V, typename Compare, typename Alloc>
struct SchemaParser<std::map<K, V, Compare, Alloc>, void> : MapSchemaParser<K, V> {};

template <typename W, typename K, typename V, typename Compare, typename Alloc>
struct WriteParser<W, std::multimap<K, V, Compare, Alloc>, void>
    : MapWriteParser<W, std::multimap<K, V, Compare, Alloc>, false> {};

template <typename R, typename K, typename V, typename Compare, typename Alloc>
struct ReadParser<R, std::multimap<K, V, Compare, Alloc>, void>
    : MapReadParser<R, std::multimap<K, V, Compare, Alloc>, false> {};

template <typename K, typename V, typename Compare, typename Alloc>
struct SchemaParser<std::multimap<K, V, Compare, Alloc>, void> : MapSchemaParser<K, V, false> {};

template <typename W, typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct WriteParser<W, std::unordered_map<K, V, Hash, Eq, Alloc>, void>
    : MapWriteParser<W, std::unordered_map<K, V, Hash, Eq, Alloc>> {};

template <typename R, typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct ReadParser<R, std::unordered_map<K, V, Hash, Eq, Alloc>, void>
    : MapReadParser<R, std::unordered_map<K, V, Hash, Eq, Alloc>> {};

template <typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct SchemaParser<std::unordered_map<K, V, Hash, Eq, Alloc>, void> : MapSchemaParser<K, V> {};

template <typename W, typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct WriteParser<W, std::unordered_multimap<K, V, Hash, Eq, Alloc>, void>
    : MapWriteParser<W, std::unordered_multimap<K, V, Hash, Eq, Alloc>, false> {};

template <typename R, typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct ReadParser<R, std::unordered_multimap<K, V, Hash, Eq, Alloc>, void>
    : MapReadParser<R, std::unordered_multimap<K, V, Hash, Eq, Alloc>, false> {};

template <typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct SchemaParser<std::unordered_multimap<K, V, Hash, Eq, Alloc>, void> : MapSchemaParser<K, V, false> {};

} // namespace detail
} // namespace nekoproto
