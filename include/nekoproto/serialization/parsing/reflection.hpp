#pragma once

#include "nekoproto/global/traits.hpp"
#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/reflection_plan.hpp"
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

template <typename FieldT>
auto parserShouldSkipEmptyField(const FieldT& field) -> bool {
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
    return makeParserError(sa::ErrorCode::InvalidField, "Required field '" + std::string(name) + "' is missing");
}

template <typename W, typename ObjectType, typename T>
auto parserWriteReflectFields(W& writer, ObjectType& object, const T& value) -> ParserResult;

template <typename R, typename T, typename Tags = NoTags>
    requires (!std::is_same_v<typename R::InputValueType, typename R::InputObjectType>)
auto parserReadReflectFields(typename R::InputValueType in, T& value, const Tags& tags = {}) -> ParserResult;

template <typename R, typename T, typename Tags = NoTags>
auto parserReadReflectFields(const typename R::InputObjectType& object, T& value, const Tags& tags = {}) -> ParserResult;

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

template <typename T, std::size_t I>
consteval bool parserReflectFieldNeedsDynamicCount() {
    using Plan = ParserFieldPlan<std::decay_t<T>, I>;
    return !Plan::ignored &&
           (traits::OptionalLikeType<typename Plan::FieldType>::value || Plan::flat);
}

template <typename T, std::size_t... Is>
consteval bool parserReflectNeedsDynamicCountImpl(std::index_sequence<Is...>) {
    return (parserReflectFieldNeedsDynamicCount<T, Is>() || ...);
}

template <typename T>
consteval bool parserReflectNeedsDynamicCount() {
    return parserReflectNeedsDynamicCountImpl<T>(
        std::make_index_sequence<Reflect<std::decay_t<T>>::value_count>{});
}

template <typename T>
auto parserReflectEmittedFieldCount(const T& value) -> std::size_t {
    if constexpr (!parserReflectNeedsDynamicCount<T>()) {
        return parserReflectFieldCount<T>();
    } else {
        std::size_t count = 0;
        Reflect<std::decay_t<T>>::forEachWhile(value, [&](auto index, const auto& field,
                                                                         std::string_view, const auto&) {
            constexpr std::size_t I = decltype(index)::value;
            using Plan = ParserFieldPlan<std::decay_t<T>, I>;
            if constexpr (Plan::ignored) {
                return true;
            }
            if (parserShouldSkipEmptyField(field)) {
                return true;
            }
            if constexpr (Plan::flat && HasValuesMeta<typename Plan::FieldType> &&
                          HasNamesMeta<typename Plan::FieldType> &&
                          !DisableReflectParser<typename Plan::FieldType>::value) {
                count += parserReflectEmittedFieldCount(field);
            } else {
                ++count;
            }
            return true;
        });
        return count;
    }
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
    const auto stags = SerializerTags::from<FieldType>(tags);
    if (stags.ignored) {
        return sa::success();
    }
    if constexpr (HasValuesMeta<FieldType> && HasNamesMeta<FieldType> &&
                  !DisableReflectParser<FieldType>::value) {
        if (stags.flat) {
            return parserWriteReflectFields<W>(writer, object, field);
        }
    }
    if (parserShouldSkipEmptyField(field)) {
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
    std::string_view fieldName = stags.rename.empty() ? name : stags.rename;
    const auto writeField = [&](const auto& parent) {
        if (!stags.leading_comment.empty()) {
            parsing::Parent<W>::addComment(writer, stags.leading_comment, parent);
        }
        auto result = parserWrite<W>(writer, field, parent, tags);
        if (!result) {
            return parserContextField(std::move(result), fieldName, "write");
        }
        if (!stags.trailing_comment.empty()) {
            parsing::Parent<W>::addComment(writer, stags.trailing_comment, parent);
        }
        return result;
    };
    if constexpr (std::is_same_v<ObjectType, typename W::OutputObjectType>) {
        return writeField(typename parsing::Parent<W>::Object{fieldName, &object});
    } else {
        return writeField(typename parsing::Parent<W>::IdObject{fieldName, &object});
    }
}

template <typename T>
consteval auto parserReflectHasFlatField() -> bool {
    bool hasFlat = false;
    Reflect<T>::visitMetaFull([&]<typename Field>(std::type_identity<Field>, std::string_view, const auto& tags) {
        if constexpr (tag_query::get<tag_property::Flat<Field>>(tags)) {
            hasFlat = true;
        }
    });
    return hasFlat;
}

template <typename T, std::size_t I>
constexpr auto parserReflectEffectiveFieldName() -> std::string_view {
    return ParserFieldPlan<T, I>::name;
}

template <typename T>
constexpr auto parserReflectEffectiveFieldNames() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::array<std::string_view, sizeof...(Is)>{parserReflectEffectiveFieldName<T, Is>()...};
    }(std::make_index_sequence<Reflect<T>::value_count>{});
}

template <typename R, typename T, typename Tags>
auto parserReadReflectField(const typename R::InputObjectType& object, T& field, std::string_view name,
                            const Tags& tags) -> ParserResult {
    using FieldType = std::decay_t<T>;
    const auto stags = SerializerTags::from<FieldType>(tags);
    if (stags.ignored) {
        return sa::success();
    }
    if constexpr (HasValuesMeta<FieldType> && HasNamesMeta<FieldType> &&
                  !DisableReflectParser<FieldType>::value) {
        if (stags.flat) {
            return parserReadReflectFields<R>(object, field, tags);
        }
    }
    std::string_view fieldName = stags.rename.empty() ? name : stags.rename;
    auto fieldValue = parsing::readerObjectField<R>(object, fieldName, tags);
    if (!fieldValue) {
        return parserReadMissingField(field, fieldName, tags);
    }
    if (parsing::readerIsEmpty<R>(fieldValue.value(), tags)) {
        if constexpr (traits::OptionalLikeType<FieldType>::value) {
            traits::OptionalLikeType<FieldType>::setNull(field);
            return sa::success();
        }
    }
    auto result = parserRead<R>(fieldValue.value(), field, tags);
    if (!result) {
        return parserContextField(std::move(result), fieldName, "parse");
    }
    return result;
}

template <typename R, typename T, typename Tags>
    requires (!std::is_same_v<typename R::InputValueType, typename R::InputObjectType>)
auto parserReadReflectField(typename R::InputValueType in, T& field, std::string_view name,
                            const Tags& tags) -> ParserResult {
    auto object = parsing::readerToObject<R>(in, NoTags{});
    if (!object) {
        return object.error();
    }
    return parserReadReflectField<R>(object.value(), field, name, tags);
}

template <typename W, typename ObjectType, typename T>
auto parserWriteReflectFields(W& writer, ObjectType& object, const T& value) -> ParserResult {
    ParserResult result;
    Reflect<std::decay_t<T>>::forEachWhile(
        value, [&](auto index, const auto& field, std::string_view, const auto&) -> bool {
            constexpr std::size_t I = decltype(index)::value;
            using Plan = ParserFieldPlan<std::decay_t<T>, I>;
            using FieldType = typename Plan::FieldType;
            if constexpr (Plan::ignored) {
                return true;
            }
            if constexpr (Plan::flat && HasValuesMeta<FieldType> && HasNamesMeta<FieldType> &&
                          !DisableReflectParser<FieldType>::value) {
                result = parserWriteReflectFields<W>(writer, object, field);
                return static_cast<bool>(result);
            }
            if (parserShouldSkipEmptyField(field)) {
                return true;
            }
            const auto writeField = [&](const auto& parent) -> ParserResult {
                if constexpr (!Plan::leading_comment.empty()) {
                    parsing::Parent<W>::addComment(writer, Plan::leading_comment, parent);
                }
#if defined(NEKO_WRITE_NULL_FOR_EMPTY_OPTIONAL)
                if constexpr (traits::OptionalLikeType<FieldType>::value) {
                    if (!traits::OptionalLikeType<FieldType>::hasValue(field)) {
                        parsing::Parent<W>::addNull(writer, parent, Plan::tags);
                        if constexpr (!Plan::trailing_comment.empty()) {
                            parsing::Parent<W>::addComment(writer, Plan::trailing_comment, parent);
                        }
                        return sa::success();
                    }
                }
#endif
                auto fieldResult = parserWrite<W>(writer, field, parent, Plan::tags);
                if (!fieldResult) {
                    return parserContextField(std::move(fieldResult), Plan::name, "write");
                }
                if constexpr (!Plan::trailing_comment.empty()) {
                    parsing::Parent<W>::addComment(writer, Plan::trailing_comment, parent);
                }
                return fieldResult;
            };
            if constexpr (std::is_same_v<ObjectType, typename W::OutputObjectType>) {
                result = writeField(typename parsing::Parent<W>::Object{Plan::name, &object});
            } else {
                result = writeField(typename parsing::Parent<W>::IdObject{Plan::name, &object});
            }
            return static_cast<bool>(result);
        });
    return result;
}

template <typename R, typename T, std::size_t I>
auto parserReadPlannedReflectField(const typename R::InputObjectType& object,
                                   typename ParserFieldPlan<T, I>::FieldType& field) -> ParserResult {
    using Plan = ParserFieldPlan<T, I>;
    using FieldType = typename Plan::FieldType;
    if constexpr (Plan::ignored) {
        return sa::success();
    }
    if constexpr (Plan::flat && HasValuesMeta<FieldType> && HasNamesMeta<FieldType> &&
                  !DisableReflectParser<FieldType>::value) {
        return parserReadReflectFields<R>(object, field, Plan::tags);
    }
    auto fieldValue = parsing::readerObjectField<R>(object, Plan::name, Plan::tags);
    if (!fieldValue) {
        return parserReadMissingField(field, Plan::name, Plan::tags);
    }
    if (parsing::readerIsEmpty<R>(fieldValue.value(), Plan::tags)) {
        if constexpr (traits::OptionalLikeType<FieldType>::value) {
            traits::OptionalLikeType<FieldType>::setNull(field);
            return sa::success();
        }
    }
    auto result = parserRead<R>(fieldValue.value(), field, Plan::tags);
    if (!result) {
        return parserContextField(std::move(result), Plan::name, "parse");
    }
    return result;
}

template <typename R, typename T, typename Tags = NoTags>
auto parserReadReflectFieldsSlow(const typename R::InputObjectType& object, T& value,
                                 const Tags& /*tags*/ = {}) -> ParserResult {
    ParserResult result;
    Reflect<std::decay_t<T>>::forEachWhile(
        value, [&](auto index, auto&& field, std::string_view, const auto&) -> bool {
            constexpr std::size_t I = decltype(index)::value;
            result = parserReadPlannedReflectField<R, std::decay_t<T>, I>(object, field);
            return static_cast<bool>(result);
        });
    return result;
}

template <typename R, typename T, typename Accessors, std::size_t... Is>
auto parserReadFieldByIndex(std::size_t matchIndex, typename R::InputValueType memberValue,
                            T& value, Accessors& accessors,
                            const std::array<std::string_view, sizeof...(Is)>& fieldNames,
                            std::index_sequence<Is...>) -> ParserResult {
    ParserResult result;
    auto readOne = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
        auto&& field = detail::ReflectProvider<T>::template getFrom<I>(accessors, value);
        using Plan = ParserFieldPlan<T, I>;
        using FieldType = typename Plan::FieldType;
        if constexpr (Plan::ignored) {
            return sa::success();
        }
        if (parsing::readerIsEmpty<R>(memberValue, Plan::tags)) {
            if constexpr (traits::OptionalLikeType<FieldType>::value) {
                traits::OptionalLikeType<FieldType>::setNull(field);
                return sa::success();
            }
        }
        auto subRes = parserRead<R>(memberValue, field, Plan::tags);
        if (!subRes) {
            return parserContextField(std::move(subRes), fieldNames[I], "parse");
        }
        return subRes;
    };
    bool handled = (((matchIndex == Is) ? (result = readOne(std::integral_constant<std::size_t, Is>{}), true) : false) || ...);
    (void)handled;
    return result;
}

template <typename T, typename Accessors, std::size_t... Is>
auto parserCheckMissingFields(std::uint64_t visited_mask, T& value, Accessors& accessors,
                              const std::array<std::string_view, sizeof...(Is)>& fieldNames,
                              std::index_sequence<Is...>) -> ParserResult {
    ParserResult result;
    auto checkOne = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
        if (!result) {
            return;
        }
        if ((visited_mask & (1ULL << I)) == 0) {
            auto&& field = detail::ReflectProvider<T>::template getFrom<I>(accessors, value);
            using Plan = ParserFieldPlan<T, I>;
            if constexpr (!Plan::ignored) {
                result = parserReadMissingField(field, fieldNames[I], Plan::tags);
            }
        }
    };
    (checkOne(std::integral_constant<std::size_t, Is>{}), ...);
    return result;
}

template <typename R, typename T, typename Tags = NoTags>
auto parserReadReflectFieldsSinglePass(const typename R::InputObjectType& object, T& value, const Tags& tags = {}) -> ParserResult {
    using RawT = std::decay_t<T>;
    constexpr std::size_t FieldCount = Reflect<RawT>::value_count;
    if constexpr (FieldCount == 0) {
        return sa::success();
    } else {
        constexpr auto fieldNames = parserReflectEffectiveFieldNames<RawT>();
        decltype(auto) accessors = detail::ReflectProvider<RawT>::accessors(value);
        std::uint64_t visited_mask = 0;
        std::size_t cursor = 0;
        ParserResult result;

        bool iterOk = parsing::readerForEachObjectMember<R>(
            object,
            [&](std::string_view memberName, typename R::InputValueType memberValue) -> bool {
                std::size_t matchIndex = static_cast<std::size_t>(-1);
                if (cursor < FieldCount && fieldNames[cursor] == memberName) {
                    matchIndex = cursor;
                    ++cursor;
                } else {
                    for (std::size_t i = 0; i < FieldCount; ++i) {
                        if (fieldNames[i] == memberName) {
                            matchIndex = i;
                            cursor = i + 1;
                            break;
                        }
                    }
                }
                if (matchIndex != static_cast<std::size_t>(-1)) {
                    visited_mask |= (1ULL << matchIndex);
                    auto fieldRes = parserReadFieldByIndex<R>(
                        matchIndex, memberValue, value, accessors, fieldNames,
                        std::make_index_sequence<FieldCount>{});
                    if (!fieldRes) {
                        result = std::move(fieldRes);
                        return false;
                    }
                }
                return true;
            },
            tags);

        if (!iterOk) {
            if (!result) {
                return result;
            }
            return parserReadReflectFieldsSlow<R>(object, value, tags);
        }

        constexpr std::uint64_t all_mask = (FieldCount >= 64) ? ~0ULL : ((1ULL << FieldCount) - 1ULL);
        if (visited_mask != all_mask) {
            auto missingRes = parserCheckMissingFields<RawT>(
                visited_mask, value, accessors, fieldNames,
                std::make_index_sequence<FieldCount>{});
            if (!missingRes) {
                return missingRes;
            }
        }
        return sa::success();
    }
}

template <typename R, typename T, typename Tags>
auto parserReadReflectFields(const typename R::InputObjectType& object, T& value, const Tags& tags) -> ParserResult {
    using RawT = std::decay_t<T>;
    constexpr std::size_t FieldCount = Reflect<RawT>::value_count;
    if constexpr (FieldCount == 0) {
        return sa::success();
    } else if constexpr (FieldCount > 64 || parserReflectHasFlatField<RawT>()) {
        return parserReadReflectFieldsSlow<R>(object, value, tags);
    } else if constexpr ((requires(const typename R::InputObjectType& obj) {
                              { R::forEachObjectMember(obj, [](std::string_view, typename R::InputValueType) { return true; }) } -> std::convertible_to<bool>;
                          } || requires(const typename R::InputObjectType& obj, const Tags& t) {
                              { R::forEachObjectMember(obj, [](std::string_view, typename R::InputValueType) { return true; }, t) } -> std::convertible_to<bool>;
                          })) {
        return parserReadReflectFieldsSinglePass<R>(object, value, tags);
    } else {
        return parserReadReflectFieldsSlow<R>(object, value, tags);
    }
}

template <typename R, typename T, typename Tags>
    requires (!std::is_same_v<typename R::InputValueType, typename R::InputObjectType>)
auto parserReadReflectFields(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
    auto object = parsing::readerToObject<R>(in, tags);
    if (!object) {
        return object.error();
    }
    return parserReadReflectFields<R>(object.value(), value, tags);
}

template <typename W, typename T>
struct WriteParser<W, T,
                   std::enable_if_t<HasValuesMeta<T> && (!is_tagged_field_v<T>) && (!std::is_enum_v<T>) &&
                                    (!DisableReflectParser<T>::value)>> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const T& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if constexpr (HasNamesMeta<T>) {
            if constexpr (requires { writer.beginRawFixedDataAsRoot(); }) {
                if (tag_query::get<tag_property::RawFixedData>(tags)) {
                    using ParentTypeValue = std::remove_cvref_t<ParentType>;
                    if constexpr (!std::is_same_v<ParentTypeValue, typename parsing::Parent<W>::Root>) {
                        return makeParserError(sa::ErrorCode::InvalidType,
                                            "raw_fixed_data is only valid for a binary root value");
                    } else {
                        parsing::Parent<W>::beginRawFixedData(writer, parent);
                        ParserResult result;
                        Reflect<T>::forEachField(value, [&](const auto& field) {
                            using FieldType = typename std::decay_t<decltype(field)>::field_type;
                            const auto fieldTags = SerializerTags::from<FieldType>(field.tags);
                            if (!result || fieldTags.ignored) {
                                return;
                            }
                            if (fieldTags.fixed_length == 0) {
                                result = makeParserError(sa::ErrorCode::InvalidLength, "raw_fixed_data field '" +
                                                                                        std::string(field.name) +
                                                                                        "' requires fixed_length");
                                return;
                            }
                            if constexpr (std::is_enum_v<FieldType>) {
                                using Underlying = std::underlying_type_t<FieldType>;
                                auto subRes = parserWrite<W>(writer, static_cast<Underlying>(field.value),
                                                             typename parsing::Parent<W>::Root{}, field.tags);
                                if (!subRes) {
                                    result = parserContextField(std::move(subRes), field.name, "write raw fixed");
                                }
                            } else if constexpr (std::is_arithmetic_v<FieldType>) {
                                auto subRes = parserWrite<W>(writer, field.value, typename parsing::Parent<W>::Root{}, field.tags);
                                if (!subRes) {
                                    result = parserContextField(std::move(subRes), field.name, "write raw fixed");
                                }
                            } else {
                                result =
                                    makeParserError(sa::ErrorCode::InvalidType,
                                                 "raw_fixed_data field '" + std::string(field.name) +
                                                     "' must be an arithmetic or enum value with a fixed wire width");
                            }
                        });
                        return result;
                    }
                }
            }
            if constexpr (parsing::SupportsUnframedObjectWriter<W>) {
                if (tag_query::get<tag_property::Unframed<std::decay_t<T>>>(tags)) {
                    parsing::Parent<W>::beginUnframedObject(writer, parent);
                    ParserResult result;
                    Reflect<T>::forEachField(value, [&](const auto& field) {
                        if (result) {
                            const auto ftags = SerializerTags::from<typename std::decay_t<decltype(field)>::field_type>(field.tags);
                            if (ftags.ignored) {
                                return;
                            }
                            auto subRes = parserWrite<W>(writer, field.value, typename parsing::Parent<W>::Root{}, field.tags);
                            if (!subRes) {
                                result = parserContextField(std::move(subRes), field.name, "write");
                            }
                        }
                    });
                    return result;
                }
            }
            const auto fieldCount = RequiresContainerSize<W> ? parserReflectEmittedFieldCount(value) : 0;
            if constexpr (requires { typename W::OutputIdObjectType; }) {
                auto object = parsing::Parent<W>::addIdObject(writer, fieldCount, parent, tags);
                auto result = parserWriteReflectFields<W>(writer, object, value);
                parsing::Parent<W>::endIdObject(writer, object, parent, tags);
                return result;
            } else {
                auto object = parsing::Parent<W>::addObject(writer, fieldCount, parent, tags);
                auto result = parserWriteReflectFields<W>(writer, object, value);
                parsing::Parent<W>::endObject(writer, object, parent, tags);
                return result;
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
                    auto itemRes = parserWrite<W>(writer, field, parent, tags);
                    if (!itemRes) {
                        result = parserContext(std::move(itemRes),
                                               "Failed to write reflected element " + std::to_string(index) + ": ");
                    } else {
                        parserWriteTrailingComment(writer, parent, tags);
                    }
                }
                ++index;
            });
            parsing::Parent<W>::endArray(writer, array, parent, tags);
            return result;
        }
    }
};

template <typename R, typename T>
struct ReadParser<R, T,
                  std::enable_if_t<HasValuesMeta<T> && (!is_tagged_field_v<T>) && (!std::is_enum_v<T>) &&
                                   (!DisableReflectParser<T>::value)>> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
        return readInPlace(in, value, tags);
    }

private:
    template <typename Tags>
    static auto readInPlace(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
        if constexpr (HasNamesMeta<T>) {
            if constexpr (requires { R::isRaw(in); }) {
                if (tag_query::get<tag_property::RawFixedData>(tags)) {
                    if (!R::isRaw(in)) {
                        return makeParserError(sa::ErrorCode::InvalidType,
                                            "raw_fixed_data requires a raw binary input segment");
                    }
                    ParserResult result;
                    auto current = in;
                    Reflect<T>::forEachField(value, [&](auto&& field) {
                        using FieldType = typename std::decay_t<decltype(field)>::field_type;
                        const auto fieldTags = SerializerTags::from<FieldType>(field.tags);
                        if (!result || fieldTags.ignored) {
                            return;
                        }
                        if (fieldTags.fixed_length == 0) {
                            result =
                                makeParserError(sa::ErrorCode::InvalidLength,
                                             "raw_fixed_data field '" + std::string(field.name) + "' requires fixed_length");
                            return;
                        }
                        if constexpr (std::is_enum_v<FieldType>) {
                            std::underlying_type_t<FieldType> raw{};
                            auto subRes = parserRead<R>(current, raw, field.tags);
                            if (!subRes) {
                                result = parserContextField(std::move(subRes), field.name, "parse raw fixed");
                            } else {
                                field.value = static_cast<FieldType>(raw);
                            }
                        } else if constexpr (std::is_arithmetic_v<FieldType>) {
                            auto subRes = parserRead<R>(current, field.value, field.tags);
                            if (!subRes) {
                                result = parserContextField(std::move(subRes), field.name, "parse raw fixed");
                            }
                        } else {
                            result = makeParserError(sa::ErrorCode::InvalidType,
                                                  "raw_fixed_data field '" + std::string(field.name) +
                                                      "' must be an arithmetic or enum value with a fixed wire width");
                        }
                        if (result) {
                            current = R::next(current);
                        }
                    });
                    return result;
                }
            }
            if constexpr (parsing::SupportsUnframedObjectReader<R>) {
                if (tag_query::get<tag_property::Unframed<std::decay_t<T>>>(tags)) {
                    if constexpr (requires { R::isFramedObject(in); }) {
                        if (R::isFramedObject(in)) {
                            return parserReadReflectFields<R>(in, value, tags);
                        }
                    }
                    ParserResult result;
                    auto current = in;
                    Reflect<T>::forEachField(value, [&](auto&& field) {
                        if (result) {
                            const auto ftags = SerializerTags::from<typename std::decay_t<decltype(field)>::field_type>(field.tags);
                            if (ftags.ignored) {
                                return;
                            }
                            auto subRes = parserRead<R>(current, field.value, field.tags);
                            if (!subRes) {
                                result = parserContextField(std::move(subRes), field.name, "parse");
                            }
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
                return makeParserError(sa::ErrorCode::InvalidLength, "Expected reflected array with " +
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
                    auto subRes = parserRead<R>(R::arrayElement(array.value(), elementIndex), field, tags);
                    if (!subRes) {
                        result = parserContext(std::move(subRes),
                                               "Failed to parse reflected element " + std::to_string(index) + ": ");
                    }
                    ++elementIndex;
                }
                ++index;
            });
            return result;
        }
    }
};

template <typename T>
struct SchemaParser<T, std::enable_if_t<HasValuesMeta<T> && (!is_tagged_field_v<T>) && (!std::is_enum_v<T>) &&
                                        (!DisableReflectParser<T>::value)>> {
    static auto toSchema() -> parsing::schema::Type {
        parsing::schema::Type schema;
        if constexpr (HasNamesMeta<T>) {
            schema = parserSchemaNamedReflection<T>();
        } else {
            schema = parserSchemaPositionalReflection<T>();
        }
        return schema;
    }
};

} // namespace detail
} // namespace nekoproto
