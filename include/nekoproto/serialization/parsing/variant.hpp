#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/reflection.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

namespace nekoproto {
namespace detail {

template <auto... Tags>
struct UnionPayloadTagList;

template <>
struct UnionPayloadTagList<> {
    constexpr static auto value = TagList<>{};
};

template <auto Head, auto... Tail>
struct UnionPayloadTagList<Head, Tail...> {
    constexpr static auto tail  = UnionPayloadTagList<Tail...>::value;
    constexpr static auto value = []() consteval {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(Head)>, UnionTag>) {
            return tail;
        } else {
            return concatTagLists(TagList<Head>{}, tail);
        }
    }();
};

// UnionTag describes this union boundary. Strip it before delegating to the
// active payload so nested union-like values keep their own default policy.
template <typename Tags>
constexpr auto unionPayloadTags(const Tags& tags) {
    using RawTags = std::remove_cvref_t<Tags>;
    if constexpr (std::is_same_v<RawTags, UnionTag>) {
        return NoTags{};
    } else if constexpr (is_tag_list_v<RawTags>) {
        return []<auto... Values>(TagList<Values...>) consteval {
            using Filtered = std::remove_cvref_t<decltype(UnionPayloadTagList<Values...>::value)>;
            return NormalizeTagList<Filtered>::value;
        }(RawTags{});
    } else {
        return tags;
    }
}

template <>
struct DisableReflectParser<std::monostate> : std::true_type {};

template <typename W>
struct WriteParser<W, std::monostate, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const std::monostate&, const ParentType& parent, const Tags& tags) -> ParserResult {
        parsing::Parent<W>::addNull(writer, parent, tags);
        return sa::success();
    }
};

template <typename R>
struct ReadParser<R, std::monostate, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::monostate&, const Tags& tags) -> ParserResult {
        if (!parsing::readerIsEmpty<R>(in, tags)) {
            return makeParserError(sa::ErrorCode::InvalidType, "Expected null for monostate");
        }
        return sa::success();
    }
};

template <>
struct SchemaParser<std::monostate, void> {
    static auto toSchema() -> parsing::schema::Type { return parsing::schema::Type::Null{}; }
};

template <typename W, typename... Ts>
struct WriteParser<W, std::variant<Ts...>, void> {
    using Variant = std::variant<Ts...>;

    template <std::size_t I = 0, typename ParentType, typename Tags>
    static auto writeActive(W& writer, const Variant& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if constexpr (I >= sizeof...(Ts)) {
            return makeParserError(sa::ErrorCode::InvalidIndex, "Variant active index is out of range");
        } else {
            if (value.index() == I) {
                return parserWrite<W>(writer, std::get<I>(value), parent, tags);
            }
            return writeActive<I + 1>(writer, value, parent, tags);
        }
    }

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Variant& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        const auto payloadTags = unionPayloadTags(tags);
        if (tag_query::get<tag_property::UnionEncodingProperty>(tags) == UnionEncoding::Untagged) {
            return writeActive(writer, value, parent, payloadTags);
        }

        auto array  = parsing::Parent<W>::addArray(writer, 2, parent, payloadTags);
        auto result = parserWrite<W>(writer, value.index(), typename parsing::Parent<W>::Array{&array});
        if (!result) {
            return parserContext(std::move(result), "Failed to write variant index: ");
        }
        return writeActive(writer, value, typename parsing::Parent<W>::Array{&array}, payloadTags);
    }
};

template <typename R, typename... Ts>
struct ReadParser<R, std::variant<Ts...>, void> {
    using Variant = std::variant<Ts...>;

    template <typename Alt, typename Tags>
    static auto tryAlternative(typename R::InputValueType in, const Tags& tags) -> ParserResult {
        Alt tmp{};
        if constexpr (requires {
                          R::checkpoint(in);
                          R::restore(R::checkpoint(in));
                      }) {
            auto checkpoint = R::checkpoint(in);
            auto result     = parserRead<R>(in, tmp, tags);
            R::restore(checkpoint);
            return result;
        } else {
            // Tree-backed readers do not mutate their input while converting a
            // value. A destructive custom reader should expose checkpoint and
            // restore with the same shape as binary::Reader.
            return parserRead<R>(in, tmp, tags);
        }
    }

    template <std::size_t I = 0, typename Tags>
    static void findAlternatives(typename R::InputValueType in, const Tags& tags, std::size_t& matchCount,
                                 std::size_t& matchIndex) {
        if constexpr (I < sizeof...(Ts)) {
            using Alt = std::variant_alternative_t<I, Variant>;
            if (tryAlternative<Alt>(in, tags)) {
                ++matchCount;
                matchIndex = I;
            }
            findAlternatives<I + 1>(in, tags, matchCount, matchIndex);
        }
    }

    template <typename Tags>
    static auto readUntagged(typename R::InputValueType in, Variant& value, const Tags& tags) -> ParserResult {
        std::size_t matchCount = 0;
        std::size_t matchIndex = 0;
        findAlternatives(in, tags, matchCount, matchIndex);
        if (matchCount == 0U) {
            return makeParserError(sa::ErrorCode::ParseError, "No variant alternative matched the input value");
        }
        if (matchCount != 1U) {
            return makeParserError(sa::ErrorCode::ParseError, "Untagged variant input matches more than one alternative");
        }
        return readAlternative(matchIndex, in, value, tags);
    }

    template <std::size_t I = 0, typename Tags>
    static auto readAlternative(std::size_t index, typename R::InputValueType in, Variant& value,
                                        const Tags& tags) -> ParserResult {
        if constexpr (I >= sizeof...(Ts)) {
            return makeParserError(sa::ErrorCode::InvalidIndex, "Variant alternative index is out of range");
        } else {
            if (index == I) {
                using Alt = std::variant_alternative_t<I, Variant>;
                Alt tmp{};
                auto result = parserRead<R>(in, tmp, tags);
                if (!result) {
                    return parserContext(std::move(result), "Failed to parse variant alternative: ");
                }
                value = std::move(tmp);
                return result;
            }
            return readAlternative<I + 1>(index, in, value, tags);
        }
    }

    template <typename Tags>
    static auto read(typename R::InputValueType in, Variant& value, const Tags& tags) -> ParserResult {
        const auto payloadTags = unionPayloadTags(tags);
        if (tag_query::get<tag_property::UnionEncodingProperty>(tags) == UnionEncoding::Untagged) {
            return readUntagged(in, value, payloadTags);
        }

        auto array = parsing::readerToArray<R>(in, payloadTags);
        if (!array) {
            return parserContext(array.error(), "Variant must be encoded as [index, value]: ");
        }
        if (R::arraySize(array.value()) != 2U) {
            return makeParserError(sa::ErrorCode::InvalidLength, "Variant array must contain exactly two elements");
        }
        std::size_t index = 0;
        auto result       = parserRead<R>(R::arrayElement(array.value(), 0), index);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse variant index: ");
        }
        return readAlternative(index, R::arrayElement(array.value(), 1), value, payloadTags);
    }
};

template <typename... Ts>
struct SchemaParser<std::variant<Ts...>, void> {
    static auto toSchema() -> parsing::schema::Type { return toSchema(NoTags{}); }

    template <typename Tags>
    static auto toSchema(const Tags& tags) -> parsing::schema::Type {
        const auto payloadTags = unionPayloadTags(tags);
        auto alternatives      = parsing::schema::Type::AnyOf{{parserSchema<Ts>(payloadTags)...}};
        if (tag_query::get<tag_property::UnionEncodingProperty>(tags) == UnionEncoding::Untagged) {
            return alternatives;
        }

        parsing::schema::Type::Integer index;
        index.minimum = parsing::schema::Type::Number{std::uint64_t{0}};
        index.maximum = parsing::schema::Type::Number{std::uint64_t{sizeof...(Ts) - 1U}};

        parsing::schema::Type::Array array;
        array.prefixItems     = {std::move(index), std::move(alternatives)};
        array.minItems        = 2U;
        array.maxItems        = 2U;
        array.additionalItems = false;
        return array;
    }
};

} // namespace detail
} // namespace nekoproto
