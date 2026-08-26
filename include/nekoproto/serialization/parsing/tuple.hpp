#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <cstddef>
#include <string>
#include <tuple>
#include <utility>

namespace nekoproto {
namespace detail {

template <typename W, typename... Ts>
struct WriteParser<W, std::tuple<Ts...>, void> {
    using Tuple = std::tuple<Ts...>;

    template <typename ParentType, typename Tags, std::size_t... Is>
    static auto writeImpl(W& writer, [[maybe_unused]] const Tuple& value, const ParentType& parent,
                          std::index_sequence<Is...>, const Tags& tags) -> ParserResult {
        auto array = parsing::Parent<W>::addArray(writer, sizeof...(Ts), parent, tags);
        if constexpr (sizeof...(Ts) == 0) {
            return sa::success();
        } else {
            ParserResult result;
            const auto writeElement = [&]<std::size_t I>() {
                if (result) {
                    result = parserContext(
                        parserWrite<W>(writer, std::get<I>(value), typename parsing::Parent<W>::Array{&array}),
                        "Failed to write tuple element " + std::to_string(I) + ": ");
                }
            };
            (writeElement.template operator()<Is>(), ...);
            return result;
        }
    }

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Tuple& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return writeImpl(writer, value, parent, std::index_sequence_for<Ts...>{}, tags);
    }
};

template <typename R, typename... Ts>
struct ReadParser<R, std::tuple<Ts...>, void> {
    using Tuple = std::tuple<Ts...>;

    template <std::size_t... Is>
    static auto readImpl([[maybe_unused]] const typename R::InputArrayType& array, [[maybe_unused]] Tuple& value,
                         std::index_sequence<Is...>) -> ParserResult {
        if constexpr (sizeof...(Ts) == 0) {
            return sa::success();
        } else {
            ParserResult result;
            const auto readElement = [&]<std::size_t I>() {
                if (result) {
                    result = parserContext(parserRead<R>(R::arrayElement(array, I), std::get<I>(value)),
                                            "Failed to parse tuple element " + std::to_string(I) + ": ");
                }
            };
            (readElement.template operator()<Is>(), ...);
            return result;
        }
    }

    template <typename Tags>
    static auto read(typename R::InputValueType in, Tuple& value, const Tags& tags) -> ParserResult {
        auto array = parsing::readerToArray<R>(in, tags);
        if (!array) {
            return array.error();
        }
        const auto actualSize = R::arraySize(array.value());
        if (actualSize != sizeof...(Ts)) {
            return makeParserError(sa::ErrorCode::InvalidLength, "Expected tuple with " + std::to_string(sizeof...(Ts)) +
                                                                  " elements, got " + std::to_string(actualSize));
        }
        return readImpl(array.value(), value, std::index_sequence_for<Ts...>{});
    }
};

template <typename... Ts>
struct SchemaParser<std::tuple<Ts...>, void> {
    static auto toSchema() -> parsing::schema::Type {
        parsing::schema::Type::Array schema;
        schema.prefixItems     = {parserSchema<Ts>()...};
        schema.minItems        = sizeof...(Ts);
        schema.maxItems        = sizeof...(Ts);
        schema.additionalItems = false;
        return schema;
    }
};

template <typename W, typename K, typename V>
struct WriteParser<W, std::pair<K, V>, void> {
    using Pair = std::pair<K, V>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Pair& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        auto object = parsing::Parent<W>::addObject(writer, 2, parent, tags);
        auto result = parserWrite<W>(writer, value.first, typename parsing::Parent<W>::Object{"first", &object});
        if (!result) {
            return parserContext(std::move(result), "Failed to write pair field 'first': ");
        }
        return parserContext(
            parserWrite<W>(writer, value.second, typename parsing::Parent<W>::Object{"second", &object}),
            "Failed to write pair field 'second': ");
    }
};

template <typename R, typename K, typename V>
struct ReadParser<R, std::pair<K, V>, void> {
    using Pair = std::pair<K, V>;

    template <typename Tags>
    static auto read(typename R::InputValueType in, Pair& value, const Tags& tags) -> ParserResult {
        auto object = parsing::readerToObject<R>(in, tags);
        if (!object) {
            return object.error();
        }
        auto first = parsing::readerObjectField<R>(object.value(), "first", NoTags{});
        if (!first) {
            return makeParserError(sa::ErrorCode::InvalidField, "Required pair field 'first' is missing");
        }
        auto result = parserRead<R>(first.value(), value.first);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse pair field 'first': ");
        }
        auto second = parsing::readerObjectField<R>(object.value(), "second", NoTags{});
        if (!second) {
            return makeParserError(sa::ErrorCode::InvalidField, "Required pair field 'second' is missing");
        }
        return parserContext(parserRead<R>(second.value(), value.second), "Failed to parse pair field 'second': ");
    }
};

template <typename K, typename V>
struct SchemaParser<std::pair<K, V>, void> {
    static auto toSchema() -> parsing::schema::Type {
        parsing::schema::Type::Object schema;
        schema.properties.emplace("first", parserSchema<K>());
        schema.properties.emplace("second", parserSchema<V>());
        schema.required = {"first", "second"};
        return schema;
    }
};

} // namespace detail
} // namespace nekoproto
