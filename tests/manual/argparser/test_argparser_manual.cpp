#include "nekoproto/argparser/argparser.hpp"

#include <array>
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

struct ServeNetworkOptions {
    std::string host = "127.0.0.1";
    int port         = 8080;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("host",
                   make_tags<arg_name<"host", "", arg_help<"listen host", ArgTags{.required = true}>>>(
                       &ServeNetworkOptions::host),
                   "port",
                   make_tags<arg_name<"port", "", arg_help<"listen port", ArgTags{.rangeMin = 1, .rangeMax = 65536}>>>(
                       &ServeNetworkOptions::port));
    };
};

struct ServeCommand {
    ServeNetworkOptions network;
    bool verbose = false;
    std::optional<std::string> config;
    std::vector<std::string> includeDirs;
    std::string root;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("network", &ServeCommand::network, "verbose",
                   make_tags<arg_name<"verbose", "v", arg_help<"enable verbose output", ArgTags{.flag = true}>>>(
                       &ServeCommand::verbose),
                   "config",
                   make_tags<arg_name<"config", "c", arg_help<"config file">>>(&ServeCommand::config),
                   "include",
                   make_tags<arg_name<"include", "I",
                                      arg_help<"extra include directory", ArgTags{.repeatable = true}>>>(
                       &ServeCommand::includeDirs),
                   "root", make_tags<arg_name<"root", "", arg_help<"document root", ArgTags{.positional = true}>>>(
                               &ServeCommand::root));
    };
};

struct BuildCommand {
    int jobs      = 1;
    bool release  = false;
    bool dryRun   = false;
    std::string output = "build";
    BuildMode mode     = BuildMode::Debug;
    std::vector<std::string> defines;
    std::string target;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("jobs", make_tags<arg_name<"jobs", "j",
                                                arg_help<"parallel jobs", ArgTags{.rangeMin = 1, .rangeMax = 65}>>>(
                               &BuildCommand::jobs),
                   "release",
                   make_tags<arg_name<"release", "r", arg_help<"release build", ArgTags{.flag = true}>>>(
                       &BuildCommand::release),
                   "dryRun", make_tags<arg_name<"dry-run", "", arg_help<"show work without executing",
                                                                         ArgTags{.flag = true}>>>(
                                 &BuildCommand::dryRun),
                   "output", make_tags<arg_name<"output", "o", arg_help<"output directory">>>(&BuildCommand::output),
                   "mode",
                   make_tags<arg_name<"mode", "m",
                                      arg_help<"build mode", arg_choices<ArgTags{}, "debug", "release">>>>(
                       &BuildCommand::mode),
                   "define", make_tags<arg_name<"define", "D",
                                                   arg_help<"preprocessor definition", ArgTags{.repeatable = true}>>>(
                                 &BuildCommand::defines),
                   "target", make_tags<arg_name<"target", "", arg_help<"target name", ArgTags{.positional = true}>>>(
                                 &BuildCommand::target));
    };
};

struct ToolCommands {
    ServeCommand serve;
    BuildCommand build;
    ArgCommand clean;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("serve", make_tags<arg_name<"serve", "",
                                                arg_help<"run an HTTP-like demo server", ArgTags{.command = true}>>>(
                                &ToolCommands::serve),
                   "build", make_tags<arg_name<"build", "", arg_help<"build a target", ArgTags{.command = true}>>>(
                                &ToolCommands::build),
                   "clean", make_tags<arg_name<"clean", "",
                                                arg_help<"remove generated files", ArgTags{.command = true}>>>(
                                &ToolCommands::clean));
    };
};

struct StandaloneOptions {
    bool verbose = false;
    int count    = 0;
    std::optional<std::string> output;
    std::vector<std::string> includeDirs;
    ServeNetworkOptions network;
    std::string input;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("verbose", make_tags<arg_name<"verbose", "v",
                                                   arg_help<"enable verbose output", ArgTags{.flag = true}>>>(
                                  &StandaloneOptions::verbose),
                   "count", make_tags<arg_name<"count", "c", arg_help<"repeat count">>>(&StandaloneOptions::count),
                   "output",
                   make_tags<arg_name<"output", "o", arg_help<"optional output file", ArgTags{.required = true}>>>(
                       &StandaloneOptions::output),
                   "include",
                   make_tags<arg_name<"include", "I",
                                      arg_help<"repeatable include directory", ArgTags{.repeatable = true}>>>(
                       &StandaloneOptions::includeDirs),
                   "network", &StandaloneOptions::network, "input",
                   make_tags<arg_name<"input", "", arg_help<"input file", ArgTags{.positional = true}>>>(
                       &StandaloneOptions::input));
    };
};

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
    std::cout << "root = " << command.root << '\n';
}

void print(const BuildCommand& command) {
    std::cout << "command = build\n";
    std::cout << "jobs = " << command.jobs << '\n';
    std::cout << "release = " << command.release << '\n';
    std::cout << "dryRun = " << command.dryRun << '\n';
    std::cout << "output = " << command.output << '\n';
    std::cout << "mode = " << mode_name(command.mode) << '\n';
    std::cout << "defines = ";
    print_vector(command.defines);
    std::cout << '\n';
    std::cout << "target = " << command.target << '\n';
}

void print(const ArgCommand& /*unused*/) {
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
    print(options.network);
    std::cout << "input = " << options.input << '\n';
}

void run_standalone_demo() {
    const char* argv[] = {"standalone-demo", "--verbose",      "--count=3", "--output", "out.txt",
                          "-I",              "include/core",   "-Iextra",    "--network.host",
                          "0.0.0.0",         "--network.port", "9000",       "input.neko"};

    ArgParserConfig config;
    config.description = "Standalone options demo with nested options, positional values, and repeatable values.";

    auto result = parser<StandaloneOptions>(static_cast<int>(std::size(argv)), argv, config);
    if (!result) {
        print_error(result.error());
        std::cout << format_help<StandaloneOptions>(config);
        return;
    }
    print(*result);
}

int main(int argc, char** argv) {
    ArgParserConfig config;
    config.programName        = argc > 0 ? argv[0] : "test_argparser_manual";
    config.description        = "Manual argparser example covering command dispatch and option parsing.";
    config.version            = "0.1.0";
    config.allowShortCluster  = true;
    config.nestedSeparator    = '.';

    if (argc == 1) {
        std::cout << format_help<ToolCommands>(config) << '\n';
        std::cout << "Examples:\n";
        std::cout << "  " << config.programName
                  << " serve --network.host 0.0.0.0 --network.port 9000 -v -I include public\n";
        std::cout << "  " << config.programName << " build -j 8 --release --mode release -DDEBUG -DNDEBUG app\n";
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
