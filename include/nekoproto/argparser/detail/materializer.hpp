#pragma once

#include "nekoproto/argparser/detail/raw_parser.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/global/traits.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace nekoproto {
namespace argparser::detail {

using PresenceList = std::vector<unsigned char>;

inline auto equalsIgnoreCase(std::string_view lhs, std::string_view rhs) -> bool {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char lhs, char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
    });
}

inline auto readEnv(std::string_view name) -> std::optional<std::string> {
    if (name.empty()) {
        return std::nullopt;
    }
    const auto key = std::string(name);
#ifdef _WIN32
    std::size_t size = 0;
    if (getenv_s(&size, nullptr, 0, key.c_str()) != 0 || size == 0) {
        return std::nullopt;
    }
    std::string value(size - 1, '\0');
    if (getenv_s(&size, value.data(), size, key.c_str()) != 0) {
        return std::nullopt;
    }
    return value;
#else
    const auto* value = std::getenv(key.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

inline void notifyDeprecatedOption(const ArgSpec& spec, const ArgParserConfig& config) {
    if (spec.deprecated && config.deprecatedOptionHandler) {
        config.deprecatedOptionHandler(spec.long_name, spec.deprecated_message);
    }
}

inline auto describeValueSource(std::string_view source, std::string_view value) -> std::string {
    std::string result(source.empty() ? "value" : source);
    result.push_back(' ');
    result.append(quoteArgValue(value));
    return result;
}

inline auto formatValidationExpectation(const ArgSpec& spec) -> std::string {
    std::string result;
    if (spec.has_range) {
        std::ostringstream stream;
        stream << "expected range [" << spec.range_min << ", " << spec.range_max << ")";
        result = stream.str();
    }
    if (!spec.choices.empty()) {
        if (!result.empty()) {
            result.append("; ");
        }
        result.append("expected one of {");
        for (std::size_t idx = 0; idx < spec.choices.size(); ++idx) {
            if (idx != 0) {
                result.append(", ");
            }
            result.append(spec.choices[idx]);
        }
        result.push_back('}');
    }
    return result;
}

inline auto contextualArgparserError(std::error_code error, const ArgSpec& spec, std::string detail) -> std::error_code {
    if (!error || error.category() != argparserErrorCategory()) {
        return error;
    }
    return makeArgparserError(static_cast<ArgParserError>(error.value()),
                                formatErrorOptionLabel(spec) + ": " + std::move(detail));
}

inline auto parseBool(std::string_view text, bool& value) -> std::error_code {
    if (text.empty()) {
        value = true;
        return {};
    }
    if (text == "1" || equalsIgnoreCase(text, "true") || equalsIgnoreCase(text, "yes") ||
        equalsIgnoreCase(text, "on")) {
        value = true;
        return {};
    }
    if (text == "0" || equalsIgnoreCase(text, "false") || equalsIgnoreCase(text, "no") ||
        equalsIgnoreCase(text, "off")) {
        value = false;
        return {};
    }
    return makeErrorCode(ArgParserError::InvalidValue);
}

template <typename T>
auto parseScalar(std::string_view text, T& value, bool case_insensitive_enum = false) -> std::error_code {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<RawT, bool>) {
        return parseBool(text, value);
    } else if constexpr (std::is_same_v<RawT, std::string>) {
        value.assign(text);
        return {};
    } else if constexpr (std::is_same_v<RawT, std::string_view>) {
        value = text;
        return {};
    } else if constexpr (std::is_enum_v<RawT>) {
        constexpr auto EnumNames  = Reflect<RawT>::names();
        constexpr auto EnumValues = Reflect<RawT>::values();
        for (std::size_t idx = 0; idx < EnumNames.size(); ++idx) {
            if (EnumNames[idx] == text || (case_insensitive_enum && equalsIgnoreCase(EnumNames[idx], text))) {
                value = EnumValues[idx];
                return {};
            }
        }
        std::underlying_type_t<RawT> raw{};
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), raw);
        if (ec != std::errc{} || ptr != text.data() + text.size()) {
            return makeErrorCode(ArgParserError::InvalidValue);
        }
        value = static_cast<RawT>(raw);
        return {};
    } else if constexpr (std::is_integral_v<RawT> || std::is_floating_point_v<RawT>) {
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || ptr != text.data() + text.size()) {
            return makeErrorCode(ArgParserError::InvalidValue);
        }
        return {};
    } else {
        static_assert(!std::is_same_v<RawT, RawT>, "argparser does not support this field type");
    }
}

template <typename T>
auto assignValue(std::string_view text, T& value, bool case_insensitive_enum = false) -> std::error_code {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_arg_optional_v<RawT>) {
        OptionalValueT<RawT> inner{};
        if (auto error = parseScalar(text, inner, case_insensitive_enum)) {
            return error;
        }
        value = std::move(inner);
        return {};
    } else if constexpr (is_vector_v<RawT>) {
        VectorValueT<RawT> inner{};
        if (auto error = parseScalar(text, inner, case_insensitive_enum)) {
            return error;
        }
        value.push_back(std::move(inner));
        return {};
    } else {
        return parseScalar(text, value, case_insensitive_enum);
    }
}

template <typename T>
auto assignTextValue(std::string_view text, char separator, T& value, bool case_insensitive_enum = false) -> std::error_code {
    using RawT = std::remove_cvref_t<T>;
    if (separator == '\0') {
        return assignValue(text, value, case_insensitive_enum);
    }
    if constexpr (!is_vector_v<RawT>) {
        return makeErrorCode(ArgParserError::InvalidDefinition);
    } else {
        std::size_t begin = 0;
        while (begin <= text.size()) {
            const auto end  = text.find(separator, begin);
            const auto part = end == std::string_view::npos ? text.substr(begin) : text.substr(begin, end - begin);
            if (auto error = assignValue(part, value, case_insensitive_enum)) {
                return error;
            }
            if (end == std::string_view::npos) {
                break;
            }
            begin = end + 1;
        }
        return {};
    }
}

template <typename T>
auto assignDefaultValue(const T& default_value, auto& value, char separator = '\0',
                                     bool case_insensitive_enum = false) -> std::error_code {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return assignTextValue(std::string_view(default_value), separator, value, case_insensitive_enum);
    } else if constexpr (std::is_same_v<RawT, const char*> || std::is_same_v<RawT, char*>) {
        return assignTextValue(std::string_view(default_value), separator, value, case_insensitive_enum);
    } else if constexpr (std::is_array_v<RawT> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<RawT>>, char>) {
        return assignTextValue(std::string_view(default_value), separator, value, case_insensitive_enum);
    } else if constexpr (is_vector_v<std::remove_cvref_t<decltype(value)>>) {
        value.push_back(default_value);
        return {};
    } else {
        value = default_value;
        return {};
    }
}

template <typename T>
inline constexpr bool is_choice_value_v = // NOLINT
    traits::is_string_like_v<T> || std::is_enum_v<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_choices_supported_v = []() consteval { // NOLINT
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_choice_value_v<RawT>) {
        return true;
    } else if constexpr (is_arg_optional_v<RawT>) {
        return is_choice_value_v<OptionalValueT<RawT>>;
    } else if constexpr (is_vector_v<RawT>) {
        return is_choice_value_v<VectorValueT<RawT>>;
    } else {
        return false;
    }
}();

template <typename T>
auto scalarInRange(const T& value, double min, double max) -> bool {
    const auto numeric = static_cast<double>(value);
    return numeric >= min && numeric < max;
}

template <typename T>
auto validateRange(const T& value, const ArgSpec& spec) -> std::error_code {
    using RawT = std::remove_cvref_t<T>;
    if (!spec.has_range) {
        return {};
    }
    if constexpr (!is_range_supported_v<RawT>) {
        return makeErrorCode(ArgParserError::InvalidDefinition);
    } else if constexpr (is_range_value_v<RawT>) {
        if (!scalarInRange(value, spec.range_min, spec.range_max)) {
            return makeErrorCode(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !scalarInRange(*value, spec.range_min, spec.range_max)) {
            return makeErrorCode(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!scalarInRange(item, spec.range_min, spec.range_max)) {
                return makeErrorCode(ArgParserError::InvalidValue);
            }
        }
    }
    return {};
}

inline auto choiceContains(std::span<const std::string_view> choices, std::string_view value,
                            bool case_insensitive_choices) -> bool {
    return std::any_of(choices.begin(), choices.end(), [&](const auto& choice) {
        return case_insensitive_choices ? equalsIgnoreCase(choice, value) : choice == value;
    });
}

template <typename T>
auto choiceValueAllowed(const T& value, std::span<const std::string_view> choices, bool case_insensitive_choices) -> bool {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (traits::is_string_like_v<RawT>) {
        return choiceContains(choices, value, case_insensitive_choices);
    } else if constexpr (std::is_enum_v<RawT>) {
        constexpr auto EnumNames  = Reflect<RawT>::names();
        constexpr auto EnumValues = Reflect<RawT>::values();
        for (std::size_t idx = 0; idx < EnumNames.size(); ++idx) {
            if (EnumValues[idx] == value) {
                return choiceContains(choices, EnumNames[idx], case_insensitive_choices);
            }
        }
        return false;
    } else {
        return false;
    }
}

template <typename T>
auto validateChoices(const T& value, const ArgSpec& spec) -> std::error_code {
    using RawT = std::remove_cvref_t<T>;
    if (spec.choices.empty()) {
        return {};
    }
    const auto choices = std::span<const std::string_view>{spec.choices.data(), spec.choices.size()};
    if constexpr (!is_choices_supported_v<RawT>) {
        return makeErrorCode(ArgParserError::InvalidDefinition);
    } else if constexpr (is_choice_value_v<RawT>) {
        if (!choiceValueAllowed(value, choices, spec.case_insensitive_choices)) {
            return makeErrorCode(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !choiceValueAllowed(*value, choices, spec.case_insensitive_choices)) {
            return makeErrorCode(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!choiceValueAllowed(item, choices, spec.case_insensitive_choices)) {
                return makeErrorCode(ArgParserError::InvalidValue);
            }
        }
    }
    return {};
}

template <typename FieldT>
auto validateFieldDefinition(const ArgSpec& spec) -> std::error_code {
    if (spec.separator != '\0' && !is_vector_v<FieldT>) {
        return makeErrorCode(ArgParserError::InvalidDefinition);
    }
    if (spec.has_range && (!is_range_supported_v<FieldT> || spec.range_min > spec.range_max)) {
        return makeErrorCode(ArgParserError::InvalidDefinition);
    }
    if (!spec.choices.empty() && !is_choices_supported_v<FieldT>) {
        return makeErrorCode(ArgParserError::InvalidDefinition);
    }
    return {};
}

template <typename FieldT>
auto validateFieldValue(const FieldT& field, const ArgSpec& spec) -> std::error_code {
    if (auto error = validateRange(field, spec)) {
        return error;
    }
    return validateChoices(field, spec);
}

template <typename FieldT>
auto assignTextToField(const ArgSpec& spec, FieldT& field, std::string_view value,
                                     std::string_view source = {}) -> std::error_code {
    const auto case_insensitive_enum = spec.case_insensitive_choices && !spec.choices.empty();
    if (auto error = assignTextValue(value, spec.separator, field, case_insensitive_enum)) {
        return contextualArgparserError(error, spec, "failed to parse " + describeValueSource(source, value));
    }
    if (auto error = validateFieldValue(field, spec)) {
        auto detail = describeValueSource(source, value);
        detail.append(" failed validation");
        if (auto expectation = formatValidationExpectation(spec); !expectation.empty()) {
            detail.append("; ");
            detail.append(expectation);
        }
        return contextualArgparserError(error, spec, std::move(detail));
    }
    return {};
}

template <typename FieldT, typename Tags>
auto assignDefaultToField(const ArgSpec& spec, FieldT& field, const Tags& tags) -> std::error_code {
    if constexpr (tag_query::has<tag_property::default_value>(decltype(tags){})) {
        const auto case_insensitive_enum = spec.case_insensitive_choices && !spec.choices.empty();
        if (auto error = assignDefaultValue(tag_query::get<tag_property::default_value>(tags), field, spec.separator,
                                              case_insensitive_enum)) {
            return contextualArgparserError(error, spec,
                                              "failed to apply default " + quoteArgValue(spec.default_value));
        }
        if (auto error = validateFieldValue(field, spec)) {
            auto detail = "default " + quoteArgValue(spec.default_value) + " failed validation";
            if (auto expectation = formatValidationExpectation(spec); !expectation.empty()) {
                detail.append("; ");
                detail.append(expectation);
            }
            return contextualArgparserError(error, spec, std::move(detail));
        }
        return {};
    } else {
        static_cast<void>(spec);
        static_cast<void>(field);
        static_cast<void>(tags);
        return {};
    }
}

template <typename FieldT, typename Tags>
auto applyDefaultOption(const ArgSpec& spec, FieldT& field, const Tags& tags, bool& supplied) -> std::error_code {
    if (auto error = validateFieldDefinition<std::remove_cvref_t<FieldT>>(spec)) {
        return contextualArgparserError(error, spec, "invalid option definition");
    }

    if (spec.has_default) {
        if (auto error = assignDefaultToField(spec, field, tags)) {
            return error;
        }
        supplied = true;
    }
    return {};
}

template <typename FieldT, typename Tags>
auto materializeOneOption(const ArgSpec& spec, const RawOptionValues& raw, FieldT& field, const Tags& tags,
                                       const ArgParserConfig& config, bool already_supplied, bool& supplied) -> std::error_code {
    static_cast<void>(tags);
    if (auto error = validateFieldDefinition<std::remove_cvref_t<FieldT>>(spec)) {
        return contextualArgparserError(error, spec, "invalid option definition");
    }

    if (raw.seen()) {
        if constexpr (is_vector_v<std::remove_cvref_t<FieldT>>) {
            if (already_supplied) {
                field.clear();
            }
        }
        for (const auto& value : raw.values) {
            if (auto error = assignTextToField(spec, field, value, "value")) {
                return error;
            }
            notifyDeprecatedOption(spec, config);
        }
        supplied = true;
        return {};
    }

    if (const auto envValue = readEnv(spec.env_name); envValue) {
        if constexpr (is_vector_v<std::remove_cvref_t<FieldT>>) {
            if (already_supplied) {
                field.clear();
            }
        }
        const auto source = spec.env_name.empty() ? std::string_view{"env value"} : std::string_view{spec.env_name};
        if (auto error = assignTextToField(spec, field, *envValue, source)) {
            return error;
        }
        supplied = true;
        return {};
    }

    return {};
}

inline auto validateRequiredOptions(const ArgSchema& schema, std::span<const unsigned char> supplied) -> std::error_code {
    for (std::size_t index = 0; index < schema.user_spec_count; ++index) {
        if (schema.specs[index].required && (supplied[index] == 0U)) {
            return makeArgparserError(ArgParserError::MissingRequired,
                                        formatErrorOptionLabel(schema.specs[index]) + " is required");
        }
    }
    return {};
}

inline auto validateCrossFieldConstraints(const ArgSchema& schema,
                                                        std::span<const unsigned char> active) -> std::error_code {
    for (std::size_t index = 0; index < schema.user_spec_count; ++index) {
        if (active[index] == 0U) {
            continue;
        }
        const auto& spec = schema.specs[index];
        for (const auto requiredIndex : spec.require_indices) {
            if (requiredIndex >= schema.specs.size() || requiredIndex == index) {
                return makeArgparserError(ArgParserError::InvalidDefinition,
                                            formatErrorOptionLabel(spec) +
                                                " has an invalid normalized requirement reference");
            }
            if (active[requiredIndex] == 0U) {
                return makeArgparserError(ArgParserError::MissingRequired,
                                            formatErrorOptionLabel(spec) + " requires " +
                                                formatErrorOptionLabel(schema.specs[requiredIndex]));
            }
        }
        for (const auto conflictIndex : spec.conflict_indices) {
            if (conflictIndex >= schema.specs.size() || conflictIndex == index) {
                return makeArgparserError(ArgParserError::InvalidDefinition,
                                            formatErrorOptionLabel(spec) +
                                                " has an invalid normalized conflict reference");
            }
            if (active[conflictIndex] != 0U) {
                return makeArgparserError(ArgParserError::InvalidValue,
                                            formatErrorOptionLabel(spec) + " conflicts with " +
                                                formatErrorOptionLabel(schema.specs[conflictIndex]));
            }
        }
    }
    return {};
}

template <typename T>
auto applyDefaultFields(T& object, const ArgSchema& schema, std::size_t& spec_index,
                                     PresenceList& supplied) -> std::error_code {
    std::error_code result;
    Reflect<std::remove_cvref_t<T>>::visitFull(
        object, [&](auto& field, std::string_view reflectedName, const auto& tags) {
            if constexpr (shouldIgnoreArgField(decltype(tags){})) {
                return;
            } else {
                if (result) {
                    return;
                }
                static_cast<void>(reflectedName);
                using FieldT = std::remove_cvref_t<decltype(field)>;

                if constexpr (is_nested_option_v<FieldT>) {
                    if (auto error = applyDefaultFields(field, schema, spec_index, supplied); error) {
                        result = error;
                    }
                } else {
                    if (spec_index >= schema.user_spec_count) {
                        result = makeArgparserError(ArgParserError::InvalidDefinition,
                                                      "schema index is out of range while applying default for field " +
                                                          quoteArgValue(reflectedName));
                        return;
                    }
                    bool defaultSupplied = false;
                    if (auto error = applyDefaultOption(schema.specs[spec_index], field, tags, defaultSupplied)) {
                        result = error;
                        return;
                    }
                    if (defaultSupplied) {
                        supplied[spec_index] = 1U;
                    }
                    ++spec_index;
                }
            }
        });
    return result;
}

template <typename T>
auto materializeFields(T& object, const ArgSchema& schema, const RawParseResult& raw,
                                   const ArgParserConfig& config, std::size_t& spec_index, PresenceList& supplied) -> std::error_code {
    std::error_code result;
    Reflect<std::remove_cvref_t<T>>::visitFull(
        object, [&](auto& field, std::string_view reflectedName, const auto& tags) {
            if constexpr (shouldIgnoreArgField(decltype(tags){})) {
                return;
            } else {
                if (result) {
                    return;
                }
                static_cast<void>(reflectedName);
                using FieldT = std::remove_cvref_t<decltype(field)>;

                if constexpr (is_nested_option_v<FieldT>) {
                    if (auto error = materializeFields(field, schema, raw, config, spec_index, supplied); error) {
                        result = error;
                    }
                } else {
                    if (spec_index >= schema.user_spec_count || spec_index >= raw.options.size()) {
                        result = makeArgparserError(ArgParserError::InvalidDefinition,
                                                      "schema index is out of range while materializing field " +
                                                          quoteArgValue(reflectedName));
                        return;
                    }
                    bool optionSupplied        = false;
                    const auto alreadySupplied = supplied[spec_index] != 0U;
                    if (auto error = materializeOneOption(schema.specs[spec_index], raw.options[spec_index], field,
                                                            tags, config, alreadySupplied, optionSupplied)) {
                        result = error;
                        return;
                    }
                    if (optionSupplied) {
                        supplied[spec_index] = 1U;
                    }
                    ++spec_index;
                }
            }
        });
    return result;
}

template <typename FieldT>
auto importedFieldSupplied(const FieldT& field) -> bool {
    using RawT = std::remove_cvref_t<FieldT>;
    if constexpr (is_arg_optional_v<RawT>) {
        return field.has_value();
    } else {
        static_cast<void>(field);
        return true;
    }
}

template <typename T>
auto markImportedFieldsSupplied(const T& object, const ArgSchema& schema, std::size_t& spec_index,
                                              PresenceList& supplied) -> std::error_code {
    std::error_code result;
    Reflect<std::remove_cvref_t<T>>::visitFull(
        object, [&](const auto& field, std::string_view reflectedName, const auto& tags) {
            if constexpr (shouldIgnoreArgField(decltype(tags){})) {
                return;
            } else {
                if (result) {
                    return;
                }
                static_cast<void>(reflectedName);
                using FieldT = std::remove_cvref_t<decltype(field)>;

                if constexpr (is_nested_option_v<FieldT>) {
                    if (auto error = markImportedFieldsSupplied(field, schema, spec_index, supplied); error) {
                        result = error;
                    }
                } else {
                    if (spec_index >= schema.user_spec_count || spec_index >= supplied.size()) {
                        result = makeArgparserError(ArgParserError::InvalidDefinition,
                                                      "schema index is out of range while marking imported field " +
                                                          quoteArgValue(reflectedName));
                        return;
                    }
                    supplied[spec_index] = importedFieldSupplied(field) ? 1U : 0U;
                    ++spec_index;
                }
            }
        });
    return result;
}

template <typename T>
auto markImportedOptionsSupplied(const T& object, const ArgSchema& schema, PresenceList& supplied) -> std::error_code {
    const auto end = std::min(schema.user_spec_count, supplied.size());
    std::fill(supplied.begin(), supplied.begin() + end, 0U);

    std::size_t spec_index = 0;
    if (auto error = markImportedFieldsSupplied(object, schema, spec_index, supplied)) {
        return error;
    }
    if (spec_index != schema.user_spec_count) {
        return makeArgparserError(ArgParserError::InvalidDefinition, "not all imported schema fields were marked");
    }
    return {};
}

template <typename FieldT>
auto fieldRelationshipActive(const FieldT& field, bool supplied) -> bool {
    using RawT = std::remove_cvref_t<FieldT>;
    if (!supplied) {
        return false;
    }
    if constexpr (std::is_same_v<RawT, bool>) {
        return field;
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (!field.has_value()) {
            return false;
        }
        if constexpr (std::is_same_v<OptionalValueT<RawT>, bool>) {
            return *field;
        }
        return true;
    } else if constexpr (is_vector_v<RawT>) {
        if (field.empty()) {
            return false;
        }
        if constexpr (std::is_same_v<VectorValueT<RawT>, bool>) {
            return std::any_of(field.begin(), field.end(), [](bool value) { return value; });
        }
        return true;
    } else {
        return true;
    }
}

template <typename T>
auto markActiveFields(const T& object, const ArgSchema& schema, std::size_t& spec_index,
                                   const PresenceList& supplied, PresenceList& active) -> std::error_code {
    std::error_code result;
    Reflect<std::remove_cvref_t<T>>::visitFull(
        object, [&](const auto& field, std::string_view reflectedName, const auto& tags) {
            if constexpr (shouldIgnoreArgField(decltype(tags){})) {
                return;
            } else {
                if (result) {
                    return;
                }
                using FieldT = std::remove_cvref_t<decltype(field)>;
                if constexpr (is_nested_option_v<FieldT>) {
                    if (auto error = markActiveFields(field, schema, spec_index, supplied, active); error) {
                        result = error;
                    }
                } else {
                    if (spec_index >= schema.user_spec_count || spec_index >= supplied.size() ||
                        spec_index >= active.size()) {
                        result = makeArgparserError(ArgParserError::InvalidDefinition,
                                                      "schema index is out of range while evaluating field " +
                                                          quoteArgValue(reflectedName));
                        return;
                    }
                    active[spec_index] = fieldRelationshipActive(field, supplied[spec_index] != 0U) ? 1U : 0U;
                    ++spec_index;
                }
            }
        });
    return result;
}

template <typename T>
auto markActiveOptions(const T& object, const ArgSchema& schema, const PresenceList& supplied,
                                    PresenceList& active) -> std::error_code {
    std::fill(active.begin(), active.end(), 0U);
    std::size_t spec_index = 0;
    if (auto error = markActiveFields(object, schema, spec_index, supplied, active)) {
        return error;
    }
    if (spec_index != schema.user_spec_count) {
        return makeArgparserError(ArgParserError::InvalidDefinition, "not all schema fields were evaluated");
    }
    return {};
}

template <typename T>
auto validateMaterializedOptions(const T& object, const ArgSchema& schema, const PresenceList& supplied) -> std::error_code {
    if (auto error =
            validateRequiredOptions(schema, std::span<const unsigned char>{supplied.data(), supplied.size()})) {
        return error;
    }
    PresenceList active(schema.specs.size(), 0U);
    if (auto error = markActiveOptions(object, schema, supplied, active)) {
        return error;
    }
    return validateCrossFieldConstraints(schema, std::span<const unsigned char>{active.data(), active.size()});
}

template <typename T>
auto applyDefaultsInto(T& object, const ArgSchema& schema, PresenceList& supplied) -> std::error_code {
    std::size_t spec_index = 0;
    if (auto error = applyDefaultFields(object, schema, spec_index, supplied)) {
        return error;
    }
    if (spec_index != schema.user_spec_count) {
        return makeArgparserError(ArgParserError::InvalidDefinition, "not all schema fields received defaults");
    }
    return {};
}

template <typename T>
auto materializeExplicitOptionsInto(T& object, const ArgSchema& schema, const RawParseResult& raw,
                                                  const ArgParserConfig& config, PresenceList& supplied) -> std::error_code {
    std::size_t spec_index = 0;
    if (auto error = materializeFields(object, schema, raw, config, spec_index, supplied)) {
        return error;
    }
    if (spec_index != schema.user_spec_count) {
        return makeArgparserError(ArgParserError::InvalidDefinition, "not all schema fields were materialized");
    }
    return {};
}

template <typename T>
auto materializeOptionsInto(T& object, const ArgSchema& schema, const RawParseResult& raw,
                                         const ArgParserConfig& config) -> std::error_code {
    if (schema.specs.size() != raw.options.size()) {
        return makeArgparserError(ArgParserError::InvalidDefinition, "schema and raw option counts differ");
    }

    PresenceList supplied(schema.specs.size(), 0);
    if (auto error = applyDefaultsInto(object, schema, supplied)) {
        return error;
    }
    if (auto error = materializeExplicitOptionsInto(object, schema, raw, config, supplied)) {
        return error;
    }
    return validateMaterializedOptions(object, schema, supplied);
}

} // namespace argparser::detail
} // namespace nekoproto
