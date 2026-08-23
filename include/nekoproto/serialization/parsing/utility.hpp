#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/private/helpers.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nekoproto {
namespace detail {

template <typename W, std::size_t N>
struct WriteParser<W, std::bitset<N>, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const std::bitset<N>& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return parserWrite<W>(writer, value.to_string('0', '1'), parent, tags);
    }
};

template <typename R, std::size_t N>
struct ReadParser<R, std::bitset<N>, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::bitset<N>& value, const Tags& tags) -> ParserResult {
        std::string str;
        auto result = parserRead<R>(in, str, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse bitset: ");
        }
        if (str.size() != N) {
            return parserError(sa::ErrorCode::InvalidLength, "Expected bitset string with " + std::to_string(N) +
                                                                  " characters, got " + std::to_string(str.size()));
        }
        for (const auto ch : str) {
            if (ch != '0' && ch != '1') {
                return parserError(sa::ErrorCode::ParseError,
                                    "Bitset string contains a character other than '0' or '1'");
            }
        }
        value = std::bitset<N>(str);
        return sa::success();
    }
};

template <std::size_t N>
struct SchemaParser<std::bitset<N>, void> {
    static auto toSchema() -> parsing::schema::Type { return parsing::schema::Type::String{}; }
};

template <typename W>
struct WriteParser<W, std::byte, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const std::byte& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return parserWrite<W>(writer, static_cast<std::uint8_t>(value), parent, tags);
    }
};

template <typename R>
struct ReadParser<R, std::byte, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::byte& value, const Tags& tags) -> ParserResult {
        std::uint8_t tmp = 0;
        auto result      = parserRead<R>(in, tmp, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse byte: ");
        }
        value = static_cast<std::byte>(tmp);
        return sa::success();
    }
};

template <>
struct SchemaParser<std::byte, void> {
    static auto toSchema() -> parsing::schema::Type { return parserSchema<std::uint8_t>(); }
};

template <typename W, typename T>
struct WriteParser<W, NameValuePair<T>, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const NameValuePair<T>& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        auto object = parsing::Parent<W>::addObject(writer, 1, parent, tags);
        return parserWrite<W>(writer, value.value,
                               typename parsing::Parent<W>::Object{{value.name, value.nameLen}, &object});
    }
};

template <typename R, typename T>
struct ReadParser<R, NameValuePair<T>, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, NameValuePair<T>& value, const Tags& tags) -> ParserResult {
        auto object = parsing::readerToObject<R>(in, tags);
        if (!object) {
            return object.error();
        }
        auto field =
            parsing::readerObjectField<R>(object.value(), std::string_view{value.name, value.nameLen}, NoTags{});
        if (!field) {
            return parserError(sa::ErrorCode::InvalidField,
                                "Required field '" + std::string(value.name, value.nameLen) + "' is missing");
        }
        return parserContext(parserRead<R>(field.value(), value.value),
                              "Failed to parse field '" + std::string(value.name, value.nameLen) + "': ");
    }
};

template <typename T>
struct SchemaParser<NameValuePair<T>, void> {
    static auto toSchema() -> parsing::schema::Type {
        parsing::schema::Type::Object object;
        object.additionalProperties = std::make_shared<parsing::schema::Type>(parserSchema<T>());
        return object;
    }
};

} // namespace detail
} // namespace nekoproto
