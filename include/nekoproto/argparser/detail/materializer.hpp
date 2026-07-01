#pragma once

#include "nekoproto/argparser/detail/raw_parser.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <sstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

using PresenceList = std::vector<unsigned char>;

inline bool equals_ignore_case(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char lhs, char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
    });
}

inline std::optional<std::string> read_env(std::string_view name) {
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

inline void notify_deprecated_option(const ArgSpec& spec, const ArgParserConfig& config) {
    if (spec.deprecated && config.deprecatedOptionHandler) {
        config.deprecatedOptionHandler(spec.long_name, spec.deprecated_message);
    }
}

inline std::string describe_value_source(std::string_view source, std::string_view value) {
    std::string result(source.empty() ? "value" : source);
    result.push_back(' ');
    result.append(quote_arg_value(value));
    return result;
}

inline std::string format_validation_expectation(const ArgSpec& spec) {
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

inline std::error_code contextual_argparser_error(std::error_code error, const ArgSpec& spec, std::string detail) {
    if (!error || error.category() != argparser_error_category()) {
        return error;
    }
    return make_argparser_error(static_cast<ArgParserError>(error.value()),
                                format_error_option_label(spec) + ": " + std::move(detail));
}

inline std::error_code parse_bool(std::string_view text, bool& value) {
    if (text.empty()) {
        value = true;
        return {};
    }
    if (text == "1" || equals_ignore_case(text, "true") || equals_ignore_case(text, "yes") ||
        equals_ignore_case(text, "on")) {
        value = true;
        return {};
    }
    if (text == "0" || equals_ignore_case(text, "false") || equals_ignore_case(text, "no") ||
        equals_ignore_case(text, "off")) {
        value = false;
        return {};
    }
    return make_error_code(ArgParserError::InvalidValue);
}

template <typename T>
std::error_code parse_scalar(std::string_view text, T& value, bool case_insensitive_enum = false) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<RawT, bool>) {
        return parse_bool(text, value);
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
            if (EnumNames[idx] == text || (case_insensitive_enum && equals_ignore_case(EnumNames[idx], text))) {
                value = EnumValues[idx];
                return {};
            }
        }
        std::underlying_type_t<RawT> raw{};
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), raw);
        if (ec != std::errc{} || ptr != text.data() + text.size()) {
            return make_error_code(ArgParserError::InvalidValue);
        }
        value = static_cast<RawT>(raw);
        return {};
    } else if constexpr (std::is_integral_v<RawT> || std::is_floating_point_v<RawT>) {
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || ptr != text.data() + text.size()) {
            return make_error_code(ArgParserError::InvalidValue);
        }
        return {};
    } else {
        static_assert(!std::is_same_v<RawT, RawT>, "argparser does not support this field type");
    }
}

template <typename T>
std::error_code assign_value(std::string_view text, T& value, bool case_insensitive_enum = false) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_arg_optional_v<RawT>) {
        optional_value_t<RawT> inner{};
        if (auto error = parse_scalar(text, inner, case_insensitive_enum)) {
            return error;
        }
        value = std::move(inner);
        return {};
    } else if constexpr (is_vector_v<RawT>) {
        vector_value_t<RawT> inner{};
        if (auto error = parse_scalar(text, inner, case_insensitive_enum)) {
            return error;
        }
        value.push_back(std::move(inner));
        return {};
    } else {
        return parse_scalar(text, value, case_insensitive_enum);
    }
}

template <typename T>
std::error_code assign_text_value(std::string_view text, char separator, T& value, bool case_insensitive_enum = false) {
    using RawT = std::remove_cvref_t<T>;
    if (separator == '\0') {
        return assign_value(text, value, case_insensitive_enum);
    }
    if constexpr (!is_vector_v<RawT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    } else {
        std::size_t begin = 0;
        while (begin <= text.size()) {
            const auto end  = text.find(separator, begin);
            const auto part = end == std::string_view::npos ? text.substr(begin) : text.substr(begin, end - begin);
            if (auto error = assign_value(part, value, case_insensitive_enum)) {
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
std::error_code assign_default_value(const T& default_value, auto& value, char separator = '\0',
                                     bool case_insensitive_enum = false) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return assign_text_value(std::string_view(default_value), separator, value, case_insensitive_enum);
    } else if constexpr (std::is_same_v<RawT, const char*> || std::is_same_v<RawT, char*>) {
        return assign_text_value(std::string_view(default_value), separator, value, case_insensitive_enum);
    } else if constexpr (std::is_array_v<RawT> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<RawT>>, char>) {
        return assign_text_value(std::string_view(default_value), separator, value, case_insensitive_enum);
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
    is_string_like_v<T> || std::is_enum_v<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_choices_supported_v = []() consteval { // NOLINT
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_choice_value_v<RawT>) {
        return true;
    } else if constexpr (is_arg_optional_v<RawT>) {
        return is_choice_value_v<optional_value_t<RawT>>;
    } else if constexpr (is_vector_v<RawT>) {
        return is_choice_value_v<vector_value_t<RawT>>;
    } else {
        return false;
    }
}();

template <typename T>
bool scalar_in_range(const T& value, double min, double max) {
    const auto numeric = static_cast<double>(value);
    return numeric >= min && numeric < max;
}

template <typename T>
std::error_code validate_range(const T& value, const ArgSpec& spec) {
    using RawT = std::remove_cvref_t<T>;
    if (!spec.has_range) {
        return {};
    }
    if constexpr (!is_range_supported_v<RawT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    } else if constexpr (is_range_value_v<RawT>) {
        if (!scalar_in_range(value, spec.range_min, spec.range_max)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !scalar_in_range(*value, spec.range_min, spec.range_max)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!scalar_in_range(item, spec.range_min, spec.range_max)) {
                return make_error_code(ArgParserError::InvalidValue);
            }
        }
    }
    return {};
}

inline bool choice_contains(std::span<const std::string_view> choices, std::string_view value,
                            bool case_insensitive_choices) {
    return std::any_of(choices.begin(), choices.end(), [&](const auto& choice) {
        return case_insensitive_choices ? equals_ignore_case(choice, value) : choice == value;
    });
}

template <typename T>
bool choice_value_allowed(const T& value, std::span<const std::string_view> choices, bool case_insensitive_choices) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_string_like_v<RawT>) {
        return choice_contains(choices, value, case_insensitive_choices);
    } else if constexpr (std::is_enum_v<RawT>) {
        constexpr auto EnumNames  = Reflect<RawT>::names();
        constexpr auto EnumValues = Reflect<RawT>::values();
        for (std::size_t idx = 0; idx < EnumNames.size(); ++idx) {
            if (EnumValues[idx] == value) {
                return choice_contains(choices, EnumNames[idx], case_insensitive_choices);
            }
        }
        return false;
    } else {
        return false;
    }
}

template <typename T>
std::error_code validate_choices(const T& value, const ArgSpec& spec) {
    using RawT = std::remove_cvref_t<T>;
    if (spec.choices.empty()) {
        return {};
    }
    const auto choices = std::span<const std::string_view>{spec.choices.data(), spec.choices.size()};
    if constexpr (!is_choices_supported_v<RawT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    } else if constexpr (is_choice_value_v<RawT>) {
        if (!choice_value_allowed(value, choices, spec.case_insensitive_choices)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !choice_value_allowed(*value, choices, spec.case_insensitive_choices)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!choice_value_allowed(item, choices, spec.case_insensitive_choices)) {
                return make_error_code(ArgParserError::InvalidValue);
            }
        }
    }
    return {};
}

template <typename FieldT>
std::error_code validate_field_definition(const ArgSpec& spec) {
    if (spec.separator != '\0' && !is_vector_v<FieldT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    }
    if (spec.has_range && (!is_range_supported_v<FieldT> || spec.range_min > spec.range_max)) {
        return make_error_code(ArgParserError::InvalidDefinition);
    }
    if (!spec.choices.empty() && !is_choices_supported_v<FieldT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    }
    return {};
}

template <typename FieldT>
std::error_code validate_field_value(const FieldT& field, const ArgSpec& spec) {
    if (auto error = validate_range(field, spec)) {
        return error;
    }
    return validate_choices(field, spec);
}

template <typename FieldT>
std::error_code assign_text_to_field(const ArgSpec& spec, FieldT& field, std::string_view value,
                                     std::string_view source = {}) {
    const auto case_insensitive_enum = spec.case_insensitive_choices && !spec.choices.empty();
    if (auto error = assign_text_value(value, spec.separator, field, case_insensitive_enum)) {
        return contextual_argparser_error(error, spec, "failed to parse " + describe_value_source(source, value));
    }
    if (auto error = validate_field_value(field, spec)) {
        auto detail = describe_value_source(source, value);
        detail.append(" failed validation");
        if (auto expectation = format_validation_expectation(spec); !expectation.empty()) {
            detail.append("; ");
            detail.append(expectation);
        }
        return contextual_argparser_error(error, spec, std::move(detail));
    }
    return {};
}

template <typename FieldT, typename Tags>
std::error_code assign_default_to_field(const ArgSpec& spec, FieldT& field, const Tags& tags) {
    if constexpr (tag_query::has<tag_property::default_value>(decltype(tags){})) {
        const auto case_insensitive_enum = spec.case_insensitive_choices && !spec.choices.empty();
        if (auto error =
                assign_default_value(tag_query::get<tag_property::default_value>(tags), field, spec.separator, case_insensitive_enum)) {
            return contextual_argparser_error(error, spec,
                                              "failed to apply default " + quote_arg_value(spec.default_value));
        }
        if (auto error = validate_field_value(field, spec)) {
            auto detail = "default " + quote_arg_value(spec.default_value) + " failed validation";
            if (auto expectation = format_validation_expectation(spec); !expectation.empty()) {
                detail.append("; ");
                detail.append(expectation);
            }
            return contextual_argparser_error(error, spec, std::move(detail));
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
std::error_code materialize_one_option(const ArgSpec& spec, const RawOptionValues& raw, FieldT& field, const Tags& tags,
                                       const ArgParserConfig& config, bool& supplied) {
    if (auto error = validate_field_definition<std::remove_cvref_t<FieldT>>(spec)) {
        return contextual_argparser_error(error, spec, "invalid option definition");
    }

    if (raw.seen()) {
        for (const auto& value : raw.values) {
            if (auto error = assign_text_to_field(spec, field, value, "value")) {
                return error;
            }
            notify_deprecated_option(spec, config);
        }
        supplied = true;
        return {};
    }

    if (const auto envValue = read_env(spec.env_name); envValue) {
        const auto source = spec.env_name.empty() ? std::string_view{"env value"} : std::string_view{spec.env_name};
        if (auto error = assign_text_to_field(spec, field, *envValue, source)) {
            return error;
        }
        supplied = true;
        return {};
    }

    if (spec.has_default) {
        if (auto error = assign_default_to_field(spec, field, tags)) {
            return error;
        }
        supplied = true;
    }
    return {};
}

inline std::error_code validate_required_options(const ArgSchema& schema, std::span<const unsigned char> supplied) {
    for (std::size_t index = 0; index < schema.specs.size(); ++index) {
        if (schema.specs[index].required && (supplied[index] == 0U)) {
            return make_argparser_error(ArgParserError::MissingRequired,
                                        format_error_option_label(schema.specs[index]) + " is required");
        }
    }
    return {};
}

inline std::error_code validate_cross_field_constraints(const ArgSchema& schema,
                                                        std::span<const unsigned char> supplied) {
    for (std::size_t index = 0; index < schema.specs.size(); ++index) {
        if (supplied[index] == 0U) {
            continue;
        }
        const auto& spec = schema.specs[index];
        for (const auto requiredName : spec.requires_names) {
            const auto requiredIndex = schema.find_reference_index(requiredName);
            if (!requiredIndex.has_value() || *requiredIndex == index) {
                return make_argparser_error(ArgParserError::InvalidDefinition,
                                            format_error_option_label(spec) + " references unknown requirement " +
                                                quote_arg_value(requiredName));
            }
            if (supplied[*requiredIndex] == 0U) {
                return make_argparser_error(ArgParserError::MissingRequired,
                                            format_error_option_label(spec) + " requires " +
                                                format_error_option_label(schema.specs[*requiredIndex]));
            }
        }
        for (const auto conflictName : spec.conflicts) {
            const auto conflictIndex = schema.find_reference_index(conflictName);
            if (!conflictIndex.has_value() || *conflictIndex == index) {
                return make_argparser_error(ArgParserError::InvalidDefinition,
                                            format_error_option_label(spec) + " references unknown conflict " +
                                                quote_arg_value(conflictName));
            }
            if (supplied[*conflictIndex] != 0U) {
                return make_argparser_error(ArgParserError::InvalidValue,
                                            format_error_option_label(spec) + " conflicts with " +
                                                format_error_option_label(schema.specs[*conflictIndex]));
            }
        }
    }
    return {};
}

template <typename T>
std::error_code materialize_fields(T& object, const ArgSchema& schema, const RawParseResult& raw,
                                   const ArgParserConfig& config, std::size_t& spec_index, PresenceList& supplied) {
    std::error_code result;
    Reflect<std::remove_cvref_t<T>>::forEach(
        object, [&](auto& field, std::string_view reflectedName, const auto& tags) {
            if (result) {
                return;
            }
            static_cast<void>(reflectedName);
            using FieldT = std::remove_cvref_t<decltype(field)>;

            if constexpr (is_nested_option_v<FieldT>) {
                if (auto error = materialize_fields(field, schema, raw, config, spec_index, supplied); error) {
                    result = error;
                }
            } else {
                if (spec_index >= schema.specs.size() || spec_index >= raw.options.size()) {
                    result = make_argparser_error(ArgParserError::InvalidDefinition,
                                                  "schema index is out of range while materializing field " +
                                                      quote_arg_value(reflectedName));
                    return;
                }
                bool optionSupplied = false;
                if (auto error = materialize_one_option(schema.specs[spec_index], raw.options[spec_index], field, tags,
                                                        config, optionSupplied)) {
                    result = error;
                    return;
                }
                supplied[spec_index] = optionSupplied ? 1U : 0U;
                ++spec_index;
            }
        });
    return result;
}

template <typename T>
std::error_code materialize_options_into(T& object, const ArgSchema& schema, const RawParseResult& raw,
                                         const ArgParserConfig& config) {
    if (schema.specs.size() != raw.options.size()) {
        return make_argparser_error(ArgParserError::InvalidDefinition, "schema and raw option counts differ");
    }

    PresenceList supplied(schema.specs.size(), 0);
    std::size_t spec_index = 0;
    if (auto error = materialize_fields(object, schema, raw, config, spec_index, supplied)) {
        return error;
    }
    if (spec_index != schema.specs.size()) {
        return make_argparser_error(ArgParserError::InvalidDefinition, "not all schema fields were materialized");
    }
    if (auto error =
            validate_required_options(schema, std::span<const unsigned char>{supplied.data(), supplied.size()})) {
        return error;
    }
    return validate_cross_field_constraints(schema, std::span<const unsigned char>{supplied.data(), supplied.size()});
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
