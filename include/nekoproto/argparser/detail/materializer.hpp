#pragma once

#include "nekoproto/argparser/detail/raw_parser.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
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
        config.deprecatedOptionHandler(spec.longName, spec.deprecatedMessage);
    }
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
std::error_code parse_scalar(std::string_view text, T& value, bool caseInsensitiveEnum = false) {
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
            if (EnumNames[idx] == text || (caseInsensitiveEnum && equals_ignore_case(EnumNames[idx], text))) {
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
std::error_code assign_value(std::string_view text, T& value, bool caseInsensitiveEnum = false) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_arg_optional_v<RawT>) {
        optional_value_t<RawT> inner{};
        if (auto error = parse_scalar(text, inner, caseInsensitiveEnum)) {
            return error;
        }
        value = std::move(inner);
        return {};
    } else if constexpr (is_vector_v<RawT>) {
        vector_value_t<RawT> inner{};
        if (auto error = parse_scalar(text, inner, caseInsensitiveEnum)) {
            return error;
        }
        value.push_back(std::move(inner));
        return {};
    } else {
        return parse_scalar(text, value, caseInsensitiveEnum);
    }
}

template <typename T>
std::error_code assign_text_value(std::string_view text, char separator, T& value, bool caseInsensitiveEnum = false) {
    using RawT = std::remove_cvref_t<T>;
    if (separator == '\0') {
        return assign_value(text, value, caseInsensitiveEnum);
    }
    if constexpr (!is_vector_v<RawT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    } else {
        std::size_t begin = 0;
        while (begin <= text.size()) {
            const auto end  = text.find(separator, begin);
            const auto part = end == std::string_view::npos ? text.substr(begin) : text.substr(begin, end - begin);
            if (auto error = assign_value(part, value, caseInsensitiveEnum)) {
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
std::error_code assign_default_value(const T& defaultValue, auto& value, char separator = '\0',
                                     bool caseInsensitiveEnum = false) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return assign_text_value(std::string_view(defaultValue), separator, value, caseInsensitiveEnum);
    } else if constexpr (std::is_same_v<RawT, const char*> || std::is_same_v<RawT, char*>) {
        return assign_text_value(std::string_view(defaultValue), separator, value, caseInsensitiveEnum);
    } else if constexpr (std::is_array_v<RawT> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<RawT>>, char>) {
        return assign_text_value(std::string_view(defaultValue), separator, value, caseInsensitiveEnum);
    } else if constexpr (is_vector_v<std::remove_cvref_t<decltype(value)>>) {
        value.push_back(defaultValue);
        return {};
    } else {
        value = defaultValue;
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
    if (!spec.hasRange) {
        return {};
    }
    if constexpr (!is_range_supported_v<RawT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    } else if constexpr (is_range_value_v<RawT>) {
        if (!scalar_in_range(value, spec.rangeMin, spec.rangeMax)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !scalar_in_range(*value, spec.rangeMin, spec.rangeMax)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!scalar_in_range(item, spec.rangeMin, spec.rangeMax)) {
                return make_error_code(ArgParserError::InvalidValue);
            }
        }
    }
    return {};
}

inline bool choice_contains(std::span<const std::string_view> choices, std::string_view value,
                            bool caseInsensitiveChoices) {
    return std::any_of(choices.begin(), choices.end(), [&](const auto& choice) {
        return caseInsensitiveChoices ? equals_ignore_case(choice, value) : choice == value;
    });
}

template <typename T>
bool choice_value_allowed(const T& value, std::span<const std::string_view> choices, bool caseInsensitiveChoices) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_string_like_v<RawT>) {
        return choice_contains(choices, value, caseInsensitiveChoices);
    } else if constexpr (std::is_enum_v<RawT>) {
        constexpr auto EnumNames  = Reflect<RawT>::names();
        constexpr auto EnumValues = Reflect<RawT>::values();
        for (std::size_t idx = 0; idx < EnumNames.size(); ++idx) {
            if (EnumValues[idx] == value) {
                return choice_contains(choices, EnumNames[idx], caseInsensitiveChoices);
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
        if (!choice_value_allowed(value, choices, spec.caseInsensitiveChoices)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !choice_value_allowed(*value, choices, spec.caseInsensitiveChoices)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!choice_value_allowed(item, choices, spec.caseInsensitiveChoices)) {
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
    if (spec.hasRange && (!is_range_supported_v<FieldT> || spec.rangeMin > spec.rangeMax)) {
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
std::error_code assign_text_to_field(const ArgSpec& spec, FieldT& field, std::string_view value) {
    const auto caseInsensitiveEnum = spec.caseInsensitiveChoices && !spec.choices.empty();
    if (auto error = assign_text_value(value, spec.separator, field, caseInsensitiveEnum)) {
        return error;
    }
    return validate_field_value(field, spec);
}

template <typename FieldT, typename Tags>
std::error_code assign_default_to_field(const ArgSpec& spec, FieldT& field, const Tags& tags) {
    if constexpr (tag_query::has<tag_prop::default_value>(decltype(tags){})) {
        const auto caseInsensitiveEnum = spec.caseInsensitiveChoices && !spec.choices.empty();
        if (auto error =
                assign_default_value(tag_query::get<tag_prop::default_value>(tags), field, spec.separator, caseInsensitiveEnum)) {
            return error;
        }
        return validate_field_value(field, spec);
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
        return error;
    }

    if (raw.seen()) {
        for (const auto& value : raw.values) {
            if (auto error = assign_text_to_field(spec, field, value)) {
                return error;
            }
            notify_deprecated_option(spec, config);
        }
        supplied = true;
        return {};
    }

    if (const auto envValue = read_env(spec.envName); envValue) {
        if (auto error = assign_text_to_field(spec, field, *envValue)) {
            return error;
        }
        supplied = true;
        return {};
    }

    if (spec.hasDefault) {
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
            return make_error_code(ArgParserError::MissingRequired);
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
        for (const auto requiredName : spec.requiresNames) {
            const auto requiredIndex = schema.find_reference_index(requiredName);
            if (!requiredIndex.has_value() || *requiredIndex == index) {
                return make_error_code(ArgParserError::InvalidDefinition);
            }
            if (supplied[*requiredIndex] == 0U) {
                return make_error_code(ArgParserError::MissingRequired);
            }
        }
        for (const auto conflictName : spec.conflicts) {
            const auto conflictIndex = schema.find_reference_index(conflictName);
            if (!conflictIndex.has_value() || *conflictIndex == index) {
                return make_error_code(ArgParserError::InvalidDefinition);
            }
            if (supplied[*conflictIndex] != 0U) {
                return make_error_code(ArgParserError::InvalidValue);
            }
        }
    }
    return {};
}

template <typename T>
std::error_code materialize_fields(T& object, const ArgSchema& schema, const RawParseResult& raw,
                                   const ArgParserConfig& config, std::size_t& specIndex, PresenceList& supplied) {
    std::error_code result;
    Reflect<std::remove_cvref_t<T>>::forEach(
        object, [&](auto& field, std::string_view reflectedName, const auto& tags) {
            if (result) {
                return;
            }
            static_cast<void>(reflectedName);
            using FieldT = std::remove_cvref_t<decltype(field)>;

            if constexpr (is_nested_option_v<FieldT>) {
                if (auto error = materialize_fields(field, schema, raw, config, specIndex, supplied); error) {
                    result = error;
                }
            } else {
                if (specIndex >= schema.specs.size() || specIndex >= raw.options.size()) {
                    result = make_error_code(ArgParserError::InvalidDefinition);
                    return;
                }
                bool optionSupplied = false;
                if (auto error = materialize_one_option(schema.specs[specIndex], raw.options[specIndex], field, tags,
                                                        config, optionSupplied)) {
                    result = error;
                    return;
                }
                supplied[specIndex] = optionSupplied ? 1U : 0U;
                ++specIndex;
            }
        });
    return result;
}

template <typename T>
std::error_code materialize_options_into(T& object, const ArgSchema& schema, const RawParseResult& raw,
                                         const ArgParserConfig& config) {
    if (schema.specs.size() != raw.options.size()) {
        return make_error_code(ArgParserError::InvalidDefinition);
    }

    PresenceList supplied(schema.specs.size(), 0);
    std::size_t specIndex = 0;
    if (auto error = materialize_fields(object, schema, raw, config, specIndex, supplied)) {
        return error;
    }
    if (specIndex != schema.specs.size()) {
        return make_error_code(ArgParserError::InvalidDefinition);
    }
    if (auto error =
            validate_required_options(schema, std::span<const unsigned char>{supplied.data(), supplied.size()})) {
        return error;
    }
    return validate_cross_field_constraints(schema, std::span<const unsigned char>{supplied.data(), supplied.size()});
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
