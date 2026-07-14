#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/parsing/parent.hpp"
#include "nekoproto/serialization/parsing/reader.hpp"
#include "nekoproto/serialization/parsing/schema/type.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <concepts>
#include <string>
#include <type_traits>

NEKO_BEGIN_NAMESPACE

using ParserResult = sa::Result<void>;

template <typename T, typename Enable = void>
struct CustomParser {};

template <typename W, typename T, typename ParentType, typename Tags = NoTags>
inline ParserResult parser_write(W& writer, const T& value, const ParentType& parent, const Tags& tags = Tags{});

template <typename R, typename T, typename Tags = NoTags>
inline ParserResult parser_read(typename R::InputValueType in, T& value, const Tags& tags = Tags{});

template <typename T, typename Tags = NoTags>
inline parsing::schema::Type parser_schema(const Tags& tags = Tags{});

namespace detail {

inline ParserResult parser_error(sa::ErrorCode code, std::string message) { return sa::Err(code, std::move(message)); }

// Build an empty transactional target without discarding stateful container
// policy objects. This keeps parse-then-commit compatible with comparators,
// hashes and allocators that are not default constructible.
template <typename T>
T parser_empty_container_like(const T& value) {
    if constexpr (requires {
                      T(0U, value.hash_function(), value.key_eq(), value.get_allocator());
                  }) {
        return T(0U, value.hash_function(), value.key_eq(), value.get_allocator());
    } else if constexpr (requires {
                             T(value.key_comp(), value.get_allocator());
                         }) {
        return T(value.key_comp(), value.get_allocator());
    } else if constexpr (requires {
                             T(value.get_allocator());
                         }) {
        return T(value.get_allocator());
    } else {
        return T{};
    }
}

inline ParserResult parser_context(ParserResult result, std::string context) {
    if (result) {
        return result;
    }
    auto error = result.error();
    error.msg  = std::move(context) + error.msg;
    return sa::Err(std::move(error));
}

template <typename W, typename T, typename Enable = void>
struct WriteParser {
    template <typename ParentType, typename Tags>
        requires requires(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
            { NEKO_NAMESPACE::CustomParser<T>::write(writer, value, parent, tags) } -> std::same_as<ParserResult>;
        }
    static ParserResult write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
        return NEKO_NAMESPACE::CustomParser<T>::write(writer, value, parent, tags);
    }
};

template <typename R, typename T, typename Enable = void>
struct ReadParser {
    template <typename Tags>
        requires requires(typename R::InputValueType in, T& value, const Tags& tags) {
            { NEKO_NAMESPACE::CustomParser<T>::template read<R>(in, value, tags) } -> std::same_as<ParserResult>;
        }
    static ParserResult read(typename R::InputValueType in, T& value, const Tags& tags) {
        return NEKO_NAMESPACE::CustomParser<T>::template read<R>(in, value, tags);
    }
};

template <typename T, typename Enable = void>
struct SchemaParser {
    static parsing::schema::Type toSchema()
        requires requires {
            { NEKO_NAMESPACE::CustomParser<T>::toSchema() } -> std::same_as<parsing::schema::Type>;
        }
    {
        return NEKO_NAMESPACE::CustomParser<T>::toSchema();
    }
};

template <typename W, typename T>
concept parser_writable =
    requires(W& writer, const std::decay_t<T>& value, typename parsing::Parent<W>::Root parent, NoTags tags) {
        { WriteParser<W, std::decay_t<T>>::write(writer, value, parent, tags) } -> std::same_as<ParserResult>;
    };

template <typename R, typename T>
concept parser_readable = requires(typename R::InputValueType in, std::decay_t<T>& value, NoTags tags) {
    { ReadParser<R, std::decay_t<T>>::read(in, value, tags) } -> std::same_as<ParserResult>;
};

template <typename T>
concept parser_schema_available = requires {
    { SchemaParser<std::decay_t<T>>::toSchema() } -> std::same_as<parsing::schema::Type>;
};

template <typename R, typename W, typename T>
concept parser_serializable = parser_writable<W, T> && parser_readable<R, T>;

template <typename W, auto Tags, typename Accessor>
struct WriteParser<W, TaggedField<Tags, Accessor>, void> {
    using Field = TaggedField<Tags, Accessor>;

    template <typename ParentType, typename ParentTags>
    static ParserResult write(W& writer, const Field& field, const ParentType& parent, const ParentTags& /*tags*/) {
        using ValueType = std::remove_cvref_t<typename Field::accessor_type>;
        static_assert(perform_check<ValueType, Tags>(), "Tag check failed for tagged serializer value");
        return parser_write<W>(writer, field.accessor, parent, Tags);
    }
};

template <typename R, auto Tags, typename Accessor>
struct ReadParser<R, TaggedField<Tags, Accessor>, void> {
    using Field = TaggedField<Tags, Accessor>;

    template <typename ParentTags>
    static ParserResult read(typename R::InputValueType in, Field& field, const ParentTags& /*tags*/) {
        using ValueType = std::remove_cvref_t<typename Field::accessor_type>;
        static_assert(perform_check<ValueType, Tags>(), "Tag check failed for tagged serializer value");
        return parser_read<R>(in, field.accessor, Tags);
    }
};

template <auto Tags, typename Accessor>
struct SchemaParser<TaggedField<Tags, Accessor>, void> {
    using Field = TaggedField<Tags, Accessor>;

    static parsing::schema::Type toSchema() {
        return parser_schema<std::remove_cvref_t<typename Field::accessor_type>>(Tags);
    }
};

} // namespace detail

template <typename W, typename T, typename ParentType, typename Tags>
inline ParserResult parser_write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
    using ValueType = std::decay_t<T>;
    if constexpr (requires {
                      { CustomParser<ValueType>::write(writer, value, parent, tags) } -> std::same_as<ParserResult>;
                  }) {
        return CustomParser<ValueType>::write(writer, value, parent, tags);
    } else if constexpr (requires { detail::WriteParser<W, ValueType>::write(writer, value, parent, tags); }) {
        return detail::WriteParser<W, ValueType>::write(writer, value, parent, tags);
    } else {
        static_assert(always_false_v<T>, "No WriteParser or CustomParser write support for this type/writer");
        return detail::parser_error(sa::ErrorCode::InvalidType,
                                    "No WriteParser or CustomParser write support for this type/writer");
    }
}

template <typename R, typename T, typename Tags>
inline ParserResult parser_read(typename R::InputValueType in, T& value, const Tags& tags) {
    using ValueType = std::decay_t<T>;
    if constexpr (requires {
                      { CustomParser<ValueType>::template read<R>(in, value, tags) } -> std::same_as<ParserResult>;
                  }) {
        return CustomParser<ValueType>::template read<R>(in, value, tags);
    } else if constexpr (requires { detail::ReadParser<R, ValueType>::read(in, value, tags); }) {
        return detail::ReadParser<R, ValueType>::read(in, value, tags);
    } else {
        static_assert(always_false_v<T>, "No ReadParser or CustomParser read support for this type/reader");
        return detail::parser_error(sa::ErrorCode::InvalidType,
                                    "No ReadParser or CustomParser read support for this type/reader");
    }
}

template <typename T, typename Tags>
inline parsing::schema::Type parser_schema(const Tags& tags) {
    using ValueType = std::decay_t<T>;
    if constexpr (requires {
                      { CustomParser<ValueType>::toSchema(tags) } -> std::same_as<parsing::schema::Type>;
                  }) {
        return CustomParser<ValueType>::toSchema(tags);
    } else if constexpr (requires {
                      { CustomParser<ValueType>::toSchema() } -> std::same_as<parsing::schema::Type>;
                  }) {
        return CustomParser<ValueType>::toSchema();
    } else if constexpr (requires { detail::SchemaParser<ValueType>::toSchema(tags); }) {
        return detail::SchemaParser<ValueType>::toSchema(tags);
    } else if constexpr (requires { detail::SchemaParser<ValueType>::toSchema(); }) {
        return detail::SchemaParser<ValueType>::toSchema();
    } else {
        static_assert(always_false_v<T>, "No SchemaParser or CustomParser schema support for this type");
    }
}

NEKO_END_NAMESPACE
