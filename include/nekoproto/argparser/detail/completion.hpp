#pragma once

#include "nekoproto/argparser/config.hpp"
#include "nekoproto/argparser/detail/schema.hpp"
#include "nekoproto/global/global.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser {

enum class CompletionShell {
    Bash,
    Zsh,
};

namespace detail {

struct CompletionOption {
    std::vector<std::string> names;
    std::vector<std::string> choices;
    std::string help;
    std::string value_name;
    ArgValueCompletion completion = ArgValueCompletion::None;
    bool flag                     = false;
    bool repeatable               = false;
    bool has_implicit             = false;
};

struct CompletionPositional {
    std::vector<std::string> choices;
    std::string value_name;
    ArgValueCompletion completion = ArgValueCompletion::None;
    bool repeatable               = false;
};

struct CompletionNode {
    std::vector<CompletionOption> options;
    std::vector<CompletionPositional> positionals;
};

struct CompletionCommand {
    std::string name;
    std::string help;
    CompletionNode node;
};

struct CompletionModel {
    std::string command_name;
    CompletionNode root;
    std::vector<CompletionCommand> commands;
    bool valid = true;
};

inline CompletionOption completion_option_from_spec(const ArgSpec& spec) {
    CompletionOption option;
    if (!spec.short_name.empty()) {
        option.names.push_back("-" + spec.short_name);
    }
    for (const auto alias : spec.aliases) {
        if (alias.size() == 1U) {
            option.names.push_back("-" + std::string(alias));
        }
    }
    option.names.push_back("--" + spec.long_name);
    for (const auto alias : spec.aliases) {
        if (alias.size() > 1U) {
            option.names.push_back("--" + std::string(alias));
        }
    }
    const auto& choices = spec.choices.empty() ? spec.inferred_completion_choices : spec.choices;
    option.choices.assign(choices.begin(), choices.end());
    option.help         = spec.help;
    option.value_name   = spec.value_name.empty() ? "VALUE" : spec.value_name;
    option.completion   = spec.completion;
    option.flag         = spec.flag;
    option.repeatable   = spec.repeatable;
    option.has_implicit = spec.has_implicit;
    return option;
}

inline void append_builtin_completion_option(CompletionNode& node, const ArgSchema& schema, bool enabled,
                                             std::string_view short_name, std::string_view long_name,
                                             std::string_view help) {
    if (!enabled) {
        return;
    }
    CompletionOption option;
    if (!schema.find_short_index(short_name).has_value()) {
        option.names.push_back("-" + std::string(short_name));
    }
    if (!schema.find_long_index(long_name).has_value()) {
        option.names.push_back("--" + std::string(long_name));
    }
    if (!option.names.empty()) {
        option.help = std::string(help);
        option.flag = true;
        node.options.push_back(std::move(option));
    }
}

inline CompletionNode make_completion_node(ArgSchema schema, const ArgParserConfig& config, bool& valid) {
    CompletionNode node;
    if (auto error = validate_schema_definition(schema, config)) {
        valid = false;
        return node;
    }

    append_builtin_completion_option(node, schema, config.addHelp, "h", "help", "show help");
    append_builtin_completion_option(node, schema, config.addVersion && !config.version.empty(), "V", "version",
                                     "show version");
    for (const auto& spec : schema.specs) {
        if (spec.hidden) {
            continue;
        }
        if (spec.positional) {
            CompletionPositional positional;
            const auto& choices = spec.choices.empty() ? spec.inferred_completion_choices : spec.choices;
            positional.choices.assign(choices.begin(), choices.end());
            positional.value_name = spec.value_name.empty() ? spec.long_name : spec.value_name;
            positional.completion = spec.completion;
            positional.repeatable = spec.repeatable;
            node.positionals.push_back(std::move(positional));
        } else {
            node.options.push_back(completion_option_from_spec(spec));
        }
    }
    return node;
}

inline std::string shell_single_quote(std::string_view value) {
    std::string result{"'"};
    for (const char ch : value) {
        if (ch == '\'') {
            result.append("'\\''");
        } else if (ch == '\n' || ch == '\r') {
            result.push_back(' ');
        } else {
            result.push_back(ch);
        }
    }
    result.push_back('\'');
    return result;
}

inline std::string completion_function_id(std::string_view command_name) {
    std::string result{"_neko_"};
    std::uint32_t hash = 2166136261U;
    for (const unsigned char ch : command_name) {
        hash ^= ch;
        hash *= 16777619U;
        result.push_back(std::isalnum(ch) != 0 ? static_cast<char>(ch) : '_');
    }
    constexpr char hex[] = "0123456789abcdef";
    result.push_back('_');
    for (int shift = 28; shift >= 0; shift -= 4) {
        result.push_back(hex[(hash >> shift) & 0x0FU]);
    }
    return result;
}

inline bool valid_completion_command_name(std::string_view command_name) {
    const auto is_alnum = [](const unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
    };
    if (command_name.empty() || (!is_alnum(command_name.front()) && command_name.front() != '_')) {
        return false;
    }
    return std::all_of(command_name.begin(), command_name.end(), [&](const unsigned char ch) {
        return is_alnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '+';
    });
}

inline void append_bash_name_test(std::string& out, std::string_view variable, const std::vector<std::string>& names) {
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index != 0U) {
            out.append(" || ");
        }
        out.append("[[ \"");
        out.append(variable);
        out.append("\" == ");
        out.append(shell_single_quote(names[index]));
        out.append(" ]]");
    }
}

inline void append_bash_candidates(std::string& out, std::string_view function_id, std::string_view value_prefix,
                                   std::string_view display_prefix, const std::vector<std::string>& choices,
                                   ArgValueCompletion completion, std::string_view indent) {
    if (!choices.empty()) {
        out.append(indent);
        out.append(function_id);
        out.append("_words \"");
        out.append(value_prefix);
        out.append("\" ");
        out.append(shell_single_quote(display_prefix));
        for (const auto& choice : choices) {
            out.push_back(' ');
            out.append(shell_single_quote(choice));
        }
        out.push_back('\n');
    } else if (completion != ArgValueCompletion::None) {
        out.append(indent);
        out.append(function_id);
        out.append("_paths ");
        out.append(completion == ArgValueCompletion::Directory ? "d" : "f");
        out.append(" \"");
        out.append(value_prefix);
        out.append("\" ");
        out.append(shell_single_quote(display_prefix));
        out.push_back('\n');
    }
}

inline void append_bash_value_completion(std::string& out, const CompletionNode& node, std::string_view function_id) {
    for (const auto& option : node.options) {
        if (option.flag || option.names.empty()) {
            continue;
        }
        out.append("    if ");
        append_bash_name_test(out, "$prev", option.names);
        out.append("; then\n");
        append_bash_candidates(out, function_id, "$cur", {}, option.choices, option.completion, "      ");
        out.append("      return\n    fi\n");

        for (const auto& name : option.names) {
            const bool long_name  = name.starts_with("--");
            const bool short_name = name.size() == 2U && name.front() == '-';
            if (!long_name && !short_name) {
                continue;
            }
            out.append("    if [[ \"$cur\" == ");
            out.append(shell_single_quote(name + (long_name ? "=" : "")));
            out.append("*");
            if (short_name) {
                out.append(" && \"$cur\" != ");
                out.append(shell_single_quote(name));
            }
            out.append(" ]]; then\n      local value_prefix=\"${cur#");
            out.append(name);
            if (long_name) {
                out.push_back('=');
            }
            out.append("}\"\n");
            append_bash_candidates(out, function_id, "$value_prefix", name + (long_name ? "=" : ""), option.choices,
                                   option.completion, "      ");
            out.append("      return\n    fi\n");
        }
    }
}

inline void append_bash_value_option_test(std::string& out, const CompletionNode& node, bool implicit) {
    bool first = true;
    for (const auto& option : node.options) {
        if (option.flag || option.has_implicit != implicit) {
            continue;
        }
        for (const auto& name : option.names) {
            if (!first) {
                out.append(" || ");
            }
            first = false;
            out.append("[[ \"$token\" == ");
            out.append(shell_single_quote(name));
            out.append(" ]]");
        }
    }
    if (first) {
        out.append("false");
    }
}

inline void append_bash_option_candidates(std::string& out, const CompletionNode& node, std::string_view function_id) {
    std::vector<std::string> names;
    for (const auto& option : node.options) {
        names.insert(names.end(), option.names.begin(), option.names.end());
    }
    if (!names.empty()) {
        out.append("      ");
        out.append(function_id);
        out.append("_words \"$cur\" ''");
        for (const auto& name : names) {
            out.push_back(' ');
            out.append(shell_single_quote(name));
        }
        out.push_back('\n');
    }
}

inline void append_bash_node(std::string& out, const CompletionNode& node, std::string_view function_id) {
    append_bash_value_completion(out, node, function_id);
    out.append("    local positional_index=0 expect_value=0 force_positional=0\n");
    out.append("    for ((i = command_index ? command_index + 1 : 1; i<COMP_CWORD; ++i)); do\n");
    out.append("      token=\"${COMP_WORDS[i]}\"\n");
    out.append("      if (( expect_value )); then\n");
    out.append("        if (( expect_value == 2 )) && [[ \"$token\" == -* && \"$token\" != -[0-9]* && \"$token\" != "
               "-.[0-9]* ]]; then\n");
    out.append("          expect_value=0\n");
    out.append("        else\n          expect_value=0\n          continue\n        fi\n      fi\n");
    out.append("      if (( !force_positional )) && [[ \"$token\" == -- ]]; then\n");
    out.append("        force_positional=1\n        continue\n      fi\n");
    out.append("      if (( !force_positional )); then\n");
    out.append("        if [[ \"$token\" == --*=* ]]; then\n          continue\n        fi\n");
    out.append("        if ");
    append_bash_value_option_test(out, node, false);
    out.append("; then\n          expect_value=1\n          continue\n        fi\n");
    out.append("        if ");
    append_bash_value_option_test(out, node, true);
    out.append("; then\n          expect_value=2\n          continue\n        fi\n");
    out.append("        [[ \"$token\" == -* ]] && continue\n");
    out.append("      fi\n      ((++positional_index))\n    done\n");
    out.append("    if (( !force_positional )) && [[ \"$cur\" == -* ]]; then\n");
    append_bash_option_candidates(out, node, function_id);
    out.append("      return\n    fi\n");

    bool has_positional_completion = false;
    for (std::size_t index = 0; index < node.positionals.size(); ++index) {
        const auto& positional = node.positionals[index];
        if (positional.choices.empty() && positional.completion == ArgValueCompletion::None) {
            continue;
        }
        out.append(!has_positional_completion ? "    if " : "    elif ");
        has_positional_completion = true;
        if (positional.repeatable) {
            out.append("(( positional_index >= ");
        } else {
            out.append("(( positional_index == ");
        }
        out.append(std::to_string(index));
        out.append(" )); then\n");
        append_bash_candidates(out, function_id, "$cur", {}, positional.choices, positional.completion, "      ");
    }
    if (has_positional_completion) {
        out.append("    fi\n");
    }
}

inline std::string format_bash_completion(const CompletionModel& model) {
    const auto function_id = completion_function_id(model.command_name);
    std::string out;
    out.append(function_id);
    out.append("_words() {\n  local value_prefix=\"$1\" display_prefix=\"$2\" candidate\n  shift 2\n");
    out.append("  for candidate in \"$@\"; do\n    [[ \"$candidate\" == \"$value_prefix\"* ]] && "
               "COMPREPLY+=(\"${display_prefix}${candidate}\")\n  done\n}\n\n");
    out.append(function_id);
    out.append("_paths() {\n  local kind=\"$1\" value_prefix=\"$2\" display_prefix=\"$3\" candidate\n  local flag=-f\n "
               " [[ \"$kind\" == d ]] && flag=-d\n");
    out.append("  while IFS= read -r candidate; do\n    COMPREPLY+=(\"${display_prefix}${candidate}\")\n  done < "
               "<(compgen \"$flag\" -- \"$value_prefix\")\n  compopt -o filenames 2>/dev/null || true\n}\n\n");
    out.append(function_id);
    out.append("() {\n  local cur=\"${COMP_WORDS[COMP_CWORD]}\" prev='' command='' command_index=0 i token\n  "
               "COMPREPLY=()\n  (( COMP_CWORD > 0 )) && prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n");

    if (!model.commands.empty()) {
        out.append("  local root_expect_value=0\n  for ((i=1; i<COMP_CWORD; ++i)); do\n");
        out.append(
            "    token=\"${COMP_WORDS[i]}\"\n    if (( root_expect_value )); then root_expect_value=0; continue; fi\n");
        out.append("    if [[ \"$token\" == --*=* ]]; then continue; fi\n    if ");
        append_bash_value_option_test(out, model.root, false);
        out.append("; then root_expect_value=1; continue; fi\n");
        for (const auto& command : model.commands) {
            out.append("    if [[ \"$token\" == ");
            out.append(shell_single_quote(command.name));
            out.append(" ]]; then command=");
            out.append(shell_single_quote(command.name));
            out.append("; command_index=$i; break; fi\n");
        }
        out.append("  done\n");
    }

    out.append("  case \"$command\" in\n");
    for (const auto& command : model.commands) {
        out.append("  ");
        out.append(shell_single_quote(command.name));
        out.append(")\n");
        append_bash_node(out, command.node, function_id);
        out.append("    ;;\n");
    }
    out.append("  '')\n");
    if (!model.commands.empty()) {
        append_bash_value_completion(out, model.root, function_id);
        out.append("    if [[ \"$cur\" == -* ]]; then\n");
        append_bash_option_candidates(out, model.root, function_id);
        out.append("    else\n      ");
        out.append(function_id);
        out.append("_words \"$cur\" ''");
        for (const auto& command : model.commands) {
            out.push_back(' ');
            out.append(shell_single_quote(command.name));
        }
        out.append("\n    fi\n");
    } else {
        append_bash_node(out, model.root, function_id);
    }
    out.append("    ;;\n  esac\n}\n\ncomplete -F ");
    out.append(function_id);
    out.append(" -- ");
    out.append(shell_single_quote(model.command_name));
    out.push_back('\n');
    return out;
}

inline std::string zsh_description_escape(std::string_view value) {
    std::string result;
    for (const char ch : value) {
        if (ch == '\n' || ch == '\r') {
            result.push_back(' ');
        } else {
            if (ch == '\\' || ch == '[' || ch == ']' || ch == ':') {
                result.push_back('\\');
            }
            result.push_back(ch);
        }
    }
    return result;
}

inline std::string zsh_action_word_escape(std::string_view value) {
    std::string result;
    for (const char ch : value) {
        if (ch == '\\' || ch == ' ' || ch == '\t' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' ||
            ch == '}' || ch == ':' || ch == '\'' || ch == '"' || ch == '$' || ch == '`') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    return result;
}

inline std::string zsh_value_action(const std::vector<std::string>& choices, ArgValueCompletion completion) {
    if (!choices.empty()) {
        std::string result{"("};
        for (std::size_t index = 0; index < choices.size(); ++index) {
            if (index != 0U) {
                result.push_back(' ');
            }
            result.append(zsh_action_word_escape(choices[index]));
        }
        result.push_back(')');
        return result;
    }
    if (completion == ArgValueCompletion::File) {
        return "_files";
    }
    if (completion == ArgValueCompletion::Directory) {
        return "_directories";
    }
    return {};
}

inline std::vector<std::string> zsh_argument_specs(const CompletionNode& node) {
    std::vector<std::string> specs;
    for (const auto& option : node.options) {
        for (const auto& name : option.names) {
            std::string spec;
            if (option.repeatable) {
                spec.push_back('*');
            }
            spec.append(name);
            if (!option.help.empty()) {
                spec.push_back('[');
                spec.append(zsh_description_escape(option.help));
                spec.push_back(']');
            }
            if (!option.flag) {
                spec.push_back(':');
                spec.append(zsh_description_escape(option.value_name));
                spec.push_back(':');
                spec.append(zsh_value_action(option.choices, option.completion));
            }
            specs.push_back(std::move(spec));
        }
    }
    for (std::size_t index = 0; index < node.positionals.size(); ++index) {
        const auto& positional = node.positionals[index];
        std::string spec;
        if (positional.repeatable) {
            spec.push_back('*');
        } else {
            spec.append(std::to_string(index + 1U));
        }
        spec.push_back(':');
        spec.append(zsh_description_escape(positional.value_name));
        spec.push_back(':');
        spec.append(zsh_value_action(positional.choices, positional.completion));
        specs.push_back(std::move(spec));
    }
    return specs;
}

inline void append_zsh_arguments(std::string& out, const CompletionNode& node,
                                 const std::vector<std::string>& extra_specs = {}) {
    const auto specs = zsh_argument_specs(node);
    out.append("    _arguments -s");
    for (const auto& spec : specs) {
        out.append(" \\\n      ");
        out.append(shell_single_quote(spec));
    }
    for (const auto& spec : extra_specs) {
        out.append(" \\\n      ");
        out.append(shell_single_quote(spec));
    }
    out.push_back('\n');
}

inline std::string format_zsh_completion(const CompletionModel& model) {
    const auto function_id = completion_function_id(model.command_name);
    std::string out{"#compdef "};
    out.append(model.command_name);
    out.append("\n\n");
    out.append(function_id);
    out.append("() {\n  local context state state_descr line command='' token\n  local -i command_index=0 i "
               "root_expect_value=0\n  typeset -A opt_args\n");

    if (!model.commands.empty()) {
        out.append("  for ((i=2; i<CURRENT; ++i)); do\n    token=\"${words[i]}\"\n    if (( root_expect_value )); then "
                   "root_expect_value=0; continue; fi\n    [[ \"$token\" == --*=* ]] && continue\n    if ");
        append_bash_value_option_test(out, model.root, false);
        out.append("; then root_expect_value=1; continue; fi\n");
        for (const auto& command : model.commands) {
            out.append("    if [[ \"$token\" == ");
            out.append(shell_single_quote(command.name));
            out.append(" ]]; then command=");
            out.append(shell_single_quote(command.name));
            out.append("; command_index=$i; break; fi\n");
        }
        out.append("  done\n  if (( command_index )); then\n    local -i remove_count=$((command_index - 1))\n    "
                   "while (( remove_count-- )); do\n      words[2]=()\n      (( CURRENT-- ))\n    done\n    case "
                   "\"$command\" in\n");
        for (const auto& command : model.commands) {
            out.append("    ");
            out.append(shell_single_quote(command.name));
            out.append(")\n");
            append_zsh_arguments(out, command.node);
            out.append("      return\n      ;;\n");
        }
        out.append("    esac\n  fi\n\n");

        std::vector<std::string> root_extra{"1:command:->command", "*::arg:->command"};
        append_zsh_arguments(out, model.root, root_extra);
        out.append("    case \"$state\" in\n    command)\n      local -a commands\n      commands=(\n");
        for (const auto& command : model.commands) {
            out.append("        ");
            out.append(shell_single_quote(command.name + ":" + zsh_description_escape(command.help)));
            out.push_back('\n');
        }
        out.append("      )\n      _describe 'command' commands\n      ;;\n    esac\n");
    } else {
        append_zsh_arguments(out, model.root);
    }
    out.append("}\n\ncompdef ");
    out.append(function_id);
    out.push_back(' ');
    out.append(shell_single_quote(model.command_name));
    out.push_back('\n');
    return out;
}

inline std::string format_completion_model(const CompletionModel& model, CompletionShell shell) {
    if (!model.valid || !valid_completion_command_name(model.command_name)) {
        return {};
    }
    switch (shell) {
    case CompletionShell::Bash:
        return format_bash_completion(model);
    case CompletionShell::Zsh:
        return format_zsh_completion(model);
    }
    return {};
}

} // namespace detail
} // namespace argparser
NEKO_END_NAMESPACE
