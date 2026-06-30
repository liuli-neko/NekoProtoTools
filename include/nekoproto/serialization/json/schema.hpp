/**
 * @file schema.hpp
 * @author llhsdmd (llhsdmd@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-04-27
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include "nekoproto/global/global.hpp"

#include <array>
#include <map>
#include <optional>
#include <memory>
#include <variant>
#include <vector>

#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/reflection.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {
#define NEKO_JSON_SCHEMA_TYPES_TABLE                                                                                   \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Null, "null")                                                                          \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Boolean, "boolean")                                                                    \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Integer, "integer")                                                                    \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Number, "number")                                                                      \
    NEKO_JSON_SCHEMA_TYPES_ENUM(String, "string")                                                                      \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Array, "array")                                                                        \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Object, "object")

#define NEKO_JSON_SCHEMA_TYPES_ENUM(Name, Value) Name,
enum class JsonSchemaType { NEKO_JSON_SCHEMA_TYPES_TABLE };
#undef NEKO_JSON_SCHEMA_TYPES_ENUM
} // namespace detail

template <>
struct Meta<detail::JsonSchemaType> {
    using T                     = detail::JsonSchemaType;
    static constexpr auto value = Enumerate{// NOLINT
#define NEKO_JSON_SCHEMA_TYPES_ENUM(name, str) str, detail::JsonSchemaType::name,
                                            NEKO_JSON_SCHEMA_TYPES_TABLE
#undef NEKO_JSON_SCHEMA_TYPES_ENUM
    };
};
#undef NEKO_JSON_SCHEMA_TYPES_TABLE

#define NEKO_JSON_SCHEMA_FORMATS_TABLE                                                                                 \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Datetime, "date-time")                                                               \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Date, "date")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Time, "time")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Duration, "duration")                                                                \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Email, "email")                                                                      \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(IdnEmail, "idn-email")                                                               \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Hostname, "hostname")                                                                \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(IdnHostname, "idn-hostname")                                                         \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Ipv4, "ipv4")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Ipv6, "ipv6")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Uri, "uri")                                                                          \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(UriReference, "uri-reference")                                                       \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Iri, "iri")                                                                          \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(IriReference, "iri-reference")                                                       \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Uuid, "uuid")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(UriTemplate, "uri-template")                                                         \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(JsonPointer, "json-pointer")                                                         \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(RelativeJsonPointer, "relative-json-pointer")                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Regex, "regex")

enum struct DefinedFormats : uint32_t {
#define NEKO_JSON_SCHEMA_FORMATS_ENUM(name, _) name,
    NEKO_JSON_SCHEMA_FORMATS_TABLE
#undef NEKO_JSON_SCHEMA_FORMATS_ENUM
};

template <>
struct Meta<DefinedFormats> {
    using T                     = DefinedFormats;
    static constexpr auto value = Enumerate{// NOLINT
#define NEKO_JSON_SCHEMA_FORMATS_ENUM(name, str) str, DefinedFormats::name,
                                            NEKO_JSON_SCHEMA_FORMATS_TABLE
#undef NEKO_JSON_SCHEMA_FORMATS_ENUM
    };
};

#undef NEKO_JSON_SCHEMA_FORMATS_TABLE

// https://json-schema.org/draft-07/schema#
struct JsonSchema final {
    std::optional<std::string> type{std::nullopt};
    std::optional<std::string> schema{std::nullopt};
    std::variant<std::monostate, bool, std::unique_ptr<JsonSchema>> additionalProperties{}; // map
    // std::optional<std::map<std::string, JsonSchema, std::less<>>> defs{};
    std::optional<std::vector<JsonSchema>> oneOf{std::nullopt};
    std::optional<std::vector<std::string>> required{std::nullopt};
    // std::optional<std::vector<std::string>> examples{};

    std::optional<std::string> ref{std::nullopt};
    using schema_number = std::optional<std::variant<int64_t, uint64_t, double>>;
    using schema_any    = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string>;
    // meta data keywords, ref: https://www.learnjsonschema.com/2020-12/meta-data/
    std::optional<std::string> title{};
    std::optional<std::string> description{std::nullopt};
    std::optional<schema_any> defaultValue{std::nullopt};
    std::optional<bool> deprecated{std::nullopt};
    std::optional<bool> readOnly{std::nullopt};
    std::optional<bool> writeOnly{std::nullopt};
    // hereafter validation keywords, ref: https://www.learnjsonschema.com/2020-12/validation/
    std::optional<schema_any> constant{std::nullopt};
    // string only keywords
    std::optional<uint64_t> minLength{std::nullopt};
    std::optional<uint64_t> maxLength{std::nullopt};
    std::optional<std::string> pattern{std::nullopt};
    // https://www.learnjsonschema.com/2020-12/format-annotation/format/
    std::optional<DefinedFormats> format{std::nullopt};
    // number only keywords
    schema_number minimum{std::nullopt};
    schema_number maximum{std::nullopt};
    schema_number exclusiveMinimum{std::nullopt};
    schema_number exclusiveMaximum{std::nullopt};
    schema_number multipleOf{std::nullopt};
    // object only keywords
    std::optional<std::map<std::string, JsonSchema>> properties{std::nullopt}; // object
    std::optional<uint64_t> minProperties{std::nullopt};
    std::optional<uint64_t> maxProperties{std::nullopt};
    // array only keywords
    std::variant<std::monostate, std::unique_ptr<JsonSchema>, std::vector<JsonSchema>> items{}; // array
    std::optional<bool> additionalItems{std::nullopt};
    std::optional<uint64_t> minItems{std::nullopt};
    std::optional<uint64_t> maxItems{std::nullopt};
    std::optional<uint64_t> minContains{std::nullopt};
    std::optional<uint64_t> maxContains{std::nullopt};
    std::optional<bool> uniqueItems{std::nullopt};
    // properties
    std::optional<std::vector<std::string>> enumeration{std::nullopt}; // enum

    struct Neko {
        using T = JsonSchema;
        static constexpr std::array names // NOLINT
            = {
                "$schema",              //
                "type",                 //
                "properties",           //
                "items",                //
                "additionalItems",      //
                "additionalProperties", //
                // "$defs",                //
                "oneOf",    //
                "required", //
                // "examples",         //
                "$ref", //
                "title",            //
                "description",      //
                "default",          //
                "deprecated",       //
                "readOnly",         //
                "writeOnly",        //
                "const",            //
                "minLength",        //
                "maxLength",        //
                "pattern",          //
                "format",           //
                "minimum",          //
                "maximum",          //
                "exclusiveMinimum", //
                "exclusiveMaximum", //
                "multipleOf",       //
                "minProperties",    //
                "maxProperties",    //
                "minItems",         //
                "maxItems",         //
                "minContains",      //
                "maxContains",      //
                "uniqueItems",      //
                "enum",             //
        };

        static constexpr std::tuple values // NOLINT
            = {
                &T::schema, //
                &T::type,
                &T::properties,           //
                &T::items,                //
                &T::additionalItems,      //
                &T::additionalProperties, //
                // &T::defs,                 //
                &T::oneOf,    //
                &T::required, //
                // &T::examples,             //
                &T::ref, //
                &T::title,                //
                &T::description,      //
                &T::defaultValue,     //
                &T::deprecated,       //
                &T::readOnly,         //
                &T::writeOnly,        //
                &T::constant,         //
                &T::minLength,        //
                &T::maxLength,        //
                &T::pattern,          //
                &T::format,           //
                &T::minimum,          //
                &T::maximum,          //
                &T::exclusiveMinimum, //
                &T::exclusiveMaximum, //
                &T::multipleOf,       //
                &T::minProperties,    //
                &T::maxProperties,    //
                &T::minItems,         //
                &T::maxItems,         //
                &T::minContains,      //
                &T::maxContains,      //
                &T::uniqueItems,      //
                &T::enumeration,      //
        };
    };
};

namespace detail {

inline void parser_schema_to_json(const parsing::schema::Type& source, JsonSchema& target);

inline void parser_schema_number(const std::optional<parsing::schema::Type::Number>& source,
                                 JsonSchema::schema_number& target) {
    if (source) {
        target = std::visit([](const auto value) -> JsonSchema::schema_number::value_type {
            return value;
        }, *source);
    }
}

inline void parser_schema_to_json(const parsing::schema::Type& source, JsonSchema& target) {
    std::visit(
        Overloads{
            [&target](const parsing::schema::Type::Null&) {
                target.type = "null";
            },
            [&target](const parsing::schema::Type::Boolean&) {
                target.type = "boolean";
            },
            [&target](const parsing::schema::Type::Integer& integer) {
                target.type = "integer";
                parser_schema_number(integer.minimum, target.minimum);
                parser_schema_number(integer.maximum, target.maximum);
            },
            [&target](const parsing::schema::Type::FloatingPoint& number) {
                target.type = "number";
                parser_schema_number(number.minimum, target.minimum);
                parser_schema_number(number.maximum, target.maximum);
            },
            [&target](const parsing::schema::Type::String& string) {
                target.type = "string";
                if (!string.enumeration.empty()) {
                    target.enumeration = string.enumeration;
                }
            },
            [&target](const parsing::schema::Type::Array& array) {
                target.type            = "array";
                target.minItems        = array.minItems;
                target.maxItems        = array.maxItems;
                target.additionalItems = array.additionalItems;
                target.uniqueItems     = array.uniqueItems;
                if (array.items) {
                    auto item = std::make_unique<JsonSchema>();
                    parser_schema_to_json(*array.items, *item);
                    target.items = std::move(item);
                } else if (!array.prefixItems.empty()) {
                    std::vector<JsonSchema> items(array.prefixItems.size());
                    for (std::size_t i = 0; i < array.prefixItems.size(); ++i) {
                        parser_schema_to_json(array.prefixItems[i], items[i]);
                    }
                    target.items = std::move(items);
                }
            },
            [&target](const parsing::schema::Type::Object& object) {
                target.type = "object";
                if (!object.properties.empty()) {
                    target.properties = std::map<std::string, JsonSchema>{};
                    for (const auto& [name, property] : object.properties) {
                        parser_schema_to_json(property, target.properties->try_emplace(name).first->second);
                    }
                }
                if (!object.required.empty()) {
                    target.required = object.required;
                }
                if (object.additionalProperties) {
                    auto additional = std::make_unique<JsonSchema>();
                    parser_schema_to_json(*object.additionalProperties, *additional);
                    target.additionalProperties = std::move(additional);
                }
            },
            [&target](const parsing::schema::Type::AnyOf& anyOf) {
                target.oneOf = std::vector<JsonSchema>(anyOf.types.size());
                for (std::size_t i = 0; i < anyOf.types.size(); ++i) {
                    parser_schema_to_json(anyOf.types[i], (*target.oneOf)[i]);
                }
            },
            [&target](const parsing::schema::Type::Optional& optional) {
                target.oneOf = std::vector<JsonSchema>(2);
                parser_schema_to_json(*optional.type, (*target.oneOf)[0]);
                (*target.oneOf)[1].type = "null";
            },
        },
        source.value);
}

} // namespace detail

template <typename R, typename W, typename T>
bool generate_schema_for(JsonSchema& schema) {
    schema.schema = "http://json-schema.org/draft-07/schema#";
    detail::parser_schema_to_json(parser_schema<std::decay_t<T>>(), schema);
    return true;
}

template <typename T>
bool generate_schema(JsonSchema& schema) {
    return generate_schema_for<typename JsonSerializer::Reader, typename JsonSerializer::Writer,
                               std::decay_t<T>>(schema);
}

template <typename T>
bool generate_schema([[maybe_unused]] const T& value, JsonSchema& schema) {
    return generate_schema<std::decay_t<T>>(schema);
}
NEKO_END_NAMESPACE
