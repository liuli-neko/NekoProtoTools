#pragma once

#include "nekoproto/argparser/config_io.hpp"
#include "nekoproto/argparser/detail/raw_parser.hpp"
#include "nekoproto/argparser/error.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nekoproto {
namespace argparser::detail {

struct ConfigIoFile {
    std::string_view format = JsonConfigIoBackend::format;
    std::string path;
};

struct ConfigIoSelection {
    std::optional<ConfigIoFile> import_file;
    std::vector<ConfigIoFile> export_files;
};

template <typename T>
struct CommandConfig {
    std::string command;
    T params;

    struct Neko {
        static constexpr auto value =
            Object("command", &CommandConfig::command, "params", &CommandConfig::params); // NOLINT
    };
};

template <auto Value>
struct CommandConfig<ArgCommand<Value>> {
    std::string command;

    struct Neko {
        static constexpr auto value = Object("command", &CommandConfig::command); // NOLINT
    };
};

inline auto configIoToArgparserError(const config_io::ConfigIoError& error) -> std::error_code {
    const auto parserError =
        error.code == config_io::makeErrorCode(config_io::ConfigIoErrorCode::UnknownFormat) ||
                error.code == config_io::makeErrorCode(config_io::ConfigIoErrorCode::BackendUnavailable)
            ? ArgParserError::InvalidDefinition
            : ArgParserError::InvalidValue;
    return makeArgparserError(parserError, error.message);
}

template <typename T>
auto importConfigFile(const ConfigIoFile& file, T& value) -> std::error_code {
    auto result = config_io::load<T>(std::filesystem::path(file.path), file.format);
    if (!result) {
        return configIoToArgparserError(result.error());
    }
    value = std::move(*result);
    return {};
}

template <typename T>
auto exportConfigFile(const ConfigIoFile& file, const T& value) -> std::error_code {
    auto result = config_io::save(value, std::filesystem::path(file.path), file.format);
    if (!result) {
        return configIoToArgparserError(result.error());
    }
    return {};
}

inline auto collectConfigIoSelection(const ArgSchema& schema, const RawParseResult& raw,
                                                   ConfigIoSelection& selection) -> std::error_code {
    for (std::size_t index = schema.user_spec_count; index < schema.specs.size(); ++index) {
        if (index >= raw.options.size() || !raw.options[index].seen()) {
            continue;
        }
        const auto& values = raw.options[index].values;
        if (values.size() != 1U) {
            return makeArgparserError(ArgParserError::InvalidValue,
                                        formatErrorOptionLabel(schema.specs[index]) + " expects exactly one path");
        }
        const auto builtin = schema.builtinSpec(index);
        if (!builtin.has_value()) {
            continue;
        }
        if (builtin->direction == ConfigIoDirection::Import) {
            if (selection.import_file.has_value()) {
                return makeArgparserError(ArgParserError::InvalidValue,
                                            "only one config import option can be used at a time");
            }
            selection.import_file = ConfigIoFile{builtin->format, std::string(values.front())};
        } else {
            selection.export_files.push_back(ConfigIoFile{builtin->format, std::string(values.front())});
        }
    }
    return {};
}

template <typename T>
auto exportConfigFiles(const ConfigIoSelection& selection, const T& value) -> std::error_code {
    for (const auto& file : selection.export_files) {
        if (auto error = exportConfigFile(file, value)) {
            return error;
        }
    }
    return {};
}

} // namespace argparser::detail
} // namespace nekoproto
