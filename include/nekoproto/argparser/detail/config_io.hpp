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

NEKO_BEGIN_NAMESPACE
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

inline std::error_code config_io_to_argparser_error(const config_io::ConfigIoError& error) {
    const auto parser_error =
        error.code == config_io::make_error_code(config_io::ConfigIoErrorCode::UnknownFormat) ||
                error.code == config_io::make_error_code(config_io::ConfigIoErrorCode::BackendUnavailable)
            ? ArgParserError::InvalidDefinition
            : ArgParserError::InvalidValue;
    return make_argparser_error(parser_error, error.message);
}

template <typename T>
std::error_code import_config_file(const ConfigIoFile& file, T& value) {
    auto result = config_io::load<T>(std::filesystem::path(file.path), file.format);
    if (!result) {
        return config_io_to_argparser_error(result.error());
    }
    value = std::move(*result);
    return {};
}

template <typename T>
std::error_code export_config_file(const ConfigIoFile& file, const T& value) {
    auto result = config_io::save(value, std::filesystem::path(file.path), file.format);
    if (!result) {
        return config_io_to_argparser_error(result.error());
    }
    return {};
}

inline std::error_code collect_config_io_selection(const ArgSchema& schema, const RawParseResult& raw,
                                                   ConfigIoSelection& selection) {
    for (std::size_t index = schema.user_spec_count; index < schema.specs.size(); ++index) {
        if (index >= raw.options.size() || !raw.options[index].seen()) {
            continue;
        }
        const auto& values = raw.options[index].values;
        if (values.size() != 1U) {
            return make_argparser_error(ArgParserError::InvalidValue,
                                        format_error_option_label(schema.specs[index]) + " expects exactly one path");
        }
        const auto builtin = schema.builtin_spec(index);
        if (!builtin.has_value()) {
            continue;
        }
        if (builtin->direction == ConfigIoDirection::Import) {
            if (selection.import_file.has_value()) {
                return make_argparser_error(ArgParserError::InvalidValue,
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
std::error_code export_config_files(const ConfigIoSelection& selection, const T& value) {
    for (const auto& file : selection.export_files) {
        if (auto error = export_config_file(file, value)) {
            return error;
        }
    }
    return {};
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
