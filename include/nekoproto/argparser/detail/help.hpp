#pragma once

#include "nekoproto/argparser/detail/schema.hpp"
#include "nekoproto/global/global.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

inline bool positional_is_required(const ArgSpec& spec) { return spec.required && !spec.has_default; }

inline std::string format_positional_usage(const ArgSpec& spec) {
    std::string result;
    result.push_back('<');
    result.append(spec.value_name.empty() ? spec.long_name : spec.value_name);
    result.push_back('>');
    if (spec.repeatable) {
        result.append("...");
    }
    if (!positional_is_required(spec)) {
        result.insert(result.begin(), '[');
        result.push_back(']');
    }
    return result;
}

inline std::string default_usage(std::string_view programName, const ArgSchema& schema) {
    std::string usage = "Usage:";
    if (!programName.empty()) {
        usage.push_back(' ');
        usage.append(programName);
    }
    usage.append(" [options]");
    for (const auto index : schema.positional_specs) {
        const auto& spec = schema.specs[index];
        if (spec.hidden) {
            continue;
        }
        usage.push_back(' ');
        usage.append(format_positional_usage(spec));
    }
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

inline std::string format_version_text(const argparser::ArgParserConfig& config) {
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

inline std::string format_option_label(const ArgSpec& spec) {
    auto append_name = [](std::string& out, std::string_view prefix, std::string_view name) {
        if (!out.empty()) {
            out.append(", ");
        }
        out.append(prefix);
        out.append(name);
    };

    std::string result;
    if (spec.positional) {
        result.append(spec.value_name.empty() ? spec.long_name : spec.value_name);
        if (spec.repeatable) {
            result.append("...");
        }
    } else {
        if (!spec.short_name.empty()) {
            append_name(result, "-", spec.short_name);
        }
        for (const auto alias : spec.aliases) {
            if (alias.size() == 1) {
                append_name(result, "-", alias);
            }
        }
        append_name(result, "--", spec.long_name);
        for (const auto alias : spec.aliases) {
            if (alias.size() > 1) {
                append_name(result, "--", alias);
            }
        }
    }
    if (!spec.flag && !spec.positional) {
        result.append(" <");
        result.append(spec.value_name.empty() ? "value" : spec.value_name);
        result.push_back('>');
    }
    return result;
}

inline void append_relation_details(std::string& result, const ArgSchema& schema,
                                    const std::vector<std::size_t>& targets, std::string_view label) {
    if (targets.empty()) {
        return;
    }

    result.append(" (");
    result.append(label);
    result.append(": ");
    for (std::size_t idx = 0; idx < targets.size(); ++idx) {
        if (idx != 0) {
            result.append(", ");
        }
        const auto target = targets[idx];
        if (target < schema.specs.size()) {
            result.append(format_error_option_label(schema.specs[target]));
        } else {
            result.append("<invalid>");
        }
    }
    result.push_back(')');
}

inline void append_option_details(std::string& result, const ArgSchema& schema, const ArgSpec& spec) {
    if (spec.required) {
        result.append(" (required)");
    }
    if (spec.has_range) {
        result.append(" (range: [");
        result.append(format_number(spec.range_min));
        result.append(", ");
        result.append(format_number(spec.range_max));
        result.append("))");
    }
    if (spec.has_default) {
        result.append(" (default: ");
        result.append(spec.default_value);
        result.push_back(')');
    }
    if (spec.has_implicit) {
        result.append(" (implicit: ");
        result.append(spec.implicit_value);
        result.push_back(')');
    }
    if (spec.repeatable) {
        result.append(" (repeatable)");
    }
    if (spec.separator != '\0') {
        result.append(" (separator: '");
        result.push_back(spec.separator);
        result.append("')");
    }
    if (!spec.env_name.empty()) {
        result.append(" (env: ");
        result.append(spec.env_name);
        result.push_back(')');
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
    if (spec.case_insensitive_choices && !spec.choices.empty()) {
        result.append(" (case-insensitive)");
    }
    if (spec.deprecated) {
        result.append(" (deprecated");
        if (!spec.deprecated_message.empty()) {
            result.append(": ");
            result.append(spec.deprecated_message);
        }
        result.push_back(')');
    }
    append_relation_details(result, schema, spec.require_indices, "requires");
    append_relation_details(result, schema, spec.conflict_indices, "conflicts");
}

inline void append_option_entry(std::string& result, const ArgSchema& schema, const ArgSpec& spec) {
    result.append("  ");
    result.append(format_option_label(spec));
    append_option_details(result, schema, spec);
    if (!spec.help.empty()) {
        result.append("\n      ");
        result.append(spec.help);
    }
    result.push_back('\n');
}

inline void append_builtin_option_entry(std::string& result, const ArgSchema& schema, bool enabled,
                                        std::string_view short_name, std::string_view long_name) {
    if (!enabled) {
        return;
    }
    const bool has_short_override = schema.find_short_index(short_name).has_value();
    const bool has_long_override  = schema.find_long_index(long_name).has_value();
    if (has_short_override && has_long_override) {
        return;
    }

    result.append("  ");
    if (!has_short_override) {
        result.push_back('-');
        result.append(short_name);
    }
    if (!has_short_override && !has_long_override) {
        result.append(", ");
    }
    if (!has_long_override) {
        result.append("--");
        result.append(long_name);
    }
    result.push_back('\n');
}

inline void append_ungrouped_options(std::string& result, const ArgSchema& schema) {
    for (const auto& spec : schema.specs) {
        if (spec.hidden || spec.positional || !spec.group.empty()) {
            continue;
        }
        append_option_entry(result, schema, spec);
    }
}

inline void append_grouped_options(std::string& result, const ArgSchema& schema) {
    std::vector<std::string_view> groups;
    for (const auto& spec : schema.specs) {
        if (spec.hidden || spec.positional || spec.group.empty()) {
            continue;
        }
        if (std::find(groups.begin(), groups.end(), spec.group) == groups.end()) {
            groups.push_back(spec.group);
        }
    }

    for (const auto group : groups) {
        result.push_back('\n');
        result.append(group);
        result.append(":\n");
        for (const auto& spec : schema.specs) {
            if (!spec.hidden && !spec.positional && spec.group == group) {
                append_option_entry(result, schema, spec);
            }
        }
    }
}

inline void append_positional_arguments(std::string& result, const ArgSchema& schema) {
    bool has_arguments = false;
    for (const auto index : schema.positional_specs) {
        if (!schema.specs[index].hidden) {
            has_arguments = true;
            break;
        }
    }
    if (!has_arguments) {
        return;
    }

    result.append("\nArguments:\n");
    for (const auto index : schema.positional_specs) {
        const auto& spec = schema.specs[index];
        if (!spec.hidden) {
            append_option_entry(result, schema, spec);
        }
    }
}

inline std::string format_help_from_schema(ArgSchema schema, const ArgParserConfig& config) {
    static_cast<void>(validate_schema_definition(schema, config));
    std::string result;
    if (!config.usage.empty()) {
        result.append(config.usage);
    } else {
        result.append(default_usage(config.programName, schema));
    }
    result.push_back('\n');
    if (!config.description.empty()) {
        result.push_back('\n');
        result.append(config.description);
        result.push_back('\n');
    }

    result.append("\nOptions:\n");
    append_builtin_option_entry(result, schema, config.addHelp, "h", "help");
    append_builtin_option_entry(result, schema, config.addVersion && !config.version.empty(), "V", "version");

    const bool hasGroups = std::any_of(schema.specs.begin(), schema.specs.end(), [](const auto& spec) {
        return !spec.hidden && !spec.positional && !spec.group.empty();
    });
    append_ungrouped_options(result, schema);
    if (hasGroups) {
        append_grouped_options(result, schema);
    }
    append_positional_arguments(result, schema);
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

} // namespace argparser::detail
NEKO_END_NAMESPACE
