#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/toml_serializer.hpp"
#include "nekoproto/serialization/yaml_serializer.hpp"

#include <array>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nekoproto {
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
    static constexpr std::string_view format = "json";
    static constexpr std::array<std::string_view, 0> aliases{};
    static constexpr std::string_view label             = "JSON";
    static constexpr std::string_view defaultImportName = "import-json";
    static constexpr std::string_view defaultExportName = "export-json";
    static constexpr std::string_view importHelp        = "import options from a JSON file";
    static constexpr std::string_view exportHelp        = "export resolved options to a JSON file";
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON) || defined(NEKO_PROTO_ENABLE_SIMDJSON)
    using Serializer                = JsonSerializer;
    static constexpr bool available = true;
#else
    static constexpr bool available = false;
#endif
};

struct YamlConfigIoBackend {
    static constexpr std::string_view format                 = "yaml";
    static constexpr std::array<std::string_view, 1> aliases = {std::string_view{"yml"}};
    static constexpr std::string_view label                  = "YAML";
    static constexpr std::string_view defaultImportName      = "import-yaml";
    static constexpr std::string_view defaultExportName      = "export-yaml";
    static constexpr std::string_view importHelp             = "import options from a YAML file";
    static constexpr std::string_view exportHelp             = "export resolved options to a YAML file";
#if defined(NEKO_PROTO_ENABLE_LIBFYAML) || defined(NEKO_PROTO_ENABLE_YAMLCPP)
    using Serializer                = YamlSerializer;
    static constexpr bool available = true;
#else
    static constexpr bool available = false;
#endif
};

struct BinaryConfigIoBackend {
    using Serializer                                         = BinarySerializer;
    static constexpr std::string_view format                 = "binary";
    static constexpr std::array<std::string_view, 1> aliases = {std::string_view{"bin"}};
    static constexpr std::string_view label                  = "binary";
    static constexpr std::string_view defaultImportName      = "import-binary";
    static constexpr std::string_view defaultExportName      = "export-binary";
    static constexpr std::string_view importHelp             = "import options from a binary file";
    static constexpr std::string_view exportHelp             = "export resolved options to a binary file";
    static constexpr bool available                          = true;
};

struct TomlConfigIoBackend {
    static constexpr std::string_view format = "toml";
    static constexpr std::array<std::string_view, 0> aliases{};
    static constexpr std::string_view label             = "TOML";
    static constexpr std::string_view defaultImportName = "import-toml";
    static constexpr std::string_view defaultExportName = "export-toml";
    static constexpr std::string_view importHelp        = "import options from a TOML file";
    static constexpr std::string_view exportHelp        = "export resolved options to a TOML file";
#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)
    using Serializer                = TomlSerializer;
    static constexpr bool available = true;
#else
    static constexpr bool available = false;
#endif
};

using ConfigIoBackendRegistry =
    std::tuple<JsonConfigIoBackend, YamlConfigIoBackend, BinaryConfigIoBackend, TomlConfigIoBackend>;

template <typename Backend>
constexpr auto configIoFormatMatches(std::string_view format) -> bool {
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
void forEachConfigIoBackendImpl(Fn&& fn, std::tuple<Backends...>) {
    (fn(std::type_identity<Backends>{}), ...);
}

template <typename Fn>
void forEachConfigIoBackend(Fn&& fn) {
    forEachConfigIoBackendImpl(std::forward<Fn>(fn), ConfigIoBackendRegistry{});
}

inline auto configIoDirectionName(ConfigIoDirection direction) -> std::string_view {
    return direction == ConfigIoDirection::Import ? "import" : "export";
}

} // namespace argparser::detail
} // namespace nekoproto
