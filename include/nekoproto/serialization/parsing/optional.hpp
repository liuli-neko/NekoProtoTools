#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <memory>
#include <optional>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename W, typename T>
struct WriteParser<W, std::optional<T>, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::optional<T>& value, const ParentType& parent, const Tags& tags) {
        if (!value.has_value()) {
            parsing::Parent<W>::addNull(writer, parent, tags);
            return sa::success();
        }
        return parser_write<W>(writer, *value, parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, std::optional<T>, void> {
    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::optional<T>& value, const Tags& tags) {
        if (R::isEmpty(in)) {
            value.reset();
            return sa::success();
        }
        T parsed{};
        auto result = parser_read<R>(in, parsed, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse optional value: ");
        }
        value = std::move(parsed);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<std::optional<T>, void> {
    static parsing::schema::Type toSchema() {
        return parsing::schema::Type::Optional{std::make_shared<parsing::schema::Type>(parser_schema<T>())};
    }
};

} // namespace detail
NEKO_END_NAMESPACE
