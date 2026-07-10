#pragma once

#include "nekoproto/argparser/detail/schema.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/global/expected.hpp"
#include "nekoproto/global/global.hpp"

#include <cctype>
#include <string_view>
#include <system_error>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

struct RawOptionValues {
    std::vector<std::string_view> values;

    [[nodiscard]] bool seen() const { return !values.empty(); }
};

struct RawParseResult {
    std::vector<RawOptionValues> options;
};

inline auto unexpected_error(std::error_code error) -> expected::unexpected<std::error_code> {
    return expected::unexpected<std::error_code>(error);
}

inline bool is_option_token(std::string_view arg) { return arg.size() > 1 && arg[0] == '-'; }

inline bool is_help_token(std::string_view arg) { return arg == "--help" || arg == "-h"; }

inline bool is_version_token(std::string_view arg) { return arg == "--version" || arg == "-V"; }

inline bool is_declared_option_token(const ArgSchema& schema, std::string_view arg) {
    if (arg.starts_with("--")) {
        auto name = arg.substr(2);
        if (const auto equal = name.find('='); equal != std::string_view::npos) {
            name = name.substr(0, equal);
        }
        return schema.find_long_index(name).has_value();
    }
    if (arg.starts_with("-") && arg.size() > 1U) {
        auto name = arg.substr(1);
        if (const auto equal = name.find('='); equal != std::string_view::npos) {
            name = name.substr(0, equal);
        }
        return schema.find_short_index(name).has_value();
    }
    return false;
}

inline bool looks_like_negative_number(std::string_view token) {
    if (token.size() < 2 || token[0] != '-') {
        return false;
    }

    std::size_t index = 1;
    if (index < token.size() && token[index] == '.') {
        ++index;
    }
    if (index >= token.size() || (std::isdigit(static_cast<unsigned char>(token[index])) == 0)) {
        return false;
    }
    while (index < token.size() && (std::isdigit(static_cast<unsigned char>(token[index])) != 0)) {
        ++index;
    }
    if (index < token.size() && token[index] == '.') {
        ++index;
        if (index >= token.size() || (std::isdigit(static_cast<unsigned char>(token[index])) == 0)) {
            return false;
        }
        while (index < token.size() && (std::isdigit(static_cast<unsigned char>(token[index])) != 0)) {
            ++index;
        }
    }
    return index == token.size();
}

inline bool looks_like_option_boundary_for_implicit(std::string_view token, const ArgParserConfig& config) {
    if (token == "--") {
        return true;
    }
    if (config.addHelp && is_help_token(token)) {
        return true;
    }
    if (config.addVersion && !config.version.empty() && is_version_token(token)) {
        return true;
    }
    if (token.starts_with("--")) {
        return true;
    }
    return is_option_token(token) && !looks_like_negative_number(token);
}

inline std::error_code record_raw_value(const ArgSchema& schema, RawParseResult& result, std::size_t spec_index,
                                        std::string_view value) {
    if (spec_index >= schema.specs.size() || spec_index >= result.options.size()) {
        return make_argparser_error(ArgParserError::InvalidDefinition,
                                    "schema index is out of range while reading " + quote_arg_value(value));
    }
    const auto& spec = schema.specs[spec_index];
    if (!spec.repeatable && result.options[spec_index].seen()) {
        return make_argparser_error(ArgParserError::InvalidValue, format_error_option_label(spec) +
                                                                      " was provided more than once; duplicate value " +
                                                                      quote_arg_value(value));
    }
    result.options[spec_index].values.emplace_back(value);
    return {};
}

inline std::error_code record_raw_positional(const ArgSchema& schema, RawParseResult& result,
                                             std::size_t& positional_index, std::string_view value) {
    if (positional_index >= schema.positional_specs.size()) {
        return make_argparser_error(ArgParserError::UnexpectedPositional,
                                    "no positional field accepts " + quote_arg_value(value));
    }
    const auto spec_index = schema.positional_specs[positional_index];
    if (auto error = record_raw_value(schema, result, spec_index, value)) {
        return error;
    }
    if (!schema.specs[spec_index].repeatable) {
        ++positional_index;
    }
    return {};
}

inline std::error_code record_raw_option(const ArgSchema& schema, RawParseResult& result, std::size_t spec_index,
                                         int argc, const char* const* argv, int& idx, std::string_view value,
                                         bool value_inline, const ArgParserConfig& config) {
    const auto& spec = schema.specs[spec_index];
    if (spec.flag || value_inline) {
        return record_raw_value(schema, result, spec_index, value);
    }
    if (idx + 1 < argc && argv[idx + 1] != nullptr) {
        const std::string_view nextValue = argv[idx + 1];
        if (!spec.has_implicit || !looks_like_option_boundary_for_implicit(nextValue, config)) {
            return record_raw_value(schema, result, spec_index, argv[++idx]);
        }
    }
    if (spec.has_implicit) {
        return record_raw_value(schema, result, spec_index, spec.implicit_value);
    }
    return make_argparser_error(ArgParserError::MissingValue, format_error_option_label(spec) + " expects a value");
}

inline expected::expected<RawParseResult, std::error_code> parse_raw_arguments(const ArgSchema& schema, int argc,
                                                                               const char* const* argv, int start_index,
                                                                               const ArgParserConfig& config) {
    RawParseResult result;
    result.options.resize(schema.specs.size());
    std::size_t positional_index = 0;
    bool forcePositional         = false;

    for (int idx = start_index; idx < argc; ++idx) {
        std::string_view arg = argv[idx] == nullptr ? std::string_view{} : std::string_view(argv[idx]);
        if (forcePositional) {
            if (auto error = record_raw_positional(schema, result, positional_index, arg)) {
                return unexpected_error(error);
            }
            continue;
        }

        if (arg == "--") {
            forcePositional = true;
            continue;
        }
        if (config.addHelp && is_help_token(arg) && !is_declared_option_token(schema, arg)) {
            return unexpected_error(make_error_code(ArgParserError::HelpRequested));
        }
        if (config.addVersion && !config.version.empty() && is_version_token(arg) &&
            !is_declared_option_token(schema, arg)) {
            return unexpected_error(make_error_code(ArgParserError::VersionRequested));
        }

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
                    continue;
                }
                return unexpected_error(make_argparser_error(ArgParserError::UnknownOption, "--" + std::string(body)));
            }
            if (auto error =
                    record_raw_option(schema, result, *spec_index, argc, argv, idx, value, value_inline, config)) {
                return unexpected_error(error);
            }
            continue;
        }

        if (arg.starts_with("-") && arg.size() > 1) {
            auto body         = arg.substr(1);
            auto value        = std::string_view{};
            bool value_inline = false;
            if (const auto equal = body.find('='); equal != std::string_view::npos) {
                value        = body.substr(equal + 1);
                body         = body.substr(0, equal);
                value_inline = true;
            }

            if (config.allowShortCluster && body.size() > 1 && !value_inline) {
                for (std::size_t cluster_index = 0; cluster_index < body.size(); ++cluster_index) {
                    const char short_name[] = {body[cluster_index], '\0'};
                    const auto spec_index   = schema.find_short_index(short_name);
                    if (!spec_index.has_value()) {
                        if (config.allowUnknown) {
                            continue;
                        }
                        return unexpected_error(
                            make_argparser_error(ArgParserError::UnknownOption, "-" + std::string(short_name)));
                    }
                    if (!schema.specs[*spec_index].flag) {
                        const auto remainder = body.substr(cluster_index + 1);
                        if (!remainder.empty()) {
                            if (auto error = record_raw_value(schema, result, *spec_index, remainder)) {
                                return unexpected_error(error);
                            }
                        } else if (auto error = record_raw_option(schema, result, *spec_index, argc, argv, idx, {},
                                                                  false, config)) {
                            return unexpected_error(error);
                        }
                        break;
                    }
                    if (auto error = record_raw_value(schema, result, *spec_index, {})) {
                        return unexpected_error(error);
                    }
                }
                continue;
            }

            auto spec_index = schema.find_short_index(body);
            if (!spec_index.has_value() && body.size() > 1 && !value_inline) {
                const char short_name[] = {body[0], '\0'};
                spec_index              = schema.find_short_index(short_name);
                if (spec_index.has_value()) {
                    value        = body.substr(1);
                    value_inline = true;
                }
            }
            if (!spec_index.has_value()) {
                if (config.allowUnknown) {
                    continue;
                }
                return unexpected_error(make_argparser_error(ArgParserError::UnknownOption, "-" + std::string(body)));
            }
            if (auto error =
                    record_raw_option(schema, result, *spec_index, argc, argv, idx, value, value_inline, config)) {
                return unexpected_error(error);
            }
            continue;
        }

        if (auto error = record_raw_positional(schema, result, positional_index, arg)) {
            return unexpected_error(error);
        }
    }

    return result;
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
