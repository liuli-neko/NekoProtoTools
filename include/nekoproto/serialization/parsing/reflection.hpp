#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/supports_unframed_objects.hpp"
#include "nekoproto/serialization/private/traits.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename R, typename W, typename T>
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
    if (tag_access::is_skipable(tags)) {
        return sa::success();
    }
    using ValueType = std::decay_t<FieldT>;
    if constexpr (traits::optional_like_type<ValueType>::value) {
        traits::optional_like_type<ValueType>::set_null(field);
        return sa::success();
    }
    return parser_error(sa::ErrorCode::InvalidField, "Required field '" + std::string(name) + "' is missing");
}

template <typename R, typename W, typename T>
ParserResult parser_write_reflect_fields(W& writer, typename W::OutputObjectType& object, const T& value);

template <typename R, typename W, typename T>
ParserResult parser_read_reflect_fields(typename R::InputValueType in, T& value);

inline void parser_schema_add_required(parsing::schema::Type::Object& object, std::string name) {
    if (std::find(object.required.begin(), object.required.end(), name) == object.required.end()) {
        object.required.push_back(std::move(name));
    }
}

template <typename R, typename W, typename FieldT, typename Tags>
void parser_schema_add_reflect_field(parsing::schema::Type::Object& object, std::string_view name, const Tags& tags) {
    auto fieldSchema = parser_schema<R, W, std::decay_t<FieldT>>();
    if (tag_access::is_fixed_length(tags)) {
        fieldSchema.fixedLength = tag_access::fixed_length<std::decay_t<FieldT>>(tags);
    }
    if (tag_access::is_flat(tags)) {
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
    object.properties.insert_or_assign(fieldName, std::move(fieldSchema));
    if constexpr (!traits::optional_like_type<std::decay_t<FieldT>>::value) {
        if (!tag_access::is_skipable(tags)) {
            parser_schema_add_required(object, std::move(fieldName));
        }
    }
}

template <typename R, typename W, typename T>
parsing::schema::Type parser_schema_named_reflection() {
    parsing::schema::Type::Object object;
    Reflect<T>::forEachMeta([&]<typename Field>(std::type_identity<Field>, std::string_view name, const auto& tags) {
        parser_schema_add_reflect_field<R, W, Field>(object, name, tags);
    });
    return object;
}

template <typename R, typename W, typename T>
parsing::schema::Type parser_schema_positional_reflection() {
    parsing::schema::Type::Array array;
    Reflect<T>::forEachMeta([&]<typename Field>(std::type_identity<Field>, const auto& tags) {
        auto fieldSchema = parser_schema<R, W, Field>();
        if (tag_access::is_fixed_length(tags)) {
            fieldSchema.fixedLength = tag_access::fixed_length<std::decay_t<Field>>(tags);
        }
        array.prefixItems.emplace_back(std::move(fieldSchema));
    });
    array.minItems        = Reflect<T>::value_count;
    array.maxItems        = Reflect<T>::value_count;
    array.additionalItems = false;
    return array;
}

template <typename R, typename W, typename T, typename Tags>
ParserResult parser_write_reflect_field(W& writer, typename W::OutputObjectType& object, const T& field,
                                        std::string_view name, const Tags& tags) {
    using FieldType = std::decay_t<T>;
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !disable_reflect_parser<R, W, FieldType>::value) {
        if (tag_access::is_flat(tags)) {
            return parser_write_reflect_fields<R, W>(writer, object, field);
        }
    }
    if (parser_should_skip_empty_field(field, tags)) {
        return sa::success();
    }
#if defined(NEKO_WRITE_NULL_FOR_EMPTY_OPTIONAL)
    if constexpr (traits::optional_like_type<FieldType>::value) {
        if (!traits::optional_like_type<FieldType>::has_value(field)) {
            parsing::Parent<W>::addNull(writer, typename parsing::Parent<W>::Object{name, &object});
            return sa::success();
        }
    }
#endif
    if constexpr (tag_access::has_comment(Tags{})) {
        const auto rest = tag_access::consume_recursive_comment(tags);
        auto result     = parser_write_reflect_field<R, W, T>(writer, object, field, name, rest);
        if (result) {
            parsing::Parent<W>::addComment(writer, tag_access::recursive_comment(tags),
                                           typename parsing::Parent<W>::Object{name, &object});
        }
        return result;
    } else if constexpr (tag_access::has_name(Tags{})) {
        std::string_view fieldName = name;
        auto nextTags              = tag_access::consume_recursive_name(tags);
        fieldName                  = tag_access::recursive_name(tags);
        return parser_write_reflect_field<R, W, T>(writer, object, field, fieldName, nextTags);
    } else {
        return parser_context(parser_write<R, W>(writer, field, typename parsing::Parent<W>::Object{name, &object}, tags),
        "Failed to write field '" + std::string(name) + "': ");
    }
}

template <typename R, typename W, typename T, typename Tags>
ParserResult parser_read_reflect_field(typename R::InputValueType in, T& field, std::string_view name,
                                       const Tags& tags) {
    using FieldType = std::decay_t<T>;
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !disable_reflect_parser<R, W, FieldType>::value) {
        if (tag_access::is_flat(tags)) {
            return parser_read_reflect_fields<R, W>(in, field);
        }
    }
    auto object = R::toObject(in);
    if (!object) {
        return object.error();
    }
    std::string_view fieldName = name;
    if constexpr (tag_access::has_recursive_name(Tags{})) {
        fieldName = tag_access::recursive_name(tags);
    }
    auto nextTags   = tag_access::consume_recursive_comment(tag_access::consume_recursive_name(tags));
    auto fieldValue = R::objectField(object.value(), fieldName);
    if (!fieldValue) {
        return parser_read_missing_field(field, fieldName, nextTags);
    }
    if (R::isEmpty(fieldValue.value())) {
        if constexpr (traits::optional_like_type<FieldType>::value) {
            traits::optional_like_type<FieldType>::set_null(field);
            return sa::success();
        }
    }
    return parser_context(parser_read<R, W>(fieldValue.value(), field, nextTags),
                          "Failed to parse field '" + std::string(fieldName) + "': ");
}

template <typename R, typename W, typename T>
ParserResult parser_write_reflect_fields(W& writer, typename W::OutputObjectType& object, const T& value) {
    ParserResult result;
    Reflect<std::decay_t<T>>::forEach(
        value, [&result, &writer, &object](auto&& field, std::string_view name, const auto& tags) {
            if (result) {
                result = parser_write_reflect_field<R, W>(writer, object, field, name, tags);
            }
        });
    return result;
}

template <typename R, typename W, typename T>
ParserResult parser_read_reflect_fields(typename R::InputValueType in, T& value) {
    auto object = R::toObject(in);
    if (!object) {
        return object.error();
    }
    ParserResult result;
    Reflect<std::decay_t<T>>::forEach(value, [&result, in](auto&& field, std::string_view name, const auto& tags) {
        if (result) {
            result = parser_read_reflect_field<R, W>(in, field, name, tags);
        }
    });
    return result;
}

template <typename R, typename W, typename T>
struct Parser<R, W, T,
              std::enable_if_t<has_values_meta<T> && (!is_field_spec_v<T>) && (!std::is_enum_v<T>) &&
                               (!disable_reflect_parser<R, W, T>::value)>> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
        if constexpr (has_names_meta<T>) {
            if constexpr (parsing::supports_unframed_objects<R, W>) {
                if (tag_access::is_unframed(tags)) {
                    parsing::Parent<W>::beginUnframedObject(writer, parent);
                    ParserResult result;
                    Reflect<T>::forEach(value, [&writer, &result](const auto& field, std::string_view name,
                                                                  const auto& tags) {
                        if (result) {
                            result = parser_context(
                                parser_write<R, W>(writer, field, typename parsing::Parent<W>::Root{}, tags),
                                "Failed to write field '" + std::string(name) + "': ");
                        }
                    });
                    return result;
                }
            }
            auto object = parsing::Parent<W>::addObject(writer, Reflect<T>::size(), parent);
            return parser_write_reflect_fields<R, W>(writer, object, value);
        } else {
            auto array = parsing::Parent<W>::addArray(writer, Reflect<T>::size(), parent);
            ParserResult result;
            std::size_t index = 0;
            Reflect<T>::forEach(value, [&writer, &array, &result, &index](auto&& field, const auto& tags) {
                if (result) {
                    result = parser_context(
                        parser_write<R, W>(writer, field, typename parsing::Parent<W>::Array{&array}, tags),
                        "Failed to write reflected element " + std::to_string(index) + ": ");
                }
                ++index;
            });
            return result;
        }
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, T& value, const Tags& tags) {
        if constexpr (has_names_meta<T>) {
            if constexpr (parsing::supports_unframed_objects<R, W>) {
                if (tag_access::is_unframed(tags)) {
                    ParserResult result;
                    auto current = in;
                    Reflect<T>::forEach(value,
                                        [&current, &result](auto& field, std::string_view name, const auto& tags) {
                                            if (result) {
                                                result =
                                                    parser_context(parser_read<R, W>(current, field, tags),
                                                                   "Failed to parse field '" + std::string(name) + "': ");
                                                current = R::next(current);
                                            }
                                        });
                    return result;
                }
            }
            return parser_read_reflect_fields<R, W>(in, value);
        } else {
            auto array = R::toArray(in);
            if (!array) {
                return array.error();
            }
            const auto actualSize   = R::arraySize(array.value());
            const auto expectedSize = Reflect<T>::size();
            if (actualSize != expectedSize) {
                return parser_error(sa::ErrorCode::InvalidLength, "Expected reflected array with " +
                                                                      std::to_string(expectedSize) + " elements, got " +
                                                                      std::to_string(actualSize));
            }
            ParserResult result;
            std::size_t index = 0;
            Reflect<T>::forEach(value, [&array, &result, &index](auto&& field, const auto& tags) {
                if (result) {
                    result = parser_context(parser_read<R, W>(R::arrayElement(array.value(), index), field, tags),
                                            "Failed to parse reflected element " + std::to_string(index) + ": ");
                }
                ++index;
            });
            return result;
        }
    }

    static parsing::schema::Type toSchema() {
        parsing::schema::Type schema;
        if constexpr (has_names_meta<T>) {
            schema = parser_schema_named_reflection<R, W, T>();
        } else {
            schema = parser_schema_positional_reflection<R, W, T>();
        }
        return schema;
    }
};

} // namespace detail
NEKO_END_NAMESPACE
