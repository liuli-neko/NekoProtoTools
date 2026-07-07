#pragma once

#include "nekoproto/global/global.hpp"

#include <functional>
#include <optional>
#include <string_view>

NEKO_BEGIN_NAMESPACE
namespace argparser {

struct ArgParserConfigIoOptions {
    bool importJson   = false;
    bool exportJson   = false;
    bool importYaml   = false;
    bool exportYaml   = false;
    bool importBinary = false;
    bool exportBinary = false;

    std::string_view importJsonName   = "import-json";
    std::string_view exportJsonName   = "export-json";
    std::string_view importYamlName   = "import-yaml";
    std::string_view exportYamlName   = "export-yaml";
    std::string_view importBinaryName = "import-binary";
    std::string_view exportBinaryName = "export-binary";
};

struct ArgParserConfig {
    std::string_view programName;
    std::string_view description;
    std::string_view usage;
    std::string_view version;
    char nestedSeparator   = '.';
    bool allowUnknown      = false;
    bool addHelp           = true;
    bool addVersion        = true;
    bool allowShortCluster = false;
    std::optional<ArgParserConfigIoOptions> configIo;
    std::function<void(std::string_view optionName, std::string_view message)> deprecatedOptionHandler;
};

} // namespace argparser
NEKO_END_NAMESPACE
