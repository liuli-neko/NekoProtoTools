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

template <typename W, typename ObjectType, typename T>
ParserResult parser_write_reflect_fields(W& writer, ObjectType& object, const T& value);

template <typename R, typename T, typename Tags = NoTags>
ParserResult parser_read_reflect_fields(typename R::InputValueType in, T& value, const Tags& tags = {});

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

template <typename T>
std::size_t parser_reflect_emitted_field_count(const T& value);

template <typename FieldT, typename Tags>
std::size_t parser_reflect_emitted_field_count_one(const FieldT& field, const Tags& tags) {
    using FieldType = std::decay_t<FieldT>;
    if (parser_should_ignore_reflect_field(tags) || parser_should_skip_empty_field(field, tags)) {
        return 0;
    }
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !disable_reflect_parser<FieldType>::value) {
        if (tag_query::get<tag_property::flat<FieldType>>(tags)) {
            return parser_reflect_emitted_field_count(field);
        }
    }
    return 1;
}

template <typename T>
std::size_t parser_reflect_emitted_field_count(const T& value) {
    std::size_t count = 0;
    Reflect<std::decay_t<T>>::forEach(
        value, [&count](const auto& field, std::string_view /*name*/, const auto& tags) {
            count += parser_reflect_emitted_field_count_one(field, tags);
        });
    return count;
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
    auto fieldSchema = parser_schema<std::decay_t<FieldT>>(tags);
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
        auto fieldSchema = parser_schema<Field>(tags);
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

template <typename W, typename ObjectType, typename T, typename Tags>
ParserResult parser_write_reflect_field(W& writer, ObjectType& object, const T& field,
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
            const auto writeNull = [&](const auto& parent) {
                parser_write_leading_comment(writer, parent, tags);
                parsing::Parent<W>::addNull(writer, parent, tags);
                parser_write_trailing_comment(writer, parent, tags);
            };
            if constexpr (std::is_same_v<ObjectType, typename W::OutputObjectType>) {
                writeNull(typename parsing::Parent<W>::Object{name, &object});
            } else {
                writeNull(typename parsing::Parent<W>::IdObject{name, &object});
            }
            return sa::success();
        }
    }
#endif
    std::string_view fieldName = name;
    if constexpr (tag_query::has<tag_property::name>(Tags{})) {
        fieldName = tag_query::get<tag_property::name>(tags);
    }
    const auto writeField = [&](const auto& parent) {
        parser_write_leading_comment(writer, parent, tags);
        auto result = parser_context(parser_write<W>(writer, field, parent, tags),
                                     "Failed to write field '" + std::string(fieldName) + "': ");
        if (result) {
            parser_write_trailing_comment(writer, parent, tags);
        }
        return result;
    };
    if constexpr (std::is_same_v<ObjectType, typename W::OutputObjectType>) {
        return writeField(typename parsing::Parent<W>::Object{fieldName, &object});
    } else {
        return writeField(typename parsing::Parent<W>::IdObject{fieldName, &object});
    }
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
    // Field tags describe the field boundary and child node, not the
    // containing reflected object.
    auto object = parsing::reader_to_object<R>(in, NoTags{});
    if (!object) {
        return object.error();
    }
    std::string_view fieldName = name;
    if constexpr (tag_query::has<tag_property::name>(Tags{})) {
        fieldName = tag_query::get<tag_property::name>(tags);
    }
    auto fieldValue = parsing::reader_object_field<R>(object.value(), fieldName, tags);
    if (!fieldValue) {
        return parser_read_missing_field(field, fieldName, tags);
    }
    if (parsing::reader_is_empty<R>(fieldValue.value(), tags)) {
        if constexpr (traits::optional_like_type<FieldType>::value) {
            traits::optional_like_type<FieldType>::set_null(field);
            return sa::success();
        }
    }
    return parser_context(parser_read<R>(fieldValue.value(), field, tags),
                          "Failed to parse field '" + std::string(fieldName) + "': ");
}

template <typename W, typename ObjectType, typename T>
ParserResult parser_write_reflect_fields(W& writer, ObjectType& object, const T& value) {
    ParserResult result;
    Reflect<std::decay_t<T>>::forEach(
        value, [&result, &writer, &object](auto&& field, std::string_view name, const auto& tags) {
            if (result) {
                result = parser_write_reflect_field<W>(writer, object, field, name, tags);
            }
        });
    return result;
}

template <typename R, typename T, typename Tags>
ParserResult parser_read_reflect_fields(typename R::InputValueType in, T& value, const Tags& tags) {
    auto object = parsing::reader_to_object<R>(in, tags);
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
            if constexpr (requires { writer.beginRawFixedDataAsRoot(); }) {
                if (tag_query::get<tag_property::raw_fixed_data>(tags)) {
                    using ParentTypeValue = std::remove_cvref_t<ParentType>;
                    if constexpr (!std::is_same_v<ParentTypeValue, typename parsing::Parent<W>::Root>) {
                        return parser_error(sa::ErrorCode::InvalidType,
                                            "raw_fixed_data is only valid for a binary root value");
                    } else {
                        parsing::Parent<W>::beginRawFixedData(writer, parent);
                        ParserResult result;
                        Reflect<T>::forEach(
                            value, [&writer, &result](const auto& field, std::string_view name, const auto& fieldTags) {
                                if (!result || parser_should_ignore_reflect_field(fieldTags)) {
                                    return;
                                }
                                using FieldType = std::remove_cvref_t<decltype(field)>;
                                if (!tag_query::has<tag_property::fixed_length<void>>(fieldTags)) {
                                    result = parser_error(
                                        sa::ErrorCode::InvalidLength,
                                        "raw_fixed_data field '" + std::string(name) + "' requires fixed_length");
                                    return;
                                }
                                if constexpr (std::is_enum_v<FieldType>) {
                                    using Underlying = std::underlying_type_t<FieldType>;
                                    result = parser_context(
                                        parser_write<W>(writer, static_cast<Underlying>(field),
                                                        typename parsing::Parent<W>::Root{}, fieldTags),
                                        "Failed to write raw fixed field '" + std::string(name) + "': ");
                                } else if constexpr (std::is_arithmetic_v<FieldType>) {
                                    result = parser_context(
                                        parser_write<W>(writer, field, typename parsing::Parent<W>::Root{}, fieldTags),
                                        "Failed to write raw fixed field '" + std::string(name) + "': ");
                                } else {
                                    result = parser_error(
                                        sa::ErrorCode::InvalidType,
                                        "raw_fixed_data field '" + std::string(name) +
                                            "' must be an arithmetic or enum value with a fixed wire width");
                                }
                            });
                        return result;
                    }
                }
            }
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
            const auto fieldCount = parser_reflect_emitted_field_count(value);
            if constexpr (requires { typename W::OutputIdObjectType; }) {
                auto object = parsing::Parent<W>::addIdObject(writer, fieldCount, parent, tags);
                return parser_write_reflect_fields<W>(writer, object, value);
            } else {
                auto object = parsing::Parent<W>::addObject(writer, fieldCount, parent, tags);
                return parser_write_reflect_fields<W>(writer, object, value);
            }
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
        if constexpr (std::is_move_assignable_v<T> && std::is_copy_constructible_v<T>) {
            T parsed = value;
            auto result = readInPlace(in, parsed, tags);
            if (result) {
                value = std::move(parsed);
            }
            return result;
        } else if constexpr (std::is_move_assignable_v<T> && std::is_default_constructible_v<T>) {
            T parsed{};
            auto result = readInPlace(in, parsed, tags);
            if (result) {
                value = std::move(parsed);
            }
            return result;
        } else {
            return readInPlace(in, value, tags);
        }
    }

private:
    template <typename Tags>
    static ParserResult readInPlace(typename R::InputValueType in, T& value, const Tags& tags) {
        if constexpr (has_names_meta<T>) {
            if constexpr (requires { R::isRaw(in); }) {
                if (tag_query::get<tag_property::raw_fixed_data>(tags)) {
                    if (!R::isRaw(in)) {
                        return parser_error(sa::ErrorCode::InvalidType,
                                            "raw_fixed_data requires a raw binary input segment");
                    }
                    ParserResult result;
                    auto current = in;
                    Reflect<T>::forEach(
                        value, [&current, &result](auto& field, std::string_view name, const auto& fieldTags) {
                            if (!result || parser_should_ignore_reflect_field(fieldTags)) {
                                return;
                            }
                            using FieldType = std::remove_cvref_t<decltype(field)>;
                            if (!tag_query::has<tag_property::fixed_length<void>>(fieldTags)) {
                                result = parser_error(
                                    sa::ErrorCode::InvalidLength,
                                    "raw_fixed_data field '" + std::string(name) + "' requires fixed_length");
                                return;
                            }
                            if constexpr (std::is_enum_v<FieldType>) {
                                std::underlying_type_t<FieldType> raw{};
                                result = parser_context(parser_read<R>(current, raw, fieldTags),
                                                        "Failed to parse raw fixed field '" + std::string(name) +
                                                            "': ");
                                if (result) {
                                    field = static_cast<FieldType>(raw);
                                }
                            } else if constexpr (std::is_arithmetic_v<FieldType>) {
                                result = parser_context(parser_read<R>(current, field, fieldTags),
                                                        "Failed to parse raw fixed field '" + std::string(name) +
                                                            "': ");
                            } else {
                                result = parser_error(
                                    sa::ErrorCode::InvalidType,
                                    "raw_fixed_data field '" + std::string(name) +
                                        "' must be an arithmetic or enum value with a fixed wire width");
                            }
                            if (result) {
                                current = R::next(current);
                            }
                        });
                    return result;
                }
            }
            if constexpr (parsing::supports_unframed_object_reader<R>) {
                if (tag_query::get<tag_property::unframed<std::decay_t<T>>>(tags)) {
                    if constexpr (requires { R::isFramedObject(in); }) {
                        if (R::isFramedObject(in)) {
                            return parser_read_reflect_fields<R>(in, value, tags);
                        }
                    }
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
            return parser_read_reflect_fields<R>(in, value, tags);
        } else {
            auto array = parsing::reader_to_array<R>(in, tags);
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
