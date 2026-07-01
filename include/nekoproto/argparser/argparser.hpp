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

#include "nekoproto/argparser/config.hpp"
#include "nekoproto/argparser/detail/help.hpp"
#include "nekoproto/argparser/detail/materializer.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/argparser/tags.hpp"
#include "nekoproto/global/expected.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <variant>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

template <typename T>
inline constexpr bool is_command_placeholder_v = // NOLINT
    std::is_same_v<std::decay_t<T>, ArgCommand>;

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
    return tag_query::get<tag_property::command>(std::get<I>(Reflect<std::remove_cvref_t<T>>::field_tags));
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
    using field_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
    if constexpr (is_command_placeholder_v<field_t>) {
        return true;
    } else {
        return NEKO_NAMESPACE::detail::has_values_meta<field_t> && std::is_default_constructible_v<field_t>;
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
    constexpr bool has_any_command = has_any_command_v<T>;
    constexpr bool is_command_set  = is_command_set_v<T>;
    static_assert(!has_any_command || is_command_set,
                  "argparser command fields cannot be mixed with normal option fields in the same struct yet");
    if constexpr (is_command_set) {
        using values = typename Reflect<std::remove_cvref_t<T>>::value_types;
        static_assert(command_types_valid<T>(), "argparser command field type must be a reflected struct or an empty "
                                                "default-constructible placeholder type");
        static_assert(tuple_types_unique<values>(),
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

template <typename T>
std::error_code parse_options_into(T& object, int argc, const char* const* argv, int start_index,
                                   const ArgParserConfig& config) {
    auto schema = collect_schema<T>(config);
    auto raw    = parse_raw_arguments(schema, argc, argv, start_index, config);
    if (!raw.has_value()) {
        return raw.error();
    }
    return materialize_options_into(object, schema, *raw, config);
}

template <typename T, std::size_t I>
std::string command_name_at() {
    constexpr auto names = Reflect<std::remove_cvref_t<T>>::names();
    constexpr auto tags  = std::get<I>(Reflect<std::remove_cvref_t<T>>::field_tags);
    constexpr auto name  = tag_query::get<tag_property::long_name>(tags);
    if constexpr (!name.empty()) {
        return std::string(name);
    } else {
        return std::string(names[I]);
    }
}

template <typename RootT, std::size_t I>
bool try_parse_command(std::string_view command, int argc, const char* const* argv, int start_index,
                       const ArgParserConfig& config, parser_result_t<RootT>& result, std::error_code& error) {
    if (command != command_name_at<RootT, I>()) {
        return false;
    }

    using command_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<RootT>>::value_types>;
    if constexpr (is_command_placeholder_v<command_t>) {
        if (start_index < argc) {
            const auto arg = argv[start_index] == nullptr ? std::string_view{} : std::string_view(argv[start_index]);
            if (config.addHelp && is_help_token(arg)) {
                error = make_error_code(ArgParserError::HelpRequested);
                return true;
            }
            if (config.addVersion && !config.version.empty() && is_version_token(arg)) {
                error = make_error_code(ArgParserError::VersionRequested);
                return true;
            }
            error = make_argparser_error(ArgParserError::UnexpectedPositional,
                                         "command " + quote_arg_value(command) + " does not accept " +
                                             quote_arg_value(arg));
            return true;
        }
        result.template emplace<I>();
    } else {
        command_t command_object{};
        if (auto parse_error = parse_options_into(command_object, argc, argv, start_index, config)) {
            error = parse_error;
            return true;
        }
        result.template emplace<I>(std::move(command_object));
    }
    return true;
}

template <typename T>
expected::expected<parser_result_t<T>, std::error_code> parse_command_set(int argc, const char* const* argv,
                                                                          const ArgParserConfig& config) {
    if (argc <= 1 || argv == nullptr || argv[1] == nullptr) {
        return unexpected_error(make_argparser_error(ArgParserError::MissingRequired, "command name is required"));
    }
    std::string_view command = argv[1];
    if (config.addHelp && is_help_token(command)) {
        return unexpected_error(make_error_code(ArgParserError::HelpRequested));
    }
    if (config.addVersion && !config.version.empty() && is_version_token(command)) {
        return unexpected_error(make_error_code(ArgParserError::VersionRequested));
    }

    parser_result_t<T> result;
    std::error_code error;
    const bool matched =
        []<std::size_t... Is>(std::index_sequence<Is...>, std::string_view command_name, int argc_value,
                              const char* const* argv_value, const ArgParserConfig& parser_config,
                              parser_result_t<T>& out, std::error_code& out_error) {
            return (try_parse_command<T, Is>(command_name, argc_value, argv_value, 2, parser_config, out, out_error) ||
                    ...);
        }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{}, command, argc, argv, config, result,
          error);

    if (!matched) {
        return unexpected_error(make_argparser_error(ArgParserError::UnknownCommand, quote_arg_value(command)));
    }
    if (error) {
        return unexpected_error(error);
    }
    return result;
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
    Reflect<std::remove_cvref_t<T>>::forEachMeta([&result](std::string_view name, const auto& tags) {
        if (!tag_query::get<tag_property::hidden>(tags)) {
            result.append("  ");
            std::string_view cname = tag_query::get<tag_property::long_name>(tags).empty()
                                         ? name
                                         : tag_query::get<tag_property::long_name>(tags);
            result.append(cname);
            const auto help = tag_query::get<tag_property::help>(tags);
            if (!help.empty()) {
                result.append("\n      ");
                result.append(help);
            }
            result.push_back('\n');
        }
    });
    return result;
}

template <typename T>
bool try_format_command_help(std::string_view command, const ArgParserConfig& config, std::string& result,
                             const auto& tags) {
    std::string program_name;
    if (!config.programName.empty()) {
        program_name.append(config.programName);
        program_name.push_back(' ');
    }
    program_name.append(command);

    ArgParserConfig command_config = config;
    command_config.programName     = program_name;
    command_config.usage           = {};
    const auto command_help        = tag_query::get<tag_property::help>(tags);
    if (!command_help.empty()) {
        command_config.description = command_help;
    }

    if constexpr (is_command_placeholder_v<T>) {
        result = format_placeholder_command_help(command_config.programName, command_config.description);
    } else {
        result = format_help_from_schema(collect_schema<T>(command_config), command_config);
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
            std::string_view command = argv[1];
            bool matched             = false;
            Reflect<std::remove_cvref_t<T>>::forEachMeta(
                [&]<typename U>(std::type_identity<U>, std::string_view name, const auto& tags) {
                    std::string_view cname = tag_query::get<tag_property::long_name>(tags).empty()
                                                 ? name
                                                 : tag_query::get<tag_property::long_name>(tags);
                    if (cname == command) {
                        matched = try_format_command_help<U>(command, config, result, tags);
                    }
                });
            if (matched) {
                return result;
            }
        }
    }

    if constexpr (is_command_set_v<T>) {
        return format_command_help<T>(config);
    } else {
        return format_help_from_schema(collect_schema<T>(config), config);
    }
}

} // namespace argparser::detail

namespace argparser {

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
        return detail::format_help_from_schema(detail::collect_schema<T>(config), config);
    }
}

inline std::string format_version(ArgParserConfig config = {}) { return detail::format_version_text(config); }

template <typename T>
expected::expected<detail::parser_result_t<T>, std::error_code> parser(int argc, const char* const* argv,
                                                                       ArgParserConfig config = {}) {
    static_assert(std::is_default_constructible_v<T>, "argparser requires a default constructible options type");
    detail::static_check_parser_definition<T>();
    detail::clear_argparser_error_detail();

    if (config.programName.empty() && argc > 0 && argv != nullptr && argv[0] != nullptr) {
        config.programName = argv[0];
    }

    if constexpr (detail::is_command_set_v<T>) {
        return detail::parse_command_set<T>(argc, argv, config);
    } else {
        T object{};
        if (auto error = detail::parse_options_into(object, argc, argv, 1, config)) {
            return detail::unexpected_error(error);
        }
        return object;
    }
}

template <typename T>
expected::expected<detail::parser_result_t<T>, std::error_code> parser(int argc, char** argv,
                                                                       ArgParserConfig config = {}) {
    return parser<T>(argc, const_cast<const char* const*>(argv), config);
}

} // namespace argparser
NEKO_END_NAMESPACE
