#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/reflection.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename R, typename W>
struct disable_reflect_parser<R, W, std::monostate> : std::true_type {};

template <typename R, typename W>
struct Parser<R, W, std::monostate, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::monostate&, const ParentType& parent, const Tags& /*tags*/) {
        parsing::Parent<W>::addNull(writer, parent);
        return sa::success();
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::monostate&, const Tags& /*tags*/) {
        if (!R::isEmpty(in)) {
            return parser_error(sa::ErrorCode::InvalidType, "Expected null for monostate");
        }
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parsing::schema::Type::Null{}; }
};

template <typename R, typename W, typename... Ts>
struct Parser<R, W, std::variant<Ts...>, void> {
    using Variant = std::variant<Ts...>;

    template <std::size_t I = 0, typename ParentType, typename Tags>
    static ParserResult writeActive(W& writer, const Variant& value, const ParentType& parent, const Tags& tags) {
        if constexpr (I >= sizeof...(Ts)) {
            return parser_error(sa::ErrorCode::InvalidIndex, "Variant active index is out of range");
        } else {
            if (value.index() == I) {
                return parser_write<R, W>(writer, std::get<I>(value), parent, tags);
            }
            return writeActive<I + 1>(writer, value, parent, tags);
        }
    }

    template <std::size_t I = 0, typename Tags>
    static ParserResult readAny(typename R::InputValueType in, Variant& value, const Tags& tags) {
        if constexpr (I >= sizeof...(Ts)) {
            return parser_error(sa::ErrorCode::ParseError, "No variant alternative matched the input value");
        } else {
            using Alt = std::variant_alternative_t<I, Variant>;
            Alt tmp{};
            auto result = parser_read<R, W>(in, tmp, tags);
            if (result) {
                value = std::move(tmp);
                return sa::success();
            }
            return readAny<I + 1>(in, value, tags);
        }
    }

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Variant& value, const ParentType& parent, const Tags& tags) {
        return writeActive(writer, value, parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Variant& value, const Tags& tags) {
        return readAny(in, value, tags);
    }

    static parsing::schema::Type toSchema() {
        return parsing::schema::Type::AnyOf{{parser_schema<R, W, Ts>()...}};
    }
};

} // namespace detail
NEKO_END_NAMESPACE
