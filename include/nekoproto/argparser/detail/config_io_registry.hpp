#pragma once

#include "nekoproto/global/global.hpp"

#include <array>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

enum class ConfigIoDirection {
    Import,
    Export,
};

struct ConfigIoBuiltinSpec {
    ConfigIoDirection direction = ConfigIoDirection::Import;
    std::string_view format;
};

struct JsonConfigIoBackend {
    static constexpr std::string_view format            = "json";
    static constexpr std::array<std::string_view, 0> aliases{};
    static constexpr std::string_view label             = "JSON";
    static constexpr std::string_view defaultImportName = "import-json";
    static constexpr std::string_view defaultExportName = "export-json";
    static constexpr std::string_view importHelp        = "import options from a JSON file";
    static constexpr std::string_view exportHelp        = "export resolved options to a JSON file";
    static constexpr bool available =
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON) || defined(NEKO_PROTO_ENABLE_SIMDJSON)
        true;
#else
        false;
#endif
};

struct YamlConfigIoBackend {
    static constexpr std::string_view format            = "yaml";
    static constexpr std::array<std::string_view, 1> aliases = {std::string_view{"yml"}};
    static constexpr std::string_view label             = "YAML";
    static constexpr std::string_view defaultImportName = "import-yaml";
    static constexpr std::string_view defaultExportName = "export-yaml";
    static constexpr std::string_view importHelp        = "import options from a YAML file";
    static constexpr std::string_view exportHelp        = "export resolved options to a YAML file";
    static constexpr bool available =
#if defined(NEKO_PROTO_ENABLE_LIBFYAML)
        true;
#else
        false;
#endif
};

struct BinaryConfigIoBackend {
    static constexpr std::string_view format            = "binary";
    static constexpr std::array<std::string_view, 1> aliases = {std::string_view{"bin"}};
    static constexpr std::string_view label             = "binary";
    static constexpr std::string_view defaultImportName = "import-binary";
    static constexpr std::string_view defaultExportName = "export-binary";
    static constexpr std::string_view importHelp        = "import options from a binary file";
    static constexpr std::string_view exportHelp        = "export resolved options to a binary file";
    static constexpr bool available                     = true;
};

struct TomlConfigIoBackend {
    static constexpr std::string_view format            = "toml";
    static constexpr std::array<std::string_view, 0> aliases{};
    static constexpr std::string_view label             = "TOML";
    static constexpr std::string_view defaultImportName = "import-toml";
    static constexpr std::string_view defaultExportName = "export-toml";
    static constexpr std::string_view importHelp        = "import options from a TOML file";
    static constexpr std::string_view exportHelp        = "export resolved options to a TOML file";
    static constexpr bool available =
#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)
        true;
#else
        false;
#endif
};

using ConfigIoBackendRegistry =
    std::tuple<JsonConfigIoBackend, YamlConfigIoBackend, BinaryConfigIoBackend, TomlConfigIoBackend>;

template <typename Backend>
constexpr bool config_io_format_matches(std::string_view format) {
    if (format == Backend::format) {
        return true;
    }
    for (const auto alias : Backend::aliases) {
        if (format == alias) {
            return true;
        }
    }
    return false;
}

template <typename Fn, typename... Backends>
void for_each_config_io_backend_impl(Fn&& fn, std::tuple<Backends...>) {
    (fn(std::type_identity<Backends>{}), ...);
}

template <typename Fn>
void for_each_config_io_backend(Fn&& fn) {
    for_each_config_io_backend_impl(std::forward<Fn>(fn), ConfigIoBackendRegistry{});
}

inline std::string_view config_io_direction_name(ConfigIoDirection direction) {
    return direction == ConfigIoDirection::Import ? "import" : "export";
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
