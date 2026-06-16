#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"

#include <atomic>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename R, typename W, typename T>
struct Parser<R, W, std::atomic<T>, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const std::atomic<T>& value, const ParentType& parent, const Tags& tags) {
        return parser_write<R, W>(writer, value.load(), parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::atomic<T>& value, const Tags& tags) {
        T parsed{};
        auto result = parser_read<R, W>(in, parsed, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse atomic value: ");
        }
        value.store(parsed);
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parser_schema<R, W, T>(); }
};

} // namespace detail
NEKO_END_NAMESPACE
