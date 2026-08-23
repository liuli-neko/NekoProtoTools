#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <atomic>
#include <utility>

namespace nekoproto {
namespace detail {

template <typename W, typename T>
struct WriteParser<W, std::atomic<T>, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const std::atomic<T>& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return parserWrite<W>(writer, value.load(), parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, std::atomic<T>, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::atomic<T>& value, const Tags& tags) -> ParserResult {
        T parsed{};
        auto result = parserRead<R>(in, parsed, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse atomic value: ");
        }
        value.store(parsed);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<std::atomic<T>, void> {
    static auto toSchema() -> parsing::schema::Type { return parserSchema<T>(); }
};

} // namespace detail
} // namespace nekoproto
