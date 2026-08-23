#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <memory>
#include <utility>

namespace nekoproto {
namespace detail {

template <typename W, typename T>
struct WriteParser<W, std::shared_ptr<T>, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const std::shared_ptr<T>& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if (!value) {
            parsing::Parent<W>::addNull(writer, parent, tags);
            return sa::success();
        }
        return parserWrite<W>(writer, *value, parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, std::shared_ptr<T>, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::shared_ptr<T>& value, const Tags& tags) -> ParserResult {
        if (parsing::readerIsEmpty<R>(in, tags)) {
            value.reset();
            return sa::success();
        }
        auto tmp    = std::make_shared<T>();
        auto result = parserRead<R>(in, *tmp, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse shared_ptr value: ");
        }
        value = std::move(tmp);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<std::shared_ptr<T>, void> {
    static auto toSchema() -> parsing::schema::Type {
        return parsing::schema::Type::Optional{std::make_shared<parsing::schema::Type>(parserSchema<T>())};
    }
};

template <typename W, typename T>
struct WriteParser<W, std::unique_ptr<T>, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const std::unique_ptr<T>& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if (!value) {
            parsing::Parent<W>::addNull(writer, parent, tags);
            return sa::success();
        }
        return parserWrite<W>(writer, *value, parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, std::unique_ptr<T>, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::unique_ptr<T>& value, const Tags& tags) -> ParserResult {
        if (parsing::readerIsEmpty<R>(in, tags)) {
            value.reset();
            return sa::success();
        }
        auto tmp    = std::make_unique<T>();
        auto result = parserRead<R>(in, *tmp, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse unique_ptr value: ");
        }
        value = std::move(tmp);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<std::unique_ptr<T>, void> {
    static auto toSchema() -> parsing::schema::Type {
        return parsing::schema::Type::Optional{std::make_shared<parsing::schema::Type>(parserSchema<T>())};
    }
};

} // namespace detail
} // namespace nekoproto
