#pragma once

#include "nekoproto/global/traits.hpp"
#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/supports_unframed_objects.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nekoproto {
namespace detail {

template <typename T>
struct DisableReflectParser : std::false_type {};

template <typename FieldT, typename Tags>
auto parserShouldSkipEmptyField(const FieldT& field, const Tags& /*tags*/) -> bool {
    using ValueType = std::decay_t<FieldT>;
    if constexpr (traits::OptionalLikeType<ValueType>::value) {
        if (!traits::OptionalLikeType<ValueType>::hasValue(field)) {
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
auto parserReadMissingField(FieldT& field, std::string_view name, const Tags& tags) -> ParserResult {
    if (tag_query::get<tag_property::Skippable>(tags)) {
        return sa::success();
    }
    using ValueType = std::decay_t<FieldT>;
    if constexpr (traits::OptionalLikeType<ValueType>::value) {
        traits::OptionalLikeType<ValueType>::setNull(field);
        return sa::success();
    }
    return parserError(sa::ErrorCode::InvalidField, "Required field '" + std::string(name) + "' is missing");
}

template <typename W, typename ObjectType, typename T>
auto parserWriteReflectFields(W& writer, ObjectType& object, const T& value) -> ParserResult;

template <typename R, typename T, typename Tags = NoTags>
auto parserReadReflectFields(typename R::InputValueType in, T& value, const Tags& tags = {}) -> ParserResult;

template <typename Tags>
constexpr auto parserShouldIgnoreReflectField(const Tags& tags) -> bool {
    return tag_query::get<tag_property::Ignore>(tags);
}

template <typename T, std::size_t... Is>
consteval auto parserReflectFieldCountImpl(std::index_sequence<Is...> /*unused*/) -> std::size_t {
    return (std::size_t{0} + ... +
            (tag_query::get<tag_property::Ignore>(std::get<Is>(Reflect<std::decay_t<T>>::field_tags))
                 ? std::size_t{0}
                 : std::size_t{1}));
}

template <typename T>
consteval auto parserReflectFieldCount() -> std::size_t {
    return parserReflectFieldCountImpl<std::decay_t<T>>(
        std::make_index_sequence<Reflect<std::decay_t<T>>::value_count>{});
}

template <typename T>
auto parserReflectEmittedFieldCount(const T& value) -> std::size_t;

template <typename FieldT, typename Tags>
auto parserReflectEmittedFieldCountOne(const FieldT& field, const Tags& tags) -> std::size_t {
    using FieldType = std::decay_t<FieldT>;
    if (parserShouldIgnoreReflectField(tags) || parserShouldSkipEmptyField(field, tags)) {
        return 0;
    }
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !DisableReflectParser<FieldType>::value) {
        if (tag_query::get<tag_property::Flat<FieldType>>(tags)) {
            return parserReflectEmittedFieldCount(field);
        }
    }
    return 1;
}

template <typename T>
auto parserReflectEmittedFieldCount(const T& value) -> std::size_t {
    std::size_t count = 0;
    Reflect<std::decay_t<T>>::visitFull(value,
                                        [&count](const auto& field, std::string_view /*name*/, const auto& tags) {
                                            count += parserReflectEmittedFieldCountOne(field, tags);
                                        });
    return count;
}

inline void parserSchemaAddRequired(parsing::schema::Type::Object& object, std::string name) {
    if (std::find(object.required.begin(), object.required.end(), name) == object.required.end()) {
        object.required.push_back(std::move(name));
    }
}

template <typename FieldT, typename Tags>
void parserSchemaAddReflectField(parsing::schema::Type::Object& object, std::string_view name, const Tags& tags) {
    if (parserShouldIgnoreReflectField(tags)) {
        return;
    }
    auto fieldSchema = parserSchema<std::decay_t<FieldT>>(tags);
    if (tag_query::has<tag_property::FixedLength<void>>(tags)) {
        fieldSchema.fixed_length = tag_query::get<tag_property::FixedLength<std::decay_t<FieldT>>>(tags);
    }
    if (tag_query::get<tag_property::Flat<std::decay_t<FieldT>>>(tags)) {
        const auto& unwrapped = parsing::schema::unwrapOptional(fieldSchema);
        if (const auto* nested = std::get_if<parsing::schema::Type::Object>(&unwrapped.value)) {
            object.properties.insert(nested->properties.begin(), nested->properties.end());
            for (const auto& required : nested->required) {
                parserSchemaAddRequired(object, required);
            }
            return;
        }
    }

    auto fieldName = std::string(name);
    if constexpr (tag_query::has<tag_property::Name>(Tags{})) {
        fieldName = std::string(tag_query::get<tag_property::Name>(tags));
    }
    object.properties.insert_or_assign(fieldName, std::move(fieldSchema));
    if constexpr (!traits::OptionalLikeType<std::decay_t<FieldT>>::value) {
        if (!tag_query::get<tag_property::Skippable>(tags)) {
            parserSchemaAddRequired(object, std::move(fieldName));
        }
    }
}

template <typename W, typename ParentType, typename Tags>
void parserWriteLeadingComment(W& writer, const ParentType& parent, const Tags& tags) {
    if constexpr (tag_query::has<tag_property::LeadingComment>(Tags{})) {
        parsing::Parent<W>::addComment(writer, tag_query::get<tag_property::LeadingComment>(tags), parent);
    }
}

template <typename W, typename ParentType, typename Tags>
void parserWriteTrailingComment(W& writer, const ParentType& parent, const Tags& tags) {
    if constexpr (tag_query::has<tag_property::TrailingComment>(Tags{})) {
        parsing::Parent<W>::addComment(writer, tag_query::get<tag_property::TrailingComment>(tags), parent);
    }
}

template <typename T>
auto parserSchemaNamedReflection() -> parsing::schema::Type {
    parsing::schema::Type::Object object;
    Reflect<T>::visitMetaFull(
        [&]<typename Field>(std::type_identity<Field>, std::string_view name, const auto& tags) {
            parserSchemaAddReflectField<Field>(object, name, tags);
        });
    return object;
}

template <typename T>
auto parserSchemaPositionalReflection() -> parsing::schema::Type {
    parsing::schema::Type::Array array;
    Reflect<T>::visitMetaTyped([&]<typename Field>(std::type_identity<Field>, const auto& tags) {
        if (parserShouldIgnoreReflectField(tags)) {
            return;
        }
        auto fieldSchema = parserSchema<Field>(tags);
        if (tag_query::has<tag_property::FixedLength<void>>(tags)) {
            fieldSchema.fixed_length = tag_query::get<tag_property::FixedLength<std::decay_t<Field>>>(tags);
        }
        array.prefixItems.emplace_back(std::move(fieldSchema));
    });
    array.minItems        = parserReflectFieldCount<T>();
    array.maxItems        = parserReflectFieldCount<T>();
    array.additionalItems = false;
    return array;
}

template <typename W, typename ObjectType, typename T, typename Tags>
auto parserWriteReflectField(W& writer, ObjectType& object, const T& field, std::string_view name,
                                        const Tags& tags) -> ParserResult {
    using FieldType = std::decay_t<T>;
    if (parserShouldIgnoreReflectField(tags)) {
        return sa::success();
    }
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !DisableReflectParser<FieldType>::value) {
        if (tag_query::get<tag_property::Flat<FieldType>>(tags)) {
            return parserWriteReflectFields<W>(writer, object, field);
        }
    }
    if (parserShouldSkipEmptyField(field, tags)) {
        return sa::success();
    }
#if defined(NEKO_WRITE_NULL_FOR_EMPTY_OPTIONAL)
    if constexpr (traits::OptionalLikeType<FieldType>::value) {
        if (!traits::OptionalLikeType<FieldType>::hasValue(field)) {
            const auto writeNull = [&](const auto& parent) {
                parserWriteLeadingComment(writer, parent, tags);
                parsing::Parent<W>::addNull(writer, parent, tags);
                parserWriteTrailingComment(writer, parent, tags);
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
    if constexpr (tag_query::has<tag_property::Name>(Tags{})) {
        fieldName = tag_query::get<tag_property::Name>(tags);
    }
    const auto writeField = [&](const auto& parent) {
        parserWriteLeadingComment(writer, parent, tags);
        auto result = parserContext(parserWrite<W>(writer, field, parent, tags),
                                     "Failed to write field '" + std::string(fieldName) + "': ");
        if (result) {
            parserWriteTrailingComment(writer, parent, tags);
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
auto parserReadReflectField(typename R::InputValueType in, T& field, std::string_view name,
                                       const Tags& tags) -> ParserResult {
    using FieldType = std::decay_t<T>;
    if (parserShouldIgnoreReflectField(tags)) {
        return sa::success();
    }
    if constexpr (has_values_meta<FieldType> && has_names_meta<FieldType> &&
                  !DisableReflectParser<FieldType>::value) {
        if (tag_query::get<tag_property::Flat<FieldType>>(tags)) {
            return parserReadReflectFields<R>(in, field);
        }
    }
    // Field tags describe the field boundary and child node, not the
    // containing reflected object.
    auto object = parsing::readerToObject<R>(in, NoTags{});
    if (!object) {
        return object.error();
    }
    std::string_view fieldName = name;
    if constexpr (tag_query::has<tag_property::Name>(Tags{})) {
        fieldName = tag_query::get<tag_property::Name>(tags);
    }
    auto fieldValue = parsing::readerObjectField<R>(object.value(), fieldName, tags);
    if (!fieldValue) {
        return parserReadMissingField(field, fieldName, tags);
    }
    if (parsing::readerIsEmpty<R>(fieldValue.value(), tags)) {
        if constexpr (traits::OptionalLikeType<FieldType>::value) {
            traits::OptionalLikeType<FieldType>::setNull(field);
            return sa::success();
        }
    }
    return parserContext(parserRead<R>(fieldValue.value(), field, tags),
                          "Failed to parse field '" + std::string(fieldName) + "': ");
}

template <typename W, typename ObjectType, typename T>
auto parserWriteReflectFields(W& writer, ObjectType& object, const T& value) -> ParserResult {
    ParserResult result;
    Reflect<std::decay_t<T>>::visitFull(
        value, [&result, &writer, &object](auto&& field, std::string_view name, const auto& tags) {
            if (result) {
                result = parserWriteReflectField<W>(writer, object, field, name, tags);
            }
        });
    return result;
}

template <typename R, typename T, typename Tags>
auto parserReadReflectFields(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
    auto object = parsing::readerToObject<R>(in, tags);
    if (!object) {
        return object.error();
    }
    ParserResult result;
    Reflect<std::decay_t<T>>::visitFull(value, [&result, in](auto&& field, std::string_view name, const auto& tags) {
        if (result) {
            result = parserReadReflectField<R>(in, field, name, tags);
        }
    });
    return result;
}

template <typename W, typename T>
struct WriteParser<W, T,
                   std::enable_if_t<has_values_meta<T> && (!is_tagged_field_v<T>) && (!std::is_enum_v<T>) &&
                                    (!DisableReflectParser<T>::value)>> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const T& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if constexpr (has_names_meta<T>) {
            if constexpr (requires { writer.beginRawFixedDataAsRoot(); }) {
                if (tag_query::get<tag_property::RawFixedData>(tags)) {
                    using ParentTypeValue = std::remove_cvref_t<ParentType>;
                    if constexpr (!std::is_same_v<ParentTypeValue, typename parsing::Parent<W>::Root>) {
                        return parserError(sa::ErrorCode::InvalidType,
                                            "raw_fixed_data is only valid for a binary root value");
                    } else {
                        parsing::Parent<W>::beginRawFixedData(writer, parent);
                        ParserResult result;
                        Reflect<T>::visitFull(value, [&writer, &result](const auto& field, std::string_view name,
                                                                        const auto& fieldTags) {
                            if (!result || parserShouldIgnoreReflectField(fieldTags)) {
                                return;
                            }
                            using FieldType = std::remove_cvref_t<decltype(field)>;
                            if (!tag_query::has<tag_property::FixedLength<void>>(fieldTags)) {
                                result = parserError(sa::ErrorCode::InvalidLength, "raw_fixed_data field '" +
                                                                                        std::string(name) +
                                                                                        "' requires fixed_length");
                                return;
                            }
                            if constexpr (std::is_enum_v<FieldType>) {
                                using Underlying = std::underlying_type_t<FieldType>;
                                result =
                                    parserContext(parserWrite<W>(writer, static_cast<Underlying>(field),
                                                                   typename parsing::Parent<W>::Root{}, fieldTags),
                                                   "Failed to write raw fixed field '" + std::string(name) + "': ");
                            } else if constexpr (std::is_arithmetic_v<FieldType>) {
                                result = parserContext(
                                    parserWrite<W>(writer, field, typename parsing::Parent<W>::Root{}, fieldTags),
                                    "Failed to write raw fixed field '" + std::string(name) + "': ");
                            } else {
                                result =
                                    parserError(sa::ErrorCode::InvalidType,
                                                 "raw_fixed_data field '" + std::string(name) +
                                                     "' must be an arithmetic or enum value with a fixed wire width");
                            }
                        });
                        return result;
                    }
                }
            }
            if constexpr (parsing::supports_unframed_object_writer<W>) {
                if (tag_query::get<tag_property::Unframed<std::decay_t<T>>>(tags)) {
                    parsing::Parent<W>::beginUnframedObject(writer, parent);
                    ParserResult result;
                    Reflect<T>::visitFull(
                        value, [&writer, &result](const auto& field, std::string_view name, const auto& tags) {
                            if (result) {
                                if (parserShouldIgnoreReflectField(tags)) {
                                    return;
                                }
                                result = parserContext(
                                    parserWrite<W>(writer, field, typename parsing::Parent<W>::Root{}, tags),
                                    "Failed to write field '" + std::string(name) + "': ");
                            }
                        });
                    return result;
                }
            }
            const auto fieldCount = parserReflectEmittedFieldCount(value);
            if constexpr (requires { typename W::OutputIdObjectType; }) {
                auto object = parsing::Parent<W>::addIdObject(writer, fieldCount, parent, tags);
                return parserWriteReflectFields<W>(writer, object, value);
            } else {
                auto object = parsing::Parent<W>::addObject(writer, fieldCount, parent, tags);
                return parserWriteReflectFields<W>(writer, object, value);
            }
        } else {
            auto array = parsing::Parent<W>::addArray(writer, parserReflectFieldCount<T>(), parent, tags);
            ParserResult result;
            std::size_t index = 0;
            Reflect<T>::visitTagged(value, [&writer, &array, &result, &index](auto&& field, const auto& tags) {
                if (result) {
                    if (parserShouldIgnoreReflectField(tags)) {
                        ++index;
                        return;
                    }
                    const auto parent = typename parsing::Parent<W>::Array{&array};
                    parserWriteLeadingComment(writer, parent, tags);
                    result = parserContext(parserWrite<W>(writer, field, parent, tags),
                                            "Failed to write reflected element " + std::to_string(index) + ": ");
                    if (result) {
                        parserWriteTrailingComment(writer, parent, tags);
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
                                   (!DisableReflectParser<T>::value)>> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
        if constexpr (std::is_move_assignable_v<T> && std::is_copy_constructible_v<T>) {
            T parsed    = value;
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
    static auto readInPlace(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
        if constexpr (has_names_meta<T>) {
            if constexpr (requires { R::isRaw(in); }) {
                if (tag_query::get<tag_property::RawFixedData>(tags)) {
                    if (!R::isRaw(in)) {
                        return parserError(sa::ErrorCode::InvalidType,
                                            "raw_fixed_data requires a raw binary input segment");
                    }
                    ParserResult result;
                    auto current = in;
                    Reflect<T>::visitFull(value, [&current, &result](auto& field, std::string_view name,
                                                                     const auto& fieldTags) {
                        if (!result || parserShouldIgnoreReflectField(fieldTags)) {
                            return;
                        }
                        using FieldType = std::remove_cvref_t<decltype(field)>;
                        if (!tag_query::has<tag_property::FixedLength<void>>(fieldTags)) {
                            result =
                                parserError(sa::ErrorCode::InvalidLength,
                                             "raw_fixed_data field '" + std::string(name) + "' requires fixed_length");
                            return;
                        }
                        if constexpr (std::is_enum_v<FieldType>) {
                            std::underlying_type_t<FieldType> raw{};
                            result = parserContext(parserRead<R>(current, raw, fieldTags),
                                                    "Failed to parse raw fixed field '" + std::string(name) + "': ");
                            if (result) {
                                field = static_cast<FieldType>(raw);
                            }
                        } else if constexpr (std::is_arithmetic_v<FieldType>) {
                            result = parserContext(parserRead<R>(current, field, fieldTags),
                                                    "Failed to parse raw fixed field '" + std::string(name) + "': ");
                        } else {
                            result = parserError(sa::ErrorCode::InvalidType,
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
                if (tag_query::get<tag_property::Unframed<std::decay_t<T>>>(tags)) {
                    if constexpr (requires { R::isFramedObject(in); }) {
                        if (R::isFramedObject(in)) {
                            return parserReadReflectFields<R>(in, value, tags);
                        }
                    }
                    ParserResult result;
                    auto current = in;
                    Reflect<T>::visitFull(
                        value, [&current, &result](auto& field, std::string_view name, const auto& tags) {
                            if (result) {
                                if (parserShouldIgnoreReflectField(tags)) {
                                    return;
                                }
                                result  = parserContext(parserRead<R>(current, field, tags),
                                                         "Failed to parse field '" + std::string(name) + "': ");
                                current = R::next(current);
                            }
                        });
                    return result;
                }
            }
            return parserReadReflectFields<R>(in, value, tags);
        } else {
            auto array = parsing::readerToArray<R>(in, tags);
            if (!array) {
                return array.error();
            }
            const auto actualSize   = R::arraySize(array.value());
            const auto expectedSize = parserReflectFieldCount<T>();
            if (actualSize != expectedSize) {
                return parserError(sa::ErrorCode::InvalidLength, "Expected reflected array with " +
                                                                      std::to_string(expectedSize) + " elements, got " +
                                                                      std::to_string(actualSize));
            }
            ParserResult result;
            std::size_t index        = 0;
            std::size_t elementIndex = 0;
            Reflect<T>::visitTagged(value, [&array, &result, &index, &elementIndex](auto&& field, const auto& tags) {
                if (result) {
                    if (parserShouldIgnoreReflectField(tags)) {
                        ++index;
                        return;
                    }
                    result = parserContext(parserRead<R>(R::arrayElement(array.value(), elementIndex), field, tags),
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
                                        (!DisableReflectParser<T>::value)>> {
    static auto toSchema() -> parsing::schema::Type {
        parsing::schema::Type schema;
        if constexpr (has_names_meta<T>) {
            schema = parserSchemaNamedReflection<T>();
        } else {
            schema = parserSchemaPositionalReflection<T>();
        }
        return schema;
    }
};

} // namespace detail
} // namespace nekoproto
