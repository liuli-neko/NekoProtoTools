/**
 * @file argparser.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief Reflection based command line argument parser.
 * @version 0.1
 * @date 2026-06-03
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/string_literal.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <expected>
#include <functional>
#include <iomanip>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <variant>
#include <vector>

NEKO_BEGIN_NAMESPACE

// Public API -----------------------------------------------------------------

enum class ArgParserError {
    Success = 0,
    UnknownOption,
    MissingValue,
    InvalidValue,
    MissingRequired,
    UnexpectedPositional,
    InvalidDefinition,
    HelpRequested,
    UnknownCommand,
    VersionRequested,
};

namespace detail {
class ArgParserErrorCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "nekoproto.argparser"; }
    [[nodiscard]] std::string message(int condition) const override {
        switch (static_cast<ArgParserError>(condition)) {
        case ArgParserError::Success:
            return "success";
        case ArgParserError::UnknownOption:
            return "unknown option";
        case ArgParserError::MissingValue:
            return "missing option value";
        case ArgParserError::InvalidValue:
            return "invalid option value";
        case ArgParserError::MissingRequired:
            return "missing required option";
        case ArgParserError::UnexpectedPositional:
            return "unexpected positional argument";
        case ArgParserError::InvalidDefinition:
            return "invalid argument definition";
        case ArgParserError::HelpRequested:
            return "help requested";
        case ArgParserError::UnknownCommand:
            return "unknown command";
        case ArgParserError::VersionRequested:
            return "version requested";
        }
        return "unknown argparser error";
    }
};

inline const std::error_category& argparser_error_category() {
    static ArgParserErrorCategory kCategory;
    return kCategory;
}
} // namespace detail

inline std::error_code make_error_code(ArgParserError error) {
    return {static_cast<int>(error), detail::argparser_error_category()};
}

template <ConstexprString Long = "", ConstexprString Short = "", ConstexprString Help = "", ConstexprString... Choices>
struct ArgTags {
    static constexpr std::string_view longName                                = Long.view();         // NOLINT
    static constexpr std::string_view shortName                               = Short.view();        // NOLINT
    static constexpr std::string_view help                                    = Help.view();         // NOLINT
    static constexpr std::array<std::string_view, sizeof...(Choices)> choices = {Choices.view()...}; // NOLINT

    bool required   = false;
    bool positional = false;
    bool flag       = false;
    bool repeatable = false;
    bool hidden     = false;
    bool command    = false;
    double rangeMin = 0.0;
    double rangeMax = 0.0;
};

template <ConstexprString Name = "">
struct ArgCommand {
    static constexpr std::string_view name = Name.view(); // NOLINT
};

struct ArgParserConfig {
    std::string_view programName;
    std::string_view description;
    std::string_view usage;
    std::string_view version;
    char nestedSeparator   = '.';
    bool allowUnknown      = false;
    bool addHelp           = true;
    bool addVersion        = true;
    bool allowShortCluster = false;
};

namespace detail {

// Type traits ----------------------------------------------------------------

template <typename T>
struct is_arg_optional : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T>
struct is_arg_optional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_arg_optional_v = is_arg_optional<std::remove_cvref_t<T>>::value; // NOLINT

template <typename T>
struct optional_value;

template <typename T>
struct optional_value<std::optional<T>> {
    using type = T;
};

template <typename T>
using optional_value_t = typename optional_value<std::remove_cvref_t<T>>::type;

template <typename T>
struct is_vector : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {
    using value_type = T;
};

template <typename T>
inline constexpr bool is_vector_v = is_vector<std::remove_cvref_t<T>>::value; // NOLINT

template <typename T>
using vector_value_t = typename is_vector<std::remove_cvref_t<T>>::value_type;

template <typename T>
inline constexpr bool is_string_like_v = // NOLINT
    std::is_same_v<std::remove_cvref_t<T>, std::string> || std::is_same_v<std::remove_cvref_t<T>, std::string_view>;

template <typename T>
inline constexpr bool is_nested_option_v = // NOLINT
    std::is_class_v<std::remove_cvref_t<T>> && !is_string_like_v<T> && !is_arg_optional_v<T> && !is_vector_v<T> &&
    detail::has_values_meta<std::remove_cvref_t<T>>;

// Tag access -----------------------------------------------------------------

template <typename Tags>
constexpr std::string_view tag_long_name(const Tags& /*unused*/) {
    if constexpr (requires { std::remove_cvref_t<Tags>::longName; }) {
        return std::remove_cvref_t<Tags>::longName;
    } else {
        return {};
    }
}

template <typename Tags>
constexpr std::string_view tag_short_name(const Tags& /*unused*/) {
    if constexpr (requires { std::remove_cvref_t<Tags>::shortName; }) {
        return std::remove_cvref_t<Tags>::shortName;
    } else {
        return {};
    }
}

template <typename Tags>
constexpr std::string_view tag_help(const Tags& /*unused*/) {
    if constexpr (requires { std::remove_cvref_t<Tags>::help; }) {
        return std::remove_cvref_t<Tags>::help;
    } else {
        return {};
    }
}

template <typename Tags>
constexpr bool tag_required(const Tags& tags) {
    if constexpr (requires { tags.required; }) {
        return tags.required;
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool tag_positional(const Tags& tags) {
    if constexpr (requires { tags.positional; }) {
        return tags.positional;
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool tag_flag(const Tags& tags) {
    if constexpr (requires { tags.flag; }) {
        return tags.flag;
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool tag_repeatable(const Tags& tags) {
    if constexpr (requires { tags.repeatable; }) {
        return tags.repeatable;
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool tag_hidden(const Tags& tags) {
    if constexpr (requires { tags.hidden; }) {
        return tags.hidden;
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool tag_command(const Tags& tags) {
    if constexpr (requires { tags.command; }) {
        return tags.command;
    } else {
        return false;
    }
}

template <typename Tags>
constexpr double tag_range_min(const Tags& tags) {
    if constexpr (requires { tags.rangeMin; }) {
        return tags.rangeMin;
    } else {
        return 0.0;
    }
}

template <typename Tags>
constexpr double tag_range_max(const Tags& tags) {
    if constexpr (requires { tags.rangeMax; }) {
        return tags.rangeMax;
    } else {
        return 0.0;
    }
}

template <typename Tags>
constexpr bool tag_has_range(const Tags& tags) {
    return tag_range_min(tags) != tag_range_max(tags);
}

template <typename Tags>
constexpr auto tag_choices(const Tags& /*unused*/) {
    if constexpr (requires { std::remove_cvref_t<Tags>::choices; }) {
        return std::remove_cvref_t<Tags>::choices;
    } else {
        return std::array<std::string_view, 0>{};
    }
}

// Token and scalar parsing ---------------------------------------------------

inline bool is_option_token(std::string_view arg) { return arg.size() > 1 && arg[0] == '-'; }

inline bool is_help_token(std::string_view arg) { return arg == "--help" || arg == "-h"; }

inline bool is_version_token(std::string_view arg) { return arg == "--version" || arg == "-V"; }

inline bool equals_ignore_case(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char lhs, char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
    });
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
std::error_code parse_scalar(std::string_view text, T& value) {
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
            if (EnumNames[idx] == text) {
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
std::error_code assign_value(std::string_view text, T& value) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_arg_optional_v<RawT>) {
        optional_value_t<RawT> inner{};
        if (auto error = parse_scalar(text, inner)) {
            return error;
        }
        value = std::move(inner);
        return {};
    } else if constexpr (is_vector_v<RawT>) {
        vector_value_t<RawT> inner{};
        if (auto error = parse_scalar(text, inner)) {
            return error;
        }
        value.push_back(std::move(inner));
        return {};
    } else {
        return parse_scalar(text, value);
    }
}

// Validation -----------------------------------------------------------------

template <typename T>
inline constexpr bool is_range_value_v = // NOLINT
    std::is_arithmetic_v<std::remove_cvref_t<T>> && !std::is_same_v<std::remove_cvref_t<T>, bool>;

template <typename T>
inline constexpr bool is_range_supported_v = []() consteval { // NOLINT
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_range_value_v<RawT>) {
        return true;
    } else if constexpr (is_arg_optional_v<RawT>) {
        return is_range_value_v<optional_value_t<RawT>>;
    } else if constexpr (is_vector_v<RawT>) {
        return is_range_value_v<vector_value_t<RawT>>;
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
std::error_code validate_range(const T& value, bool hasRange, double min, double max) {
    using RawT = std::remove_cvref_t<T>;
    if (!hasRange) {
        return {};
    }
    if constexpr (!is_range_supported_v<RawT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    } else if constexpr (is_range_value_v<RawT>) {
        if (!scalar_in_range(value, min, max)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !scalar_in_range(*value, min, max)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!scalar_in_range(item, min, max)) {
                return make_error_code(ArgParserError::InvalidValue);
            }
        }
    } else {
        return make_error_code(ArgParserError::InvalidDefinition);
    }
    return {};
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

inline bool choice_contains(std::span<const std::string_view> choices, std::string_view value) {
    return std::any_of(choices.begin(), choices.end(), [&](const auto& choice) { return choice == value; });
}

template <typename T>
bool choice_value_allowed(const T& value, std::span<const std::string_view> choices) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_string_like_v<RawT>) {
        return choice_contains(choices, value);
    } else if constexpr (std::is_enum_v<RawT>) {
        constexpr auto EnumNames  = Reflect<RawT>::names();
        constexpr auto EnumValues = Reflect<RawT>::values();
        for (std::size_t idx = 0; idx < EnumNames.size(); ++idx) {
            if (EnumValues[idx] == value) {
                return choice_contains(choices, EnumNames[idx]);
            }
        }
        return false;
    } else {
        return false;
    }
}

template <typename T>
std::error_code validate_choices(const T& value, std::span<const std::string_view> choices) {
    using RawT = std::remove_cvref_t<T>;
    if (choices.empty()) {
        return {};
    }
    if constexpr (!is_choices_supported_v<RawT>) {
        return make_error_code(ArgParserError::InvalidDefinition);
    } else if constexpr (is_choice_value_v<RawT>) {
        if (!choice_value_allowed(value, choices)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_arg_optional_v<RawT>) {
        if (value.has_value() && !choice_value_allowed(*value, choices)) {
            return make_error_code(ArgParserError::InvalidValue);
        }
    } else if constexpr (is_vector_v<RawT>) {
        for (const auto& item : value) {
            if (!choice_value_allowed(item, choices)) {
                return make_error_code(ArgParserError::InvalidValue);
            }
        }
    }
    return {};
}

struct ArgSpec {
    std::string longName;
    std::string shortName;
    std::string help;
    bool required   = false;
    bool positional = false;
    bool flag       = false;
    bool repeatable = false;
    bool hidden     = false;
    bool seen       = false;
    bool hasRange   = false;
    double rangeMin = 0.0;
    double rangeMax = 0.0;
    std::vector<std::string_view> choices;
    std::function<std::error_code(std::string_view)> assign;
};

// Command metadata -----------------------------------------------------------

template <typename T>
constexpr bool field_is_flag(const T& /*unused*/, bool taggedFlag) {
    using RawT = std::remove_cvref_t<T>;
    return taggedFlag || std::is_same_v<RawT, bool>;
}

template <typename T>
inline constexpr bool is_command_placeholder_v = // NOLINT
    std::is_class_v<std::remove_cvref_t<T>> && std::is_empty_v<std::remove_cvref_t<T>> &&
    std::is_default_constructible_v<std::remove_cvref_t<T>>;

template <typename Tuple>
struct tuple_to_variant;

template <typename... Args>
struct tuple_to_variant<std::tuple<Args...>> {
    using type = std::variant<Args...>;
};

template <typename Tuple>
using tuple_to_variant_t = typename tuple_to_variant<Tuple>::type;

template <typename Tuple, std::size_t I>
consteval bool tuple_type_unique_at() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return ((I == Is || !std::is_same_v<std::tuple_element_t<I, Tuple>, std::tuple_element_t<Is, Tuple>>) && ...);
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple>
consteval bool tuple_types_unique() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (tuple_type_unique_at<Tuple, Is>() && ...);
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename T, std::size_t I>
consteval bool field_is_command() {
    return tag_command(std::get<I>(Reflect<std::remove_cvref_t<T>>::value_tags));
}

template <typename T>
consteval bool has_any_command() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (field_is_command<T, Is>() || ...);
    }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
}

template <typename T>
consteval bool all_fields_are_commands() {
    if constexpr (Reflect<std::remove_cvref_t<T>>::value_count == 0) {
        return false;
    } else {
        return []<std::size_t... Is>(std::index_sequence<Is...>) {
            return (field_is_command<T, Is>() && ...);
        }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
    }
}

template <typename T>
inline constexpr bool has_any_command_v = has_any_command<T>(); // NOLINT

template <typename T>
inline constexpr bool is_command_set_v = all_fields_are_commands<T>(); // NOLINT

template <typename T, std::size_t I>
consteval bool command_type_valid_at() {
    using FieldT = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
    if constexpr (is_command_placeholder_v<FieldT>) {
        return true;
    } else {
        return detail::has_values_meta<FieldT> && std::is_default_constructible_v<FieldT>;
    }
}

template <typename T>
consteval bool command_types_valid() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (command_type_valid_at<T, Is>() && ...);
    }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
}

template <typename T>
consteval void static_check_parser_definition() {
    constexpr bool HasAnyCommand = has_any_command_v<T>;
    constexpr bool IsCommandSet  = is_command_set_v<T>;
    static_assert(!HasAnyCommand || IsCommandSet,
                  "argparser command fields cannot be mixed with normal option fields in the same struct yet");
    if constexpr (IsCommandSet) {
        using Values = typename Reflect<std::remove_cvref_t<T>>::value_types;
        static_assert(command_types_valid<T>(), "argparser command field type must be a reflected struct or an empty "
                                                "default-constructible placeholder type");
        static_assert(tuple_types_unique<Values>(),
                      "argparser command result variant requires unique command field types");
    }
}

template <typename T>
struct ParserResult {
    using type = T;
};

template <typename T>
    requires(is_command_set_v<T>)
struct ParserResult<T> {
    using type = tuple_to_variant_t<typename Reflect<std::remove_cvref_t<T>>::value_types>;
};

template <typename T>
using parser_result_t = typename ParserResult<std::remove_cvref_t<T>>::type;

// Spec collection ------------------------------------------------------------

inline std::string join_arg_name(std::string_view prefix, std::string_view name, char separator) {
    if (prefix.empty()) {
        return std::string(name);
    }
    std::string result(prefix);
    result.push_back(separator);
    result.append(name);
    return result;
}

template <typename T>
void collect_specs(T& object, std::string_view prefix, const ArgParserConfig& config, std::vector<ArgSpec>& specs,
                   std::vector<std::size_t>& positionalSpecs) {
    static_assert(detail::has_values_meta<std::remove_cvref_t<T>>, "argparser requires a reflected options type");
    Reflect<std::remove_cvref_t<T>>::forEach(object, [&](auto& field, std::string_view reflectedName,
                                                         const auto& tags) {
        using FieldT                = std::remove_cvref_t<decltype(field)>;
        const auto explicitLongName = tag_long_name(tags);
        const auto name             = explicitLongName.empty() ? reflectedName : explicitLongName;

        if constexpr (is_nested_option_v<FieldT>) {
            const auto nextPrefix = join_arg_name(prefix, name, config.nestedSeparator);
            collect_specs(field, nextPrefix, config, specs, positionalSpecs);
        } else {
            ArgSpec spec;
            spec.longName          = join_arg_name(prefix, name, config.nestedSeparator);
            spec.shortName         = std::string(tag_short_name(tags));
            spec.help              = std::string(tag_help(tags));
            spec.required          = tag_required(tags);
            spec.positional        = tag_positional(tags);
            spec.flag              = field_is_flag(field, tag_flag(tags));
            spec.repeatable        = tag_repeatable(tags) || is_vector_v<FieldT>;
            spec.hidden            = tag_hidden(tags);
            spec.hasRange          = tag_has_range(tags);
            spec.rangeMin          = tag_range_min(tags);
            spec.rangeMax          = tag_range_max(tags);
            constexpr auto Choices = tag_choices(tags);
            spec.choices.assign(Choices.begin(), Choices.end());
            spec.assign = [&field, tags](std::string_view text) {
                constexpr auto Choices = tag_choices(tags);
                if (tag_has_range(tags) && (!is_range_supported_v<std::remove_cvref_t<decltype(field)>> ||
                                            tag_range_min(tags) > tag_range_max(tags))) {
                    return make_error_code(ArgParserError::InvalidDefinition);
                }
                if (!Choices.empty() && !is_choices_supported_v<std::remove_cvref_t<decltype(field)>>) {
                    return make_error_code(ArgParserError::InvalidDefinition);
                }
                if (auto error = assign_value(text, field)) {
                    return error;
                }
                if (auto error = validate_range(field, tag_has_range(tags), tag_range_min(tags), tag_range_max(tags))) {
                    return error;
                }
                return validate_choices(field, std::span<const std::string_view>{Choices.data(), Choices.size()});
            };

            specs.push_back(std::move(spec));
            if (specs.back().positional) {
                positionalSpecs.push_back(specs.size() - 1);
            }
        }
    });
}

inline ArgSpec* find_long_spec(std::vector<ArgSpec>& specs, std::string_view name) {
    for (auto& spec : specs) {
        if (!spec.positional && spec.longName == name) {
            return &spec;
        }
    }
    return nullptr;
}

inline ArgSpec* find_short_spec(std::vector<ArgSpec>& specs, std::string_view name) {
    for (auto& spec : specs) {
        if (!spec.positional && !spec.shortName.empty() && spec.shortName == name) {
            return &spec;
        }
    }
    return nullptr;
}

inline std::error_code apply_spec(ArgSpec& spec, std::string_view value) {
    if (!spec.repeatable && spec.seen && spec.positional) {
        return make_error_code(ArgParserError::UnexpectedPositional);
    }
    if (auto error = spec.assign(value)) {
        return error;
    }
    spec.seen = true;
    return {};
}

inline std::error_code apply_positional(std::vector<ArgSpec>& specs, const std::vector<std::size_t>& positionalSpecs,
                                        std::size_t& positionalIndex, std::string_view value) {
    if (positionalIndex >= positionalSpecs.size()) {
        return make_error_code(ArgParserError::UnexpectedPositional);
    }
    auto& spec = specs[positionalSpecs[positionalIndex]];
    if (auto error = apply_spec(spec, value)) {
        return error;
    }
    if (!spec.repeatable) {
        ++positionalIndex;
    }
    return {};
}

// Help and version formatting ------------------------------------------------

inline std::string default_usage(std::string_view programName) {
    std::string usage = "Usage:";
    if (!programName.empty()) {
        usage.push_back(' ');
        usage.append(programName);
    }
    usage.append(" [options]");
    return usage;
}

inline std::string default_command_usage(std::string_view programName) {
    std::string usage = "Usage:";
    if (!programName.empty()) {
        usage.push_back(' ');
        usage.append(programName);
    }
    usage.append(" <command> [options]");
    return usage;
}

inline std::string format_version_text(const ArgParserConfig& config) {
    std::string result;
    if (!config.programName.empty()) {
        result.append(config.programName);
        if (!config.version.empty()) {
            result.push_back(' ');
        }
    }
    result.append(config.version);
    result.push_back('\n');
    return result;
}

inline std::string format_number(double value) {
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}

inline std::string format_help_from_specs(const std::vector<ArgSpec>& specs, const ArgParserConfig& config) {
    std::string result;
    if (!config.usage.empty()) {
        result.append(config.usage);
    } else {
        result.append(default_usage(config.programName));
    }
    result.push_back('\n');
    if (!config.description.empty()) {
        result.push_back('\n');
        result.append(config.description);
        result.push_back('\n');
    }
    result.append("\nOptions:\n");
    if (config.addHelp) {
        result.append("  -h, --help\n");
    }
    if (config.addVersion && !config.version.empty()) {
        result.append("  -V, --version\n");
    }
    for (const auto& spec : specs) {
        if (spec.hidden) {
            continue;
        }
        result.append("  ");
        if (!spec.shortName.empty()) {
            result.push_back('-');
            result.append(spec.shortName);
            result.append(", ");
        }
        if (!spec.positional) {
            result.append("--");
        }
        result.append(spec.longName);
        if (!spec.flag && !spec.positional) {
            result.append(" <value>");
        }
        if (spec.required) {
            result.append(" (required)");
        }
        if (spec.hasRange) {
            result.append(" (range: [");
            result.append(format_number(spec.rangeMin));
            result.append(", ");
            result.append(format_number(spec.rangeMax));
            result.append("))");
        }
        if (!spec.choices.empty()) {
            result.append(" (choices: {");
            for (std::size_t idx = 0; idx < spec.choices.size(); ++idx) {
                if (idx != 0) {
                    result.append(", ");
                }
                result.append(spec.choices[idx]);
            }
            result.append("})");
        }
        if (!spec.help.empty()) {
            result.append("\n      ");
            result.append(spec.help);
        }
        result.push_back('\n');
    }
    return result;
}

template <typename T, std::size_t I>
std::string command_name_at() {
    constexpr auto Names = Reflect<std::remove_cvref_t<T>>::names();
    constexpr auto Tags  = std::get<I>(Reflect<std::remove_cvref_t<T>>::value_tags);
    constexpr auto Name  = tag_long_name(Tags);
    if constexpr (!Name.empty()) {
        return std::string(Name);
    } else {
        return std::string(Names[I]);
    }
}

template <typename T>
std::string format_command_help(const ArgParserConfig& config) {
    std::string result;
    if (!config.usage.empty()) {
        result.append(config.usage);
    } else {
        result.append(default_command_usage(config.programName));
    }
    result.push_back('\n');
    if (!config.description.empty()) {
        result.push_back('\n');
        result.append(config.description);
        result.push_back('\n');
    }
    result.append("\nCommands:\n");
    if (config.addHelp) {
        result.append("  -h, --help\n");
    }
    if (config.addVersion && !config.version.empty()) {
        result.append("  -V, --version\n");
    }
    []<std::size_t... Is>(std::index_sequence<Is...>, std::string& out) {
        (([&] {
             constexpr auto Tags = std::get<Is>(Reflect<std::remove_cvref_t<T>>::value_tags);
             if constexpr (!tag_hidden(Tags)) {
                 out.append("  ");
                 out.append(command_name_at<T, Is>());
                 constexpr auto Help = tag_help(Tags);
                 if constexpr (!Help.empty()) {
                     out.append("\n      ");
                     out.append(Help);
                 }
                 out.push_back('\n');
             }
         }()),
         ...);
    }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{}, result);
    return result;
}

inline std::string format_placeholder_command_help(std::string_view programName, std::string_view description) {
    std::string result = "Usage:";
    if (!programName.empty()) {
        result.push_back(' ');
        result.append(programName);
    }
    result.push_back('\n');
    if (!description.empty()) {
        result.push_back('\n');
        result.append(description);
        result.push_back('\n');
    }
    return result;
}

template <typename T>
std::vector<ArgSpec> collect_specs_for(T& object, const ArgParserConfig& config,
                                       std::vector<std::size_t>& positionalSpecs) {
    std::vector<ArgSpec> specs;
    collect_specs(object, {}, config, specs, positionalSpecs);
    return specs;
}

// Option parsing -------------------------------------------------------------

template <typename T>
std::error_code parse_options_into(T& object, int argc, const char* const* argv, int startIndex,
                                   const ArgParserConfig& config) {
    std::vector<std::size_t> positionalSpecs;
    auto specs                  = collect_specs_for(object, config, positionalSpecs);
    std::size_t positionalIndex = 0;
    bool forcePositional        = false;

    for (int idx = startIndex; idx < argc; ++idx) {
        std::string_view arg = argv[idx] == nullptr ? std::string_view{} : std::string_view(argv[idx]);
        if (forcePositional) {
            if (auto error = apply_positional(specs, positionalSpecs, positionalIndex, arg)) {
                return error;
            }
            continue;
        }

        if (arg == "--") {
            forcePositional = true;
            continue;
        }

        if (config.addHelp && (arg == "--help" || arg == "-h")) {
            return make_error_code(ArgParserError::HelpRequested);
        }
        if (config.addVersion && !config.version.empty() && is_version_token(arg)) {
            return make_error_code(ArgParserError::VersionRequested);
        }

        if (arg.starts_with("--")) {
            auto body        = arg.substr(2);
            auto value       = std::string_view{};
            bool valueInline = false;
            if (const auto equal = body.find('='); equal != std::string_view::npos) {
                value       = body.substr(equal + 1);
                body        = body.substr(0, equal);
                valueInline = true;
            }

            auto* spec = find_long_spec(specs, body);
            if (spec == nullptr) {
                if (config.allowUnknown) {
                    continue;
                }
                return make_error_code(ArgParserError::UnknownOption);
            }
            if (!spec->flag && !valueInline) {
                if (idx + 1 >= argc || argv[idx + 1] == nullptr) {
                    return make_error_code(ArgParserError::MissingValue);
                }
                value = argv[++idx];
            }
            if (auto error = apply_spec(*spec, value)) {
                return error;
            }
            continue;
        }

        if (arg.starts_with("-") && arg.size() > 1) {
            auto body        = arg.substr(1);
            auto value       = std::string_view{};
            bool valueInline = false;
            if (const auto equal = body.find('='); equal != std::string_view::npos) {
                value       = body.substr(equal + 1);
                body        = body.substr(0, equal);
                valueInline = true;
            }

            if (config.allowShortCluster && body.size() > 1 && !valueInline) {
                const char firstShortName[] = {body[0], '\0'};
                auto* firstSpec             = find_short_spec(specs, firstShortName);
                if (firstSpec != nullptr && !firstSpec->flag) {
                    if (auto error = apply_spec(*firstSpec, body.substr(1))) {
                        return error;
                    }
                    continue;
                }

                for (char ch : body) {
                    const char shortName[] = {ch, '\0'};
                    auto* spec             = find_short_spec(specs, shortName);
                    if (spec == nullptr) {
                        if (config.allowUnknown) {
                            continue;
                        }
                        return make_error_code(ArgParserError::UnknownOption);
                    }
                    if (!spec->flag) {
                        return make_error_code(ArgParserError::MissingValue);
                    }
                    if (auto error = apply_spec(*spec, {})) {
                        return error;
                    }
                }
                continue;
            }

            auto* spec = find_short_spec(specs, body);
            if (spec == nullptr && body.size() > 1 && !valueInline) {
                const char shortName[] = {body[0], '\0'};
                spec                   = find_short_spec(specs, shortName);
                if (spec != nullptr) {
                    value       = body.substr(1);
                    valueInline = true;
                }
            }
            if (spec == nullptr) {
                if (config.allowUnknown) {
                    continue;
                }
                return make_error_code(ArgParserError::UnknownOption);
            }
            if (!spec->flag && !valueInline) {
                if (idx + 1 >= argc || argv[idx + 1] == nullptr) {
                    return make_error_code(ArgParserError::MissingValue);
                }
                value = argv[++idx];
            }
            if (auto error = apply_spec(*spec, value)) {
                return error;
            }
            continue;
        }

        if (auto error = apply_positional(specs, positionalSpecs, positionalIndex, arg)) {
            return error;
        }
    }

    for (const auto& spec : specs) {
        if (spec.required && !spec.seen) {
            return make_error_code(ArgParserError::MissingRequired);
        }
    }
    return {};
}

// Command parsing ------------------------------------------------------------

template <typename RootT, std::size_t I>
bool try_parse_command(std::string_view command, int argc, const char* const* argv, int startIndex,
                       const ArgParserConfig& config, parser_result_t<RootT>& result, std::error_code& error) {
    if (command != command_name_at<RootT, I>()) {
        return false;
    }
    using CommandT = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<RootT>>::value_types>;
    if constexpr (is_command_placeholder_v<CommandT>) {
        if (startIndex < argc) {
            const auto arg = argv[startIndex] == nullptr ? std::string_view{} : std::string_view(argv[startIndex]);
            if (config.addHelp && is_help_token(arg)) {
                error = make_error_code(ArgParserError::HelpRequested);
                return true;
            }
            if (config.addVersion && !config.version.empty() && is_version_token(arg)) {
                error = make_error_code(ArgParserError::VersionRequested);
                return true;
            }
            error = make_error_code(ArgParserError::UnexpectedPositional);
            return true;
        }
        result.template emplace<I>();
    } else {
        CommandT commandObject{};
        if (auto parseError = parse_options_into(commandObject, argc, argv, startIndex, config)) {
            error = parseError;
            return true;
        }
        result.template emplace<I>(std::move(commandObject));
    }
    return true;
}

template <typename T>
std::expected<parser_result_t<T>, std::error_code> parse_command_set(int argc, const char* const* argv,
                                                                     const ArgParserConfig& config) {
    if (argc <= 1 || argv == nullptr || argv[1] == nullptr) {
        return std::unexpected(make_error_code(ArgParserError::MissingRequired));
    }
    std::string_view command = argv[1];
    if (config.addHelp && is_help_token(command)) {
        return std::unexpected(make_error_code(ArgParserError::HelpRequested));
    }
    if (config.addVersion && !config.version.empty() && is_version_token(command)) {
        return std::unexpected(make_error_code(ArgParserError::VersionRequested));
    }

    parser_result_t<T> result;
    std::error_code error;
    bool matched =
        []<std::size_t... Is>(std::index_sequence<Is...>, std::string_view commandName, int argcValue,
                              const char* const* argvValue, const ArgParserConfig& parserConfig,
                              parser_result_t<T>& out, std::error_code& outError) {
            return (try_parse_command<T, Is>(commandName, argcValue, argvValue, 2, parserConfig, out, outError) || ...);
        }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{}, command, argc, argv, config, result,
          error);

    if (!matched) {
        return std::unexpected(make_error_code(ArgParserError::UnknownCommand));
    }
    if (error) {
        return std::unexpected(error);
    }
    return result;
}

template <typename RootT, std::size_t I>
bool try_format_command_help(std::string_view command, const ArgParserConfig& config, std::string& result) {
    if (command != command_name_at<RootT, I>()) {
        return false;
    }

    constexpr auto Tags    = std::get<I>(Reflect<std::remove_cvref_t<RootT>>::value_tags);
    const auto commandName = command_name_at<RootT, I>();
    std::string programName;
    if (!config.programName.empty()) {
        programName.append(config.programName);
        programName.push_back(' ');
    }
    programName.append(commandName);

    ArgParserConfig commandConfig = config;
    commandConfig.programName     = programName;
    commandConfig.usage           = {};
    constexpr auto CommandHelp    = tag_help(Tags);
    if constexpr (!CommandHelp.empty()) {
        commandConfig.description = CommandHelp;
    }

    using CommandT = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<RootT>>::value_types>;
    if constexpr (is_command_placeholder_v<CommandT>) {
        result = format_placeholder_command_help(commandConfig.programName, commandConfig.description);
    } else {
        CommandT object{};
        std::vector<std::size_t> positionalSpecs;
        auto specs = collect_specs_for(object, commandConfig, positionalSpecs);
        result     = format_help_from_specs(specs, commandConfig);
    }
    return true;
}

template <typename T>
std::string format_context_help(int argc, const char* const* argv, ArgParserConfig config) {
    if (config.programName.empty() && argc > 0 && argv != nullptr && argv[0] != nullptr) {
        config.programName = argv[0];
    }

    if constexpr (is_command_set_v<T>) {
        if (argc > 1 && argv != nullptr && argv[1] != nullptr && !is_help_token(argv[1]) &&
            !is_version_token(argv[1])) {
            std::string result;
            const auto matched =
                []<std::size_t... Is>(std::index_sequence<Is...>, std::string_view command, const ArgParserConfig& cfg,
                                      std::string& out) {
                    return (try_format_command_help<T, Is>(command, cfg, out) || ...);
                }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{}, std::string_view(argv[1]),
                  config, result);
            if (matched) {
                return result;
            }
        }
    }

    if constexpr (is_command_set_v<T>) {
        return format_command_help<T>(config);
    } else {
        T object{};
        std::vector<std::size_t> positionalSpecs;
        auto specs = collect_specs_for(object, config, positionalSpecs);
        return format_help_from_specs(specs, config);
    }
}
} // namespace detail

template <typename T>
std::string format_help(int argc, const char* const* argv, ArgParserConfig config = {}) {
    detail::static_check_parser_definition<T>();
    return detail::format_context_help<T>(argc, argv, config);
}

template <typename T>
std::string format_help(int argc, char** argv, ArgParserConfig config = {}) {
    return format_help<T>(argc, const_cast<const char* const*>(argv), config);
}

template <typename T>
std::string format_help(ArgParserConfig config = {}) {
    detail::static_check_parser_definition<T>();
    if constexpr (detail::is_command_set_v<T>) {
        return detail::format_command_help<T>(config);
    } else {
        T object{};
        std::vector<std::size_t> positionalSpecs;
        auto specs = detail::collect_specs_for(object, config, positionalSpecs);
        return detail::format_help_from_specs(specs, config);
    }
}

inline std::string format_version(ArgParserConfig config = {}) { return detail::format_version_text(config); }

template <typename T>
std::expected<detail::parser_result_t<T>, std::error_code> parser(int argc, const char* const* argv,
                                                                  ArgParserConfig config = {}) {
    static_assert(std::is_default_constructible_v<T>, "argparser requires a default constructible options type");
    detail::static_check_parser_definition<T>();

    if (config.programName.empty() && argc > 0 && argv != nullptr && argv[0] != nullptr) {
        config.programName = argv[0];
    }

    if constexpr (detail::is_command_set_v<T>) {
        return detail::parse_command_set<T>(argc, argv, config);
    } else {
        T object{};
        if (auto error = detail::parse_options_into(object, argc, argv, 1, config)) {
            return std::unexpected(error);
        }
        return object;
    }
}

template <typename T>
std::expected<detail::parser_result_t<T>, std::error_code> parser(int argc, char** argv, ArgParserConfig config = {}) {
    return parser<T>(argc, const_cast<const char* const*>(argv), config);
}

NEKO_END_NAMESPACE

namespace std {
template <>
struct is_error_code_enum<NEKO_NAMESPACE::ArgParserError> : true_type {};
} // namespace std
