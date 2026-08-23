#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <memory>
#include <optional>
#include <utility>

namespace nekoproto {
namespace detail {

template <typename W, typename T>
struct WriteParser<W, std::optional<T>, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const std::optional<T>& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if (!value.has_value()) {
            parsing::Parent<W>::addNull(writer, parent, tags);
            return sa::success();
        }
        return parserWrite<W>(writer, *value, parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, std::optional<T>, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::optional<T>& value, const Tags& tags) -> ParserResult {
        if (parsing::readerIsEmpty<R>(in, tags)) {
            value.reset();
            return sa::success();
        }
        T parsed{};
        auto result = parserRead<R>(in, parsed, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse optional value: ");
        }
        value = std::move(parsed);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<std::optional<T>, void> {
    static auto toSchema() -> parsing::schema::Type {
        return parsing::schema::Type::Optional{std::make_shared<parsing::schema::Type>(parserSchema<T>())};
    }
};

} // namespace detail
} // namespace nekoproto
