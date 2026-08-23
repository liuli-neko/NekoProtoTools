#pragma once

#include "nekoproto/argparser/detail/schema.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/global/expected.hpp"
#include "nekoproto/global/global.hpp"

#include <cctype>
#include <string_view>
#include <system_error>
#include <vector>

namespace nekoproto {
namespace argparser::detail {

struct RawOptionValues {
    std::vector<std::string_view> values;

    [[nodiscard]] auto seen() const -> bool { return !values.empty(); }
};

struct RawParseResult {
    std::vector<RawOptionValues> options;
};

inline auto unexpectedError(std::error_code error) -> expected::unexpected<std::error_code> {
    return expected::unexpected<std::error_code>(error);
}

inline auto isOptionToken(std::string_view arg) -> bool { return arg.size() > 1 && arg[0] == '-'; }

inline auto isHelpToken(std::string_view arg) -> bool { return arg == "--help" || arg == "-h"; }

inline auto isVersionToken(std::string_view arg) -> bool { return arg == "--version" || arg == "-V"; }

inline auto isDeclaredOptionToken(const ArgSchema& schema, std::string_view arg) -> bool {
    if (arg.starts_with("--")) {
        auto name = arg.substr(2);
        if (const auto equal = name.find('='); equal != std::string_view::npos) {
            name = name.substr(0, equal);
        }
        return schema.findLongIndex(name).has_value();
    }
    if (arg.starts_with("-") && arg.size() > 1U) {
        auto name = arg.substr(1);
        if (const auto equal = name.find('='); equal != std::string_view::npos) {
            name = name.substr(0, equal);
        }
        return schema.findShortIndex(name).has_value();
    }
    return false;
}

inline auto looksLikeNegativeNumber(std::string_view token) -> bool {
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

inline auto looksLikeOptionBoundaryForImplicit(std::string_view token, const ArgParserConfig& config) -> bool {
    if (token == "--") {
        return true;
    }
    if (config.addHelp && isHelpToken(token)) {
        return true;
    }
    if (config.addVersion && !config.version.empty() && isVersionToken(token)) {
        return true;
    }
    if (token.starts_with("--")) {
        return true;
    }
    return isOptionToken(token) && !looksLikeNegativeNumber(token);
}

inline auto recordRawValue(const ArgSchema& schema, RawParseResult& result, std::size_t spec_index,
                                        std::string_view value) -> std::error_code {
    if (spec_index >= schema.specs.size() || spec_index >= result.options.size()) {
        return makeArgparserError(ArgParserError::InvalidDefinition,
                                    "schema index is out of range while reading " + quoteArgValue(value));
    }
    const auto& spec = schema.specs[spec_index];
    if (!spec.repeatable && result.options[spec_index].seen()) {
        return makeArgparserError(ArgParserError::InvalidValue, formatErrorOptionLabel(spec) +
                                                                      " was provided more than once; duplicate value " +
                                                                      quoteArgValue(value));
    }
    result.options[spec_index].values.emplace_back(value);
    return {};
}

inline auto recordRawPositional(const ArgSchema& schema, RawParseResult& result,
                                             std::size_t& positional_index, std::string_view value) -> std::error_code {
    if (positional_index >= schema.positional_specs.size()) {
        return makeArgparserError(ArgParserError::UnexpectedPositional,
                                    "no positional field accepts " + quoteArgValue(value));
    }
    const auto spec_index = schema.positional_specs[positional_index];
    if (auto error = recordRawValue(schema, result, spec_index, value)) {
        return error;
    }
    if (!schema.specs[spec_index].repeatable) {
        ++positional_index;
    }
    return {};
}

inline auto recordRawOption(const ArgSchema& schema, RawParseResult& result, std::size_t spec_index,
                                         int argc, const char* const* argv, int& idx, std::string_view value,
                                         bool value_inline, const ArgParserConfig& config) -> std::error_code {
    const auto& spec = schema.specs[spec_index];
    if (spec.flag || value_inline) {
        return recordRawValue(schema, result, spec_index, value);
    }
    if (idx + 1 < argc && argv[idx + 1] != nullptr) {
        const std::string_view nextValue = argv[idx + 1];
        if (!spec.has_implicit || !looksLikeOptionBoundaryForImplicit(nextValue, config)) {
            return recordRawValue(schema, result, spec_index, argv[++idx]);
        }
    }
    if (spec.has_implicit) {
        return recordRawValue(schema, result, spec_index, spec.implicit_value);
    }
    return makeArgparserError(ArgParserError::MissingValue, formatErrorOptionLabel(spec) + " expects a value");
}

inline auto parseRawArguments(const ArgSchema& schema, int argc,
                                                                               const char* const* argv, int start_index,
                                                                               const ArgParserConfig& config) -> expected::expected<RawParseResult, std::error_code> {
    RawParseResult result;
    result.options.resize(schema.specs.size());
    std::size_t positional_index = 0;
    bool forcePositional         = false;

    for (int idx = start_index; idx < argc; ++idx) {
        std::string_view arg = argv[idx] == nullptr ? std::string_view{} : std::string_view(argv[idx]);
        if (forcePositional) {
            if (auto error = recordRawPositional(schema, result, positional_index, arg)) {
                return unexpectedError(error);
            }
            continue;
        }

        if (arg == "--") {
            forcePositional = true;
            continue;
        }
        if (config.addHelp && isHelpToken(arg) && !isDeclaredOptionToken(schema, arg)) {
            return unexpectedError(makeErrorCode(ArgParserError::HelpRequested));
        }
        if (config.addVersion && !config.version.empty() && isVersionToken(arg) &&
            !isDeclaredOptionToken(schema, arg)) {
            return unexpectedError(makeErrorCode(ArgParserError::VersionRequested));
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

            const auto spec_index = schema.findLongIndex(body);
            if (!spec_index.has_value()) {
                if (config.allowUnknown) {
                    continue;
                }
                return unexpectedError(makeArgparserError(ArgParserError::UnknownOption, "--" + std::string(body)));
            }
            if (auto error =
                    recordRawOption(schema, result, *spec_index, argc, argv, idx, value, value_inline, config)) {
                return unexpectedError(error);
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
                    const auto spec_index   = schema.findShortIndex(short_name);
                    if (!spec_index.has_value()) {
                        if (config.allowUnknown) {
                            continue;
                        }
                        return unexpectedError(
                            makeArgparserError(ArgParserError::UnknownOption, "-" + std::string(short_name)));
                    }
                    if (!schema.specs[*spec_index].flag) {
                        const auto remainder = body.substr(cluster_index + 1);
                        if (!remainder.empty()) {
                            if (auto error = recordRawValue(schema, result, *spec_index, remainder)) {
                                return unexpectedError(error);
                            }
                        } else if (auto error = recordRawOption(schema, result, *spec_index, argc, argv, idx, {},
                                                                  false, config)) {
                            return unexpectedError(error);
                        }
                        break;
                    }
                    if (auto error = recordRawValue(schema, result, *spec_index, {})) {
                        return unexpectedError(error);
                    }
                }
                continue;
            }

            auto spec_index = schema.findShortIndex(body);
            if (!spec_index.has_value() && body.size() > 1 && !value_inline) {
                const char short_name[] = {body[0], '\0'};
                spec_index              = schema.findShortIndex(short_name);
                if (spec_index.has_value()) {
                    value        = body.substr(1);
                    value_inline = true;
                }
            }
            if (!spec_index.has_value()) {
                if (config.allowUnknown) {
                    continue;
                }
                return unexpectedError(makeArgparserError(ArgParserError::UnknownOption, "-" + std::string(body)));
            }
            if (auto error =
                    recordRawOption(schema, result, *spec_index, argc, argv, idx, value, value_inline, config)) {
                return unexpectedError(error);
            }
            continue;
        }

        if (auto error = recordRawPositional(schema, result, positional_index, arg)) {
            return unexpectedError(error);
        }
    }

    return result;
}

} // namespace argparser::detail
} // namespace nekoproto
