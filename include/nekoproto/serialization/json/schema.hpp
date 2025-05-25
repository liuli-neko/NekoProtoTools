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
#include <variant>
#include <vector>

#include "nekoproto/serialization/reflection.hpp"

NEKO_BEGIN_NAMESPACE

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

struct ExtUnits final {
    std::optional<std::string_view> unitAscii{};   // ascii representation of the unit, e.g. "m^2" for square meters
    std::optional<std::string_view> unitUnicode{}; // unicode representation of the unit, e.g. "m²" for square meters
    constexpr bool operator==(const ExtUnits&) const noexcept = default;
};

struct Schema final {
    bool reflection_helper{}; // NOLINT needed to support automatic reflection, because ref is a std::optional
    std::optional<std::string_view> ref{};
    using schema_number = std::optional<std::variant<int64_t, uint64_t, double>>;
    using schema_any    = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string_view>;
    // meta data keywords, ref: https://www.learnjsonschema.com/2020-12/meta-data/
    std::optional<std::string_view> title{};
    std::optional<std::string_view> description{};
    std::optional<schema_any> defaultValue{};
    std::optional<bool> deprecated{};
    std::optional<std::vector<std::string_view>> examples{};
    std::optional<bool> readOnly{};
    std::optional<bool> writeOnly{};
    // hereafter validation keywords, ref: https://www.learnjsonschema.com/2020-12/validation/
    std::optional<schema_any> constant{};
    // string only keywords
    std::optional<uint64_t> minLength{};
    std::optional<uint64_t> maxLength{};
    std::optional<std::string_view> pattern{};
    // https://www.learnjsonschema.com/2020-12/format-annotation/format/
    std::optional<DefinedFormats> format{};
    // number only keywords
    schema_number minimum{};
    schema_number maximum{};
    schema_number exclusiveMinimum{};
    schema_number exclusiveMaximum{};
    schema_number multipleOf{};
    // object only keywords
    std::optional<uint64_t> minProperties{};
    std::optional<uint64_t> maxProperties{};
    // std::optional<std::map<std::string_view, std::vector<std::string_view>>> dependent_required{};
    std::optional<std::vector<std::string_view>> required{};
    // array only keywords
    std::optional<uint64_t> minItems{};
    std::optional<uint64_t> maxItems{};
    std::optional<uint64_t> minContains{};
    std::optional<uint64_t> maxContains{};
    std::optional<bool> uniqueItems{};
    // properties
    std::optional<std::vector<std::string_view>> enumeration{}; // enum

    // out of json schema specification
    std::optional<ExtUnits> ExtUnits{}; // NOLINT
    std::optional<bool>
        ExtAdvanced{}; // NOLINT flag to indicate that the parameter is advanced and can be hidden in default views

    struct Neko {
        using T = Schema;
        static constexpr std::array names{"$ref",             // NOLINT
                                          "title",            //
                                          "description",      //
                                          "default",          //
                                          "deprecated",       //
                                          "examples",         //
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
                                          "required",         //
                                          "minItems",         //
                                          "maxItems",         //
                                          "minContains",      //
                                          "maxContains",      //
                                          "uniqueItems",      //
                                          "enum",             //
                                          "ExtUnits",         //
                                          "ExtAdvanced"};

        static constexpr std::tuple values = {&T::ref,              // NOLINT
                                              &T::title,            //
                                              &T::description,      //
                                              &T::defaultValue,     //
                                              &T::deprecated,       //
                                              &T::examples,         //
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
                                              &T::required,         //
                                              &T::minItems,         //
                                              &T::maxItems,         //
                                              &T::minContains,      //
                                              &T::maxContains,      //
                                              &T::uniqueItems,      //
                                              &T::enumeration,      //
                                              &T::ExtUnits,         //
                                              &T::ExtAdvanced};
    };
};

namespace detail {
struct Schematic final {
    std::optional<std::vector<std::string_view>> type{};
    std::optional<std::map<std::string_view, Schema, std::less<>>> properties{}; // object
    std::optional<Schema> items{};                                               // array
    std::optional<std::variant<bool, Schema>> additionalProperties{};            // map
    std::optional<std::map<std::string_view, Schematic, std::less<>>> defs{};
    std::optional<std::vector<Schematic>> oneOf{};
    std::optional<std::vector<std::string_view>> required{};
    std::optional<std::vector<std::string_view>> examples{};
    Schema attributes{};

    struct Neko {
        using T = Schematic;
        static constexpr std::array names{"type",                 // NOLINT
                                          "properties",           //
                                          "items",                //
                                          "additionalProperties", //
                                          "$defs",                //
                                          "oneOf",                //
                                          "required",             //
                                          "examples"};

        static constexpr std::tuple values = {&T::type,                 // NOLINT
                                              &T::properties,           //
                                              &T::items,                //
                                              &T::additionalProperties, //
                                              &T::defs,                 //
                                              &T::oneOf,                //
                                              &T::required,             //
                                              &T::examples};
    };
};

template <typename T = void>
struct ToJsonSchema {
    static Schema schema(auto& s, Schematic& defs) {
        
    }
};
} // namespace detail
NEKO_END_NAMESPACE