#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/private/helpers.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename R, typename W, std::size_t N>
struct Parser<R, W, std::bitset<N>, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::bitset<N>& value, const ParentType& parent, const Tags& tags) {
        return parser_write<R, W>(writer, value.to_string('0', '1'), parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::bitset<N>& value, const Tags& tags) {
        std::string str;
        auto result = parser_read<R, W>(in, str, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse bitset: ");
        }
        if (str.size() != N) {
            return parser_error(sa::ErrorCode::InvalidLength,
                                "Expected bitset string with " + std::to_string(N) + " characters, got " +
                                    std::to_string(str.size()));
        }
        for (const auto ch : str) {
            if (ch != '0' && ch != '1') {
                return parser_error(sa::ErrorCode::ParseError,
                                    "Bitset string contains a character other than '0' or '1'");
            }
        }
        value = std::bitset<N>(str);
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parsing::schema::Type::String{}; }
};

template <typename R, typename W>
struct Parser<R, W, std::byte, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::byte& value, const ParentType& parent, const Tags& tags) {
        return parser_write<R, W>(writer, static_cast<std::uint8_t>(value), parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::byte& value, const Tags& tags) {
        std::uint8_t tmp = 0;
        auto result = parser_read<R, W>(in, tmp, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse byte: ");
        }
        value = static_cast<std::byte>(tmp);
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parser_schema<R, W, std::uint8_t>(); }
};

template <typename R, typename W, typename T>
struct Parser<R, W, NameValuePair<T>, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const NameValuePair<T>& value, const ParentType& parent, const Tags& tags) {
        auto object = parsing::Parent<W>::addObject(writer, 1, parent);
        return parser_write<R, W>(writer, value.value,
                                  typename parsing::Parent<W>::Object{{value.name, value.nameLen}, &object}, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, NameValuePair<T>& value, const Tags& tags) {
        auto object = R::toObject(in);
        if (!object) {
            return object.error();
        }
        auto field = R::objectField(object.value(), {value.name, value.nameLen});
        if (!field) {
            return parser_error(sa::ErrorCode::InvalidField,
                                "Required field '" + std::string(value.name, value.nameLen) + "' is missing");
        }
        return parser_context(parser_read<R, W>(field.value(), value.value, tags),
                              "Failed to parse field '" + std::string(value.name, value.nameLen) + "': ");
    }

    static parsing::schema::Type toSchema() {
        parsing::schema::Type::Object object;
        object.additionalProperties =
            std::make_shared<parsing::schema::Type>(parser_schema<R, W, T>());
        return object;
    }
};

} // namespace detail
NEKO_END_NAMESPACE
