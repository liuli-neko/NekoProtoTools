#pragma once

#include "nekoproto/global/global.hpp"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser {

struct ArgParserConfigIoOptionNames {
    std::string_view format;
    std::optional<std::string_view> importName;
    std::optional<std::string_view> exportName;
};

struct ArgParserConfigIoOptions {
    std::vector<std::string_view> importFormats;
    std::vector<std::string_view> exportFormats;
    std::vector<ArgParserConfigIoOptionNames> optionNames;

    void enableImportFormat(std::string_view format) { importFormats.push_back(format); }
    void enableExportFormat(std::string_view format) { exportFormats.push_back(format); }

    void enableFormat(std::string_view format) {
        enableImportFormat(format);
        enableExportFormat(format);
    }
};

struct ArgParserConfig {
    /** Non-owning display strings; keep their backing storage alive for the parser call. */
    std::string_view programName;
    std::string_view description;
    std::string_view usage;
    std::string_view version;
    /** Separates nested option paths, for example '.' in --database.host. */
    char nestedSeparator = '.';
    /** Skip only an unknown option token; its following token is never guessed as its value. */
    bool allowUnknown      = false;
    bool addHelp           = true;
    bool addVersion        = true;
    bool allowShortCluster = false;
    std::optional<ArgParserConfigIoOptions> configIo;
    std::function<void(std::string_view optionName, std::string_view message)> deprecatedOptionHandler;
};

} // namespace argparser
NEKO_END_NAMESPACE
