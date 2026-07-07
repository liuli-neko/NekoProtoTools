#include "nekoproto/argparser/argparser.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

NEKO_USE_NAMESPACE
using namespace NEKO_NAMESPACE::argparser;

enum class BuildMode {
    Debug,
    Release,
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<::BuildMode, void> {
    constexpr static auto value = Enumerate("debug", ::BuildMode::Debug, "release", ::BuildMode::Release);
};
NEKO_END_NAMESPACE

void set_demo_env(const char* name, const char* value) {
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
                   make_tags<arg_value_name<"HOST">, 
                            arg_env<"NEKO_ARGPARSER_MANUAL_HOST">,
                            arg_help<"listen host">, 
                            ArgTags{.required = true}>(&ServeNetworkOptions::host),
                   "port",
                   make_tags<arg_value_name<"PORT">, 
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
                make_tags<arg_short_name<'v'>, 
                        arg_group<"General">, 
                        arg_help<"enable verbose output">,
                        arg_aliases<"V", "debug">, 
                        ArgTags{.flag = true}>(&ServeCommand::verbose),
                
                "config",
                make_tags<arg_short_name<'c'>, 
                        arg_group<"General">, 
                        arg_env<"NEKO_ARGPARSER_MANUAL_CONFIG">,
                        arg_value_name<"FILE">, 
                        arg_help<"config file">>(&ServeCommand::config),

                "include",
                make_tags<arg_short_name<'I'>, 
                        arg_group<"Paths">, 
                        arg_separator<','>, 
                        arg_value_name<"DIRS">,
                        arg_help<"extra include directories">, 
                        ArgTags{.repeatable = true}>(
                    &ServeCommand::includeDirs),

                "color",
                make_tags<arg_group<"General">, 
                        arg_aliases<"colour">, 
                        arg_implicit<"auto"_cs>,
                        arg_choices<"auto", "always", "never">, 
                        arg_case_insensitive_choices,
                        arg_help<"colorize output">>(&ServeCommand::color),

                "root",
                make_tags<arg_group<"Paths">, 
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
                make_tags<arg_short_name<'j'>, 
                        arg_group<"Build">, 
                        arg_value_name<"N">, 
                        arg_default<4>,
                        arg_help<"parallel jobs">, 
                        ArgTags{.range_min = 1, .range_max = 65}>(&BuildCommand::jobs),

                "release",
                make_tags<arg_short_name<'r'>, 
                        arg_group<"Build">, 
                        arg_help<"release build">,
                        ArgTags{.flag = true}>(&BuildCommand::release),

                "dryRun",
                make_tags<arg_long_name<"dry-run">, 
                        arg_group<"Build">, 
                        arg_help<"show work without executing">,
                        arg_conflicts<"release">, 
                        ArgTags{.flag = true}>(&BuildCommand::dryRun),

                "legacy",
                make_tags<arg_long_name<"legacy-mode">, 
                        arg_group<"Compatibility">, 
                        arg_help<"legacy build mode">,
                        arg_deprecated<"use --mode release instead">, 
                        ArgTags{.flag = true}>(&BuildCommand::legacy),
                        
                "traceParser",
                make_tags<arg_long_name<"trace-parser">, 
                        arg_help<"internal parser tracing">,
                        ArgTags{.flag = true, .hidden = true}>(&BuildCommand::traceParser),

                "output",
                make_tags<arg_short_name<'o'>, 
                        arg_group<"Paths">, 
                        arg_env<"NEKO_ARGPARSER_MANUAL_OUTPUT">,
                        arg_value_name<"DIR">, 
                        arg_default<"build"_cs>, 
                        arg_help<"output directory">>(&BuildCommand::output),

                "mode",
                make_tags<arg_short_name<'m'>, 
                        arg_group<"Build">, 
                        arg_default<"debug"_cs>,
                        arg_choices<"debug", "release">, 
                        arg_case_insensitive_choices, 
                        arg_help<"build mode">>(&BuildCommand::mode),

                "define",
                make_tags<arg_short_name<'D'>, 
                        arg_group<"C/C++">, 
                        arg_separator<','>, 
                        arg_value_name<"DEFINE">,
                        arg_help<"preprocessor definitions">, 
                        ArgTags{.repeatable = true}>(&BuildCommand::defines),

                "publish",
                make_tags<arg_group<"Publish">, 
                        arg_help<"publish build artifact">,
                        arg_requires<"token">, 
                        ArgTags{.flag = true}>(&BuildCommand::publish),

                "token",
                make_tags<arg_group<"Publish">, 
                        arg_env<"NEKO_ARGPARSER_MANUAL_TOKEN">,
                        arg_value_name<"TOKEN">, 
                        arg_help<"publish token">>(&BuildCommand::token),

                "json",
                make_tags<arg_group<"Output">, 
                        arg_help<"json diagnostics">, 
                        arg_conflicts<"yaml">,
                        ArgTags{.flag = true}>(&BuildCommand::json),

                "yaml",
                make_tags<arg_group<"Output">, 
                        arg_help<"yaml diagnostics">, 
                        ArgTags{.flag = true}>(&BuildCommand::yaml),

                "target",
                make_tags<arg_help<"target name">, 
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
                   make_tags<arg_help<"run an HTTP-like demo server">, 
                            ArgTags{.command = true}>(&ToolCommands::serve),

                   "build",
                   make_tags<arg_help<"build a target">, 
                            ArgTags{.command = true}>(&ToolCommands::build),

                   "clean",
                   make_tags<arg_help<"remove generated files">, 
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
                   make_tags<arg_short_name<'v'>, 
                            arg_group<"General">, 
                            arg_help<"enable verbose output">,
                            ArgTags{.flag = true}>(&StandaloneOptions::verbose),

                   "count",
                   make_tags<arg_short_name<'c'>, 
                            arg_value_name<"N">, 
                            arg_default<1>, 
                            arg_help<"repeat count">,
                            ArgTags{.range_min = 1, .range_max = 10}>(&StandaloneOptions::count),

                   "output",
                   make_tags<arg_short_name<'o'>, 
                            arg_group<"Paths">, 
                            arg_env<"NEKO_ARGPARSER_MANUAL_OUTPUT">,
                            arg_value_name<"FILE">, 
                            arg_help<"optional output file">, 
                            ArgTags{.required = true}>(&StandaloneOptions::output),

                   "include",
                   make_tags<arg_short_name<'I'>, 
                            arg_group<"Paths">, 
                            arg_separator<','>, 
                            arg_value_name<"DIRS">,
                            arg_help<"repeatable include directory">, 
                            ArgTags{.repeatable = true}>(&StandaloneOptions::includeDirs),

                   "mode",
                   make_tags<arg_short_name<'m'>, 
                            arg_group<"General">, 
                            arg_default<"debug"_cs>,
                            arg_choices<"debug", "release">, 
                            arg_case_insensitive_choices, arg_help<"build mode">>(&StandaloneOptions::mode),

                   "color",
                   make_tags<arg_group<"General">, 
                            arg_aliases<"colour">, 
                            arg_implicit<"auto"_cs>,
                            arg_choices<"auto", "always", "never">, 
                            arg_case_insensitive_choices,
                            arg_help<"colorize output">>(&StandaloneOptions::color),

                   "token",
                   make_tags<arg_group<"Security">, 
                            arg_env<"NEKO_ARGPARSER_MANUAL_TOKEN">,
                            arg_value_name<"TOKEN">, 
                            arg_help<"login token">>(&StandaloneOptions::token),

                   "login",
                   make_tags<arg_group<"Security">, 
                            arg_requires<"token">, 
                            arg_help<"enable login">,
                            ArgTags{.flag = true}>(&StandaloneOptions::login),

                   "json",
                   make_tags<arg_group<"Output">, 
                            arg_conflicts<"yaml">, 
                            arg_help<"json output">,
                            ArgTags{.flag = true}>(&StandaloneOptions::json),

                   "yaml",
                   make_tags<arg_group<"Output">, 
                            arg_help<"yaml output">, 
                            ArgTags{.flag = true}>(&StandaloneOptions::yaml),

                   "legacyMode",
                   make_tags<arg_long_name<"legacy-mode">, 
                            arg_group<"Compatibility">,
                            arg_deprecated<"use --mode release instead">, 
                            arg_help<"legacy compatibility flag">,
                            ArgTags{.flag = true}>(&StandaloneOptions::legacyMode),

                   "traceParser",
                   make_tags<arg_long_name<"trace-parser">, 
                            arg_help<"internal parser tracing">,
                            ArgTags{.flag = true, .hidden = true}>(&StandaloneOptions::traceParser),

                   "network", &StandaloneOptions::network, 
                   
                   "input",
                   make_tags<arg_help<"input file">, 
                            ArgTags{.positional = true}>(&StandaloneOptions::input));
    };
};
// clang-format on
void print_error(std::error_code error) {
    std::cout << "parse error: " << error.category().name() << ": " << error.message() << '\n';
}

void print_vector(const std::vector<std::string>& values) {
    std::cout << '[';
    for (std::size_t idx = 0; idx < values.size(); ++idx) {
        if (idx != 0) {
            std::cout << ", ";
        }
        std::cout << values[idx];
    }
    std::cout << ']';
}

std::string_view mode_name(BuildMode mode) {
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
    print_vector(command.includeDirs);
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
    std::cout << "mode = " << mode_name(command.mode) << '\n';
    std::cout << "defines = ";
    print_vector(command.defines);
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
    print_vector(options.includeDirs);
    std::cout << '\n';
    std::cout << "mode = " << mode_name(options.mode) << '\n';
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

void run_standalone_demo() {
    set_demo_env("NEKO_ARGPARSER_MANUAL_TOKEN", "env-token");

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
    set_demo_env("NEKO_ARGPARSER_MANUAL_TOKEN", nullptr);
    if (!result) {
        print_error(result.error());
        std::cout << format_help<StandaloneOptions>(config);
        return;
    }
    print(*result);
}

int main(int argc, char** argv) {
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
    config.configIo->exportYaml   = true;
    config.configIo->importYaml   = true;
    config.configIo->exportJson   = true;
    config.configIo->importJson   = true;
    config.configIo->exportBinary = true;
    config.configIo->importBinary = true;

    if (argc == 1) {
        std::cout << format_help<ToolCommands>(config) << '\n';
        std::cout << "Examples:\n";
        std::cout << "  " << config.programName
                  << " serve --network.host 0.0.0.0 --network.port 9000 -v -I include,src --colour=always public\n";
        std::cout << "  " << config.programName << " build -j 8 --release --mode RELEASE --define DEBUG,NDEBUG app\n";
        std::cout << "  " << config.programName << " build --publish --token secret --json app\n";
        std::cout << "  " << config.programName << " build --help\n";
        std::cout << "  " << config.programName << " --version\n";
        std::cout << "  " << config.programName << " clean\n\n";
        std::cout << "Standalone parser demo output:\n";
        run_standalone_demo();
        return 0;
    }

    auto result = parser<ToolCommands>(argc, argv, config);
    if (!result) {
        if (result.error() == make_error_code(ArgParserError::HelpRequested)) {
            std::cout << format_help<ToolCommands>(argc, argv, config);
            return 0;
        }
        if (result.error() == make_error_code(ArgParserError::VersionRequested)) {
            std::cout << format_version(config);
            return 0;
        }
        print_error(result.error());
        std::cout << format_help<ToolCommands>(argc, argv, config);
        return 1;
    }

    std::visit([](const auto& command) { print(command); }, *result);
    return 0;
}
