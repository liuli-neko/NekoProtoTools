#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/parsing/parent.hpp"
#include "nekoproto/serialization/parsing/schema/type.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <concepts>
#include <string>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
namespace detail {

using ParserResult = sa::Result<void>;

template <typename R, typename W, typename T, typename Enable = void>
struct CustomParser {};

inline ParserResult parser_error(sa::ErrorCode code, std::string message) {
    return sa::error(code, std::move(message));
}

inline ParserResult parser_context(ParserResult result, std::string context) {
    if (result) {
        return result;
    }
    auto error = result.error();
    error.msg  = std::move(context) + error.msg;
    return error;
}

template <typename R, typename W, typename T, typename Enable = void>
struct Parser {
    template <typename ParentType, typename Tags>
        requires requires(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
            { CustomParser<R, W, T>::write(writer, value, parent, tags) } -> std::same_as<ParserResult>;
        }
    static ParserResult write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
        return CustomParser<R, W, T>::write(writer, value, parent, tags);
    }

    template <typename Tags>
        requires requires(typename R::InputValueType in, T& value, const Tags& tags) {
            { CustomParser<R, W, T>::read(in, value, tags) } -> std::same_as<ParserResult>;
        }
    static ParserResult read(typename R::InputValueType in, T& value, const Tags& tags) {
        return CustomParser<R, W, T>::read(in, value, tags);
    }

    static parsing::schema::Type toSchema()
        requires requires {
            { CustomParser<R, W, T>::toSchema() } -> std::same_as<parsing::schema::Type>;
        }
    {
        return CustomParser<R, W, T>::toSchema();
    }
};

template <typename R, typename W, typename T>
concept parser_writable =
    requires(W& writer, const std::decay_t<T>& value, typename parsing::Parent<W>::Root parent, NoTags tags) {
        { Parser<R, W, std::decay_t<T>>::write(writer, value, parent, tags) } -> std::same_as<ParserResult>;
    };

template <typename R, typename W, typename T>
concept parser_readable = requires(typename R::InputValueType in, std::decay_t<T>& value, NoTags tags) {
    { Parser<R, W, std::decay_t<T>>::read(in, value, tags) } -> std::same_as<ParserResult>;
};

template <typename R, typename W, typename T>
concept parser_serializable = parser_writable<R, W, T> && parser_readable<R, W, T>;

template <typename R, typename W, typename T, typename ParentType, typename Tags = NoTags>
inline ParserResult parser_write(W& writer, const T& value, const ParentType& parent, const Tags& tags = Tags{});

template <typename R, typename W, typename T, typename Tags = NoTags>
inline ParserResult parser_read(typename R::InputValueType in, T& value, const Tags& tags = Tags{});

template <typename R, typename W, typename T, typename ParentType, typename Tags>
inline ParserResult parser_write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
    using ValueType = std::decay_t<T>;
    using TagType   = std::remove_cvref_t<Tags>;
    if constexpr (tag_access::has_recursive_comment(TagType{})) {
        const auto rest = tag_access::consume_recursive_comment(tags);
        auto result     = parser_write<R, W>(writer, value, parent, rest);
        if (result) {
            parsing::Parent<W>::addComment(writer, tag_access::recursive_comment(tags), parent);
        }
        return result;
    } else if constexpr (requires { Parser<R, W, ValueType>::write(writer, value, parent, tags); }) {
        return Parser<R, W, ValueType>::write(writer, value, parent, tags);
    } else {
        static_assert(always_false_v<T>, "No Parser or CustomParser write support for this type/reader/writer");
        return parser_error(sa::ErrorCode::InvalidType,
                            "No Parser or CustomParser write support for this type/reader/writer");
    }
}

template <typename R, typename W, typename T, typename Tags>
inline ParserResult parser_read(typename R::InputValueType in, T& value, const Tags& tags) {
    using ValueType = std::decay_t<T>;
    using TagType   = std::remove_cvref_t<Tags>;
    if constexpr (tag_access::has_recursive_comment(TagType{})) {
        return parser_read<R, W>(in, value, tag_access::consume_recursive_comment(tags));
    } else if constexpr (requires { Parser<R, W, ValueType>::read(in, value, tags); }) {
        return Parser<R, W, ValueType>::read(in, value, tags);
    } else {
        static_assert(always_false_v<T>, "No Parser or CustomParser read support for this type/reader/writer");
        return parser_error(sa::ErrorCode::InvalidType,
                            "No Parser or CustomParser read support for this type/reader/writer");
    }
}

template <typename R, typename W, typename T>
inline parsing::schema::Type parser_schema() {
    using ValueType = std::decay_t<T>;
    if constexpr (requires { Parser<R, W, ValueType>::toSchema(); }) {
        return Parser<R, W, ValueType>::toSchema();
    } else {
        static_assert(always_false_v<T>, "No Parser or CustomParser schema support for this type/reader/writer");
    }
}

template <typename R, typename W, auto Tags, typename Accessor>
struct Parser<R, W, FieldSpec<Tags, Accessor>, void> {
    using Field = FieldSpec<Tags, Accessor>;

    template <typename ParentType, typename ParentTags>
    static ParserResult write(W& writer, const Field& field, const ParentType& parent,
                              const ParentTags& /*tags*/) {
        return parser_write<R, W>(writer, field.accessor, parent, Tags);
    }

    template <typename ParentTags>
    static ParserResult read(typename R::InputValueType in, Field& field, const ParentTags& /*tags*/) {
        return parser_read<R, W>(in, field.accessor, Tags);
    }

    static parsing::schema::Type toSchema() {
        return parser_schema<R, W, std::remove_cvref_t<typename Field::accessor_type>>();
    }
};

} // namespace detail
NEKO_END_NAMESPACE
