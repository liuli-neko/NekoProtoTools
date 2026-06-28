#include <gtest/gtest.h>

#include "nekoproto/argparser/argparser.hpp"

#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <type_traits>
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

void set_test_env(const char* name, const char* value) {
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

struct DatabaseOptions {
    std::string host = "localhost";
    int port         = 0;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("host",
                   make_tags<arg_name<"host", "", arg_help<"database host", ArgTags{.required = true}>>>(
                       &DatabaseOptions::host),
                   "port", make_tags<arg_name<"port", "", arg_help<"database port">>>(&DatabaseOptions::port));
    };
};

struct CliOptions {
    bool verbose = false;
    int count    = 0;
    std::optional<std::string> output;
    std::vector<std::string> include;
    DatabaseOptions database;
    std::string input;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("verbose",
                   make_tags<arg_name<"verbose", "v", arg_help<"enable verbose logs", ArgTags{.flag = true}>>>(
                       &CliOptions::verbose),
                   "count",
                   make_tags<arg_name<"count", "c", arg_help<"count value", ArgTags{.rangeMin = 0, .rangeMax = 100}>>>(
                       &CliOptions::count),
                   "output",
                   make_tags<arg_name<"output", "o", arg_help<"output file", ArgTags{.required = true}>>>(
                       &CliOptions::output),
                   "include",
                   make_tags<arg_name<"include", "I", arg_help<"include path", ArgTags{.repeatable = true}>>>(
                       &CliOptions::include),
                   "database", &CliOptions::database, "input",
                   make_tags<arg_name<"input", "", arg_help<"input file", ArgTags{.positional = true}>>>(
                       &CliOptions::input));
    };
};

TEST(ArgParser, ParseStructWithTagsAndNestedOptions) {
    const char* argv[] = {"demo",     "--output",        "out.txt", "--verbose", "--count=42", "--database.host",
                          "db.local", "--database.port", "5432",    "-I",        "inc/a",      "--include=inc/b",
                          "main.txt"};

    auto result = parser<CliOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->verbose);
    EXPECT_EQ(result->count, 42);
    ASSERT_TRUE(result->output.has_value());
    EXPECT_EQ(*result->output, "out.txt");
    ASSERT_EQ(result->include.size(), 2);
    EXPECT_EQ(result->include[0], "inc/a");
    EXPECT_EQ(result->include[1], "inc/b");
    EXPECT_EQ(result->database.host, "db.local");
    EXPECT_EQ(result->database.port, 5432);
    EXPECT_EQ(result->input, "main.txt");
}

TEST(ArgParser, ShortOptionAndInlineValue) {
    const char* argv[] = {"demo", "-oout.txt", "-v", "-c", "7", "--database.host=db.local"};

    auto result = parser<CliOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->verbose);
    EXPECT_EQ(result->count, 7);
    ASSERT_TRUE(result->output.has_value());
    EXPECT_EQ(*result->output, "out.txt");
    EXPECT_EQ(result->database.host, "db.local");
}

TEST(ArgParser, ErrorCode) {
    const char* argv[] = {"demo", "--database.host", "db.local"};

    auto result = parser<CliOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::MissingRequired));
}

TEST(ArgParser, RangeConstraint) {
    const char* argv[] = {"demo", "--output", "out.txt", "--database.host", "db.local", "--count", "101"};

    auto result = parser<CliOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::InvalidValue));
}

TEST(ArgParser, HelpRequestedAndFormatHelp) {
    const char* argv[] = {"demo", "--help"};

    auto result = parser<CliOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::HelpRequested));

    ArgParserConfig config;
    config.programName = "demo";
    config.description = "Demo command";
    auto help          = format_help<CliOptions>(config);
    EXPECT_NE(help.find("--database.host"), std::string::npos);
    EXPECT_NE(help.find("--output <value> (required)"), std::string::npos);
}

struct BuildCommand {
    int jobs       = 1;
    bool release   = false;
    BuildMode mode = BuildMode::Debug;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("jobs",
                   make_tags<arg_name<"jobs", "j", arg_help<"parallel jobs", ArgTags{.rangeMin = 1, .rangeMax = 65}>>>(
                       &BuildCommand::jobs),
                   "release",
                   make_tags<arg_name<"release", "r", arg_help<"release build", ArgTags{.flag = true}>>>(
                       &BuildCommand::release),
                   "mode",
                   make_tags<arg_name<"mode", "m", arg_help<"build mode", arg_choices<ArgTags{}, "debug", "release">>>>(
                       &BuildCommand::mode));
    };
};

struct ToolCommands {
    BuildCommand build;
    ArgCommand clean;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("build",
                   make_tags<arg_name<"build", "", arg_help<"build project", ArgTags{.command = true}>>>(
                       &ToolCommands::build),
                   "clean",
                   make_tags<arg_name<"clean", "", arg_help<"clean project", ArgTags{.command = true}>>>(
                       &ToolCommands::clean));
    };
};

TEST(ArgParser, CommandSetReturnsVariant) {
    const char* argv[] = {"tool", "build", "--jobs", "8", "--release", "--mode", "release"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    using ExpectedType = std::expected<std::variant<BuildCommand, ArgCommand>, std::error_code>;
    static_assert(std::is_same_v<decltype(result), ExpectedType>);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(std::holds_alternative<BuildCommand>(*result));
    const auto& build = std::get<BuildCommand>(*result);
    EXPECT_EQ(build.jobs, 8);
    EXPECT_TRUE(build.release);
    EXPECT_EQ(build.mode, BuildMode::Release);
}

TEST(ArgParser, CommandShortValueWinsOverCluster) {
    const char* argv[] = {"tool", "build", "-j8", "-r"};
    ArgParserConfig config;
    config.allowShortCluster = true;

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(std::holds_alternative<BuildCommand>(*result));
    const auto& build = std::get<BuildCommand>(*result);
    EXPECT_EQ(build.jobs, 8);
    EXPECT_TRUE(build.release);
}

TEST(ArgParser, ChoicesConstraint) {
    const char* argv[] = {"tool", "build", "--mode", "profile"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::InvalidValue));
}

TEST(ArgParser, PlaceholderCommand) {
    const char* argv[] = {"tool", "clean"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(std::holds_alternative<ArgCommand>(*result));
}

TEST(ArgParser, CommandContextHelpAndVersion) {
    const char* helpArgv[] = {"tool", "build", "--help"};
    ArgParserConfig config;
    config.programName = "tool";
    config.version     = "1.2.3";

    auto helpResult = parser<ToolCommands>(static_cast<int>(std::size(helpArgv)), helpArgv, config);
    ASSERT_FALSE(helpResult.has_value());
    EXPECT_EQ(helpResult.error(), make_error_code(ArgParserError::HelpRequested));

    auto help = format_help<ToolCommands>(static_cast<int>(std::size(helpArgv)), helpArgv, config);
    NEKO_LOG_INFO("test", "Help: {}", help);
    EXPECT_NE(help.find("Usage: tool build [options]"), std::string::npos);
    EXPECT_NE(help.find("--jobs <value>"), std::string::npos);
    EXPECT_NE(help.find("-V, --version"), std::string::npos);
    EXPECT_EQ(help.find("Commands:"), std::string::npos);

    const char* versionArgv[] = {"tool", "--version"};
    auto versionResult        = parser<ToolCommands>(static_cast<int>(std::size(versionArgv)), versionArgv, config);
    ASSERT_FALSE(versionResult.has_value());
    EXPECT_EQ(versionResult.error(), make_error_code(ArgParserError::VersionRequested));
    EXPECT_EQ(format_version(config), "tool 1.2.3\n");
}

TEST(ArgParser, PlaceholderCommandContextHelp) {
    const char* argv[] = {"tool", "clean", "--help"};
    ArgParserConfig config;
    config.programName = "tool";

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::HelpRequested));

    auto help = format_help<ToolCommands>(static_cast<int>(std::size(argv)), argv, config);
    static_assert(NEKO_NAMESPACE::argparser::detail::is_command_set_v<ToolCommands>,
                  "ToolCommands must be a command set");
    NEKO_LOG_INFO("test", "{}", help);
    EXPECT_NE(help.find("Usage: tool clean"), std::string::npos);
    EXPECT_NE(help.find("clean project"), std::string::npos);
}

struct DefaultOptions {
    int count = 0;
    std::string output;
    std::optional<std::string> profile;
    std::vector<std::string> include;
    BuildMode mode = BuildMode::Debug;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("count",
                   make_tags<arg_name<"count", "c",
                                      arg_default<5, arg_help<"count value", ArgTags{.rangeMin = 1, .rangeMax = 10}>>>>(
                       &DefaultOptions::count),
                   "output",
                   make_tags<arg_name<"output", "o",
                                      arg_default<"build"_cs, arg_help<"output directory">>>>(
                       &DefaultOptions::output),
                   "profile",
                   make_tags<arg_name<"profile", "p",
                                      arg_default<"dev"_cs, arg_help<"profile name">>>>(
                       &DefaultOptions::profile),
                   "include",
                   make_tags<arg_name<"include", "I",
                                      arg_default<"include"_cs, arg_help<"include path">>>>(
                       &DefaultOptions::include),
                   "mode",
                   make_tags<arg_name<"mode", "m",
                                      arg_default<"release"_cs,
                                                  arg_help<"build mode",
                                                           arg_choices<ArgTags{}, "debug", "release">>>>>(
                       &DefaultOptions::mode));
    };
};

TEST(ArgParser, DefaultValuesAreAppliedWhenOptionIsMissing) {
    const char* argv[] = {"demo"};

    auto result = parser<DefaultOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    NEKO_LOG_INFO("test", "defaults: count={}, output={}, profile={}, includeSize={}, mode={}", result->count,
                  result->output, result->profile.value_or("<none>"), result->include.size(),
                  result->mode == BuildMode::Release ? "release" : "debug");
    EXPECT_EQ(result->count, 5);
    EXPECT_EQ(result->output, "build");
    ASSERT_TRUE(result->profile.has_value());
    EXPECT_EQ(*result->profile, "dev");
    ASSERT_EQ(result->include.size(), 1);
    EXPECT_EQ(result->include[0], "include");
    EXPECT_EQ(result->mode, BuildMode::Release);
}

TEST(ArgParser, ExplicitValuesOverrideDefaults) {
    const char* argv[] = {"demo", "--count", "7", "--output", "dist", "--profile", "prod", "-I", "custom",
                          "--mode", "debug"};

    auto result = parser<DefaultOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    NEKO_LOG_INFO("test", "explicit: count={}, output={}, profile={}, includeSize={}, mode={}", result->count,
                  result->output, result->profile.value_or("<none>"), result->include.size(),
                  result->mode == BuildMode::Release ? "release" : "debug");
    EXPECT_EQ(result->count, 7);
    EXPECT_EQ(result->output, "dist");
    ASSERT_TRUE(result->profile.has_value());
    EXPECT_EQ(*result->profile, "prod");
    ASSERT_EQ(result->include.size(), 1);
    EXPECT_EQ(result->include[0], "custom");
    EXPECT_EQ(result->mode, BuildMode::Debug);
}

TEST(ArgParser, HelpShowsDefaultValues) {
    ArgParserConfig config;
    config.programName = "demo";

    auto help = format_help<DefaultOptions>(config);

    NEKO_LOG_INFO("test", "Default help: {}", help);
    EXPECT_NE(help.find("--count <value> (range: [1, 10)) (default: 5)"), std::string::npos);
    EXPECT_NE(help.find("--output <value> (default: build)"), std::string::npos);
    EXPECT_NE(help.find("--mode <value> (default: release) (choices: {debug, release})"), std::string::npos);
}

struct AdvancedTagOptions {
    int port = 0;
    std::optional<std::string> token;
    std::vector<std::string> include;
    std::vector<int> levels;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("port",
                   make_tags<arg_name<"port", "p",
                                      arg_value_name<"PORT",
                                                     arg_env<"NEKO_ARGPARSER_TEST_PORT",
                                                             arg_default<8080,
                                                                         arg_help<"listen port",
                                                                                  ArgTags{.rangeMin = 1,
                                                                                          .rangeMax = 65536}>>>>>>(
                       &AdvancedTagOptions::port),
                   "token",
                   make_tags<arg_name<"token", "",
                                      arg_env<"NEKO_ARGPARSER_TEST_TOKEN",
                                              arg_value_name<"TOKEN", arg_help<"api token">>>>>(
                       &AdvancedTagOptions::token),
                   "include",
                   make_tags<arg_name<"include", "I",
                                      arg_separator<',', arg_value_name<"PATHS", arg_help<"include paths">>>>>(
                       &AdvancedTagOptions::include),
                   "levels",
                   make_tags<arg_name<"level", "",
                                      arg_separator<';',
                                                    arg_value_name<"LEVELS",
                                                                   arg_help<"numeric levels",
                                                                            ArgTags{.rangeMin = 0, .rangeMax = 10}>>>>>(
                       &AdvancedTagOptions::levels));
    };
};

TEST(ArgParser, EnvValuesAreUsedBeforeDefaultsAndAfterCli) {
    set_test_env("NEKO_ARGPARSER_TEST_PORT", nullptr);
    set_test_env("NEKO_ARGPARSER_TEST_TOKEN", nullptr);

    const char* defaultArgv[] = {"demo"};
    auto defaultResult        = parser<AdvancedTagOptions>(static_cast<int>(std::size(defaultArgv)), defaultArgv);
    ASSERT_TRUE(defaultResult.has_value()) << defaultResult.error().message();
    EXPECT_EQ(defaultResult->port, 8080);
    EXPECT_FALSE(defaultResult->token.has_value());

    set_test_env("NEKO_ARGPARSER_TEST_PORT", "9090");
    set_test_env("NEKO_ARGPARSER_TEST_TOKEN", "secret");

    const char* envArgv[] = {"demo"};
    auto envResult        = parser<AdvancedTagOptions>(static_cast<int>(std::size(envArgv)), envArgv);
    ASSERT_TRUE(envResult.has_value()) << envResult.error().message();
    NEKO_LOG_INFO("test", "env: port={}, token={}", envResult->port, envResult->token.value_or("<none>"));
    EXPECT_EQ(envResult->port, 9090);
    ASSERT_TRUE(envResult->token.has_value());
    EXPECT_EQ(*envResult->token, "secret");

    const char* cliArgv[] = {"demo", "--port", "7070", "--token", "cli"};
    auto cliResult        = parser<AdvancedTagOptions>(static_cast<int>(std::size(cliArgv)), cliArgv);
    ASSERT_TRUE(cliResult.has_value()) << cliResult.error().message();
    EXPECT_EQ(cliResult->port, 7070);
    ASSERT_TRUE(cliResult->token.has_value());
    EXPECT_EQ(*cliResult->token, "cli");

    set_test_env("NEKO_ARGPARSER_TEST_PORT", nullptr);
    set_test_env("NEKO_ARGPARSER_TEST_TOKEN", nullptr);
}

TEST(ArgParser, SeparatorSplitsVectorValues) {
    const char* argv[] = {"demo", "--include", "core,extra", "-I", "third", "--level", "1;2;3"};

    auto result = parser<AdvancedTagOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_EQ(result->include.size(), 3);
    EXPECT_EQ(result->include[0], "core");
    EXPECT_EQ(result->include[1], "extra");
    EXPECT_EQ(result->include[2], "third");
    ASSERT_EQ(result->levels.size(), 3);
    EXPECT_EQ(result->levels[0], 1);
    EXPECT_EQ(result->levels[1], 2);
    EXPECT_EQ(result->levels[2], 3);
}

TEST(ArgParser, ValueNameAndEnvAreShownInHelp) {
    ArgParserConfig config;
    config.programName = "demo";

    auto help = format_help<AdvancedTagOptions>(config);

    NEKO_LOG_INFO("test", "Advanced tag help: {}", help);
    EXPECT_NE(help.find("--port <PORT> (range: [1, 65536)) (default: 8080) (env: NEKO_ARGPARSER_TEST_PORT)"),
              std::string::npos);
    EXPECT_NE(help.find("--token <TOKEN> (env: NEKO_ARGPARSER_TEST_TOKEN)"), std::string::npos);
    EXPECT_NE(help.find("--include <PATHS>"), std::string::npos);
    EXPECT_NE(help.find("--level <LEVELS>"), std::string::npos);
}

TEST(ArgParser, UnknownCommand) {
    const char* argv[] = {"tool", "deploy"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::UnknownCommand));
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
