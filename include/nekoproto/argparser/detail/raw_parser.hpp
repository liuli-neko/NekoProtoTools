#pragma once

#include "nekoproto/argparser/detail/schema.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/global/global.hpp"

#include <cctype>
#include <expected>
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

inline bool is_option_token(std::string_view arg) { return arg.size() > 1 && arg[0] == '-'; }

inline bool is_help_token(std::string_view arg) { return arg == "--help" || arg == "-h"; }

inline bool is_version_token(std::string_view arg) { return arg == "--version" || arg == "-V"; }

inline bool looks_like_negative_number(std::string_view token) {
    if (token.size() < 2 || token[0] != '-') {
        return false;
    }

    std::size_t index = 1;
    if (index < token.size() && token[index] == '.') {
        ++index;
    }
    if (index >= token.size() || !std::isdigit(static_cast<unsigned char>(token[index]))) {
        return false;
    }
    while (index < token.size() && std::isdigit(static_cast<unsigned char>(token[index]))) {
        ++index;
    }
    if (index < token.size() && token[index] == '.') {
        ++index;
        if (index >= token.size() || !std::isdigit(static_cast<unsigned char>(token[index]))) {
            return false;
        }
        while (index < token.size() && std::isdigit(static_cast<unsigned char>(token[index]))) {
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

inline std::error_code record_raw_value(const ArgSchema& schema, RawParseResult& result, std::size_t specIndex,
                                        std::string_view value) {
    if (specIndex >= schema.specs.size() || specIndex >= result.options.size()) {
        return make_error_code(ArgParserError::InvalidDefinition);
    }
    const auto& spec = schema.specs[specIndex];
    if (!spec.repeatable && spec.positional && result.options[specIndex].seen()) {
        return make_error_code(ArgParserError::UnexpectedPositional);
    }
    result.options[specIndex].values.emplace_back(value);
    return {};
}

inline std::error_code record_raw_positional(const ArgSchema& schema, RawParseResult& result,
                                             std::size_t& positionalIndex, std::string_view value) {
    if (positionalIndex >= schema.positionalSpecs.size()) {
        return make_error_code(ArgParserError::UnexpectedPositional);
    }
    const auto specIndex = schema.positionalSpecs[positionalIndex];
    if (auto error = record_raw_value(schema, result, specIndex, value)) {
        return error;
    }
    if (!schema.specs[specIndex].repeatable) {
        ++positionalIndex;
    }
    return {};
}

inline std::error_code record_raw_option(const ArgSchema& schema, RawParseResult& result, std::size_t specIndex,
                                         int argc, const char* const* argv, int& idx, std::string_view value,
                                         bool valueInline, const ArgParserConfig& config) {
    const auto& spec = schema.specs[specIndex];
    if (spec.flag || valueInline) {
        return record_raw_value(schema, result, specIndex, value);
    }
    if (idx + 1 < argc && argv[idx + 1] != nullptr) {
        const std::string_view nextValue = argv[idx + 1];
        if (!spec.hasImplicit || !looks_like_option_boundary_for_implicit(nextValue, config)) {
            return record_raw_value(schema, result, specIndex, argv[++idx]);
        }
    }
    if (spec.hasImplicit) {
        return record_raw_value(schema, result, specIndex, spec.implicitValue);
    }
    return make_error_code(ArgParserError::MissingValue);
}

inline std::expected<RawParseResult, std::error_code> parse_raw_arguments(const ArgSchema& schema, int argc,
                                                                          const char* const* argv, int startIndex,
                                                                          const ArgParserConfig& config) {
    RawParseResult result;
    result.options.resize(schema.specs.size());
    std::size_t positionalIndex = 0;
    bool forcePositional        = false;

    for (int idx = startIndex; idx < argc; ++idx) {
        std::string_view arg = argv[idx] == nullptr ? std::string_view{} : std::string_view(argv[idx]);
        if (forcePositional) {
            if (auto error = record_raw_positional(schema, result, positionalIndex, arg)) {
                return std::unexpected(error);
            }
            continue;
        }

        if (arg == "--") {
            forcePositional = true;
            continue;
        }
        if (config.addHelp && is_help_token(arg)) {
            return std::unexpected(make_error_code(ArgParserError::HelpRequested));
        }
        if (config.addVersion && !config.version.empty() && is_version_token(arg)) {
            return std::unexpected(make_error_code(ArgParserError::VersionRequested));
        }

        if (arg.starts_with("--")) {
            auto body        = arg.substr(2);
            auto value       = std::string_view{};
            bool valueInline = false;
            if (const auto equal = body.find('='); equal != std::string_view::npos) {
                value       = body.substr(equal + 1);
                body        = body.substr(0, equal);
                valueInline = true;
            }

            const auto specIndex = schema.find_long_index(body);
            if (!specIndex.has_value()) {
                if (config.allowUnknown) {
                    continue;
                }
                return std::unexpected(make_error_code(ArgParserError::UnknownOption));
            }
            if (auto error =
                    record_raw_option(schema, result, *specIndex, argc, argv, idx, value, valueInline, config)) {
                return std::unexpected(error);
            }
            continue;
        }

        if (arg.starts_with("-") && arg.size() > 1) {
            auto body        = arg.substr(1);
            auto value       = std::string_view{};
            bool valueInline = false;
            if (const auto equal = body.find('='); equal != std::string_view::npos) {
                value       = body.substr(equal + 1);
                body        = body.substr(0, equal);
                valueInline = true;
            }

            if (config.allowShortCluster && body.size() > 1 && !valueInline) {
                const char firstShortName[] = {body[0], '\0'};
                const auto firstSpecIndex   = schema.find_short_index(firstShortName);
                if (firstSpecIndex.has_value() && !schema.specs[*firstSpecIndex].flag) {
                    if (auto error = record_raw_value(schema, result, *firstSpecIndex, body.substr(1))) {
                        return std::unexpected(error);
                    }
                    continue;
                }

                for (char ch : body) {
                    const char shortName[] = {ch, '\0'};
                    const auto specIndex   = schema.find_short_index(shortName);
                    if (!specIndex.has_value()) {
                        if (config.allowUnknown) {
                            continue;
                        }
                        return std::unexpected(make_error_code(ArgParserError::UnknownOption));
                    }
                    if (!schema.specs[*specIndex].flag) {
                        return std::unexpected(make_error_code(ArgParserError::MissingValue));
                    }
                    if (auto error = record_raw_value(schema, result, *specIndex, {})) {
                        return std::unexpected(error);
                    }
                }
                continue;
            }

            auto specIndex = schema.find_short_index(body);
            if (!specIndex.has_value() && body.size() > 1 && !valueInline) {
                const char shortName[] = {body[0], '\0'};
                specIndex              = schema.find_short_index(shortName);
                if (specIndex.has_value()) {
                    value       = body.substr(1);
                    valueInline = true;
                }
            }
            if (!specIndex.has_value()) {
                if (config.allowUnknown) {
                    continue;
                }
                return std::unexpected(make_error_code(ArgParserError::UnknownOption));
            }
            if (auto error =
                    record_raw_option(schema, result, *specIndex, argc, argv, idx, value, valueInline, config)) {
                return std::unexpected(error);
            }
            continue;
        }

        if (auto error = record_raw_positional(schema, result, positionalIndex, arg)) {
            return std::unexpected(error);
        }
    }

    return result;
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
