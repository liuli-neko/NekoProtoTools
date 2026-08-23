#include "nekoproto/argparser/argparser.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

using namespace nekoproto;
using namespace nekoproto::argparser;

enum class BuildMode {
    Debug,
    Release,
};

namespace nekoproto {
template <>
struct Meta<::BuildMode, void> {
    constexpr static auto value = Enumerate("debug", ::BuildMode::Debug, "release", ::BuildMode::Release);
};
} // namespace nekoproto

void setDemoEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value == nullptr ? "" : value);
#else
    if (value == nullptr) {
        unsetenv(name);
    } else {
        setenv(name, value, 1);
    }
#endif
}
// clang-format off
struct ServeNetworkOptions {
    std::string host = "127.0.0.1";
    int port         = 8080;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("host",
                   makeTags<arg_absolute_name<"host">,
                            arg_value_name<"HOST">, 
                            arg_env<"NEKO_ARGPARSER_MANUAL_HOST">,
                            arg_help<"listen host">, 
                            ArgTags{.required = true}>(&ServeNetworkOptions::host),
                   "port",
                   makeTags<arg_absolute_name<"port">,
                            arg_value_name<"PORT">, 
                            arg_default<8080>, 
                            arg_help<"listen port">,
                            ArgTags{.range_min = 1, .range_max = 65536}>(&ServeNetworkOptions::port));
    };
};

struct ServeCommand {
    ServeNetworkOptions network;
    bool verbose = false;
    std::optional<std::string> config;
    std::vector<std::string> includeDirs;
    std::string color = "never";
    std::string root;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object(
                "network", &ServeCommand::network, 
                
                "verbose",
                makeTags<arg_short_name<'v'>, 
                        arg_group<"General">, 
                        arg_help<"enable verbose output">,
                        arg_aliases<"V", "debug">, 
                        ArgTags{.flag = true}>(&ServeCommand::verbose),
                
                "config",
                makeTags<arg_complete_file,
                        arg_short_name<'c'>,
                        arg_group<"General">, 
                        arg_env<"NEKO_ARGPARSER_MANUAL_CONFIG">,
                        arg_value_name<"FILE">, 
                        arg_help<"config file">>(&ServeCommand::config),

                "include",
                makeTags<arg_complete_directory,
                        arg_short_name<'I'>,
                        arg_group<"Paths">,
                        arg_separator<','>, 
                        arg_value_name<"DIRS">,
                        arg_help<"extra include directories">, 
                        ArgTags{.repeatable = true}>(
                    &ServeCommand::includeDirs),

                "color",
                makeTags<arg_group<"General">, 
                        arg_aliases<"colour">, 
                        arg_implicit<"auto"_cs>,
                        arg_choices<"auto", "always", "never">, 
                        arg_case_insensitive_choices,
                        arg_help<"colorize output">>(&ServeCommand::color),

                "root",
                makeTags<arg_complete_directory,
                        arg_group<"Paths">,
                        arg_help<"document root">, 
                        ArgTags{.positional = true}>(
                    &ServeCommand::root));
    };
};

struct BuildCommand {
    int jobs         = 1;
    bool release     = false;
    bool dryRun      = false;
    bool legacy      = false;
    bool traceParser = false;
    std::string output;
    BuildMode mode = BuildMode::Debug;
    std::vector<std::string> defines;
    bool publish = false;
    std::string token;
    bool json = false;
    bool yaml = false;
    std::string target;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object(
                "jobs",
                makeTags<arg_short_name<'j'>, 
                        arg_group<"Build">, 
                        arg_value_name<"N">, 
                        arg_default<4>,
                        arg_help<"parallel jobs">, 
                        ArgTags{.range_min = 1, .range_max = 65}>(&BuildCommand::jobs),

                "release",
                makeTags<arg_short_name<'r'>, 
                        arg_group<"Build">, 
                        arg_help<"release build">,
                        ArgTags{.flag = true}>(&BuildCommand::release),

                "dryRun",
                makeTags<arg_long_name<"dry-run">, 
                        arg_group<"Build">, 
                        arg_help<"show work without executing">,
                        arg_conflicts<"release">, 
                        ArgTags{.flag = true}>(&BuildCommand::dryRun),

                "legacy",
                makeTags<arg_long_name<"legacy-mode">, 
                        arg_group<"Compatibility">, 
                        arg_help<"legacy build mode">,
                        arg_deprecated<"use --mode release instead">, 
                        ArgTags{.flag = true}>(&BuildCommand::legacy),
                        
                "traceParser",
                makeTags<arg_long_name<"trace-parser">, 
                        arg_help<"internal parser tracing">,
                        ArgTags{.flag = true, .hidden = true}>(&BuildCommand::traceParser),

                "output",
                makeTags<arg_complete_directory,
                        arg_short_name<'o'>,
                        arg_group<"Paths">, 
                        arg_env<"NEKO_ARGPARSER_MANUAL_OUTPUT">,
                        arg_value_name<"DIR">, 
                        arg_default<"build"_cs>, 
                        arg_help<"output directory">>(&BuildCommand::output),

                "mode",
                makeTags<arg_short_name<'m'>, 
                        arg_group<"Build">, 
                        arg_default<"debug"_cs>,
                        arg_choices<"debug", "release">, 
                        arg_case_insensitive_choices, 
                        arg_help<"build mode">>(&BuildCommand::mode),

                "define",
                makeTags<arg_short_name<'D'>, 
                        arg_group<"C/C++">, 
                        arg_separator<','>, 
                        arg_value_name<"DEFINE">,
                        arg_help<"preprocessor definitions">, 
                        ArgTags{.repeatable = true}>(&BuildCommand::defines),

                "publish",
                makeTags<arg_group<"Publish">, 
                        arg_help<"publish build artifact">,
                        arg_requires<"token">, 
                        ArgTags{.flag = true}>(&BuildCommand::publish),

                "token",
                makeTags<arg_group<"Publish">, 
                        arg_env<"NEKO_ARGPARSER_MANUAL_TOKEN">,
                        arg_value_name<"TOKEN">, 
                        arg_help<"publish token">>(&BuildCommand::token),

                "json",
                makeTags<arg_group<"Output">, 
                        arg_help<"json diagnostics">, 
                        arg_conflicts<"yaml">,
                        ArgTags{.flag = true}>(&BuildCommand::json),

                "yaml",
                makeTags<arg_group<"Output">, 
                        arg_help<"yaml diagnostics">, 
                        ArgTags{.flag = true}>(&BuildCommand::yaml),

                "target",
                makeTags<arg_help<"target name">, 
                        ArgTags{.positional = true}>(&BuildCommand::target));
    };
};

struct ToolCommands {
    ServeCommand serve;
    BuildCommand build;
    ArgCommand<"clean"_cs> clean;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("serve",
                   makeTags<arg_help<"run an HTTP-like demo server">, 
                            ArgTags{.command = true}>(&ToolCommands::serve),

                   "build",
                   makeTags<arg_help<"build a target">, 
                            ArgTags{.command = true}>(&ToolCommands::build),

                   "clean",
                   makeTags<arg_help<"remove generated files">, 
                            ArgTags{.command = true}>(&ToolCommands::clean));
    };
};

struct StandaloneOptions {
    bool verbose = false;
    int count    = 0;
    std::optional<std::string> output;
    std::vector<std::string> includeDirs;
    BuildMode mode    = BuildMode::Debug;
    std::string color = "never";
    std::optional<std::string> token;
    bool login       = false;
    bool json        = false;
    bool yaml        = false;
    bool legacyMode  = false;
    bool traceParser = false;
    ServeNetworkOptions network;
    std::string input;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("verbose",
                   makeTags<arg_short_name<'v'>, 
                            arg_group<"General">, 
                            arg_help<"enable verbose output">,
                            ArgTags{.flag = true}>(&StandaloneOptions::verbose),

                   "count",
                   makeTags<arg_short_name<'c'>, 
                            arg_value_name<"N">, 
                            arg_default<1>, 
                            arg_help<"repeat count">,
                            ArgTags{.range_min = 1, .range_max = 10}>(&StandaloneOptions::count),

                   "output",
                   makeTags<arg_complete_file,
                            arg_short_name<'o'>,
                            arg_group<"Paths">, 
                            arg_env<"NEKO_ARGPARSER_MANUAL_OUTPUT">,
                            arg_value_name<"FILE">, 
                            arg_help<"optional output file">, 
                            ArgTags{.required = true}>(&StandaloneOptions::output),

                   "include",
                   makeTags<arg_complete_directory,
                            arg_short_name<'I'>,
                            arg_group<"Paths">, 
                            arg_separator<','>, 
                            arg_value_name<"DIRS">,
                            arg_help<"repeatable include directory">, 
                            ArgTags{.repeatable = true}>(&StandaloneOptions::includeDirs),

                   "mode",
                   makeTags<arg_short_name<'m'>, 
                            arg_group<"General">, 
                            arg_default<"debug"_cs>,
                            arg_choices<"debug", "release">, 
                            arg_case_insensitive_choices, arg_help<"build mode">>(&StandaloneOptions::mode),

                   "color",
                   makeTags<arg_group<"General">, 
                            arg_aliases<"colour">, 
                            arg_implicit<"auto"_cs>,
                            arg_choices<"auto", "always", "never">, 
                            arg_case_insensitive_choices,
                            arg_help<"colorize output">>(&StandaloneOptions::color),

                   "token",
                   makeTags<arg_group<"Security">, 
                            arg_env<"NEKO_ARGPARSER_MANUAL_TOKEN">,
                            arg_value_name<"TOKEN">, 
                            arg_help<"login token">>(&StandaloneOptions::token),

                   "login",
                   makeTags<arg_group<"Security">, 
                            arg_requires<"token">, 
                            arg_help<"enable login">,
                            ArgTags{.flag = true}>(&StandaloneOptions::login),

                   "json",
                   makeTags<arg_group<"Output">, 
                            arg_conflicts<"yaml">, 
                            arg_help<"json output">,
                            ArgTags{.flag = true}>(&StandaloneOptions::json),

                   "yaml",
                   makeTags<arg_group<"Output">, 
                            arg_help<"yaml output">, 
                            ArgTags{.flag = true}>(&StandaloneOptions::yaml),

                   "legacyMode",
                   makeTags<arg_long_name<"legacy-mode">, 
                            arg_group<"Compatibility">,
                            arg_deprecated<"use --mode release instead">, 
                            arg_help<"legacy compatibility flag">,
                            ArgTags{.flag = true}>(&StandaloneOptions::legacyMode),

                   "traceParser",
                   makeTags<arg_long_name<"trace-parser">, 
                            arg_help<"internal parser tracing">,
                            ArgTags{.flag = true, .hidden = true}>(&StandaloneOptions::traceParser),

                   "network", &StandaloneOptions::network, 
                   
                   "input",
                   makeTags<arg_complete_file,
                            arg_help<"input file">,
                            ArgTags{.positional = true}>(&StandaloneOptions::input));
    };
};
// clang-format on
void printError(std::error_code error) {
    std::cout << "parse error: " << error.category().name() << ": " << error.message() << '\n';
}

void printVector(const std::vector<std::string>& values) {
    std::cout << '[';
    for (std::size_t idx = 0; idx < values.size(); ++idx) {
        if (idx != 0) {
            std::cout << ", ";
        }
        std::cout << values[idx];
    }
    std::cout << ']';
}

auto modeName(BuildMode mode) -> std::string_view {
    switch (mode) {
    case BuildMode::Debug:
        return "debug";
    case BuildMode::Release:
        return "release";
    }
    return "<unknown>";
}

void print(const ServeNetworkOptions& options) {
    std::cout << "network.host = " << options.host << '\n';
    std::cout << "network.port = " << options.port << '\n';
}

void print(const ServeCommand& command) {
    std::cout << "command = serve\n";
    print(command.network);
    std::cout << "verbose = " << command.verbose << '\n';
    std::cout << "config = " << command.config.value_or("<none>") << '\n';
    std::cout << "includeDirs = ";
    printVector(command.includeDirs);
    std::cout << '\n';
    std::cout << "color = " << command.color << '\n';
    std::cout << "root = " << command.root << '\n';
}

void print(const BuildCommand& command) {
    std::cout << "command = build\n";
    std::cout << "jobs = " << command.jobs << '\n';
    std::cout << "release = " << command.release << '\n';
    std::cout << "dryRun = " << command.dryRun << '\n';
    std::cout << "legacy = " << command.legacy << '\n';
    std::cout << "traceParser = " << command.traceParser << '\n';
    std::cout << "output = " << command.output << '\n';
    std::cout << "mode = " << modeName(command.mode) << '\n';
    std::cout << "defines = ";
    printVector(command.defines);
    std::cout << '\n';
    std::cout << "publish = " << command.publish << '\n';
    std::cout << "token = " << (command.token.empty() ? "<none>" : command.token) << '\n';
    std::cout << "json = " << command.json << '\n';
    std::cout << "yaml = " << command.yaml << '\n';
    std::cout << "target = " << command.target << '\n';
}

void print(const ArgCommand<"clean"_cs>& /*unused*/) {
    std::cout << "command = clean\n";
    std::cout << "no command-specific options\n";
}

void print(const StandaloneOptions& options) {
    std::cout << "standalone options\n";
    std::cout << "verbose = " << options.verbose << '\n';
    std::cout << "count = " << options.count << '\n';
    std::cout << "output = " << options.output.value_or("<none>") << '\n';
    std::cout << "includeDirs = ";
    printVector(options.includeDirs);
    std::cout << '\n';
    std::cout << "mode = " << modeName(options.mode) << '\n';
    std::cout << "color = " << options.color << '\n';
    std::cout << "token = " << options.token.value_or("<none>") << '\n';
    std::cout << "login = " << options.login << '\n';
    std::cout << "json = " << options.json << '\n';
    std::cout << "yaml = " << options.yaml << '\n';
    std::cout << "legacyMode = " << options.legacyMode << '\n';
    std::cout << "traceParser = " << options.traceParser << '\n';
    print(options.network);
    std::cout << "input = " << options.input << '\n';
}

void runStandaloneDemo() {
    setDemoEnv("NEKO_ARGPARSER_MANUAL_TOKEN", "env-token");

    const char* argv[] = {"standalone-demo",
                          "--verbose",
                          "--count=3",
                          "--output",
                          "out.txt",
                          "-I",
                          "include/core,include/extra",
                          "--network.host",
                          "0.0.0.0",
                          "--network.port",
                          "9000",
                          "--mode",
                          "RELEASE",
                          "--colour",
                          "--login",
                          "--json",
                          "--legacy-mode",
                          "--trace-parser",
                          "input.neko"};

    ArgParserConfig config;
    config.description = "Standalone options demo with nested options, positional values, and advanced tags.";
    config.deprecatedOptionHandler = [](std::string_view optionName, std::string_view message) {
        std::cout << "warning: --" << optionName << ": " << message << '\n';
    };

    auto result = parser<StandaloneOptions>(static_cast<int>(std::size(argv)), argv, config);
    setDemoEnv("NEKO_ARGPARSER_MANUAL_TOKEN", nullptr);
    if (!result) {
        printError(result.error());
        std::cout << formatHelp<StandaloneOptions>(config);
        return;
    }
    print(*result);
}

auto main(int argc, char** argv) -> int {
    ArgParserConfig config;
    config.programName             = argc > 0 ? argv[0] : "test_argparser_manual";
    config.description             = "Manual argparser example covering command dispatch and option parsing.";
    config.version                 = "0.1.0";
    config.allowShortCluster       = true;
    config.nestedSeparator         = '.';
    config.deprecatedOptionHandler = [](std::string_view optionName, std::string_view message) {
        std::cout << "warning: --" << optionName << ": " << message << '\n';
    };
    config.configIo.emplace();
    config.configIo->enableFormat("yaml");
    config.configIo->enableFormat("json");
    config.configIo->enableFormat("binary");
    config.configIo->enableFormat("toml");

    if (argc == 3 && std::string_view(argv[1]) == "--generate-completion") {
        const auto shell_name = std::string_view(argv[2]);
        if (shell_name == "bash") {
            std::cout << formatCompletion<ToolCommands>(CompletionShell::Bash, "test_argparser_manual", config);
            return 0;
        }
        if (shell_name == "zsh") {
            std::cout << formatCompletion<ToolCommands>(CompletionShell::Zsh, "test_argparser_manual", config);
            return 0;
        }
        std::cerr << "unsupported shell: " << shell_name << '\n';
        return 1;
    }

    if (argc == 1) {
        std::cout << formatHelp<ToolCommands>(config) << '\n';
        std::cout << "Examples:\n";
        std::cout << "  " << config.programName
                  << " serve --network.host 0.0.0.0 --network.port 9000 -v -I include,src --colour=always public\n";
        std::cout << "  " << config.programName << " build -j 8 --release --mode RELEASE --define DEBUG,NDEBUG app\n";
        std::cout << "  " << config.programName << " build --publish --token secret --json app\n";
        std::cout << "  " << config.programName << " build --help\n";
        std::cout << "  " << config.programName << " --version\n";
        std::cout << "  " << config.programName << " clean\n\n";
        std::cout << "Standalone parser demo output:\n";
        runStandaloneDemo();
        return 0;
    }

    auto result = parser<ToolCommands>(argc, argv, config);
    if (!result) {
        if (result.error() == makeErrorCode(ArgParserError::HelpRequested)) {
            std::cout << formatHelp<ToolCommands>(argc, argv, config);
            return 0;
        }
        if (result.error() == makeErrorCode(ArgParserError::VersionRequested)) {
            std::cout << formatVersion(config);
            return 0;
        }
        printError(result.error());
        std::cout << formatHelp<ToolCommands>(argc, argv, config);
        return 1;
    }

    std::visit([](const auto& command) { print(command); }, *result);
    return 0;
}
