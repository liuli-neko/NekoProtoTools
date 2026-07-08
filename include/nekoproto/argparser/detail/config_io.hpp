#pragma once

#include "nekoproto/argparser/detail/config_io_registry.hpp"
#include "nekoproto/argparser/detail/raw_parser.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/toml_serializer.hpp"
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
struct ConfigIoBackendSerializer;

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
template <>
struct ConfigIoBackendSerializer<JsonConfigIoBackend> {
    using type = JsonSerializer;
};
#endif

#if !defined(NEKO_PROTO_NO_YAML_SERIALIZER)
template <>
struct ConfigIoBackendSerializer<YamlConfigIoBackend> {
    using type = YamlSerializer;
};
#endif

template <>
struct ConfigIoBackendSerializer<BinaryConfigIoBackend> {
    using type = BinarySerializer;
};

#if !defined(NEKO_PROTO_NO_TOML_SERIALIZER)
template <>
struct ConfigIoBackendSerializer<TomlConfigIoBackend> {
    using type = TomlSerializer;
};
#endif

template <typename Backend, typename T>
std::error_code import_config_file_with_backend(const ConfigIoFile& file, T& value) {
    if constexpr (Backend::available) {
        using Serializer = typename ConfigIoBackendSerializer<Backend>::type;
        return import_serialized_config<Serializer>(file.path, value, Backend::label);
    } else {
        return make_argparser_error(ArgParserError::InvalidDefinition,
                                    std::string(Backend::label) + " config import is not available");
    }
}

template <typename Backend, typename T>
std::error_code export_config_file_with_backend(const ConfigIoFile& file, const T& value) {
    if constexpr (Backend::available) {
        using Serializer = typename ConfigIoBackendSerializer<Backend>::type;
        return export_serialized_config<Serializer>(file.path, value, Backend::label);
    } else {
        return make_argparser_error(ArgParserError::InvalidDefinition,
                                    std::string(Backend::label) + " config export is not available");
    }
}

template <typename T>
std::error_code import_config_file(const ConfigIoFile& file, T& value) {
    std::error_code result;
    bool matched = false;
    for_each_config_io_backend([&]<typename Backend>(std::type_identity<Backend>) {
        if (matched || file.format != Backend::format) {
            return;
        }
        matched = true;
        result  = import_config_file_with_backend<Backend>(file, value);
    });
    if (matched) {
        return result;
    }
    return make_argparser_error(ArgParserError::InvalidDefinition,
                                "unknown config import format " + std::string(file.format));
}

template <typename T>
std::error_code export_config_file(const ConfigIoFile& file, const T& value) {
    std::error_code result;
    bool matched = false;
    for_each_config_io_backend([&]<typename Backend>(std::type_identity<Backend>) {
        if (matched || file.format != Backend::format) {
            return;
        }
        matched = true;
        result  = export_config_file_with_backend<Backend>(file, value);
    });
    if (matched) {
        return result;
    }
    return make_argparser_error(ArgParserError::InvalidDefinition,
                                "unknown config export format " + std::string(file.format));
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
