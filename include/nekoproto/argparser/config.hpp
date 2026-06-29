#pragma once

#include "nekoproto/global/global.hpp"

#include <functional>
#include <string_view>

NEKO_BEGIN_NAMESPACE
namespace argparser {

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
    std::function<void(std::string_view optionName, std::string_view message)> deprecatedOptionHandler;
};

} // namespace argparser
NEKO_END_NAMESPACE
