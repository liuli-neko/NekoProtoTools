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
#include <optional>
#include <variant>
#include <vector>

NEKO_BEGIN_NAMESPACE

enum class DefinedFormats : uint32_t {
    datetime,              // NOLINT
    date,                  // NOLINT
    time,                  // NOLINT
    duration,              // NOLINT
    email,                 // NOLINT
    idn_email,             // NOLINT
    hostname,              // NOLINT
    idn_hostname,          // NOLINT
    ipv4,                  // NOLINT
    ipv6,                  // NOLINT
    uri,                   // NOLINT
    uri_reference,         // NOLINT
    iri,                   // NOLINT
    iri_reference,         // NOLINT
    uuid,                  // NOLINT
    uri_template,          // NOLINT
    json_pointer,          // NOLINT
    relative_json_pointer, // NOLINT
    regex                  // NOLINT
};

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

NEKO_END_NAMESPACE