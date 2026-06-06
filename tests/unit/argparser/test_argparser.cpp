#include <gtest/gtest.h>

#include "nekoproto/argparser/argparser.hpp"

#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

NEKO_USE_NAMESPACE

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

struct DatabaseOptions {
    std::string host = "localhost";
    int port         = 0;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("host", make_tags<ArgTags<"host", "", "database host">{.required = true}>(&DatabaseOptions::host),
                   "port", make_tags<ArgTags<"port", "", "database port">{}>(&DatabaseOptions::port));
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
            Object("verbose", make_tags<ArgTags<"verbose", "v", "enable verbose logs">{.flag = true}>(
                                  &CliOptions::verbose),
                   "count", make_tags<ArgTags<"count", "c", "count value">{.rangeMin = 0, .rangeMax = 100}>(
                                &CliOptions::count),
                   "output",
                   make_tags<ArgTags<"output", "o", "output file">{.required = true}>(&CliOptions::output),
                   "include", make_tags<ArgTags<"include", "I", "include path">{.repeatable = true}>(
                                  &CliOptions::include),
                   "database", &CliOptions::database, "input",
                   make_tags<ArgTags<"input", "", "input file">{.positional = true}>(&CliOptions::input));
    };
};

TEST(ArgParser, ParseStructWithTagsAndNestedOptions) {
    const char* argv[] = {"demo",          "--output",        "out.txt", "--verbose", "--count=42",
                          "--database.host", "db.local",      "--database.port", "5432",
                          "-I",            "inc/a",           "--include=inc/b", "main.txt"};

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
    int jobs     = 1;
    bool release = false;
    BuildMode mode = BuildMode::Debug;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("jobs", make_tags<ArgTags<"jobs", "j", "parallel jobs">{.rangeMin = 1, .rangeMax = 65}>(
                               &BuildCommand::jobs),
                   "release",
                   make_tags<ArgTags<"release", "r", "release build">{.flag = true}>(&BuildCommand::release),
                   "mode", make_tags<ArgTags<"mode", "m", "build mode", "debug", "release">{}>(
                               &BuildCommand::mode));
    };
};

struct ToolCommands {
    BuildCommand build;
    ArgCommand<"clean"> clean;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("build", make_tags<ArgTags<"build", "", "build project">{.command = true}>(&ToolCommands::build),
                   "clean", make_tags<ArgTags<"clean", "", "clean project">{.command = true}>(&ToolCommands::clean));
    };
};

TEST(ArgParser, CommandSetReturnsVariant) {
    const char* argv[] = {"tool", "build", "--jobs", "8", "--release", "--mode", "release"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    using ExpectedType = std::expected<std::variant<BuildCommand, ArgCommand<"clean">>, std::error_code>;
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
    EXPECT_TRUE(std::holds_alternative<ArgCommand<"clean">>(*result));
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
    EXPECT_NE(help.find("Usage: tool build [options]"), std::string::npos);
    EXPECT_NE(help.find("--jobs <value>"), std::string::npos);
    EXPECT_NE(help.find("-V, --version"), std::string::npos);
    EXPECT_EQ(help.find("Commands:"), std::string::npos);

    const char* versionArgv[] = {"tool", "--version"};
    auto versionResult = parser<ToolCommands>(static_cast<int>(std::size(versionArgv)), versionArgv, config);
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
    EXPECT_NE(help.find("Usage: tool clean"), std::string::npos);
    EXPECT_NE(help.find("clean project"), std::string::npos);
}

TEST(ArgParser, UnknownCommand) {
    const char* argv[] = {"tool", "deploy"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::UnknownCommand));
}
