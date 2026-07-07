#pragma once

#include "nekoproto/argparser/detail/raw_parser.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/yaml_serializer.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

enum class ConfigIoFormat {
    Json,
    Yaml,
    Binary,
};

struct ConfigIoFile {
    ConfigIoFormat format = ConfigIoFormat::Json;
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

inline std::string config_io_format_name(ConfigIoFormat format) {
    switch (format) {
    case ConfigIoFormat::Json:
        return "JSON";
    case ConfigIoFormat::Yaml:
        return "YAML";
    case ConfigIoFormat::Binary:
        return "binary";
    }
    return "unknown";
}

inline std::error_code read_config_file(std::string_view path, std::vector<char>& buffer) {
    std::ifstream file{std::string(path), std::ios::binary};
    if (!file) {
        return make_argparser_error(ArgParserError::InvalidValue,
                                    "could not open import file " + quote_arg_value(path));
    }
    buffer.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    if (!file.eof() && file.fail()) {
        return make_argparser_error(ArgParserError::InvalidValue,
                                    "could not read import file " + quote_arg_value(path));
    }
    return {};
}

inline std::error_code write_config_file(std::string_view path, const std::vector<char>& buffer) {
    std::ofstream file{std::string(path), std::ios::binary};
    if (!file) {
        return make_argparser_error(ArgParserError::InvalidValue,
                                    "could not open export file " + quote_arg_value(path));
    }
    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!file) {
        return make_argparser_error(ArgParserError::InvalidValue,
                                    "could not write export file " + quote_arg_value(path));
    }
    return {};
}

template <typename Serializer, typename T>
std::error_code import_serialized_config(std::string_view path, T& value, std::string_view label) {
    std::vector<char> buffer;
    if (auto error = read_config_file(path, buffer)) {
        return error;
    }
    typename Serializer::InputSerializer input(buffer.data(), buffer.size());
    if (!input(value)) {
        std::string detail = "failed to import ";
        detail.append(label);
        detail.append(" config ");
        detail.append(quote_arg_value(path));
        if (input.error() != nullptr) {
            detail.append(": ");
            detail.append(input.error()->msg);
        }
        return make_argparser_error(ArgParserError::InvalidValue, std::move(detail));
    }
    return {};
}

template <typename Serializer, typename T>
std::error_code export_serialized_config(std::string_view path, const T& value, std::string_view label) {
    std::vector<char> buffer;
    typename Serializer::OutputSerializer output(buffer);
    if (!output(value) || !output.end()) {
        std::string detail = "failed to export ";
        detail.append(label);
        detail.append(" config ");
        detail.append(quote_arg_value(path));
        if (output.error() != nullptr) {
            detail.append(": ");
            detail.append(output.error()->msg);
        }
        return make_argparser_error(ArgParserError::InvalidValue, std::move(detail));
    }
    return write_config_file(path, buffer);
}

template <typename T>
std::error_code import_config_file(const ConfigIoFile& file, T& value) {
    switch (file.format) {
    case ConfigIoFormat::Json:
#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
        return import_serialized_config<JsonSerializer>(file.path, value, "JSON");
#else
        return make_argparser_error(ArgParserError::InvalidDefinition, "JSON config import is not available");
#endif
    case ConfigIoFormat::Yaml:
#if !defined(NEKO_PROTO_NO_YAML_SERIALIZER)
        return import_serialized_config<YamlSerializer>(file.path, value, "YAML");
#else
        return make_argparser_error(ArgParserError::InvalidDefinition, "YAML config import is not available");
#endif
    case ConfigIoFormat::Binary:
        return import_serialized_config<BinarySerializer>(file.path, value, "binary");
    }
    return make_argparser_error(ArgParserError::InvalidDefinition, "unknown config import format");
}

template <typename T>
std::error_code export_config_file(const ConfigIoFile& file, const T& value) {
    switch (file.format) {
    case ConfigIoFormat::Json:
#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
        return export_serialized_config<JsonSerializer>(file.path, value, "JSON");
#else
        return make_argparser_error(ArgParserError::InvalidDefinition, "JSON config export is not available");
#endif
    case ConfigIoFormat::Yaml:
#if !defined(NEKO_PROTO_NO_YAML_SERIALIZER)
        return export_serialized_config<YamlSerializer>(file.path, value, "YAML");
#else
        return make_argparser_error(ArgParserError::InvalidDefinition, "YAML config export is not available");
#endif
    case ConfigIoFormat::Binary:
        return export_serialized_config<BinarySerializer>(file.path, value, "binary");
    }
    return make_argparser_error(ArgParserError::InvalidDefinition, "unknown config export format");
}

inline std::optional<ConfigIoFormat> import_format_for_builtin(ArgBuiltinSpecKind kind) {
    switch (kind) {
    case ArgBuiltinSpecKind::ImportJson:
        return ConfigIoFormat::Json;
    case ArgBuiltinSpecKind::ImportYaml:
        return ConfigIoFormat::Yaml;
    case ArgBuiltinSpecKind::ImportBinary:
        return ConfigIoFormat::Binary;
    default:
        return std::nullopt;
    }
}

inline std::optional<ConfigIoFormat> export_format_for_builtin(ArgBuiltinSpecKind kind) {
    switch (kind) {
    case ArgBuiltinSpecKind::ExportJson:
        return ConfigIoFormat::Json;
    case ArgBuiltinSpecKind::ExportYaml:
        return ConfigIoFormat::Yaml;
    case ArgBuiltinSpecKind::ExportBinary:
        return ConfigIoFormat::Binary;
    default:
        return std::nullopt;
    }
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
        const auto kind = schema.builtin_kind(index);
        if (auto format = import_format_for_builtin(kind); format.has_value()) {
            if (selection.import_file.has_value()) {
                return make_argparser_error(ArgParserError::InvalidValue,
                                            "only one config import option can be used at a time");
            }
            selection.import_file = ConfigIoFile{*format, std::string(values.front())};
        } else if (auto export_format = export_format_for_builtin(kind); export_format.has_value()) {
            selection.export_files.push_back(ConfigIoFile{*export_format, std::string(values.front())});
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
