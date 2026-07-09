#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/supports_unframed_objects.hpp"
#include "nekoproto/global/traits.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename T>
struct disable_reflect_parser : std::false_type {};

template <typename FieldT, typename Tags>
bool parser_should_skip_empty_field(const FieldT& field, const Tags& /*tags*/) {
    using ValueType = std::decay_t<FieldT>;
    if constexpr (traits::optional_like_type<ValueType>::value) {
        if (!traits::optional_like_type<ValueType>::has_value(field)) {
#if defined(NEKO_WRITE_NULL_FOR_EMPTY_OPTIONAL)
            return false;
#else
            return true;
#endif
        }
    }
    return false;
}

template <typename FieldT, typename Tags>
ParserResult parser_read_missing_field(FieldT& field, std::string_view name, const Tags& tags) {
    if (tag_query::get<tag_property::skippable>(tags)) {
        return sa::success();
    }
    using ValueType = std::decay_t<FieldT>;
    if constexpr (traits::optional_like_type<ValueType>::value) {
        traits::optional_like_type<ValueType>::set_null(field);
        return sa::success();
    }
    return parser_error(sa::ErrorCode::InvalidField, "Required field '" + std::string(name) + "' is missing");
}

template <typename W, typename T>
ParserResult parser_write_reflect_fields(W& writer, typename W::OutputObjectType& object, const T& value);

template <typename R, typename T>
ParserResult parser_read_reflect_fields(typename R::InputValueType in, T& value);

template <typename Tags>
constexpr bool parser_should_ignore_reflect_field(const Tags& tags) {
    return tag_query::get<tag_property::ignore>(tags);
}

template <typename T, std::size_t... Is>
consteval std::size_t parser_reflect_field_count_impl(std::index_sequence<Is...> /*unused*/) {
    return (std::size_t{0} + ... +
            (tag_query::get<tag_property::ignore>(std::get<Is>(Reflect<std::decay_t<T>>::field_tags))
                 ? std::size_t{0}
                 : std::size_t{1}));
}

template <typename T>
consteval std::size_t parser_reflect_field_count() {
    return parser_reflect_field_count_impl<std::decay_t<T>>(
        std::make_index_sequence<Reflect<std::decay_t<T>>::value_count>{});
}

inline void parser_schema_add_required(parsing::schema::Type::Object& object, std::string name) {
    if (std::find(object.required.begin(), object.required.end(), name) == object.required.end()) {
        object.required.push_back(std::move(name));
    }
}

template <typename FieldT, typename Tags>
void parser_schema_add_reflect_field(parsing::schema::Type::Object& object, std::string_view name, const Tags& tags) {
    if (parser_should_ignore_reflect_field(tags)) {
        return;
    }
    auto fieldSchema = parser_schema<std::decay_t<FieldT>>();
    if (tag_query::has<tag_property::fixed_length<void>>(tags)) {
        fieldSchema.fixed_length = tag_query::get<tag_property::fixed_length<std::decay_t<FieldT>>>(tags);
    }
    if (tag_query::get<tag_property::flat<std::decay_t<FieldT>>>(tags)) {
        const auto& unwrapped = parsing::schema::unwrapOptional(fieldSchema);
        if (const auto* nested = std::get_if<parsing::schema::Type::Object>(&unwrapped.value)) {
            object.properties.insert(nested->properties.begin(), nested->properties.end());
            for (const auto& required : nested->required) {
                parser_schema_add_required(object, required);
            }
            return;
        }
    }

    auto fieldName = std::string(name);
    if constexpr (tag_query::has<tag_property::name>(Tags{})) {
        fieldName = std::string(tag_query::get<tag_property::name>(tags));
    }
    object.properties.insert_or_assign(fieldName, std::move(fieldSchema));
    if constexpr (!traits::optional_like_type<std::decay_t<FieldT>>::value) {
        if (!tag_query::get<tag_property::skippable>(tags)) {
            parser_schema_add_required(object, std::move(fieldName));
        }
    }
}

template <typename W, typename ParentType, typename Tags>
void parser_write_leading_comment(W& writer, const ParentType& parent, const Tags& tags) {
    if constexpr (tag_query::has<tag_property::leading_comment>(Tags{})) {
        parsing::Parent<W>::addComment(writer, tag_query::get<tag_property::leading_comment>(tags), parent);
    }
}

template <typename W, typename ParentType, typename Tags>
void parser_write_trailing_comment(W& writer, const ParentType& parent, const Tags& tags) {
    if constexpr (tag_query::has<tag_property::trailing_comment>(Tags{})) {
        parsing::Parent<W>::addComment(writer, tag_query::get<tag_property::trailing_comment>(tags), parent);
    }
}

template <typename T>
parsing::schema::Type parser_schema_named_reflection() {
    parsing::schema::Type::Object object;
    Reflect<T>::forEachMeta([&]<typename Field>(std::type_identity<Field>, std::string_view name, const auto& tags) {
        parser_schema_add_reflect_field<Field>(object, name, tags);
    });
    return object;
}

template <typename T>
parsing::schema::Type parser_schema_positional_reflection() {
    parsing::schema::Type::Array array;
    Reflect<T>::forEachMeta([&]<typename Field>(std::type_identity<Field>, const auto& tags) {
        if (parser_should_ignore_reflect_field(tags)) {
            return;
        }
        auto fieldSchema = parser_schema<Field>();
        if (tag_query::has<tag_property::fixed_length<void>>(tags)) {
            fieldSchema.fixed_length = tag_query::get<tag_property::fixed_length<std::decay_t<Field>>>(tags);
        }
        array.prefixItems.emplace_back(std::move(fieldSchema));
    });
    array.minItems        = parser_reflect_field_count<T>();
    array.maxItems        = parser_reflect_field_count<T>();
    array.additionalItems = false;
    return array;
}

template <typename W, typename T, typename Tags>
ParserResult parser_write_reflect_field(W& writer, typename W::OutputObjectType& object, const T& field,
                                        std::string_view name, const Tags& tags) {
    using FieldType = std::decay_t<T>;
    if (parser_should_ignore_reflect_field(tags)) {
        return sa::success();
    }
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !disable_reflect_parser<FieldType>::value) {
        if (tag_query::get<tag_property::flat<FieldType>>(tags)) {
            return parser_write_reflect_fields<W>(writer, object, field);
        }
    }
    if (parser_should_skip_empty_field(field, tags)) {
        return sa::success();
    }
#if defined(NEKO_WRITE_NULL_FOR_EMPTY_OPTIONAL)
    if constexpr (traits::optional_like_type<FieldType>::value) {
        if (!traits::optional_like_type<FieldType>::has_value(field)) {
            const auto parent = typename parsing::Parent<W>::Object{name, &object};
            parser_write_leading_comment(writer, parent, tags);
            parsing::Parent<W>::addNull(writer, parent, tags);
            parser_write_trailing_comment(writer, parent, tags);
            return sa::success();
        }
    }
#endif
    std::string_view fieldName = name;
    if constexpr (tag_query::has<tag_property::name>(Tags{})) {
        fieldName = tag_query::get<tag_property::name>(tags);
    }
    const auto parent = typename parsing::Parent<W>::Object{fieldName, &object};
    parser_write_leading_comment(writer, parent, tags);
    auto result       = parser_context(parser_write<W>(writer, field, parent, tags),
                                       "Failed to write field '" + std::string(fieldName) + "': ");
    if (result) {
        parser_write_trailing_comment(writer, parent, tags);
    }
    return result;
}

template <typename R, typename T, typename Tags>
ParserResult parser_read_reflect_field(typename R::InputValueType in, T& field, std::string_view name,
                                       const Tags& tags) {
    using FieldType = std::decay_t<T>;
    if (parser_should_ignore_reflect_field(tags)) {
        return sa::success();
    }
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !disable_reflect_parser<FieldType>::value) {
        if (tag_query::get<tag_property::flat<FieldType>>(tags)) {
            return parser_read_reflect_fields<R>(in, field);
        }
    }
    auto object = R::toObject(in);
    if (!object) {
        return object.error();
    }
    std::string_view fieldName = name;
    if constexpr (tag_query::has<tag_property::name>(Tags{})) {
        fieldName = tag_query::get<tag_property::name>(tags);
    }
    auto fieldValue = R::objectField(object.value(), fieldName);
    if (!fieldValue) {
        return parser_read_missing_field(field, fieldName, tags);
    }
    if (R::isEmpty(fieldValue.value())) {
        if constexpr (traits::optional_like_type<FieldType>::value) {
            traits::optional_like_type<FieldType>::set_null(field);
            return sa::success();
        }
    }
    return parser_context(parser_read<R>(fieldValue.value(), field, tags),
                          "Failed to parse field '" + std::string(fieldName) + "': ");
}

template <typename W, typename T>
ParserResult parser_write_reflect_fields(W& writer, typename W::OutputObjectType& object, const T& value) {
    ParserResult result;
    Reflect<std::decay_t<T>>::forEach(
        value, [&result, &writer, &object](auto&& field, std::string_view name, const auto& tags) {
            if (result) {
                result = parser_write_reflect_field<W>(writer, object, field, name, tags);
            }
        });
    return result;
}

template <typename R, typename T>
ParserResult parser_read_reflect_fields(typename R::InputValueType in, T& value) {
    auto object = R::toObject(in);
    if (!object) {
        return object.error();
    }
    ParserResult result;
    Reflect<std::decay_t<T>>::forEach(value, [&result, in](auto&& field, std::string_view name, const auto& tags) {
        if (result) {
            result = parser_read_reflect_field<R>(in, field, name, tags);
        }
    });
    return result;
}

template <typename W, typename T>
struct WriteParser<W, T,
                   std::enable_if_t<has_values_meta<T> && (!is_tagged_field_v<T>) && (!std::is_enum_v<T>) &&
                                    (!disable_reflect_parser<T>::value)>> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
        if constexpr (has_names_meta<T>) {
            if constexpr (parsing::supports_unframed_object_writer<W>) {
                if (tag_query::get<tag_property::unframed<std::decay_t<T>>>(tags)) {
                    parsing::Parent<W>::beginUnframedObject(writer, parent);
                    ParserResult result;
                    Reflect<T>::forEach(
                        value, [&writer, &result](const auto& field, std::string_view name, const auto& tags) {
                            if (result) {
                                if (parser_should_ignore_reflect_field(tags)) {
                                    return;
                                }
                                result = parser_context(
                                    parser_write<W>(writer, field, typename parsing::Parent<W>::Root{}, tags),
                                    "Failed to write field '" + std::string(name) + "': ");
                            }
                        });
                    return result;
                }
            }
            auto object = parsing::Parent<W>::addObject(writer, parser_reflect_field_count<T>(), parent, tags);
            return parser_write_reflect_fields<W>(writer, object, value);
        } else {
            auto array = parsing::Parent<W>::addArray(writer, parser_reflect_field_count<T>(), parent, tags);
            ParserResult result;
            std::size_t index = 0;
            Reflect<T>::forEach(value, [&writer, &array, &result, &index](auto&& field, const auto& tags) {
                if (result) {
                    if (parser_should_ignore_reflect_field(tags)) {
                        ++index;
                        return;
                    }
                    const auto parent = typename parsing::Parent<W>::Array{&array};
                    parser_write_leading_comment(writer, parent, tags);
                    result            = parser_context(parser_write<W>(writer, field, parent, tags),
                                                       "Failed to write reflected element " + std::to_string(index) + ": ");
                    if (result) {
                        parser_write_trailing_comment(writer, parent, tags);
                    }
                }
                ++index;
            });
            return result;
        }
    }
};

template <typename R, typename T>
struct ReadParser<R, T,
                  std::enable_if_t<has_values_meta<T> && (!is_tagged_field_v<T>) && (!std::is_enum_v<T>) &&
                                   (!disable_reflect_parser<T>::value)>> {
    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, T& value, const Tags& tags) {
        if constexpr (has_names_meta<T>) {
            if constexpr (parsing::supports_unframed_object_reader<R>) {
                if (tag_query::get<tag_property::unframed<std::decay_t<T>>>(tags)) {
                    ParserResult result;
                    auto current = in;
                    Reflect<T>::forEach(
                        value, [&current, &result](auto& field, std::string_view name, const auto& tags) {
                            if (result) {
                                if (parser_should_ignore_reflect_field(tags)) {
                                    return;
                                }
                                result  = parser_context(parser_read<R>(current, field, tags),
                                                         "Failed to parse field '" + std::string(name) + "': ");
                                current = R::next(current);
                            }
                        });
                    return result;
                }
            }
            return parser_read_reflect_fields<R>(in, value);
        } else {
            auto array = R::toArray(in);
            if (!array) {
                return array.error();
            }
            const auto actualSize   = R::arraySize(array.value());
            const auto expectedSize = parser_reflect_field_count<T>();
            if (actualSize != expectedSize) {
                return parser_error(sa::ErrorCode::InvalidLength, "Expected reflected array with " +
                                                                      std::to_string(expectedSize) + " elements, got " +
                                                                      std::to_string(actualSize));
            }
            ParserResult result;
            std::size_t index = 0;
            std::size_t elementIndex = 0;
            Reflect<T>::forEach(value, [&array, &result, &index, &elementIndex](auto&& field, const auto& tags) {
                if (result) {
                    if (parser_should_ignore_reflect_field(tags)) {
                        ++index;
                        return;
                    }
                    result = parser_context(parser_read<R>(R::arrayElement(array.value(), elementIndex), field, tags),
                                            "Failed to parse reflected element " + std::to_string(index) + ": ");
                    ++elementIndex;
                }
                ++index;
            });
            return result;
        }
    }
};

template <typename T>
struct SchemaParser<T, std::enable_if_t<has_values_meta<T> && (!is_tagged_field_v<T>) && (!std::is_enum_v<T>) &&
                                        (!disable_reflect_parser<T>::value)>> {
    static parsing::schema::Type toSchema() {
        parsing::schema::Type schema;
        if constexpr (has_names_meta<T>) {
            schema = parser_schema_named_reflection<T>();
        } else {
            schema = parser_schema_positional_reflection<T>();
        }
        return schema;
    }
};

} // namespace detail
NEKO_END_NAMESPACE
