#pragma once

#include "nekoproto/argparser/detail/schema.hpp"
#include "nekoproto/global/global.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace nekoproto {
namespace argparser::detail {

inline auto positionalIsRequired(const ArgSpec& spec) -> bool { return spec.required && !spec.has_default; }

inline auto formatPositionalUsage(const ArgSpec& spec) -> std::string {
    std::string result;
    result.push_back('<');
    result.append(spec.value_name.empty() ? spec.long_name : spec.value_name);
    result.push_back('>');
    if (spec.repeatable) {
        result.append("...");
    }
    if (!positionalIsRequired(spec)) {
        result.insert(result.begin(), '[');
        result.push_back(']');
    }
    return result;
}

inline auto defaultUsage(std::string_view programName, const ArgSchema& schema) -> std::string {
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
        usage.append(formatPositionalUsage(spec));
    }
    return usage;
}

inline auto defaultCommandUsage(std::string_view programName) -> std::string {
    std::string usage = "Usage:";
    if (!programName.empty()) {
        usage.push_back(' ');
        usage.append(programName);
    }
    usage.append(" <command> [options]");
    return usage;
}

inline auto formatVersionText(const argparser::ArgParserConfig& config) -> std::string {
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

inline auto formatNumber(double value) -> std::string {
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}

inline auto formatOptionLabel(const ArgSpec& spec) -> std::string {
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

inline void appendRelationDetails(std::string& result, const ArgSchema& schema,
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
            result.append(formatErrorOptionLabel(schema.specs[target]));
        } else {
            result.append("<invalid>");
        }
    }
    result.push_back(')');
}

inline void appendOptionDetails(std::string& result, const ArgSchema& schema, const ArgSpec& spec) {
    if (spec.required) {
        result.append(" (required)");
    }
    if (spec.has_range) {
        result.append(" (range: [");
        result.append(formatNumber(spec.range_min));
        result.append(", ");
        result.append(formatNumber(spec.range_max));
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
    appendRelationDetails(result, schema, spec.require_indices, "requires");
    appendRelationDetails(result, schema, spec.conflict_indices, "conflicts");
}

inline void appendOptionEntry(std::string& result, const ArgSchema& schema, const ArgSpec& spec) {
    result.append("  ");
    result.append(formatOptionLabel(spec));
    appendOptionDetails(result, schema, spec);
    if (!spec.help.empty()) {
        result.append("\n      ");
        result.append(spec.help);
    }
    result.push_back('\n');
}

inline void appendBuiltinOptionEntry(std::string& result, const ArgSchema& schema, bool enabled,
                                        std::string_view short_name, std::string_view long_name) {
    if (!enabled) {
        return;
    }
    const bool has_short_override = schema.findShortIndex(short_name).has_value();
    const bool has_long_override  = schema.findLongIndex(long_name).has_value();
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

inline void appendUngroupedOptions(std::string& result, const ArgSchema& schema) {
    for (const auto& spec : schema.specs) {
        if (spec.hidden || spec.positional || !spec.group.empty()) {
            continue;
        }
        appendOptionEntry(result, schema, spec);
    }
}

inline void appendGroupedOptions(std::string& result, const ArgSchema& schema) {
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
                appendOptionEntry(result, schema, spec);
            }
        }
    }
}

inline void appendPositionalArguments(std::string& result, const ArgSchema& schema) {
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
            appendOptionEntry(result, schema, spec);
        }
    }
}

inline auto formatHelpFromSchema(ArgSchema schema, const ArgParserConfig& config) -> std::string {
    static_cast<void>(validateSchemaDefinition(schema, config));
    std::string result;
    if (!config.usage.empty()) {
        result.append(config.usage);
    } else {
        result.append(defaultUsage(config.programName, schema));
    }
    result.push_back('\n');
    if (!config.description.empty()) {
        result.push_back('\n');
        result.append(config.description);
        result.push_back('\n');
    }

    result.append("\nOptions:\n");
    appendBuiltinOptionEntry(result, schema, config.addHelp, "h", "help");
    appendBuiltinOptionEntry(result, schema, config.addVersion && !config.version.empty(), "V", "version");

    const bool hasGroups = std::any_of(schema.specs.begin(), schema.specs.end(), [](const auto& spec) {
        return !spec.hidden && !spec.positional && !spec.group.empty();
    });
    appendUngroupedOptions(result, schema);
    if (hasGroups) {
        appendGroupedOptions(result, schema);
    }
    appendPositionalArguments(result, schema);
    return result;
}

inline auto formatPlaceholderCommandHelp(std::string_view programName, std::string_view description) -> std::string {
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
} // namespace nekoproto
