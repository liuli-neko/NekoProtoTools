#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <memory>
#include <optional>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename R, typename W, typename T>
struct Parser<R, W, std::optional<T>, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::optional<T>& value, const ParentType& parent, const Tags& tags) {
        if (!value.has_value()) {
            parsing::Parent<W>::addNull(writer, parent);
            return sa::success();
        }
        return parser_write<R, W>(writer, *value, parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::optional<T>& value, const Tags& tags) {
        if (R::isEmpty(in)) {
            value.reset();
            return sa::success();
        }
        T parsed{};
        auto result = parser_read<R, W>(in, parsed, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse optional value: ");
        }
        value = std::move(parsed);
        return sa::success();
    }

    static parsing::schema::Type toSchema() {
        return parsing::schema::Type::Optional{std::make_shared<parsing::schema::Type>(parser_schema<R, W, T>())};
    }
};

} // namespace detail
NEKO_END_NAMESPACE
