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
#include "nekoproto/argparser/config_io.hpp"
#include "nekoproto/argparser/detail/completion.hpp"
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

namespace nekoproto {
namespace argparser::detail {

template <typename T>
inline constexpr bool is_command_placeholder_v = detail::IsCommandType<T>::value;

template <typename Tuple>
struct TupleToVariant;

template <typename... Args>
struct TupleToVariant<std::tuple<Args...>> {
    using type = std::variant<Args...>;
};

template <typename Tuple>
using tuple_to_variant_t = typename TupleToVariant<Tuple>::type;

template <typename Tuple, typename T>
struct TupleAppend;

template <typename... Args, typename T>
struct TupleAppend<std::tuple<Args...>, T> {
    using type = std::tuple<Args..., T>;
};

template <typename Tuple, std::size_t I>
consteval auto tupleTypeUniqueAt() -> bool {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return ((I == Is || !std::is_same_v<std::tuple_element_t<I, Tuple>, std::tuple_element_t<Is, Tuple>>) && ...);
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple>
consteval auto tupleTypesUnique() -> bool {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (tupleTypeUniqueAt<Tuple, Is>() && ...);
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename T, std::size_t I>
consteval auto fieldIsIgnored() -> bool {
    return tag_query::get<tag_property::Ignore>(std::get<I>(Reflect<std::remove_cvref_t<T>>::field_tags));
}

template <typename T, std::size_t I>
consteval auto fieldIsCommand() -> bool {
    using field_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
    return !fieldIsIgnored<T, I>() &&
           (is_command_placeholder_v<field_t> ||
            tag_query::get<tag_property::Command>(std::get<I>(Reflect<std::remove_cvref_t<T>>::field_tags)));
}

template <typename T>
consteval auto hasAnyCommand() -> bool {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (fieldIsCommand<T, Is>() || ...);
    }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
}

template <typename T>
consteval auto allFieldsAreCommands() -> bool {
    if constexpr (Reflect<std::remove_cvref_t<T>>::value_count == 0) {
        return false;
    } else {
        return []<std::size_t... Is>(std::index_sequence<Is...>) {
            return hasAnyCommand<T>() && ((fieldIsIgnored<T, Is>() || fieldIsCommand<T, Is>()) && ...);
        }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
    }
}

template <typename T>
inline constexpr bool has_any_command_v = hasAnyCommand<T>(); // NOLINT

template <typename T>
inline constexpr bool is_command_set_v = allFieldsAreCommands<T>(); // NOLINT

template <typename T, std::size_t I>
consteval auto commandTypeValidAt() -> bool {
    if constexpr (fieldIsIgnored<T, I>()) {
        return true;
    } else {
        using field_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
        if constexpr (is_command_placeholder_v<field_t>) {
            return true;
        } else {
            return nekoproto::detail::HasValuesMeta<field_t> && std::is_default_constructible_v<field_t>;
        }
    }
}

template <typename T>
consteval auto commandTypesValid() -> bool {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (commandTypeValidAt<T, Is>() && ...);
    }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{});
}

template <typename T, std::size_t I, typename Acc, bool Done = I == Reflect<std::remove_cvref_t<T>>::value_count>
struct CommandResultTupleBuilder;

template <typename T, std::size_t I, typename... Acc>
struct CommandResultTupleBuilder<T, I, std::tuple<Acc...>, false> {
    using field_t  = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<T>>::value_types>;
    using next_acc = std::conditional_t<fieldIsIgnored<T, I>(), std::tuple<Acc...>,
                                        typename TupleAppend<std::tuple<Acc...>, field_t>::type>;
    using type     = typename CommandResultTupleBuilder<T, I + 1, next_acc>::type;
};

template <typename T, std::size_t I, typename Acc>
struct CommandResultTupleBuilder<T, I, Acc, true> {
    using type = Acc;
};

template <typename T>
using command_result_tuple_t = typename CommandResultTupleBuilder<std::remove_cvref_t<T>, 0, std::tuple<>>::type;

template <typename T>
consteval void staticCheckParserDefinition() {
    constexpr bool hasAnyCommand = has_any_command_v<T>;
    constexpr bool is_command_set  = is_command_set_v<T>;
    static_assert(!hasAnyCommand || is_command_set,
                  "argparser command fields cannot be mixed with normal option fields in the same struct yet");
    if constexpr (is_command_set) {
        static_assert(commandTypesValid<T>(), "argparser command field type must be a reflected struct or an empty "
                                                "default-constructible placeholder type");
        static_assert(tupleTypesUnique<command_result_tuple_t<T>>(),
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
auto parseOptionsInto(T& object, int argc, const char* const* argv, int start_index,
                                   const ArgParserConfig& config, const ConfigIoFile* external_import = nullptr,
                                   const ConfigIoSelection* external_exports = nullptr) -> std::error_code {
    auto schema = collectSchema<T>(config);
    if (auto error = validateSchemaDefinition(schema, config)) {
        return error;
    }
    auto raw = parseRawArguments(schema, argc, argv, start_index, config);
    if (!raw.has_value()) {
        return raw.error();
    }
    if (schema.specs.size() != raw->options.size()) {
        return makeArgparserError(ArgParserError::InvalidDefinition, "schema and raw option counts differ");
    }

    ConfigIoSelection local_io;
    if (auto error = collectConfigIoSelection(schema, *raw, local_io)) {
        return error;
    }
    if (external_import != nullptr && local_io.import_file.has_value()) {
        return makeArgparserError(ArgParserError::InvalidValue,
                                    "only one config import option can be used at a time");
    }

    PresenceList supplied(schema.specs.size(), 0);
    if (auto error = applyDefaultsInto(object, schema, supplied)) {
        return error;
    }
    const auto* import_file = external_import != nullptr         ? external_import
                              : local_io.import_file.has_value() ? &*local_io.import_file
                                                                 : nullptr;
    if (import_file != nullptr) {
        if (auto error = importConfigFile(*import_file, object)) {
            return error;
        }
        if (auto error = markImportedOptionsSupplied(object, schema, supplied)) {
            return error;
        }
    }
    if (auto error = materializeExplicitOptionsInto(object, schema, *raw, config, supplied)) {
        return error;
    }
    if (auto error = validateMaterializedOptions(object, schema, supplied)) {
        return error;
    }

    const auto& exports = external_exports == nullptr ? local_io : *external_exports;
    return exportConfigFiles(exports, object);
}

template <typename T, std::size_t I>
auto commandNameAt() -> std::string {
    constexpr auto names = Reflect<std::remove_cvref_t<T>>::names();
    constexpr auto tags  = std::get<I>(Reflect<std::remove_cvref_t<T>>::field_tags);
    constexpr auto name  = tag_query::get<tag_property::LongName>(tags);
    if constexpr (!name.empty()) {
        return std::string(name);
    } else {
        return std::string(names[I]);
    }
}

inline auto collectCommandConfigIoSchema(const ArgParserConfig& config) -> ArgSchema {
    ArgSchema schema;
    schema.nested_separator = config.nestedSeparator;
    schema.user_spec_count  = 0;
    appendConfigIoSpecs(config, schema);
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

inline auto recordCommandRootOption(const ArgSchema& schema, RawParseResult& raw, int argc,
                                                  const char* const* argv, int& idx, const ArgParserConfig& config) -> std::error_code {
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

        const auto spec_index = schema.findLongIndex(body);
        if (!spec_index.has_value()) {
            if (config.allowUnknown) {
                return {};
            }
            return makeArgparserError(ArgParserError::UnknownOption, "--" + std::string(body));
        }
        return recordRawOption(schema, raw, *spec_index, argc, argv, idx, value, value_inline, config);
    }
    return makeArgparserError(ArgParserError::UnknownOption, std::string(arg));
}

inline auto
parseCommandRootArguments(int argc, const char* const* argv, const ArgParserConfig& config) -> expected::expected<CommandRootParseResult, std::error_code> {
    auto schema = collectCommandConfigIoSchema(config);
    if (auto error = validateSchemaDefinition(schema, config)) {
        return unexpectedError(error);
    }

    RawParseResult raw;
    raw.options.resize(schema.specs.size());
    CommandRootParseResult result;

    for (int idx = 1; idx < argc; ++idx) {
        std::string_view arg = argv[idx] == nullptr ? std::string_view{} : std::string_view(argv[idx]);
        if (config.addHelp && isHelpToken(arg) && !isDeclaredOptionToken(schema, arg)) {
            return unexpectedError(makeErrorCode(ArgParserError::HelpRequested));
        }
        if (config.addVersion && !config.version.empty() && isVersionToken(arg) &&
            !isDeclaredOptionToken(schema, arg)) {
            return unexpectedError(makeErrorCode(ArgParserError::VersionRequested));
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
                return unexpectedError(makeArgparserError(ArgParserError::UnknownOption, std::string(arg)));
            }
            if (auto error = recordCommandRootOption(schema, raw, argc, argv, idx, config)) {
                return unexpectedError(error);
            }
            continue;
        }

        if (arg.starts_with("-") && arg.size() > 1) {
            if (config.allowUnknown) {
                continue;
            }
            return unexpectedError(makeArgparserError(ArgParserError::UnknownOption, std::string(arg)));
        }

        result.command     = std::string(arg);
        result.start_index = idx + 1;
        break;
    }

    if (auto error = collectConfigIoSelection(schema, raw, result.io)) {
        return unexpectedError(error);
    }
    if (result.command.empty() && result.io.import_file.has_value()) {
        CommandSelectorConfig selector;
        if (auto error = importConfigFile(*result.io.import_file, selector)) {
            return unexpectedError(error);
        }
        result.command     = std::move(selector.command);
        result.start_index = argc;
    }
    if (result.command.empty()) {
        return unexpectedError(makeArgparserError(ArgParserError::MissingRequired, "command name is required"));
    }
    return result;
}

inline auto validateImportedCommandName(std::string_view expected, std::string_view actual) -> std::error_code {
    if (actual == expected) {
        return {};
    }
    std::string message = "imported config command ";
    message.append(quoteArgValue(actual));
    message.append(" does not match selected command ");
    message.append(quoteArgValue(expected));
    return makeArgparserError(ArgParserError::InvalidValue, std::move(message));
}

template <typename T>
auto makeCommandConfig(std::string command, T&& params) -> CommandConfig<std::remove_cvref_t<T>> {
    using CommandT = std::remove_cvref_t<T>;
    if constexpr (is_command_placeholder_v<CommandT>) {
        return CommandConfig<CommandT>{.command = std::move(command)};
    } else {
        return CommandConfig<CommandT>{.command = std::move(command), .params = std::forward<T>(params)};
    }
}

template <typename RootT, typename CommandT, std::size_t... Is>
auto commandNameForTypeImpl(std::index_sequence<Is...> /*unused*/) -> std::string {
    std::string name;
    ((std::is_same_v<std::remove_cvref_t<CommandT>,
                     std::tuple_element_t<Is, typename Reflect<std::remove_cvref_t<RootT>>::value_types>> &&
              !fieldIsIgnored<RootT, Is>()
          ? (name = commandNameAt<RootT, Is>(), true)
          : false),
     ...);
    return name;
}

template <typename RootT, typename CommandT>
auto commandNameForType() -> std::string {
    return commandNameForTypeImpl<RootT, CommandT>(
        std::make_index_sequence<Reflect<std::remove_cvref_t<RootT>>::value_count>{});
}

template <typename RootT>
auto exportCommandConfigFiles(const ConfigIoSelection& selection, const parser_result_t<RootT>& result) -> std::error_code {
    std::error_code error;
    std::visit(
        [&](const auto& selected) {
            using CommandT     = std::remove_cvref_t<decltype(selected)>;
            const auto command = commandNameForType<RootT, CommandT>();
            auto config_value  = makeCommandConfig(command, selected);
            error              = exportConfigFiles(selection, config_value);
        },
        result);
    return error;
}

inline auto mergeConfigIoSelection(ConfigIoSelection& target, ConfigIoSelection source) -> std::error_code {
    if (source.import_file.has_value()) {
        if (target.import_file.has_value()) {
            return makeArgparserError(ArgParserError::InvalidValue,
                                        "only one config import option can be used at a time");
        }
        target.import_file = std::move(source.import_file);
    }
    target.export_files.insert(target.export_files.end(), std::make_move_iterator(source.export_files.begin()),
                               std::make_move_iterator(source.export_files.end()));
    return {};
}

template <typename RootT, std::size_t I>
auto tryParseCommand(std::string_view command, int argc, const char* const* argv, int start_index,
                       const ArgParserConfig& config, const ConfigIoSelection& root_io, parser_result_t<RootT>& result,
                       ConfigIoSelection& selected_io, std::error_code& error) -> bool {
    if constexpr (fieldIsIgnored<RootT, I>()) {
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
        if (command != commandNameAt<RootT, I>()) {
            return false;
        }

        using command_t = std::tuple_element_t<I, typename Reflect<std::remove_cvref_t<RootT>>::value_types>;
        if constexpr (is_command_placeholder_v<command_t>) {
            auto schema = collectCommandConfigIoSchema(config);
            if (auto definition_error = validateSchemaDefinition(schema, config)) {
                error = definition_error;
                return true;
            }
            auto raw = parseRawArguments(schema, argc, argv, start_index, config);
            if (!raw.has_value()) {
                error = raw.error();
                return true;
            }
            ConfigIoSelection local_io;
            if (auto io_error = collectConfigIoSelection(schema, *raw, local_io)) {
                error = io_error;
                return true;
            }
            ConfigIoSelection command_io = root_io;
            if (auto merge_error = mergeConfigIoSelection(command_io, std::move(local_io))) {
                error = merge_error;
                return true;
            }
            if (command_io.import_file.has_value()) {
                CommandConfig<command_t> imported;
                if (auto import_error = importConfigFile(*command_io.import_file, imported)) {
                    error = import_error;
                    return true;
                }
                if (auto name_error = validateImportedCommandName(command, imported.command)) {
                    error = name_error;
                    return true;
                }
            }
            selected_io = std::move(command_io);
            result.template emplace<command_t>();
        } else {
            command_t command_object{};

            const ArgParserConfig& command_config = config;
            auto schema                           = collectSchema<command_t>(command_config);
            if (auto definition_error = validateSchemaDefinition(schema, command_config)) {
                error = definition_error;
                return true;
            }
            auto raw = parseRawArguments(schema, argc, argv, start_index, command_config);
            if (!raw.has_value()) {
                error = raw.error();
                return true;
            }
            if (schema.specs.size() != raw->options.size()) {
                error = makeArgparserError(ArgParserError::InvalidDefinition, "schema and raw option counts differ");
                return true;
            }
            ConfigIoSelection local_io;
            if (auto io_error = collectConfigIoSelection(schema, *raw, local_io)) {
                error = io_error;
                return true;
            }
            ConfigIoSelection command_io = root_io;
            if (auto merge_error = mergeConfigIoSelection(command_io, std::move(local_io))) {
                error = merge_error;
                return true;
            }

            PresenceList supplied(schema.specs.size(), 0);
            if (auto default_error = applyDefaultsInto(command_object, schema, supplied)) {
                error = default_error;
                return true;
            }
            if (command_io.import_file.has_value()) {
                CommandConfig<command_t> imported;
                if (auto import_error = importConfigFile(*command_io.import_file, imported)) {
                    error = import_error;
                    return true;
                }
                if (auto name_error = validateImportedCommandName(command, imported.command)) {
                    error = name_error;
                    return true;
                }
                command_object = std::move(imported.params);
                if (auto import_presence_error = markImportedOptionsSupplied(command_object, schema, supplied)) {
                    error = import_presence_error;
                    return true;
                }
            }
            if (auto parse_error =
                    materializeExplicitOptionsInto(command_object, schema, *raw, command_config, supplied)) {
                error = parse_error;
                return true;
            }
            if (auto validation_error = validateMaterializedOptions(command_object, schema, supplied)) {
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
auto parseCommandSet(int argc, const char* const* argv,
                                                                          const ArgParserConfig& config) -> expected::expected<parser_result_t<T>, std::error_code> {
    if (argv == nullptr) {
        return unexpectedError(makeArgparserError(ArgParserError::MissingRequired, "command name is required"));
    }
    auto root = parseCommandRootArguments(argc, argv, config);
    if (!root.has_value()) {
        return unexpectedError(root.error());
    }

    parser_result_t<T> result;
    ConfigIoSelection selected_io;
    std::error_code error;
    const bool matched =
        []<std::size_t... Is>(std::index_sequence<Is...>, std::string_view command_name, int argc_value,
                              const char* const* argv_value, const ArgParserConfig& parser_config,
                              const ConfigIoSelection& root_io, int command_start_index, parser_result_t<T>& out,
                              ConfigIoSelection& out_io, std::error_code& out_error) {
            return (tryParseCommand<T, Is>(command_name, argc_value, argv_value, command_start_index, parser_config,
                                             root_io, out, out_io, out_error) ||
                    ...);
        }(std::make_index_sequence<Reflect<std::remove_cvref_t<T>>::value_count>{}, root->command, argc, argv, config,
          root->io, root->start_index, result, selected_io, error);

    if (!matched) {
        return unexpectedError(makeArgparserError(ArgParserError::UnknownCommand, quoteArgValue(root->command)));
    }
    if (error) {
        return unexpectedError(error);
    }
    if (auto export_error = exportCommandConfigFiles<T>(selected_io, result)) {
        return unexpectedError(export_error);
    }
    return result;
}

template <typename T>
auto formatCommandHelp(const ArgParserConfig& config) -> std::string {
    std::string result;
    if (!config.usage.empty()) {
        result.append(config.usage);
    } else {
        result.append(defaultCommandUsage(config.programName));
    }
    result.push_back('\n');
    if (!config.description.empty()) {
        result.push_back('\n');
        result.append(config.description);
        result.push_back('\n');
    }

    auto root_schema = collectCommandConfigIoSchema(config);
    if (config.addHelp || (config.addVersion && !config.version.empty()) || !root_schema.specs.empty()) {
        result.append("\nOptions:\n");
        appendBuiltinOptionEntry(result, root_schema, config.addHelp, "h", "help");
        appendBuiltinOptionEntry(result, root_schema, config.addVersion && !config.version.empty(), "V", "version");
        appendUngroupedOptions(result, root_schema);
    }
    result.append("\nCommands:\n");
    Reflect<std::remove_cvref_t<T>>::forEachMetaNamed([&result](std::string_view name, const auto& tags) {
        if (!tag_query::get<tag_property::Ignore>(tags) && !tag_query::get<tag_property::Hidden>(tags)) {
            result.append("  ");
            std::string_view cname = tag_query::get<tag_property::LongName>(tags).empty()
                                         ? name
                                         : tag_query::get<tag_property::LongName>(tags);
            result.append(cname);
            const auto help = tag_query::get<tag_property::Help>(tags);
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
auto tryFormatCommandHelp(std::string_view command, const ArgParserConfig& config, std::string& result,
                             const auto& tags) -> bool {
    std::string program_name;
    if (!config.programName.empty()) {
        program_name.append(config.programName);
        program_name.push_back(' ');
    }
    program_name.append(command);

    ArgParserConfig command_config = config;
    command_config.programName     = program_name;
    command_config.usage           = {};
    const auto command_help        = tag_query::get<tag_property::Help>(tags);
    if (!command_help.empty()) {
        command_config.description = command_help;
    }

    if constexpr (is_command_placeholder_v<T>) {
        result = formatPlaceholderCommandHelp(command_config.programName, command_config.description);
    } else {
        result = formatHelpFromSchema(collectSchema<T>(command_config), command_config);
    }
    return true;
}

template <typename T>
auto formatContextHelp(int argc, const char* const* argv, ArgParserConfig config) -> std::string {
    if (config.programName.empty() && argc > 0 && argv != nullptr && argv[0] != nullptr) {
        config.programName = argv[0];
    }

    if constexpr (is_command_set_v<T>) {
        if (argc > 1 && argv != nullptr && argv[1] != nullptr && !isHelpToken(argv[1]) &&
            !isVersionToken(argv[1])) {
            std::string result;
            std::string_view command = argv[1];
            bool matched             = false;
            Reflect<std::remove_cvref_t<T>>::forEachMetaFull(
                [&]<typename U>(std::type_identity<U>, std::string_view name, const auto& tags) {
                    if (tag_query::get<tag_property::Ignore>(tags)) {
                        return;
                    }
                    std::string_view cname = tag_query::get<tag_property::LongName>(tags).empty()
                                                 ? name
                                                 : tag_query::get<tag_property::LongName>(tags);
                    if (cname == command) {
                        matched = tryFormatCommandHelp<U>(command, config, result, tags);
                    }
                });
            if (matched) {
                return result;
            }
        }
    }

    if constexpr (is_command_set_v<T>) {
        return formatCommandHelp<T>(config);
    } else {
        return formatHelpFromSchema(collectSchema<T>(config), config);
    }
}

template <typename T>
auto collectCompletionModel(std::string_view command_name, const ArgParserConfig& config) -> CompletionModel {
    CompletionModel model;
    model.command_name = std::string(command_name);
    if constexpr (is_command_set_v<T>) {
        model.root = makeCompletionNode(collectCommandConfigIoSchema(config), config, model.valid);
        Reflect<std::remove_cvref_t<T>>::forEachMetaFull(
            [&]<typename U>(std::type_identity<U>, std::string_view name, const auto& tags) {
                if (tag_query::get<tag_property::Ignore>(tags) || tag_query::get<tag_property::Hidden>(tags)) {
                    return;
                }
                CompletionCommand command;
                const auto explicit_name = tag_query::get<tag_property::LongName>(tags);
                command.name             = std::string(explicit_name.empty() ? name : explicit_name);
                command.help             = std::string(tag_query::get<tag_property::Help>(tags));
                if constexpr (is_command_placeholder_v<U>) {
                    command.node = makeCompletionNode(collectCommandConfigIoSchema(config), config, model.valid);
                } else {
                    command.node = makeCompletionNode(collectSchema<U>(config), config, model.valid);
                }
                model.commands.push_back(std::move(command));
            });
    } else {
        model.root = makeCompletionNode(collectSchema<T>(config), config, model.valid);
    }
    return model;
}

} // namespace argparser::detail

namespace argparser {

template <typename T>
auto formatHelp(int argc, const char* const* argv, ArgParserConfig config = {}) -> std::string {
    detail::staticCheckParserDefinition<T>();
    return detail::formatContextHelp<T>(argc, argv, config);
}

template <typename T>
auto formatHelp(int argc, char** argv, ArgParserConfig config = {}) -> std::string {
    return formatHelp<T>(argc, const_cast<const char* const*>(argv), config);
}

template <typename T>
auto formatHelp(ArgParserConfig config = {}) -> std::string {
    detail::staticCheckParserDefinition<T>();
    if constexpr (detail::is_command_set_v<T>) {
        return detail::formatCommandHelp<T>(config);
    } else {
        return detail::formatHelpFromSchema(detail::collectSchema<T>(config), config);
    }
}

inline auto formatVersion(ArgParserConfig config = {}) -> std::string { return detail::formatVersionText(config); }

template <typename T>
auto formatCompletion(CompletionShell shell, std::string_view command_name, ArgParserConfig config = {}) -> std::string {
    detail::staticCheckParserDefinition<T>();
    config.programName = command_name;
    return detail::formatCompletionModel(detail::collectCompletionModel<T>(command_name, config), shell);
}

template <typename T>
auto parser(int argc, const char* const* argv,
                                                                       ArgParserConfig config = {}) -> expected::expected<detail::parser_result_t<T>, std::error_code> {
    static_assert(std::is_default_constructible_v<T>, "argparser requires a default constructible options type");
    detail::staticCheckParserDefinition<T>();
    detail::clearArgparserErrorDetail();

    if (argc < 0 || (argc > 0 && argv == nullptr)) {
        return detail::unexpectedError(
            detail::makeArgparserError(ArgParserError::InvalidValue, "argc/argv is not a valid argument vector"));
    }

    if (config.programName.empty() && argc > 0 && argv != nullptr && argv[0] != nullptr) {
        config.programName = argv[0];
    }

    if constexpr (detail::is_command_set_v<T>) {
        return detail::parseCommandSet<T>(argc, argv, config);
    } else {
        T object{};
        if (auto error = detail::parseOptionsInto(object, argc, argv, 1, config)) {
            return detail::unexpectedError(error);
        }
        return object;
    }
}

template <typename T>
auto parser(int argc, char** argv,
                                                                       ArgParserConfig config = {}) -> expected::expected<detail::parser_result_t<T>, std::error_code> {
    return parser<T>(argc, const_cast<const char* const*>(argv), config);
}

} // namespace argparser
} // namespace nekoproto
