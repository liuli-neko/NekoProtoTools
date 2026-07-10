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
#include "nekoproto/argparser/detail/config_io.hpp"
#include "nekoproto/argparser/detail/help.hpp"
#include "nekoproto/argparser/detail/materializer.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/argparser/tags.hpp"
#include "nekoproto/global/expected.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

template <typename T>
inline constexpr bool is_command_placeholder_v = detail::is_command_type<T>::value;

template <typename Tuple>
struct tuple_to_variant;

template <typename... Args>
struct tuple_to_variant<std::tuple<Args...>> {
    using type = std::variant<Args...>;
};

template <typename Tuple>
using tuple_to_variant_t = typename tuple_to_variant<Tuple>::type;

template <typename Tuple, typename T>
struct tuple_append;

template <typename... Args, typename T>
struct tuple_append<std::tuple<Args...>, T> {
    using type = std::tuple<Args..., T>;
};

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
consteval bool field_is_ignored() {
    return tag_query::get<tag_property::ignore>(std::get<I>(Reflect<std::remove_cvref_t<T>>::field_tags));
}

template <typename T, std::size_t I>
consteval bool field_is_command() {
    using field_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
    return !field_is_ignored<T, I>() &&
           (is_command_placeholder_v<field_t> ||
            tag_query::get<tag_property::command>(std::get<I>(Reflect<std::remove_cvref_t<T>>::field_tags)));
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
            return has_any_command<T>() && ((field_is_ignored<T, Is>() || field_is_command<T, Is>()) && ...);
        }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
    }
}

template <typename T>
inline constexpr bool has_any_command_v = has_any_command<T>(); // NOLINT

template <typename T>
inline constexpr bool is_command_set_v = all_fields_are_commands<T>(); // NOLINT

template <typename T, std::size_t I>
consteval bool command_type_valid_at() {
    if constexpr (field_is_ignored<T, I>()) {
        return true;
    } else {
        using field_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
        if constexpr (is_command_placeholder_v<field_t>) {
            return true;
        } else {
            return NEKO_NAMESPACE::detail::has_values_meta<field_t> && std::is_default_constructible_v<field_t>;
        }
    }
}

template <typename T>
consteval bool command_types_valid() {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (command_type_valid_at<T, Is>() && ...);
    }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
}

template <typename T, std::size_t I, typename Acc, bool Done = I == Reflect<std::remove_cvref_t<T>>::value_count>
struct command_result_tuple_builder;

template <typename T, std::size_t I, typename... Acc>
struct command_result_tuple_builder<T, I, std::tuple<Acc...>, false> {
    using field_t  = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
    using next_acc = std::conditional_t<field_is_ignored<T, I>(), std::tuple<Acc...>,
                                        typename tuple_append<std::tuple<Acc...>, field_t>::type>;
    using type     = typename command_result_tuple_builder<T, I + 1, next_acc>::type;
};

template <typename T, std::size_t I, typename Acc>
struct command_result_tuple_builder<T, I, Acc, true> {
    using type = Acc;
};

template <typename T>
using command_result_tuple_t = typename command_result_tuple_builder<std::remove_cvref_t<T>, 0, std::tuple<>>::type;

template <typename T>
consteval void static_check_parser_definition() {
    constexpr bool has_any_command = has_any_command_v<T>;
    constexpr bool is_command_set  = is_command_set_v<T>;
    static_assert(!has_any_command || is_command_set,
                  "argparser command fields cannot be mixed with normal option fields in the same struct yet");
    if constexpr (is_command_set) {
        static_assert(command_types_valid<T>(), "argparser command field type must be a reflected struct or an empty "
                                                "default-constructible placeholder type");
        static_assert(tuple_types_unique<command_result_tuple_t<T>>(),
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
    using type = tuple_to_variant_t<command_result_tuple_t<T>>;
};

template <typename T>
using parser_result_t = typename ParserResult<std::remove_cvref_t<T>>::type;

template <typename T>
std::error_code parse_options_into(T& object, int argc, const char* const* argv, int start_index,
                                   const ArgParserConfig& config, const ConfigIoFile* external_import = nullptr,
                                   const ConfigIoSelection* external_exports = nullptr) {
    auto schema = collect_schema<T>(config);
    if (auto error = validate_schema_definition(schema, config)) {
        return error;
    }
    auto raw = parse_raw_arguments(schema, argc, argv, start_index, config);
    if (!raw.has_value()) {
        return raw.error();
    }
    if (schema.specs.size() != raw->options.size()) {
        return make_argparser_error(ArgParserError::InvalidDefinition, "schema and raw option counts differ");
    }

    ConfigIoSelection local_io;
    if (auto error = collect_config_io_selection(schema, *raw, local_io)) {
        return error;
    }
    if (external_import != nullptr && local_io.import_file.has_value()) {
        return make_argparser_error(ArgParserError::InvalidValue,
                                    "only one config import option can be used at a time");
    }

    PresenceList supplied(schema.specs.size(), 0);
    if (auto error = apply_defaults_into(object, schema, supplied)) {
        return error;
    }
    const auto* import_file = external_import != nullptr         ? external_import
                              : local_io.import_file.has_value() ? &*local_io.import_file
                                                                 : nullptr;
    if (import_file != nullptr) {
        if (auto error = import_config_file(*import_file, object)) {
            return error;
        }
        if (auto error = mark_imported_options_supplied(object, schema, supplied)) {
            return error;
        }
    }
    if (auto error = materialize_explicit_options_into(object, schema, *raw, config, supplied)) {
        return error;
    }
    if (auto error = validate_materialized_options(object, schema, supplied)) {
        return error;
    }

    const auto& exports = external_exports == nullptr ? local_io : *external_exports;
    return export_config_files(exports, object);
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

inline ArgSchema collect_command_config_io_schema(const ArgParserConfig& config) {
    ArgSchema schema;
    schema.nested_separator = config.nestedSeparator;
    schema.user_spec_count  = 0;
    append_config_io_specs(config, schema);
    return schema;
}

struct CommandSelectorConfig {
    std::string command;

    struct Neko {
        static constexpr auto value = Object("command", &CommandSelectorConfig::command); // NOLINT
    };
};

struct CommandRootParseResult {
    std::string command;
    int start_index = 0;
    ConfigIoSelection io;
};

inline std::error_code record_command_root_option(const ArgSchema& schema, RawParseResult& raw, int argc,
                                                  const char* const* argv, int& idx, const ArgParserConfig& config) {
    std::string_view arg = argv[idx] == nullptr ? std::string_view{} : std::string_view(argv[idx]);
    if (arg.starts_with("--")) {
        auto body         = arg.substr(2);
        auto value        = std::string_view{};
        bool value_inline = false;
        if (const auto equal = body.find('='); equal != std::string_view::npos) {
            value        = body.substr(equal + 1);
            body         = body.substr(0, equal);
            value_inline = true;
        }

        const auto spec_index = schema.find_long_index(body);
        if (!spec_index.has_value()) {
            if (config.allowUnknown) {
                return {};
            }
            return make_argparser_error(ArgParserError::UnknownOption, "--" + std::string(body));
        }
        return record_raw_option(schema, raw, *spec_index, argc, argv, idx, value, value_inline, config);
    }
    return make_argparser_error(ArgParserError::UnknownOption, std::string(arg));
}

inline expected::expected<CommandRootParseResult, std::error_code>
parse_command_root_arguments(int argc, const char* const* argv, const ArgParserConfig& config) {
    auto schema = collect_command_config_io_schema(config);
    if (auto error = validate_schema_definition(schema, config)) {
        return unexpected_error(error);
    }

    RawParseResult raw;
    raw.options.resize(schema.specs.size());
    CommandRootParseResult result;

    for (int idx = 1; idx < argc; ++idx) {
        std::string_view arg = argv[idx] == nullptr ? std::string_view{} : std::string_view(argv[idx]);
        if (config.addHelp && is_help_token(arg) && !is_declared_option_token(schema, arg)) {
            return unexpected_error(make_error_code(ArgParserError::HelpRequested));
        }
        if (config.addVersion && !config.version.empty() && is_version_token(arg) &&
            !is_declared_option_token(schema, arg)) {
            return unexpected_error(make_error_code(ArgParserError::VersionRequested));
        }

        if (arg == "--") {
            if (idx + 1 < argc && argv[idx + 1] != nullptr) {
                result.command     = argv[idx + 1];
                result.start_index = idx + 2;
                break;
            }
            break;
        }

        if (arg.starts_with("--")) {
            if (schema.specs.empty()) {
                if (config.allowUnknown) {
                    continue;
                }
                return unexpected_error(make_argparser_error(ArgParserError::UnknownOption, std::string(arg)));
            }
            if (auto error = record_command_root_option(schema, raw, argc, argv, idx, config)) {
                return unexpected_error(error);
            }
            continue;
        }

        if (arg.starts_with("-") && arg.size() > 1) {
            if (config.allowUnknown) {
                continue;
            }
            return unexpected_error(make_argparser_error(ArgParserError::UnknownOption, std::string(arg)));
        }

        result.command     = std::string(arg);
        result.start_index = idx + 1;
        break;
    }

    if (auto error = collect_config_io_selection(schema, raw, result.io)) {
        return unexpected_error(error);
    }
    if (result.command.empty() && result.io.import_file.has_value()) {
        CommandSelectorConfig selector;
        if (auto error = import_config_file(*result.io.import_file, selector)) {
            return unexpected_error(error);
        }
        result.command     = std::move(selector.command);
        result.start_index = argc;
    }
    if (result.command.empty()) {
        return unexpected_error(make_argparser_error(ArgParserError::MissingRequired, "command name is required"));
    }
    return result;
}

inline std::error_code validate_imported_command_name(std::string_view expected, std::string_view actual) {
    if (actual == expected) {
        return {};
    }
    std::string message = "imported config command ";
    message.append(quote_arg_value(actual));
    message.append(" does not match selected command ");
    message.append(quote_arg_value(expected));
    return make_argparser_error(ArgParserError::InvalidValue, std::move(message));
}

template <typename T>
CommandConfig<std::remove_cvref_t<T>> make_command_config(std::string command, T&& params) {
    using CommandT = std::remove_cvref_t<T>;
    if constexpr (is_command_placeholder_v<CommandT>) {
        return CommandConfig<CommandT>{.command = std::move(command)};
    } else {
        return CommandConfig<CommandT>{.command = std::move(command), .params = std::forward<T>(params)};
    }
}

template <typename RootT, typename CommandT, std::size_t... Is>
std::string command_name_for_type_impl(std::index_sequence<Is...> /*unused*/) {
    std::string name;
    ((std::is_same_v<std::remove_cvref_t<CommandT>,
                     std::tuple_element_t<Is, typename Reflect<std::remove_cvref_t<RootT>>::value_types>> &&
              !field_is_ignored<RootT, Is>()
          ? (name = command_name_at<RootT, Is>(), true)
          : false),
     ...);
    return name;
}

template <typename RootT, typename CommandT>
std::string command_name_for_type() {
    return command_name_for_type_impl<RootT, CommandT>(
        std::make_index_sequence<Reflect<std::remove_cvref_t<RootT>>::value_count>{});
}

template <typename RootT>
std::error_code export_command_config_files(const ConfigIoSelection& selection, const parser_result_t<RootT>& result) {
    std::error_code error;
    std::visit(
        [&](const auto& selected) {
            using CommandT     = std::remove_cvref_t<decltype(selected)>;
            const auto command = command_name_for_type<RootT, CommandT>();
            auto config_value  = make_command_config(command, selected);
            error              = export_config_files(selection, config_value);
        },
        result);
    return error;
}

inline std::error_code merge_config_io_selection(ConfigIoSelection& target, ConfigIoSelection source) {
    if (source.import_file.has_value()) {
        if (target.import_file.has_value()) {
            return make_argparser_error(ArgParserError::InvalidValue,
                                        "only one config import option can be used at a time");
        }
        target.import_file = std::move(source.import_file);
    }
    target.export_files.insert(target.export_files.end(), std::make_move_iterator(source.export_files.begin()),
                               std::make_move_iterator(source.export_files.end()));
    return {};
}

template <typename RootT, std::size_t I>
bool try_parse_command(std::string_view command, int argc, const char* const* argv, int start_index,
                       const ArgParserConfig& config, const ConfigIoSelection& root_io, parser_result_t<RootT>& result,
                       ConfigIoSelection& selected_io, std::error_code& error) {
    if constexpr (field_is_ignored<RootT, I>()) {
        static_cast<void>(command);
        static_cast<void>(argc);
        static_cast<void>(argv);
        static_cast<void>(start_index);
        static_cast<void>(config);
        static_cast<void>(root_io);
        static_cast<void>(result);
        static_cast<void>(selected_io);
        static_cast<void>(error);
        return false;
    } else {
        if (command != command_name_at<RootT, I>()) {
            return false;
        }

        using command_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<RootT>>::value_types>;
        if constexpr (is_command_placeholder_v<command_t>) {
            auto schema = collect_command_config_io_schema(config);
            if (auto definition_error = validate_schema_definition(schema, config)) {
                error = definition_error;
                return true;
            }
            auto raw = parse_raw_arguments(schema, argc, argv, start_index, config);
            if (!raw.has_value()) {
                error = raw.error();
                return true;
            }
            ConfigIoSelection local_io;
            if (auto io_error = collect_config_io_selection(schema, *raw, local_io)) {
                error = io_error;
                return true;
            }
            ConfigIoSelection command_io = root_io;
            if (auto merge_error = merge_config_io_selection(command_io, std::move(local_io))) {
                error = merge_error;
                return true;
            }
            if (command_io.import_file.has_value()) {
                CommandConfig<command_t> imported;
                if (auto import_error = import_config_file(*command_io.import_file, imported)) {
                    error = import_error;
                    return true;
                }
                if (auto name_error = validate_imported_command_name(command, imported.command)) {
                    error = name_error;
                    return true;
                }
            }
            selected_io = std::move(command_io);
            result.template emplace<command_t>();
        } else {
            command_t command_object{};

            const ArgParserConfig& command_config = config;
            auto schema                           = collect_schema<command_t>(command_config);
            if (auto definition_error = validate_schema_definition(schema, command_config)) {
                error = definition_error;
                return true;
            }
            auto raw = parse_raw_arguments(schema, argc, argv, start_index, command_config);
            if (!raw.has_value()) {
                error = raw.error();
                return true;
            }
            if (schema.specs.size() != raw->options.size()) {
                error = make_argparser_error(ArgParserError::InvalidDefinition, "schema and raw option counts differ");
                return true;
            }
            ConfigIoSelection local_io;
            if (auto io_error = collect_config_io_selection(schema, *raw, local_io)) {
                error = io_error;
                return true;
            }
            ConfigIoSelection command_io = root_io;
            if (auto merge_error = merge_config_io_selection(command_io, std::move(local_io))) {
                error = merge_error;
                return true;
            }

            PresenceList supplied(schema.specs.size(), 0);
            if (auto default_error = apply_defaults_into(command_object, schema, supplied)) {
                error = default_error;
                return true;
            }
            if (command_io.import_file.has_value()) {
                CommandConfig<command_t> imported;
                if (auto import_error = import_config_file(*command_io.import_file, imported)) {
                    error = import_error;
                    return true;
                }
                if (auto name_error = validate_imported_command_name(command, imported.command)) {
                    error = name_error;
                    return true;
                }
                command_object = std::move(imported.params);
                if (auto import_presence_error = mark_imported_options_supplied(command_object, schema, supplied)) {
                    error = import_presence_error;
                    return true;
                }
            }
            if (auto parse_error =
                    materialize_explicit_options_into(command_object, schema, *raw, command_config, supplied)) {
                error = parse_error;
                return true;
            }
            if (auto validation_error = validate_materialized_options(command_object, schema, supplied)) {
                error = validation_error;
                return true;
            }
            selected_io = std::move(command_io);
            result.template emplace<command_t>(std::move(command_object));
        }
        return true;
    }
}

template <typename T>
expected::expected<parser_result_t<T>, std::error_code> parse_command_set(int argc, const char* const* argv,
                                                                          const ArgParserConfig& config) {
    if (argv == nullptr) {
        return unexpected_error(make_argparser_error(ArgParserError::MissingRequired, "command name is required"));
    }
    auto root = parse_command_root_arguments(argc, argv, config);
    if (!root.has_value()) {
        return unexpected_error(root.error());
    }

    parser_result_t<T> result;
    ConfigIoSelection selected_io;
    std::error_code error;
    const bool matched =
        []<std::size_t... Is>(std::index_sequence<Is...>, std::string_view command_name, int argc_value,
                              const char* const* argv_value, const ArgParserConfig& parser_config,
                              const ConfigIoSelection& root_io, int command_start_index, parser_result_t<T>& out,
                              ConfigIoSelection& out_io, std::error_code& out_error) {
            return (try_parse_command<T, Is>(command_name, argc_value, argv_value, command_start_index, parser_config,
                                             root_io, out, out_io, out_error) ||
                    ...);
        }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{}, root->command, argc, argv, config,
          root->io, root->start_index, result, selected_io, error);

    if (!matched) {
        return unexpected_error(make_argparser_error(ArgParserError::UnknownCommand, quote_arg_value(root->command)));
    }
    if (error) {
        return unexpected_error(error);
    }
    if (auto export_error = export_command_config_files<T>(selected_io, result)) {
        return unexpected_error(export_error);
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

    auto root_schema = collect_command_config_io_schema(config);
    if (config.addHelp || (config.addVersion && !config.version.empty()) || !root_schema.specs.empty()) {
        result.append("\nOptions:\n");
        append_builtin_option_entry(result, root_schema, config.addHelp, "h", "help");
        append_builtin_option_entry(result, root_schema, config.addVersion && !config.version.empty(), "V", "version");
        append_ungrouped_options(result, root_schema);
    }
    result.append("\nCommands:\n");
    Reflect<std::remove_cvref_t<T>>::forEachMeta([&result](std::string_view name, const auto& tags) {
        if (!tag_query::get<tag_property::ignore>(tags) && !tag_query::get<tag_property::hidden>(tags)) {
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
                    if (tag_query::get<tag_property::ignore>(tags)) {
                        return;
                    }
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

    if (argc < 0 || (argc > 0 && argv == nullptr)) {
        return detail::unexpected_error(
            detail::make_argparser_error(ArgParserError::InvalidValue, "argc/argv is not a valid argument vector"));
    }

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
