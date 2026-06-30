#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <memory>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename W, typename T>
struct WriteParser<W, std::shared_ptr<T>, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::shared_ptr<T>& value, const ParentType& parent, const Tags& tags) {
        if (!value) {
            parsing::Parent<W>::addNull(writer, parent);
            return sa::success();
        }
        return parser_write<W>(writer, *value, parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, std::shared_ptr<T>, void> {
    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::shared_ptr<T>& value, const Tags& tags) {
        if (R::isEmpty(in)) {
            value.reset();
            return sa::success();
        }
        auto tmp    = std::make_shared<T>();
        auto result = parser_read<R>(in, *tmp, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse shared_ptr value: ");
        }
        value = std::move(tmp);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<std::shared_ptr<T>, void> {
    static parsing::schema::Type toSchema() {
        return parsing::schema::Type::Optional{std::make_shared<parsing::schema::Type>(parser_schema<T>())};
    }
};

template <typename W, typename T>
struct WriteParser<W, std::unique_ptr<T>, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::unique_ptr<T>& value, const ParentType& parent, const Tags& tags) {
        if (!value) {
            parsing::Parent<W>::addNull(writer, parent);
            return sa::success();
        }
        return parser_write<W>(writer, *value, parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, std::unique_ptr<T>, void> {
    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::unique_ptr<T>& value, const Tags& tags) {
        if (R::isEmpty(in)) {
            value.reset();
            return sa::success();
        }
        auto tmp    = std::make_unique<T>();
        auto result = parser_read<R>(in, *tmp, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse unique_ptr value: ");
        }
        value = std::move(tmp);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<std::unique_ptr<T>, void> {
    static parsing::schema::Type toSchema() {
        return parsing::schema::Type::Optional{std::make_shared<parsing::schema::Type>(parser_schema<T>())};
    }
};

} // namespace detail
NEKO_END_NAMESPACE
