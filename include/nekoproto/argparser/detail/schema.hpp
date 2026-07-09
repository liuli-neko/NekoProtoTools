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

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {
struct ArgSpec {
    std::string long_name;
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
    std::vector<std::string_view> conflicts;
    std::vector<std::string_view> requires_names;
};

struct ArgSchema {
    std::vector<ArgSpec> specs;
    std::vector<std::size_t> positional_specs;
    std::vector<std::optional<ConfigIoBuiltinSpec>> builtin_specs;
    std::size_t user_spec_count = 0;

    void push_user_spec(ArgSpec spec) {
        specs.push_back(std::move(spec));
        builtin_specs.push_back(std::nullopt);
    }

    void push_builtin_spec(ConfigIoBuiltinSpec builtin, ArgSpec spec) {
        specs.push_back(std::move(spec));
        builtin_specs.push_back(builtin);
    }

    [[nodiscard]] std::optional<ConfigIoBuiltinSpec> builtin_spec(std::size_t index) const {
        return index < builtin_specs.size() ? builtin_specs[index] : std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> find_long_index(std::string_view name) const {
        for (std::size_t index = 0; index < specs.size(); ++index) {
            const auto& spec = specs[index];
            if (!spec.positional && spec.long_name == name) {
                return index;
            }
            for (const auto alias : spec.aliases) {
                if (alias.size() > 1 && alias == name) {
                    return index;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> find_short_index(std::string_view name) const {
        for (std::size_t index = 0; index < specs.size(); ++index) {
            const auto& spec = specs[index];
            if (!spec.positional && !spec.short_name.empty() && spec.short_name == name) {
                return index;
            }
            for (const auto alias : spec.aliases) {
                if (alias.size() == 1 && alias == name) {
                    return index;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> find_reference_index(std::string_view name) const {
        const auto normalized = trim_option_prefix(name);
        if (normalized.empty()) {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < specs.size(); ++index) {
            if (spec_matches_reference(specs[index], normalized)) {
                return index;
            }
        }
        return std::nullopt;
    }

private:
    static std::string_view trim_option_prefix(std::string_view name) {
        if (name.starts_with("--")) {
            return name.substr(2);
        }
        if (name.starts_with("-")) {
            return name.substr(1);
        }
        return name;
    }

    static bool spec_matches_reference(const ArgSpec& spec, std::string_view normalized) {
        if (spec.long_name == normalized) {
            return true;
        }
        if (!spec.short_name.empty() && spec.short_name == normalized) {
            return true;
        }
        return std::any_of(spec.aliases.begin(), spec.aliases.end(),
                           [&](const auto alias) { return alias == normalized; });
    }
};

inline std::string join_arg_name(std::string_view prefix, std::string_view name, char separator) {
    if (prefix.empty()) {
        return std::string(name);
    }
    std::string result(prefix);
    result.push_back(separator);
    result.append(name);
    return result;
}

inline std::string quote_arg_value(std::string_view value) {
    std::string result = "'";
    result.append(value);
    result.push_back('\'');
    return result;
}

inline std::string format_error_option_label(const ArgSpec& spec) {
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
constexpr bool field_type_is_flag(bool tagged_flag) {
    return tagged_flag || std::is_same_v<std::remove_cvref_t<T>, bool>;
}

template <typename T>
std::string default_value_to_string(const T& value) {
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

template <typename FieldT, typename Tags>
ArgSpec make_arg_spec(std::string_view prefix, std::string_view reflected_name, const Tags& tags,
                      const argparser::ArgParserConfig& config) {
    const auto explicit_long_name = tag_query::get<tag_property::long_name>(tags);
    const auto name               = explicit_long_name.empty() ? reflected_name : explicit_long_name;

    ArgSpec spec;
    spec.long_name = join_arg_name(prefix, name, config.nestedSeparator);
    if constexpr (tag_query::has<tag_property::short_name>(Tags{})) {
        spec.short_name = "";
        spec.short_name.push_back(tag_query::get<tag_property::short_name>(tags));
    }
    if constexpr (tag_query::has<tag_property::aliases>(Tags{})) {
        auto aliases = tag_query::get<tag_property::aliases>(tags);
        spec.aliases.assign(aliases.begin(), aliases.end());
    }
    if constexpr (tag_query::has<tag_property::help>(Tags{})) {
        spec.help = std::string(tag_query::get<tag_property::help>(tags));
    }
    if constexpr (tag_query::has<tag_property::value_name>(Tags{})) {
        spec.value_name = std::string(tag_query::get<tag_property::value_name>(tags));
    }
    if constexpr (tag_query::has<tag_property::env_name>(Tags{})) {
        spec.env_name = std::string(tag_query::get<tag_property::env_name>(tags));
    }
    if constexpr (tag_query::has<tag_property::group>(Tags{})) {
        spec.group = std::string(tag_query::get<tag_property::group>(tags));
    }
    if constexpr (tag_query::has<tag_property::deprecated_message>(Tags{})) {
        spec.deprecated_message = std::string(tag_query::get<tag_property::deprecated_message>(tags));
    }
    if constexpr (tag_query::has<tag_property::required>(Tags{})) {
        spec.required = tag_query::get<tag_property::required>(tags);
    }
    if constexpr (tag_query::has<tag_property::positional>(Tags{})) {
        spec.positional = tag_query::get<tag_property::positional>(tags);
    }
    spec.flag         = field_type_is_flag<FieldT>(tag_query::get<tag_property::flag>(tags));
    spec.repeatable   = tag_query::get<tag_property::repeatable>(tags) || is_vector_v<FieldT>;
    spec.hidden       = tag_query::get<tag_property::hidden>(tags);
    spec.has_range    = tag_query::has<tag_property::range_min>(tags) || tag_query::has<tag_property::range_max>(tags);
    spec.has_default  = tag_query::has<tag_property::default_value>(tags);
    spec.has_implicit = tag_query::has<tag_property::implicit_value>(tags);
    if constexpr (tag_query::has<tag_property::case_insensitive_choices>(Tags{})) {
        spec.case_insensitive_choices = tag_query::get<tag_property::case_insensitive_choices>(tags);
    }
    if constexpr (tag_query::has<tag_property::deprecated_message>(Tags{})) {
        spec.deprecated = tag_query::has<tag_property::deprecated_message>(tags);
    }
    if constexpr (tag_query::has<tag_property::range_min>(Tags{})) {
        spec.range_min = tag_query::get<tag_property::range_min>(tags);
    }
    if constexpr (tag_query::has<tag_property::range_max>(Tags{})) {
        spec.range_max = tag_query::get<tag_property::range_max>(tags);
    }
    if constexpr (tag_query::has<tag_property::separator>(Tags{})) {
        spec.separator = tag_query::get<tag_property::separator>(tags);
    }

    if constexpr (tag_query::has<tag_property::default_value>(decltype(tags){})) {
        spec.default_value = default_value_to_string(tag_query::get<tag_property::default_value>(tags));
    }
    if constexpr (tag_query::has<tag_property::implicit_value>(decltype(tags){})) {
        spec.implicit_value = default_value_to_string(tag_query::get<tag_property::implicit_value>(tags));
    }

    if constexpr (tag_query::has<tag_property::choices>(Tags{})) {
        auto choices = tag_query::get<tag_property::choices>(tags);
        spec.choices.assign(choices.begin(), choices.end());
    }
    if constexpr (tag_query::has<tag_property::conflicts>(Tags{})) {
        auto conflicts = tag_query::get<tag_property::conflicts>(tags);
        spec.conflicts.assign(conflicts.begin(), conflicts.end());
    }
    if constexpr (tag_query::has<tag_property::requires_names>(Tags{})) {
        auto requires_names = tag_query::get<tag_property::requires_names>(tags);
        spec.requires_names.assign(requires_names.begin(), requires_names.end());
    }
    return spec;
}

template <typename Tags>
constexpr bool should_ignore_arg_field(const Tags& tags) {
    return tag_query::get<tag_property::ignore>(tags);
}

inline ArgSpec make_config_io_spec(std::string_view name, std::string help) {
    ArgSpec spec;
    spec.long_name  = std::string(name);
    spec.help       = std::move(help);
    spec.value_name = "PATH";
    return spec;
}

inline bool config_io_builtin_name_available(const ArgSchema& schema, std::string_view name) {
    if (name.empty()) {
        NEKO_LOG_WARN("argparser", "built-in argparser config option with an empty name is disabled");
        return false;
    }
    if (schema.find_reference_index(name).has_value()) {
        NEKO_LOG_WARN("argparser", "built-in argparser config option --{} is disabled because it is already used",
                      name);
        return false;
    }
    return true;
}

inline void push_config_io_spec_if_available(ArgSchema& schema, ConfigIoBuiltinSpec builtin, std::string_view name,
                                             std::string help) {
    if (!config_io_builtin_name_available(schema, name)) {
        return;
    }
    schema.push_builtin_spec(builtin, make_config_io_spec(name, std::move(help)));
}

inline bool config_io_format_registered(std::string_view format) {
    bool registered = false;
    for_each_config_io_backend([&]<typename Backend>(std::type_identity<Backend>) {
        registered = registered || config_io_format_matches<Backend>(format);
    });
    return registered;
}

inline void warn_unknown_config_io_formats(const std::vector<std::string_view>& formats, ConfigIoDirection direction) {
    for (const auto format : formats) {
        if (!config_io_format_registered(format)) {
            NEKO_LOG_WARN("argparser", "unknown argparser config {} format '{}' is ignored",
                          config_io_direction_name(direction), format);
        }
    }
}

template <typename Backend>
bool config_io_format_enabled(const std::vector<std::string_view>& formats) {
    return std::any_of(formats.begin(), formats.end(),
                       [](const auto format) { return config_io_format_matches<Backend>(format); });
}

template <typename Backend>
std::string_view config_io_option_name(const ArgParserConfigIoOptions& io, ConfigIoDirection direction) {
    for (const auto& option_names : io.optionNames) {
        if (!config_io_format_matches<Backend>(option_names.format)) {
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
void append_config_io_backend_specs(const ArgParserConfigIoOptions& io, ArgSchema& schema) {
    if (config_io_format_enabled<Backend>(io.importFormats)) {
        if constexpr (Backend::available) {
            push_config_io_spec_if_available(schema, {ConfigIoDirection::Import, Backend::format},
                                             config_io_option_name<Backend>(io, ConfigIoDirection::Import),
                                             std::string(Backend::importHelp));
        } else {
            NEKO_LOG_WARN("argparser", "{} config import is disabled because the backend is not available",
                          Backend::label);
        }
    }
    if (config_io_format_enabled<Backend>(io.exportFormats)) {
        if constexpr (Backend::available) {
            push_config_io_spec_if_available(schema, {ConfigIoDirection::Export, Backend::format},
                                             config_io_option_name<Backend>(io, ConfigIoDirection::Export),
                                             std::string(Backend::exportHelp));
        } else {
            NEKO_LOG_WARN("argparser", "{} config export is disabled because the backend is not available",
                          Backend::label);
        }
    }
}

inline void append_config_io_specs(const ArgParserConfig& config, ArgSchema& schema) {
    if (!config.configIo.has_value()) {
        return;
    }
    const auto& io = *config.configIo;
    warn_unknown_config_io_formats(io.importFormats, ConfigIoDirection::Import);
    warn_unknown_config_io_formats(io.exportFormats, ConfigIoDirection::Export);
    for_each_config_io_backend(
        [&]<typename Backend>(std::type_identity<Backend>) { append_config_io_backend_specs<Backend>(io, schema); });
}

template <typename T>
void collect_schema_into(std::string_view prefix, const ArgParserConfig& config, ArgSchema& schema) {
    static_assert(NEKO_NAMESPACE::detail::has_values_meta<std::remove_cvref_t<T>>,
                  "argparser requires a reflected options type");

    Reflect<std::remove_cvref_t<T>>::forEachMeta(
        [&]<typename FieldT>(std::type_identity<FieldT>, std::string_view reflected_name, const auto& tags) {
            if constexpr (should_ignore_arg_field(decltype(tags){})) {
                return;
            } else {
                const auto explicit_long_name = tag_query::get<tag_property::long_name>(tags);
                const auto name               = explicit_long_name.empty() ? reflected_name : explicit_long_name;

                if constexpr (is_nested_option_v<FieldT>) {
                    collect_schema_into<FieldT>(join_arg_name(prefix, name, config.nestedSeparator), config, schema);
                } else {
                    schema.push_user_spec(make_arg_spec<FieldT>(prefix, reflected_name, tags, config));
                    if (schema.specs.back().positional) {
                        schema.positional_specs.push_back(schema.specs.size() - 1);
                    }
                }
            }
        });
}

template <typename T>
ArgSchema collect_schema(const ArgParserConfig& config) {
    ArgSchema schema;
    collect_schema_into<T>({}, config, schema);
    schema.user_spec_count = schema.specs.size();
    append_config_io_specs(config, schema);
    return schema;
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
