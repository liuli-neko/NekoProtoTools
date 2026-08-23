#pragma once

#include "nekoproto/argparser/config.hpp"
#include "nekoproto/argparser/detail/config_io_registry.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/argparser/tags.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nekoproto {
namespace argparser::detail {
enum class ArgValueCompletion {
    None,
    File,
    Directory,
};

struct ArgSpec {
    std::string long_name;
    std::string scope;
    std::string short_name;
    std::vector<std::string_view> aliases;
    std::string help;
    std::string value_name;
    std::string env_name;
    std::string group;
    std::string deprecated_message;
    bool required                 = false;
    bool positional               = false;
    bool flag                     = false;
    bool repeatable               = false;
    bool hidden                   = false;
    bool has_range                = false;
    bool has_default              = false;
    bool has_implicit             = false;
    bool case_insensitive_choices = false;
    bool deprecated               = false;
    double range_min              = 0.0;
    double range_max              = 0.0;
    char separator                = '\0';
    std::string default_value;
    std::string implicit_value;
    std::vector<std::string_view> choices;
    std::vector<std::string_view> inferred_completion_choices;
    std::vector<std::string_view> conflicts;
    std::vector<std::string_view> requires_names;
    std::vector<std::size_t> conflict_indices;
    std::vector<std::size_t> require_indices;
    ArgValueCompletion completion = ArgValueCompletion::None;
};

struct ArgSchema {
    std::vector<ArgSpec> specs;
    std::vector<std::size_t> positional_specs;
    std::vector<std::optional<ConfigIoBuiltinSpec>> builtin_specs;
    std::size_t user_spec_count = 0;
    char nested_separator       = '.';

    void pushUserSpec(ArgSpec spec) {
        specs.push_back(std::move(spec));
        builtin_specs.push_back(std::nullopt);
    }

    void pushBuiltinSpec(ConfigIoBuiltinSpec builtin, ArgSpec spec) {
        specs.push_back(std::move(spec));
        builtin_specs.push_back(builtin);
    }

    [[nodiscard]] auto builtinSpec(std::size_t index) const -> std::optional<ConfigIoBuiltinSpec> {
        return index < builtin_specs.size() ? builtin_specs[index] : std::nullopt;
    }

    [[nodiscard]] auto findLongIndex(std::string_view name) const -> std::optional<std::size_t> {
        for (std::size_t index = 0; index < specs.size(); ++index) {
            const auto& spec = specs[index];
            if (!spec.positional) {
                if (spec.long_name == name) {
                    return index;
                }
                for (const auto alias : spec.aliases) {
                    if (alias.size() > 1 && alias == name) {
                        return index;
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto findShortIndex(std::string_view name) const -> std::optional<std::size_t> {
        for (std::size_t index = 0; index < specs.size(); ++index) {
            const auto& spec = specs[index];
            if (!spec.positional) {
                if (!spec.short_name.empty() && spec.short_name == name) {
                    return index;
                }
                for (const auto alias : spec.aliases) {
                    if (alias.size() == 1 && alias == name) {
                        return index;
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto findReferenceIndex(const ArgSpec& source,
                                                                  std::string_view reference) const -> std::optional<std::size_t> {
        if (reference.starts_with("--")) {
            return findLongIndex(reference.substr(2));
        }
        if (reference.starts_with("-") && reference.size() == 2U) {
            return findShortIndex(reference.substr(1));
        }

        const auto normalized = resolveReferenceName(source, reference);
        if (normalized.empty()) {
            return std::nullopt;
        }
        if (const auto result = findExactReferenceIndex(normalized); result.has_value()) {
            return result;
        }

        // A single unprefixed character may still denote a short option.
        if (reference.size() == 1U) {
            return findShortIndex(reference);
        }
        return std::nullopt;
    }

private:
    [[nodiscard]] auto findExactReferenceIndex(std::string_view name) const -> std::optional<std::size_t> {
        for (std::size_t index = 0; index < specs.size(); ++index) {
            if (specMatchesReference(specs[index], name)) {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto resolveReferenceName(const ArgSpec& source, std::string_view reference) const -> std::string {
        if (reference.empty()) {
            return {};
        }
        if (reference.starts_with('/')) {
            return std::string(reference.substr(1));
        }

        auto scope = std::string_view(source.scope);
        while (reference.starts_with("../")) {
            const auto parent = scope.rfind(nested_separator);
            scope             = parent == std::string_view::npos ? std::string_view{} : scope.substr(0, parent);
            reference.remove_prefix(3);
        }
        if (reference.starts_with("./")) {
            reference.remove_prefix(2);
            return joinReferenceScope(scope, reference);
        }

        // Dotted (or otherwise nested) names retain the old root-absolute meaning.
        if (reference.find(nested_separator) != std::string_view::npos) {
            return std::string(reference);
        }
        return joinReferenceScope(scope, reference);
    }

    [[nodiscard]] auto joinReferenceScope(std::string_view scope, std::string_view name) const -> std::string {
        if (name.empty()) {
            return {};
        }
        if (scope.empty()) {
            return std::string(name);
        }
        std::string result(scope);
        result.push_back(nested_separator);
        result.append(name);
        return result;
    }

    static auto specMatchesReference(const ArgSpec& spec, std::string_view normalized) -> bool {
        if (spec.long_name == normalized || spec.short_name == normalized) {
            return true;
        }
        return std::any_of(spec.aliases.begin(), spec.aliases.end(),
                           [&](const auto alias) { return alias == normalized; });
    }
};

inline auto joinArgName(std::string_view prefix, std::string_view name, char separator) -> std::string {
    if (prefix.empty()) {
        return std::string(name);
    }
    std::string result(prefix);
    result.push_back(separator);
    result.append(name);
    return result;
}

inline auto quoteArgValue(std::string_view value) -> std::string {
    std::string result = "'";
    result.append(value);
    result.push_back('\'');
    return result;
}

inline auto formatErrorOptionLabel(const ArgSpec& spec) -> std::string {
    std::string result;
    if (spec.positional) {
        result = "positional '";
        result.append(spec.long_name);
        result.push_back('\'');
        return result;
    }

    result = "--";
    result.append(spec.long_name);
    if (!spec.short_name.empty()) {
        result.append(" (-");
        result.append(spec.short_name);
        result.push_back(')');
    }
    return result;
}

template <typename T>
constexpr auto fieldTypeIsFlag(bool tagged_flag) -> bool {
    return tagged_flag || std::is_same_v<std::remove_cvref_t<T>, bool>;
}

template <typename T>
auto defaultValueToString(const T& value) -> std::string {
    using raw_t = std::remove_cvref_t<T>;
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string(std::string_view(value));
    } else if constexpr (std::is_same_v<raw_t, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum_v<raw_t>) {
        constexpr auto enum_names  = Reflect<raw_t>::names();
        constexpr auto enum_values = Reflect<raw_t>::values();
        for (std::size_t idx = 0; idx < enum_names.size(); ++idx) {
            if (enum_values[idx] == value) {
                return std::string(enum_names[idx]);
            }
        }
        return std::to_string(static_cast<std::underlying_type_t<raw_t>>(value));
    } else if constexpr (requires(std::ostringstream& stream, const T& item) { stream << item; }) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else {
        return {};
    }
}

template <typename T>
struct ArgScalarType {
    using type = std::remove_cvref_t<T>;
};

template <typename T>
struct ArgScalarType<std::optional<T>> {
    using type = T;
};

template <typename T, typename Alloc>
struct ArgScalarType<std::vector<T, Alloc>> {
    using type = T;
};

template <typename T>
using arg_scalar_type_t = typename ArgScalarType<std::remove_cvref_t<T>>::type;

template <typename FieldT, typename Tags>
auto makeArgSpec(std::string_view prefix, std::string_view reflected_name, const Tags& tags,
                      const argparser::ArgParserConfig& config) -> ArgSpec {
    const auto explicit_long_name = tag_query::get<tag_property::LongName>(tags);
    const auto absolute_long_name = tag_query::get<tag_property::AbsoluteLongName>(tags);
    const auto name               = explicit_long_name.empty() ? reflected_name : explicit_long_name;

    ArgSpec spec;
    spec.scope = std::string(prefix);
    if (absolute_long_name.empty()) {
        spec.long_name = joinArgName(prefix, name, config.nestedSeparator);
    } else {
        spec.long_name = std::string(absolute_long_name);
        const auto end = spec.long_name.rfind(config.nestedSeparator);
        spec.scope     = end == std::string::npos ? std::string{} : spec.long_name.substr(0, end);
    }
    if constexpr (tag_query::has<tag_property::ShortName>(Tags{})) {
        spec.short_name = "";
        spec.short_name.push_back(tag_query::get<tag_property::ShortName>(tags));
    }
    if constexpr (tag_query::has<tag_property::Aliases>(Tags{})) {
        auto aliases = tag_query::get<tag_property::Aliases>(tags);
        spec.aliases.assign(aliases.begin(), aliases.end());
    }
    if constexpr (tag_query::has<tag_property::Help>(Tags{})) {
        spec.help = std::string(tag_query::get<tag_property::Help>(tags));
    }
    if constexpr (tag_query::has<tag_property::ValueName>(Tags{})) {
        spec.value_name = std::string(tag_query::get<tag_property::ValueName>(tags));
    } else if constexpr (tag_query::hasTag<ArgCompleteFileImpl>(Tags{})) {
        spec.value_name = "FILE";
    } else if constexpr (tag_query::hasTag<ArgCompleteDirectoryImpl>(Tags{})) {
        spec.value_name = "DIR";
    }
    if constexpr (tag_query::has<tag_property::EnvName>(Tags{})) {
        spec.env_name = std::string(tag_query::get<tag_property::EnvName>(tags));
    }
    if constexpr (tag_query::has<tag_property::Group>(Tags{})) {
        spec.group = std::string(tag_query::get<tag_property::Group>(tags));
    }
    if constexpr (tag_query::has<tag_property::DeprecatedMessage>(Tags{})) {
        spec.deprecated_message = std::string(tag_query::get<tag_property::DeprecatedMessage>(tags));
    }
    if constexpr (tag_query::has<tag_property::Required>(Tags{})) {
        spec.required = tag_query::get<tag_property::Required>(tags);
    }
    if constexpr (tag_query::has<tag_property::Positional>(Tags{})) {
        spec.positional = tag_query::get<tag_property::Positional>(tags);
    }
    spec.flag         = fieldTypeIsFlag<FieldT>(tag_query::get<tag_property::Flag>(tags));
    spec.repeatable   = tag_query::get<tag_property::Repeatable>(tags) || is_vector_v<FieldT>;
    spec.hidden       = tag_query::get<tag_property::Hidden>(tags);
    spec.has_range    = tag_query::has<tag_property::RangeMin>(tags) || tag_query::has<tag_property::RangeMax>(tags);
    spec.has_default  = tag_query::has<tag_property::default_value>(tags);
    spec.has_implicit = tag_query::has<tag_property::implicit_value>(tags);
    if constexpr (tag_query::has<tag_property::CaseInsensitiveChoices>(Tags{})) {
        spec.case_insensitive_choices = tag_query::get<tag_property::CaseInsensitiveChoices>(tags);
    }
    if constexpr (tag_query::hasTag<ArgCompleteFileImpl>(Tags{})) {
        spec.completion = ArgValueCompletion::File;
    } else if constexpr (tag_query::hasTag<ArgCompleteDirectoryImpl>(Tags{})) {
        spec.completion = ArgValueCompletion::Directory;
    }
    if constexpr (tag_query::has<tag_property::DeprecatedMessage>(Tags{})) {
        spec.deprecated = tag_query::has<tag_property::DeprecatedMessage>(tags);
    }
    if constexpr (tag_query::has<tag_property::RangeMin>(Tags{})) {
        spec.range_min = tag_query::get<tag_property::RangeMin>(tags);
    }
    if constexpr (tag_query::has<tag_property::RangeMax>(Tags{})) {
        spec.range_max = tag_query::get<tag_property::RangeMax>(tags);
    }
    if constexpr (tag_query::has<tag_property::Separator>(Tags{})) {
        spec.separator = tag_query::get<tag_property::Separator>(tags);
    }

    if constexpr (tag_query::has<tag_property::default_value>(decltype(tags){})) {
        spec.default_value = defaultValueToString(tag_query::get<tag_property::default_value>(tags));
    }
    if constexpr (tag_query::has<tag_property::implicit_value>(decltype(tags){})) {
        spec.implicit_value = defaultValueToString(tag_query::get<tag_property::implicit_value>(tags));
    }

    if constexpr (tag_query::has<tag_property::Choices>(Tags{})) {
        auto choices = tag_query::get<tag_property::Choices>(tags);
        spec.choices.assign(choices.begin(), choices.end());
    }
    if constexpr (!tag_query::has<tag_property::Choices>(Tags{}) && std::is_enum_v<arg_scalar_type_t<FieldT>>) {
        constexpr auto enum_names = Reflect<arg_scalar_type_t<FieldT>>::names();
        spec.inferred_completion_choices.assign(enum_names.begin(), enum_names.end());
    }
    if constexpr (tag_query::has<tag_property::Conflicts>(Tags{})) {
        auto conflicts = tag_query::get<tag_property::Conflicts>(tags);
        spec.conflicts.assign(conflicts.begin(), conflicts.end());
    }
    if constexpr (tag_query::has<tag_property::RequiresNames>(Tags{})) {
        auto requires_names = tag_query::get<tag_property::RequiresNames>(tags);
        spec.requires_names.assign(requires_names.begin(), requires_names.end());
    }
    return spec;
}

template <typename Tags>
constexpr auto shouldIgnoreArgField(const Tags& tags) -> bool {
    return tag_query::get<tag_property::Ignore>(tags);
}

template <typename FieldT, typename Tags>
consteval void staticCheckOptionField() {
    using raw_t = std::remove_cvref_t<FieldT>;
    constexpr bool has_value_completion =
        tag_query::hasTag<ArgCompleteFileImpl>(Tags{}) || tag_query::hasTag<ArgCompleteDirectoryImpl>(Tags{});
    static_assert(!is_argparser_borrowed_text_v<raw_t>,
                  "argparser option fields cannot contain std::string_view; use owning std::string storage");
    static_assert(!has_value_completion || !fieldTypeIsFlag<FieldT>(tag_query::get<tag_property::Flag>(Tags{})),
                  "argparser value completion tags cannot be used with flags");
    static_assert(!has_value_completion || !tag_query::has<tag_property::Choices>(Tags{}),
                  "argparser choices and file/directory completion tags cannot be combined");
    if constexpr (tag_query::has<tag_property::Separator>(Tags{})) {
        static_assert(is_vector_v<raw_t>, "argparser separator tags require a std::vector field");
    }
}

inline auto makeConfigIoSpec(std::string_view name, std::string help) -> ArgSpec {
    ArgSpec spec;
    spec.long_name  = std::string(name);
    spec.help       = std::move(help);
    spec.value_name = "PATH";
    spec.completion = ArgValueCompletion::File;
    return spec;
}

inline auto configIoBuiltinNameAvailable(const ArgSchema& schema, std::string_view name) -> bool {
    if (name.empty()) {
        NEKO_LOG_WARN("argparser", "built-in argparser config option with an empty name is disabled");
        return false;
    }
    if (schema.findLongIndex(name).has_value()) {
        NEKO_LOG_WARN("argparser", "built-in argparser config option --{} is disabled because it is already used",
                      name);
        return false;
    }
    return true;
}

inline void pushConfigIoSpecIfAvailable(ArgSchema& schema, ConfigIoBuiltinSpec builtin, std::string_view name,
                                             std::string help) {
    if (!configIoBuiltinNameAvailable(schema, name)) {
        return;
    }
    schema.pushBuiltinSpec(builtin, makeConfigIoSpec(name, std::move(help)));
}

inline auto configIoFormatRegistered(std::string_view format) -> bool {
    bool registered = false;
    forEachConfigIoBackend([&]<typename Backend>(std::type_identity<Backend>) {
        registered = registered || configIoFormatMatches<Backend>(format);
    });
    return registered;
}

inline void warnUnknownConfigIoFormats(const std::vector<std::string_view>& formats, ConfigIoDirection direction) {
    for (const auto format : formats) {
        if (!configIoFormatRegistered(format)) {
            NEKO_LOG_WARN("argparser", "unknown argparser config {} format '{}' is ignored",
                          configIoDirectionName(direction), format);
        }
    }
}

template <typename Backend>
auto configIoFormatEnabled(const std::vector<std::string_view>& formats) -> bool {
    return std::any_of(formats.begin(), formats.end(),
                       [](const auto format) { return configIoFormatMatches<Backend>(format); });
}

template <typename Backend>
auto configIoOptionName(const ArgParserConfigIoOptions& io, ConfigIoDirection direction) -> std::string_view {
    for (const auto& option_names : io.optionNames) {
        if (!configIoFormatMatches<Backend>(option_names.format)) {
            continue;
        }
        const auto& name = direction == ConfigIoDirection::Import ? option_names.importName : option_names.exportName;
        if (name.has_value()) {
            return *name;
        }
    }
    return direction == ConfigIoDirection::Import ? Backend::defaultImportName : Backend::defaultExportName;
}

template <typename Backend>
void appendConfigIoBackendSpecs(const ArgParserConfigIoOptions& io, ArgSchema& schema) {
    if (configIoFormatEnabled<Backend>(io.importFormats)) {
        if constexpr (Backend::available) {
            pushConfigIoSpecIfAvailable(schema, {ConfigIoDirection::Import, Backend::format},
                                             configIoOptionName<Backend>(io, ConfigIoDirection::Import),
                                             std::string(Backend::importHelp));
        } else {
            NEKO_LOG_WARN("argparser", "{} config import is disabled because the backend is not available",
                          Backend::label);
        }
    }
    if (configIoFormatEnabled<Backend>(io.exportFormats)) {
        if constexpr (Backend::available) {
            pushConfigIoSpecIfAvailable(schema, {ConfigIoDirection::Export, Backend::format},
                                             configIoOptionName<Backend>(io, ConfigIoDirection::Export),
                                             std::string(Backend::exportHelp));
        } else {
            NEKO_LOG_WARN("argparser", "{} config export is disabled because the backend is not available",
                          Backend::label);
        }
    }
}

inline void appendConfigIoSpecs(const ArgParserConfig& config, ArgSchema& schema) {
    if (!config.configIo.has_value()) {
        return;
    }
    const auto& io = *config.configIo;
    warnUnknownConfigIoFormats(io.importFormats, ConfigIoDirection::Import);
    warnUnknownConfigIoFormats(io.exportFormats, ConfigIoDirection::Export);
    forEachConfigIoBackend(
        [&]<typename Backend>(std::type_identity<Backend>) { appendConfigIoBackendSpecs<Backend>(io, schema); });
}

template <typename T>
void collectSchemaInto(std::string_view prefix, const ArgParserConfig& config, ArgSchema& schema) {
    static_assert(nekoproto::detail::has_values_meta<std::remove_cvref_t<T>>,
                  "argparser requires a reflected options type");

    Reflect<std::remove_cvref_t<T>>::forEachMetaFull(
        [&]<typename FieldT>(std::type_identity<FieldT>, std::string_view reflected_name, const auto& tags) {
            if constexpr (shouldIgnoreArgField(decltype(tags){})) {
                return;
            } else {
                const auto explicit_long_name = tag_query::get<tag_property::LongName>(tags);
                const auto name               = explicit_long_name.empty() ? reflected_name : explicit_long_name;

                if constexpr (is_nested_option_v<FieldT>) {
                    if (tag_query::get<nekoproto::tag_property::Flat<std::remove_cvref_t<FieldT>>>(tags)) {
                        collectSchemaInto<FieldT>(prefix, config, schema);
                    } else {
                        collectSchemaInto<FieldT>(joinArgName(prefix, name, config.nestedSeparator), config,
                                                    schema);
                    }
                } else {
                    staticCheckOptionField<FieldT, std::remove_cvref_t<decltype(tags)>>();
                    schema.pushUserSpec(makeArgSpec<FieldT>(prefix, reflected_name, tags, config));
                    if (schema.specs.back().positional) {
                        schema.positional_specs.push_back(schema.specs.size() - 1);
                    }
                }
            }
        });
}

template <typename T>
auto collectSchema(const ArgParserConfig& config) -> ArgSchema {
    ArgSchema schema;
    schema.nested_separator = config.nestedSeparator;
    collectSchemaInto<T>({}, config, schema);
    schema.user_spec_count = schema.specs.size();
    appendConfigIoSpecs(config, schema);
    return schema;
}

inline auto isValidOptionPath(std::string_view path, char separator) -> bool {
    if (path.empty()) {
        return false;
    }
    bool previous_separator = true;
    for (const char ch : path) {
        if (ch == separator) {
            if (previous_separator) {
                return false;
            }
            previous_separator = true;
            continue;
        }
        if (!isOptionNameCharacter(ch)) {
            return false;
        }
        previous_separator = false;
    }
    return !previous_separator;
}

inline auto validateSchemaDefinition(ArgSchema& schema, const ArgParserConfig& /*config*/) -> std::error_code {
    if (schema.nested_separator == '\0' || schema.nested_separator == '/' || schema.nested_separator == '=' ||
        isOptionNameCharacter(schema.nested_separator) || schema.nested_separator <= ' ') {
        return makeArgparserError(ArgParserError::InvalidDefinition,
                                    "nestedSeparator must be a visible non-option-name character other than '/', '='");
    }

    struct NameEntry {
        std::string_view name;
        std::size_t index = 0;
    };
    std::vector<NameEntry> long_names;
    std::vector<NameEntry> short_names;

    const auto check_name = [&](std::vector<NameEntry>& entries, std::string_view name, std::size_t index,
                                std::string_view kind) -> std::error_code {
        for (const auto& entry : entries) {
            if (entry.name == name) {
                return makeArgparserError(ArgParserError::InvalidDefinition,
                                            std::string(kind) + " option name " + quoteArgValue(name) +
                                                " is used by both " +
                                                formatErrorOptionLabel(schema.specs[entry.index]) + " and " +
                                                formatErrorOptionLabel(schema.specs[index]));
            }
        }
        entries.push_back({name, index});
        return {};
    };

    for (std::size_t index = 0; index < schema.specs.size(); ++index) {
        const auto& spec = schema.specs[index];
        if (!isValidOptionPath(spec.long_name, schema.nested_separator)) {
            return makeArgparserError(ArgParserError::InvalidDefinition,
                                        formatErrorOptionLabel(spec) + " has an invalid long option path");
        }
        if (spec.positional && spec.flag) {
            return makeArgparserError(ArgParserError::InvalidDefinition,
                                        formatErrorOptionLabel(spec) + " cannot be both positional and a flag");
        }
        if (!spec.positional) {
            if (auto error = check_name(long_names, spec.long_name, index, "long")) {
                return error;
            }
            if (!spec.short_name.empty()) {
                if (!isShortOptionCharacter(spec.short_name.front())) {
                    return makeArgparserError(ArgParserError::InvalidDefinition,
                                                formatErrorOptionLabel(spec) + " has an invalid short option name");
                }
                if (auto error = check_name(short_names, spec.short_name, index, "short")) {
                    return error;
                }
            }
        }
        for (const auto alias : spec.aliases) {
            if (alias.empty()) {
                return makeArgparserError(ArgParserError::InvalidDefinition,
                                            formatErrorOptionLabel(spec) + " has an empty alias");
            }
            if (alias.size() == 1U) {
                if (!isShortOptionCharacter(alias.front())) {
                    return makeArgparserError(ArgParserError::InvalidDefinition,
                                                formatErrorOptionLabel(spec) + " has an invalid short alias");
                }
                if (auto error = check_name(short_names, alias, index, "short")) {
                    return error;
                }
            } else {
                if (!isValidOptionPath(alias, schema.nested_separator)) {
                    return makeArgparserError(ArgParserError::InvalidDefinition,
                                                formatErrorOptionLabel(spec) + " has an invalid long alias");
                }
                if (auto error = check_name(long_names, alias, index, "long")) {
                    return error;
                }
            }
        }
    }

    for (std::size_t position = 0; position < schema.positional_specs.size(); ++position) {
        const auto index = schema.positional_specs[position];
        if (schema.specs[index].repeatable && position + 1U != schema.positional_specs.size()) {
            return makeArgparserError(ArgParserError::InvalidDefinition,
                                        formatErrorOptionLabel(schema.specs[index]) +
                                            " is repeatable and therefore must be the last positional option");
        }
    }

    for (std::size_t index = 0; index < schema.user_spec_count; ++index) {
        auto& spec = schema.specs[index];
        spec.require_indices.clear();
        spec.conflict_indices.clear();
        for (const auto reference : spec.requires_names) {
            const auto target = schema.findReferenceIndex(spec, reference);
            if (!target.has_value() || *target == index) {
                return makeArgparserError(ArgParserError::InvalidDefinition, formatErrorOptionLabel(spec) +
                                                                                   " references unknown requirement " +
                                                                                   quoteArgValue(reference));
            }
            spec.require_indices.push_back(*target);
        }
        for (const auto reference : spec.conflicts) {
            const auto target = schema.findReferenceIndex(spec, reference);
            if (!target.has_value() || *target == index) {
                return makeArgparserError(ArgParserError::InvalidDefinition, formatErrorOptionLabel(spec) +
                                                                                   " references unknown conflict " +
                                                                                   quoteArgValue(reference));
            }
            spec.conflict_indices.push_back(*target);
        }
    }
    return {};
}

} // namespace argparser::detail
} // namespace nekoproto
