#include <gtest/gtest.h>

#include "nekoproto/argparser/argparser.hpp"
#include "nekoproto/argparser/tags.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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

template <>
struct NEKO_NAMESPACE::Meta<::BuildMode, void> {
    constexpr static auto value = Enumerate("debug", ::BuildMode::Debug, "release", ::BuildMode::Release);
};

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
// clang-format off
struct DatabaseOptions {
    std::string host = "localhost";
    int port         = 0;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("host",
                   make_tags<arg_help<"database host">, 
                   ArgTags{.required = true}>(&DatabaseOptions::host),

                   "port", 
                   make_tags<arg_help<"database port">>(&DatabaseOptions::port));
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
                   make_tags<arg_short_name<'v'>, 
                            arg_help<"enable verbose logs">, 
                            ArgTags{.flag = true}>(&CliOptions::verbose),

                   "count",
                   make_tags<arg_short_name<'c'>, 
                            arg_help<"count value">, 
                            ArgTags{.range_min = 0, .range_max = 100}>(&CliOptions::count),

                   "output",
                   make_tags<arg_short_name<'o'>, 
                            arg_help<"output file">, 
                            ArgTags{.required = true}>(&CliOptions::output),

                   "include",
                   make_tags<arg_short_name<'I'>, 
                            arg_help<"include path">, 
                            ArgTags{.repeatable = true}>(&CliOptions::include),

                   "database", &CliOptions::database, 
                   
                   "input",
                   make_tags<arg_help<"input file">, 
                   ArgTags{.positional = true}>(&CliOptions::input));
    };
};
// clang-format off
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

TEST(ArgParser, ErrorMessagesIncludeArgumentContext) {
    const char* missingArgv[] = {"demo", "--database.host", "db.local"};
    auto missing              = parser<CliOptions>(static_cast<int>(std::size(missingArgv)), missingArgv);
    ASSERT_FALSE(missing.has_value());
    auto message = last_error().message;
    EXPECT_NE(message.find("--output"), std::string::npos);

    const char* rangeArgv[] = {"demo", "--output", "out.txt", "--database.host", "db.local", "--count", "101"};
    auto range              = parser<CliOptions>(static_cast<int>(std::size(rangeArgv)), rangeArgv);
    ASSERT_FALSE(range.has_value());
    message = last_error().message;
    EXPECT_NE(message.find("--count"), std::string::npos);
    EXPECT_NE(message.find("101"), std::string::npos);

    const char* unknownArgv[] = {"demo", "--does-not-exist"};
    auto unknown              = parser<CliOptions>(static_cast<int>(std::size(unknownArgv)), unknownArgv);
    ASSERT_FALSE(unknown.has_value());
    message = last_error().message;
    EXPECT_NE(message.find("--does-not-exist"), std::string::npos);
}

TEST(ArgParser, LastErrorOwnsContextWhileErrorCodeMessageStaysStable) {
    const char* missingArgv[] = {"demo", "--database.host", "db.local"};
    auto missing              = parser<CliOptions>(static_cast<int>(std::size(missingArgv)), missingArgv);

    ASSERT_FALSE(missing.has_value());
    const auto first_diagnostic = last_error();
    EXPECT_EQ(make_error_code(first_diagnostic.error), make_error_code(ArgParserError::MissingRequired));
    EXPECT_NE(first_diagnostic.message.find("--output"), std::string::npos);
    EXPECT_EQ(missing.error().message(), "missing required option");

    const char* unknownArgv[] = {"demo", "--does-not-exist"};
    auto unknown              = parser<CliOptions>(static_cast<int>(std::size(unknownArgv)), unknownArgv);
    ASSERT_FALSE(unknown.has_value());

    EXPECT_NE(first_diagnostic.message.find("--output"), std::string::npos);
    EXPECT_NE(last_error().message.find("--does-not-exist"), std::string::npos);
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
    NEKO_LOG_INFO("test", "Help: \n{}", help);
    EXPECT_NE(help.find("--database.host"), std::string::npos);
    EXPECT_NE(help.find("--output <value> (required)"), std::string::npos);
}

struct UsagePositionalOptions {
    std::string input;
    std::optional<std::string> output;
    std::vector<std::string> extras;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("input", make_tags<arg_value_name<"INPUT">, ArgTags{.required = true, .positional = true}>(
                                &UsagePositionalOptions::input),
                   "output", make_tags<arg_value_name<"OUTPUT">, ArgTags{.positional = true}>(
                                 &UsagePositionalOptions::output),
                   "extras", make_tags<arg_value_name<"EXTRA">, ArgTags{.positional = true}>(
                                 &UsagePositionalOptions::extras));
    };
};

TEST(ArgParser, HelpSeparatesPositionalsAndAddsThemToUsage) {
    ArgParserConfig config;
    config.programName = "demo";

    const auto help = format_help<UsagePositionalOptions>(config);

    EXPECT_NE(help.find("Usage: demo [options] <INPUT> [<OUTPUT>] [<EXTRA>...]"), std::string::npos);
    const auto options = help.find("Options:\n");
    const auto arguments = help.find("Arguments:\n");
    ASSERT_NE(options, std::string::npos);
    ASSERT_NE(arguments, std::string::npos);
    EXPECT_EQ(help.substr(options, arguments - options).find("INPUT"), std::string::npos);
    EXPECT_NE(help.find("Arguments:\n  INPUT (required)"), std::string::npos);
    EXPECT_NE(help.find("  OUTPUT"), std::string::npos);
    EXPECT_NE(help.find("  EXTRA... (repeatable)"), std::string::npos);
}
// clang-format off
struct BuildCommand {
    int jobs       = 1;
    bool release   = false;
    BuildMode mode = BuildMode::Debug;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("jobs",
                   make_tags<arg_short_name<'j'>, 
                            arg_help<"parallel jobs">, 
                            ArgTags{.range_min = 1, .range_max = 65}>(&BuildCommand::jobs),

                   "release",
                   make_tags<arg_short_name<'r'>, 
                            arg_help<"release build">, 
                            ArgTags{.flag = true}>(&BuildCommand::release),

                   "mode",
                   make_tags<arg_short_name<'m'>, 
                            arg_help<"build mode">, 
                            arg_choices<"debug", "release">>(&BuildCommand::mode));
    };
};

struct ToolCommands {
    BuildCommand build;
    ArgCommand<1> clean;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("build",
                   make_tags<arg_help<"build project">, 
                   ArgTags{.command = true}>(&ToolCommands::build),

                   "clean",
                   make_tags<arg_help<"clean project">, 
                            ArgTags{.command = true}>(&ToolCommands::clean));
    };
};

struct ToolCommandsWithIgnoredField {
    BuildCommand build;
    int ignored = 7;
    ArgCommand<2> clean;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("build",
                   make_tags<arg_help<"build project">,
                             ArgTags{.command = true}>(&ToolCommandsWithIgnoredField::build),

                   "ignored",
                   make_tags<arg_ignore_tag, arg_help<"internal root state">>(
                       &ToolCommandsWithIgnoredField::ignored),

                   "clean",
                   make_tags<arg_help<"clean project">,
                             ArgTags{.command = true}>(&ToolCommandsWithIgnoredField::clean));
    };
};
// clang-format on
TEST(ArgParser, CommandSetReturnsVariant) {
    const char* argv[] = {"tool", "build", "--jobs", "8", "--release", "--mode", "release"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    using ExpectedType = expected::expected<std::variant<BuildCommand, ArgCommand<1>>, std::error_code>;
    static_assert(std::is_same_v<decltype(result), ExpectedType>);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(std::holds_alternative<BuildCommand>(*result));
    const auto& build = std::get<BuildCommand>(*result);
    EXPECT_EQ(build.jobs, 8);
    EXPECT_TRUE(build.release);
    EXPECT_EQ(build.mode, BuildMode::Release);
}

TEST(ArgParser, IgnoredNormalFieldsDoNotBreakCommandSetDetection) {
    static_assert(NEKO_NAMESPACE::argparser::detail::is_command_set_v<ToolCommandsWithIgnoredField>);

    const char* argv[] = {"tool", "build", "--jobs", "3"};

    auto result = parser<ToolCommandsWithIgnoredField>(static_cast<int>(std::size(argv)), argv);

    using ExpectedType = expected::expected<std::variant<BuildCommand, ArgCommand<2>>, std::error_code>;
    static_assert(std::is_same_v<decltype(result), ExpectedType>);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(std::holds_alternative<BuildCommand>(*result));
    EXPECT_EQ(std::get<BuildCommand>(*result).jobs, 3);

    auto help = format_help<ToolCommandsWithIgnoredField>();
    EXPECT_EQ(help.find("internal root state"), std::string::npos);
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

TEST(ArgParser, ShortClusterAllowsTheLastValueOptionToConsumeTheSuffix) {
    const char* argv[] = {"tool", "build", "-rj8"};
    ArgParserConfig config;
    config.allowShortCluster = true;

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << last_error().message;
    const auto& build = std::get<BuildCommand>(*result);
    EXPECT_TRUE(build.release);
    EXPECT_EQ(build.jobs, 8);
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
    EXPECT_TRUE(std::holds_alternative<ArgCommand<1>>(*result));
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
    NEKO_LOG_INFO("test", "Help: \n{}", help);
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
    NEKO_LOG_INFO("test", "Help: \n{}", help);
    EXPECT_NE(help.find("Usage: tool clean"), std::string::npos);
    EXPECT_NE(help.find("clean project"), std::string::npos);
}
// clang-format off
struct DefaultOptions {
    int count = 0;
    std::string output;
    std::optional<std::string> profile;
    std::vector<std::string> include;
    BuildMode mode = BuildMode::Debug;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object(
                "count", make_tags<arg_short_name<'c'>, 
                                arg_default<5>, 
                                arg_help<"count value">,
                                ArgTags{.range_min = 1, .range_max = 10}>(&DefaultOptions::count),

                "output",
                make_tags<arg_short_name<'o'>, 
                        arg_default<"build"_cs>, 
                        arg_help<"output directory">>(&DefaultOptions::output),

                "profile",
                make_tags<arg_short_name<'p'>, 
                        arg_default<"dev"_cs>, 
                        arg_help<"profile name">>(&DefaultOptions::profile),

                "include",
                make_tags<arg_short_name<'I'>, 
                        arg_default<"include"_cs>, 
                        arg_help<"include path">>(&DefaultOptions::include),

                "mode",
                make_tags<arg_short_name<'m'>, 
                        arg_default<"release"_cs>, 
                        arg_help<"build mode">,
                        arg_choices<"debug", "release">>(&DefaultOptions::mode));
    };
};
// clang-format on
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
    const char* argv[] = {"demo", "--count", "7",      "--output", "dist", "--profile",
                          "prod", "-I",      "custom", "--mode",   "debug"};

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

// clang-format off
struct ParallelTagOptions {
    int count = 0;
    std::string output;
    bool verbose = false;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("count",
                   make_tags<arg_short_name<'c'>,
                            arg_help<"count value">,
                            ArgTags{.required = true, .range_min = 2, .range_max = 5}>(&ParallelTagOptions::count),

                   "output",
                   make_tags<arg_short_name<'o'>, 
                            arg_help<"output directory">, 
                            arg_default<"dist"_cs>>(&ParallelTagOptions::output),

                   "verbose",
                   make_tags<arg_short_name<'v'>, 
                            arg_help<"enable verbose output">, 
                            ArgTags{.flag = true}>(&ParallelTagOptions::verbose));
    };
};
// clang-format on
TEST(ArgParser, ParallelTagsComposeArgMetadata) {
    const char* argv[] = {"demo", "--count", "3", "-v"};

    auto result = parser<ParallelTagOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->count, 3);
    EXPECT_EQ(result->output, "dist");
    EXPECT_TRUE(result->verbose);

    ArgParserConfig config;
    config.programName = "demo";
    auto help          = format_help<ParallelTagOptions>(config);

    EXPECT_NE(help.find("-c, --count <value> (required) (range: [2, 5))"), std::string::npos);
    EXPECT_NE(help.find("-o, --output <value> (default: dist)"), std::string::npos);
    EXPECT_NE(help.find("-v, --verbose"), std::string::npos);
}

struct ArgIgnoredNestedOptions {
    int visible = 0;
    int ignored = 7;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("visible", &ArgIgnoredNestedOptions::visible, "ignored",
                   make_tags<arg_ignore_tag, arg_help<"ignored nested value">>(&ArgIgnoredNestedOptions::ignored));
    };
};

struct ArgIgnoredOptions {
    bool verbose        = false;
    std::string ignored = "keep";
    ArgIgnoredNestedOptions nested;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("verbose", make_tags<arg_help<"visible flag">, ArgTags{.flag = true}>(&ArgIgnoredOptions::verbose),
                   "ignored", make_tags<arg_ignore_tag, arg_help<"ignored root value">>(&ArgIgnoredOptions::ignored),
                   "nested", &ArgIgnoredOptions::nested);
    };
};

TEST(ArgParser, IgnoreTagRemovesFieldsFromSchemaHelpAndMaterialization) {
    constexpr auto ignoredSpec = make_tags<arg_ignore_tag>(&ArgIgnoredOptions::ignored);
    static_assert(tag_query::get<argparser::tag_property::ignore>(field_tags_v<decltype(ignoredSpec)>));

    const char* argv[] = {"demo", "--verbose", "--nested.visible", "42"};
    auto result        = parser<ArgIgnoredOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->verbose);
    EXPECT_EQ(result->ignored, "keep");
    EXPECT_EQ(result->nested.visible, 42);
    EXPECT_EQ(result->nested.ignored, 7);

    ArgParserConfig config;
    config.programName = "demo";
    auto help          = format_help<ArgIgnoredOptions>(config);
    EXPECT_NE(help.find("--verbose"), std::string::npos);
    EXPECT_NE(help.find("--nested.visible"), std::string::npos);
    EXPECT_EQ(help.find("--ignored"), std::string::npos);
    EXPECT_EQ(help.find("--nested.ignored"), std::string::npos);
    EXPECT_EQ(help.find("ignored root value"), std::string::npos);
    EXPECT_EQ(help.find("ignored nested value"), std::string::npos);

    const char* ignoredArgv[] = {"demo", "--ignored", "changed"};
    auto ignoredResult        = parser<ArgIgnoredOptions>(static_cast<int>(std::size(ignoredArgv)), ignoredArgv);
    ASSERT_FALSE(ignoredResult.has_value());
    EXPECT_EQ(ignoredResult.error(), make_error_code(ArgParserError::UnknownOption));
}

struct IgnoredUnsupportedField {
    std::string value;
};

struct NonIntrusiveIgnoredUnsupportedOptions {
    bool enabled = false;
    std::vector<IgnoredUnsupportedField> ignoredFields;
};

template <>
struct NEKO_NAMESPACE::Meta<::NonIntrusiveIgnoredUnsupportedOptions, void> {
    constexpr static auto value = // NOLINT
        Object("enabled",
               make_tags<arg_long_name<"enabled">, arg_help<"visible flag">, ArgTags{.flag = true}>(
                   &::NonIntrusiveIgnoredUnsupportedOptions::enabled),
               "ignoredFields",
               make_tags<arg_ignore_tag, arg_help<"ignored unsupported field">>(
                   &::NonIntrusiveIgnoredUnsupportedOptions::ignoredFields));
};

TEST(ArgParser, IgnoreTagSkipsUnsupportedFieldTypes) {
    const char* argv[] = {"demo", "--enabled"};
    auto result        = parser<NonIntrusiveIgnoredUnsupportedOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->enabled);
    EXPECT_TRUE(result->ignoredFields.empty());

    ArgParserConfig config;
    config.programName = "demo";
    auto help          = format_help<NonIntrusiveIgnoredUnsupportedOptions>(config);
    EXPECT_NE(help.find("--enabled"), std::string::npos);
    EXPECT_EQ(help.find("--ignoredFields"), std::string::npos);
    EXPECT_EQ(help.find("ignored unsupported field"), std::string::npos);
}

struct TagListComposeOptions {
    bool enabled = false;
};

TEST(ArgParser, TagListsFlattenAndLaterTagsOverrideEarlierOnes) {
    constexpr auto base_tag_pack = TagList<arg_name<"old", 'o'>, arg_help<"old help">>{};
    constexpr auto nested_tags   = TagList<base_tag_pack, TagList<arg_name<"new", 'n'>, arg_help<"new help">>{}>{};
    constexpr auto spec          = make_tags<nested_tags, ArgTags{.flag = true}>(&TagListComposeOptions::enabled);
    constexpr auto tags          = field_tags_v<decltype(spec)>;

    static_assert(std::tuple_size_v<decltype(tags)> == 5);
    static_assert(std::get<0>(tags).long_name == "old");
    static_assert(tags.template get<2>().long_name == "new");
    static_assert(std::get<4>(tags).flag);
    static_assert(tag_query::get<argparser::tag_property::long_name>(tags) == "new");
    static_assert(tag_query::get<argparser::tag_property::short_name>(tags) == 'n');
    static_assert(tag_query::get<argparser::tag_property::help>(tags) == "new help");
    static_assert(tag_query::get<argparser::tag_property::flag>(tags));
    static_assert(tag_query::has_tag<argparser::detail::arg_name_impl<"new", 'n'>>(tags));
    static_assert(tag_query::get_tag<argparser::detail::arg_name_impl<"new", 'n'>>(tags).long_name == "new");
}

struct AdvancedTagOptions {
    int port = 0;
    std::optional<std::string> token;
    std::vector<std::string> include;
    std::vector<int> levels;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("port",
                   make_tags<arg_name<"port", 'p'>, arg_value_name<"PORT">, arg_env<"NEKO_ARGPARSER_TEST_PORT">,
                             arg_default<8080>, arg_help<"listen port">, ArgTags{.range_min = 1, .range_max = 65536}>(
                       &AdvancedTagOptions::port),
                   "token",
                   make_tags<arg_env<"NEKO_ARGPARSER_TEST_TOKEN">, arg_value_name<"TOKEN">, arg_help<"api token">>(
                       &AdvancedTagOptions::token),
                   "include",
                   make_tags<arg_name<"include", 'I'>, arg_separator<','>, arg_value_name<"PATHS">,
                             arg_help<"include paths">>(&AdvancedTagOptions::include),
                   "levels",
                   make_tags<arg_long_name<"level">, arg_separator<';'>, arg_value_name<"LEVELS">,
                             arg_help<"numeric levels">, ArgTags{.range_min = 0, .range_max = 10}>(
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

struct AliasImplicitGroupOptions {
    std::string color;
    std::string output;
    bool verbose = false;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("color",
                   make_tags<arg_short_name<'c'>, arg_aliases<"C", "colour">, arg_group<"Display">,
                             arg_implicit<"auto"_cs>, arg_help<"color mode">>(&AliasImplicitGroupOptions::color),

                   "output",
                   make_tags<arg_short_name<'o'>, arg_group<"Paths">, arg_value_name<"PATH">, arg_help<"output file">>(
                       &AliasImplicitGroupOptions::output),

                   "verbose",
                   make_tags<arg_short_name<'v'>, arg_group<"General">, arg_help<"enable verbose output">,
                             ArgTags{.flag = true}>(&AliasImplicitGroupOptions::verbose));
    };
};

TEST(ArgParser, AliasesAreAcceptedForLongAndShortOptions) {
    const char* argv[] = {"demo", "--colour", "always", "-o", "out.txt", "-v"};

    auto result = parser<AliasImplicitGroupOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->color, "always");
    EXPECT_EQ(result->output, "out.txt");
    EXPECT_TRUE(result->verbose);
}

TEST(ArgParser, ImplicitValueIsUsedWhenNextTokenLooksLikeAnotherOption) {
    const char* argv[] = {"demo", "-C", "--output", "out.txt"};

    auto result = parser<AliasImplicitGroupOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->color, "auto");
    EXPECT_EQ(result->output, "out.txt");
}

TEST(ArgParser, HelpShowsAliasesImplicitValuesAndGroups) {
    ArgParserConfig config;
    config.programName = "demo";

    auto help = format_help<AliasImplicitGroupOptions>(config);

    NEKO_LOG_INFO("test", "Alias/implicit/group help: \n{}", help);
    EXPECT_NE(help.find("Options:\n  -h, --help\n"), std::string::npos);
    EXPECT_NE(help.find("Display:\n  -c, -C, --color, --colour <value> (implicit: auto)"), std::string::npos);
    EXPECT_NE(help.find("Paths:\n  -o, --output <PATH>"), std::string::npos);
    EXPECT_NE(help.find("General:\n  -v, --verbose"), std::string::npos);
}

struct RelationshipOptions {
    bool login = false;
    std::string token;
    bool json = false;
    bool yaml = false;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("login",
                   make_tags<arg_requires<"token">, arg_help<"enable login">, ArgTags{.flag = true}>(
                       &RelationshipOptions::login),

                   "token", make_tags<arg_help<"login token">>(&RelationshipOptions::token),

                   "json",
                   make_tags<arg_conflicts<"yaml">, arg_help<"json output">, ArgTags{.flag = true}>(
                       &RelationshipOptions::json),

                   "yaml", make_tags<arg_help<"yaml output">, ArgTags{.flag = true}>(&RelationshipOptions::yaml));
    };
};

TEST(ArgParser, RequiresValidationRunsAfterParsing) {
    const char* missingArgv[] = {"demo", "--login"};

    auto missing = parser<RelationshipOptions>(static_cast<int>(std::size(missingArgv)), missingArgv);

    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), make_error_code(ArgParserError::MissingRequired));

    const char* okArgv[] = {"demo", "--login", "--token", "secret"};
    auto ok              = parser<RelationshipOptions>(static_cast<int>(std::size(okArgv)), okArgv);

    ASSERT_TRUE(ok.has_value()) << ok.error().message();
    EXPECT_TRUE(ok->login);
    EXPECT_EQ(ok->token, "secret");
}

TEST(ArgParser, ConflictsValidationRunsAfterParsing) {
    const char* argv[] = {"demo", "--json", "--yaml"};

    auto result = parser<RelationshipOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::InvalidValue));
}

TEST(ArgParser, FalseBooleanValuesDoNotActivateRelationships) {
    const char* argv[] = {"demo", "--json=false", "--yaml"};

    auto result = parser<RelationshipOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << last_error().message;
    EXPECT_FALSE(result->json);
    EXPECT_TRUE(result->yaml);
}

struct ScopedRelationshipOptions {
    bool login   = false;
    bool publish = false;
    std::string token;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object(
                "login", make_tags<arg_requires<"./token">, ArgTags{.flag = true}>(&ScopedRelationshipOptions::login),
                "publish",
                make_tags<arg_requires<"../global-token">, ArgTags{.flag = true}>(&ScopedRelationshipOptions::publish),
                "token", &ScopedRelationshipOptions::token);
    };
};

struct ScopedRelationshipRoot {
    ScopedRelationshipOptions auth;
    std::string globalToken;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("auth", &ScopedRelationshipRoot::auth, "globalToken",
                   make_tags<arg_long_name<"global-token">>(&ScopedRelationshipRoot::globalToken));
    };
};

struct AbsoluteNameOptions {
    struct Network {
        int port = 0;

        struct Neko {
            constexpr static auto value = // NOLINT
                Object("port", make_tags<arg_absolute_name<"listen-port">>(&Network::port));
        };
    };

    Network network;

    struct Neko {
        constexpr static auto value = Object("network", &AbsoluteNameOptions::network); // NOLINT
    };
};

struct DuplicateNameOptions {
    bool first  = false;
    bool second = false;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("first", make_tags<arg_long_name<"same">, ArgTags{.flag = true}>(&DuplicateNameOptions::first),
                   "second", make_tags<arg_long_name<"same">, ArgTags{.flag = true}>(&DuplicateNameOptions::second));
    };
};

struct NonRepeatableOptions {
    int count = 0;

    struct Neko {
        constexpr static auto value = Object("count", &NonRepeatableOptions::count); // NOLINT
    };
};

struct InvalidPositionalOrderOptions {
    std::vector<std::string> rest;
    std::string output;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("rest", make_tags<ArgTags{.positional = true}>(&InvalidPositionalOrderOptions::rest), "output",
                   make_tags<ArgTags{.positional = true}>(&InvalidPositionalOrderOptions::output));
    };
};

TEST(ArgParser, NestedReferencesAndAbsoluteNamesAreResolvedDeterministically) {
    const auto relationshipHelp = format_help<ScopedRelationshipRoot>();
    EXPECT_NE(relationshipHelp.find("--auth.login (requires: --auth.token)"), std::string::npos);
    EXPECT_NE(relationshipHelp.find("--auth.publish (requires: --global-token)"), std::string::npos);
    EXPECT_EQ(relationshipHelp.find("requires: ./token"), std::string::npos);
    EXPECT_EQ(relationshipHelp.find("requires: ../global-token"), std::string::npos);

    const char* missingArgv[] = {"demo", "--auth.login"};
    auto missing              = parser<ScopedRelationshipRoot>(static_cast<int>(std::size(missingArgv)), missingArgv);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), make_error_code(ArgParserError::MissingRequired));
    EXPECT_NE(last_error().message.find("--auth.token"), std::string::npos);

    const char* argv[] = {"demo",           "--auth.login",   "--auth.token", "secret",
                          "--auth.publish", "--global-token", "release"};
    auto result        = parser<ScopedRelationshipRoot>(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value()) << last_error().message;
    EXPECT_TRUE(result->auth.login);
    EXPECT_TRUE(result->auth.publish);
    EXPECT_EQ(result->auth.token, "secret");
    EXPECT_EQ(result->globalToken, "release");

    const char* absoluteArgv[] = {"demo", "--listen-port", "8080"};
    auto absolute              = parser<AbsoluteNameOptions>(static_cast<int>(std::size(absoluteArgv)), absoluteArgv);
    ASSERT_TRUE(absolute.has_value()) << last_error().message;
    EXPECT_EQ(absolute->network.port, 8080);

    auto help = format_help<ScopedRelationshipRoot>();
    NEKO_LOG_INFO("test", "{}", help);
}

TEST(ArgParser, SchemaAndRepeatedValueErrorsAreReportedBeforeMaterialization) {
    const char* duplicateNameArgv[] = {"demo"};
    auto duplicateName =
        parser<DuplicateNameOptions>(static_cast<int>(std::size(duplicateNameArgv)), duplicateNameArgv);
    ASSERT_FALSE(duplicateName.has_value());
    EXPECT_EQ(duplicateName.error(), make_error_code(ArgParserError::InvalidDefinition));

    const char* duplicateValueArgv[] = {"demo", "--count", "1", "--count", "2"};
    auto duplicateValue =
        parser<NonRepeatableOptions>(static_cast<int>(std::size(duplicateValueArgv)), duplicateValueArgv);
    ASSERT_FALSE(duplicateValue.has_value());
    EXPECT_EQ(duplicateValue.error(), make_error_code(ArgParserError::InvalidValue));

    const char* positionalArgv[] = {"demo"};
    auto positional =
        parser<InvalidPositionalOrderOptions>(static_cast<int>(std::size(positionalArgv)), positionalArgv);
    ASSERT_FALSE(positional.has_value());
    EXPECT_EQ(positional.error(), make_error_code(ArgParserError::InvalidDefinition));
}

struct UserHelpOverrideOptions {
    bool help    = false;
    bool version = false;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("help",
                   make_tags<arg_name<"help", 'h'>, arg_help<"application help flag">, ArgTags{.flag = true}>(
                       &UserHelpOverrideOptions::help),
                   "version",
                   make_tags<arg_name<"version", 'V'>, arg_help<"application version flag">, ArgTags{.flag = true}>(
                       &UserHelpOverrideOptions::version));
    };
};

TEST(ArgParser, UserHelpAndVersionOptionsOverrideBuiltins) {
    const char* argv[] = {"demo", "--help", "--version"};
    ArgParserConfig config;
    config.version = "1.0";

    auto result = parser<UserHelpOverrideOptions>(static_cast<int>(std::size(argv)), argv, config);
    ASSERT_TRUE(result.has_value()) << last_error().message;
    EXPECT_TRUE(result->help);
    EXPECT_TRUE(result->version);

    const auto help = format_help<UserHelpOverrideOptions>(config);
    EXPECT_NE(help.find("application help flag"), std::string::npos);
    const auto help_name = help.find("--help");
    ASSERT_NE(help_name, std::string::npos);
    EXPECT_EQ(help.find("--help", help_name + 1U), std::string::npos);
}

struct RelaxedChoiceOptions {
    BuildMode mode = BuildMode::Debug;
    std::string color;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("mode",
                   make_tags<arg_short_name<'m'>, arg_case_insensitive_choices, arg_choices<"debug", "release">>(
                       &RelaxedChoiceOptions::mode),

                   "color",
                   make_tags<arg_case_insensitive_choices, arg_choices<"auto", "never">>(&RelaxedChoiceOptions::color));
    };
};

TEST(ArgParser, CaseInsensitiveChoicesAcceptRelaxedInput) {
    const char* argv[] = {"demo", "--mode", "RELEASE", "--color", "AUTO"};

    auto result = parser<RelaxedChoiceOptions>(static_cast<int>(std::size(argv)), argv);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->mode, BuildMode::Release);
    EXPECT_EQ(result->color, "AUTO");
}

TEST(ArgParser, ChoicesRemainStrictWithoutRelaxedTag) {
    const char* argv[] = {"tool", "build", "--mode", "RELEASE"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::InvalidValue));
}

struct DeprecatedOptions {
    bool legacy = false;
    bool modern = false;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("legacy",
                   make_tags<arg_deprecated<"use --modern instead">, arg_help<"legacy mode">, ArgTags{.flag = true}>(
                       &DeprecatedOptions::legacy),
                   "modern", make_tags<arg_help<"modern mode">, ArgTags{.flag = true}>(&DeprecatedOptions::modern));
    };
};

TEST(ArgParser, DeprecatedOptionIsAcceptedAndReported) {
    const char* argv[] = {"demo", "--legacy"};
    std::vector<std::string> warnings;
    ArgParserConfig config;
    config.deprecatedOptionHandler = [&](std::string_view option_name, std::string_view message) {
        warnings.push_back(std::string(option_name) + ":" + std::string(message));
    };

    auto result = parser<DeprecatedOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->legacy);
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0], "legacy:use --modern instead");

    auto help = format_help<DeprecatedOptions>();
    NEKO_LOG_INFO("test", "Help: \n{}", help);
    EXPECT_NE(help.find("--legacy (deprecated: use --modern instead)"), std::string::npos);
}

TEST(ArgParser, UnknownCommand) {
    const char* argv[] = {"tool", "deploy"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::UnknownCommand));
}

TEST(ArgParser, CommandRootHandlesUnknownOptionsConsistently) {
    const char* strictArgv[] = {"tool", "--unknown", "build"};
    auto strict              = parser<ToolCommands>(static_cast<int>(std::size(strictArgv)), strictArgv);
    ASSERT_FALSE(strict.has_value());
    EXPECT_EQ(strict.error(), make_error_code(ArgParserError::UnknownOption));

    ArgParserConfig config;
    config.allowUnknown = true;
    auto relaxed        = parser<ToolCommands>(static_cast<int>(std::size(strictArgv)), strictArgv, config);
    ASSERT_TRUE(relaxed.has_value()) << last_error().message;
    EXPECT_TRUE(std::holds_alternative<BuildCommand>(*relaxed));
}

TEST(ArgParser, InvalidArgumentVectorIsRejectedWithoutDereferencingNull) {
    auto result = parser<NonRepeatableOptions>(2, static_cast<const char* const*>(nullptr));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::InvalidValue));
}

std::filesystem::path argparser_config_io_path(std::string_view file_name) {
    auto path = std::filesystem::temp_directory_path() / std::string(file_name);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return path;
}

void write_file_bytes(const std::filesystem::path& path, const std::vector<char>& bytes) {
    std::ofstream file{path, std::ios::binary};
    ASSERT_TRUE(file) << path;
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(file) << path;
}

std::vector<char> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    EXPECT_TRUE(file) << path;
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

template <typename Serializer, typename T>
void write_serialized_config(const std::filesystem::path& path, const T& value) {
    std::vector<char> buffer;
    typename Serializer::OutputSerializer output(buffer);
    EXPECT_TRUE(output(value));
    EXPECT_TRUE(output.end());
    write_file_bytes(path, buffer);
}

template <typename Serializer, typename T>
T read_serialized_config(const std::filesystem::path& path) {
    auto buffer = read_file_bytes(path);
    T value{};
    typename Serializer::InputSerializer input(buffer.data(), buffer.size());
    EXPECT_TRUE(input(value));
    return value;
}

// clang-format off
struct ConfigIoOptions {
    int count = 0;
    std::string output;
    std::optional<std::string> token;
    BuildMode mode = BuildMode::Debug;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("count",
                   make_tags<arg_default<1>,
                             arg_help<"count value">,
                             ArgTags{.range_min = 1, .range_max = 10}>(&ConfigIoOptions::count),

                   "output",
                   make_tags<arg_env<"NEKO_ARGPARSER_CONFIG_IO_OUTPUT">,
                             arg_default<"default"_cs>,
                             arg_help<"output value">>(&ConfigIoOptions::output),

                   "token",
                   make_tags<arg_help<"optional token">>(&ConfigIoOptions::token),

                   "mode",
                   make_tags<arg_default<"release"_cs>,
                             arg_choices<"debug", "release">,
                             arg_help<"build mode">>(&ConfigIoOptions::mode));
    };
};
// clang-format on

ArgParserConfig config_io_parser_config() {
    ArgParserConfig config;
    config.configIo.emplace();
    return config;
}

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
TEST(ArgParser, JsonConfigImportUsesExpectedPriorityAndExportsResolvedOptions) {
    set_test_env("NEKO_ARGPARSER_CONFIG_IO_OUTPUT", nullptr);
    auto import_path = argparser_config_io_path("neko_argparser_import.json");
    auto export_path = argparser_config_io_path("neko_argparser_export.json");

    ConfigIoOptions imported;
    imported.count  = 3;
    imported.output = "from-file";
    imported.mode   = BuildMode::Debug;
    write_serialized_config<JsonSerializer>(import_path, imported);

    set_test_env("NEKO_ARGPARSER_CONFIG_IO_OUTPUT", "from-env");

    auto config = config_io_parser_config();
    config.configIo->enableFormat("json");
    const auto import_arg = import_path.string();
    const auto export_arg = export_path.string();
    const char* argv[]    = {"demo", "--import-json", import_arg.c_str(), "--count",
                             "5",    "--export-json", export_arg.c_str()};

    auto result = parser<ConfigIoOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->count, 5);
    EXPECT_EQ(result->output, "from-env");
    EXPECT_FALSE(result->token.has_value());
    EXPECT_EQ(result->mode, BuildMode::Debug);

    auto exported = read_serialized_config<JsonSerializer, ConfigIoOptions>(export_path);
    EXPECT_EQ(exported.count, 5);
    EXPECT_EQ(exported.output, "from-env");
    EXPECT_FALSE(exported.token.has_value());
    EXPECT_EQ(exported.mode, BuildMode::Debug);

    set_test_env("NEKO_ARGPARSER_CONFIG_IO_OUTPUT", nullptr);
}

TEST(ArgParser, JsonConfigImportRejectsMissingNonOptionalFields) {
    auto import_path = argparser_config_io_path("neko_argparser_missing_required_import.json");
    write_file_bytes(import_path, {'{', '}'});

    auto config = config_io_parser_config();
    config.configIo->enableImportFormat("json");
    const auto import_arg = import_path.string();
    const char* argv[]    = {"demo", "--import-json", import_arg.c_str()};

    auto result = parser<ConfigIoOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(ArgParserError::InvalidValue));
    EXPECT_NE(last_error().message.find("Required field 'count' is missing"), std::string::npos);
}

TEST(ArgParser, CommandConfigJsonImportExportsCommandWrapper) {
    auto import_path = argparser_config_io_path("neko_argparser_command_import.json");
    auto export_path = argparser_config_io_path("neko_argparser_command_export.json");

    NEKO_NAMESPACE::argparser::detail::CommandConfig<BuildCommand> imported;
    imported.command        = "build";
    imported.params.jobs    = 4;
    imported.params.release = true;
    imported.params.mode    = BuildMode::Release;
    write_serialized_config<JsonSerializer>(import_path, imported);

    auto config = config_io_parser_config();
    config.configIo->enableFormat("json");
    const auto import_arg = import_path.string();
    const auto export_arg = export_path.string();
    const char* argv[]    = {
        "tool", "--import-json", import_arg.c_str(), "--export-json", export_arg.c_str(), "build", "--jobs", "8"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(std::holds_alternative<BuildCommand>(*result));
    const auto& build = std::get<BuildCommand>(*result);
    EXPECT_EQ(build.jobs, 8);
    EXPECT_TRUE(build.release);
    EXPECT_EQ(build.mode, BuildMode::Release);

    auto exported =
        read_serialized_config<JsonSerializer, NEKO_NAMESPACE::argparser::detail::CommandConfig<BuildCommand>>(
            export_path);
    EXPECT_EQ(exported.command, "build");
    EXPECT_EQ(exported.params.jobs, 8);
    EXPECT_TRUE(exported.params.release);
    EXPECT_EQ(exported.params.mode, BuildMode::Release);

    const char* import_only_argv[] = {"tool", "--import-json", import_arg.c_str()};
    auto import_only = parser<ToolCommands>(static_cast<int>(std::size(import_only_argv)), import_only_argv, config);
    ASSERT_TRUE(import_only.has_value()) << import_only.error().message();
    ASSERT_TRUE(std::holds_alternative<BuildCommand>(*import_only));
    EXPECT_EQ(std::get<BuildCommand>(*import_only).jobs, 4);
}

TEST(ArgParser, PlaceholderCommandJsonExportOmitsParams) {
    auto export_path = argparser_config_io_path("neko_argparser_clean_command_export.json");

    auto config = config_io_parser_config();
    config.configIo->enableExportFormat("json");
    const auto export_arg = export_path.string();
    const char* argv[]    = {"tool", "--export-json", export_arg.c_str(), "clean"};

    auto result = parser<ToolCommands>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(std::holds_alternative<ArgCommand<1>>(*result));

    auto bytes = read_file_bytes(export_path);
    auto text  = std::string(bytes.begin(), bytes.end());
    EXPECT_NE(text.find("command"), std::string::npos);
    EXPECT_NE(text.find("clean"), std::string::npos);
    EXPECT_EQ(text.find("params"), std::string::npos);
}
#endif

#if !defined(NEKO_PROTO_NO_YAML_SERIALIZER)
TEST(ArgParser, YamlConfigImportAndExport) {
    set_test_env("NEKO_ARGPARSER_CONFIG_IO_OUTPUT", nullptr);
    auto import_path = argparser_config_io_path("neko_argparser_import.yaml");
    auto export_path = argparser_config_io_path("neko_argparser_export.yaml");

    ConfigIoOptions imported;
    imported.count  = 6;
    imported.output = "yaml-file";
    imported.token  = "yaml-token";
    imported.mode   = BuildMode::Release;
    write_serialized_config<YamlSerializer>(import_path, imported);

    auto config = config_io_parser_config();
    config.configIo->enableFormat("yaml");
    const auto import_arg = import_path.string();
    const auto export_arg = export_path.string();
    const char* argv[]    = {"demo", "--import-yaml", import_arg.c_str(), "--export-yaml", export_arg.c_str()};

    auto result = parser<ConfigIoOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->count, 6);
    EXPECT_EQ(result->output, "yaml-file");
    ASSERT_TRUE(result->token.has_value());
    EXPECT_EQ(*result->token, "yaml-token");
    EXPECT_EQ(result->mode, BuildMode::Release);

    auto exported = read_serialized_config<YamlSerializer, ConfigIoOptions>(export_path);
    EXPECT_EQ(exported.count, 6);
    EXPECT_EQ(exported.output, "yaml-file");
    ASSERT_TRUE(exported.token.has_value());
    EXPECT_EQ(*exported.token, "yaml-token");
}
#endif

#if !defined(NEKO_PROTO_NO_TOML_SERIALIZER)
TEST(ArgParser, TomlConfigImportAndExport) {
    set_test_env("NEKO_ARGPARSER_CONFIG_IO_OUTPUT", nullptr);
    auto import_path = argparser_config_io_path("neko_argparser_import.toml");
    auto export_path = argparser_config_io_path("neko_argparser_export.toml");

    ConfigIoOptions imported;
    imported.count  = 7;
    imported.output = "toml-file";
    imported.token  = "toml-token";
    imported.mode   = BuildMode::Release;
    write_serialized_config<TomlSerializer>(import_path, imported);

    auto config = config_io_parser_config();
    config.configIo->enableFormat("toml");
    const auto import_arg = import_path.string();
    const auto export_arg = export_path.string();
    const char* argv[]    = {"demo", "--import-toml", import_arg.c_str(), "--export-toml", export_arg.c_str()};

    auto result = parser<ConfigIoOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->count, 7);
    EXPECT_EQ(result->output, "toml-file");
    ASSERT_TRUE(result->token.has_value());
    EXPECT_EQ(*result->token, "toml-token");
    EXPECT_EQ(result->mode, BuildMode::Release);

    auto exported = read_serialized_config<TomlSerializer, ConfigIoOptions>(export_path);
    EXPECT_EQ(exported.count, 7);
    EXPECT_EQ(exported.output, "toml-file");
    ASSERT_TRUE(exported.token.has_value());
    EXPECT_EQ(*exported.token, "toml-token");
    EXPECT_EQ(exported.mode, BuildMode::Release);
}
#endif

TEST(ArgParser, BinaryConfigImportAndExport) {
    set_test_env("NEKO_ARGPARSER_CONFIG_IO_OUTPUT", nullptr);
    auto import_path = argparser_config_io_path("neko_argparser_import.bin");
    auto export_path = argparser_config_io_path("neko_argparser_export.bin");

    ConfigIoOptions imported;
    imported.count  = 2;
    imported.output = "binary-file";
    imported.token  = "binary-token";
    imported.mode   = BuildMode::Debug;
    write_serialized_config<BinarySerializer>(import_path, imported);

    auto config = config_io_parser_config();
    config.configIo->enableFormat("bin");
    const auto import_arg = import_path.string();
    const auto export_arg = export_path.string();
    const char* argv[]    = {"demo", "--import-binary", import_arg.c_str(), "--export-binary", export_arg.c_str()};

    auto result = parser<ConfigIoOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->count, 2);
    EXPECT_EQ(result->output, "binary-file");
    ASSERT_TRUE(result->token.has_value());
    EXPECT_EQ(*result->token, "binary-token");
    EXPECT_EQ(result->mode, BuildMode::Debug);

    auto exported = read_serialized_config<BinarySerializer, ConfigIoOptions>(export_path);
    EXPECT_EQ(exported.count, 2);
    EXPECT_EQ(exported.output, "binary-file");
    ASSERT_TRUE(exported.token.has_value());
    EXPECT_EQ(*exported.token, "binary-token");
}

// clang-format off
struct ExportPositionalCommand {
    int count = 0;
    std::string root;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("count", make_tags<arg_help<"count value">>(&ExportPositionalCommand::count),
                   "root", make_tags<arg_help<"root path">, ArgTags{.positional = true}>(
                               &ExportPositionalCommand::root));
    };
};

struct ExportPositionalTool {
    ExportPositionalCommand serve;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("serve", make_tags<arg_help<"serve command">, ArgTags{.command = true}>(
                                &ExportPositionalTool::serve));
    };
};
// clang-format on

TEST(ArgParser, CommandConfigExportCanAppearAfterCommandOptionsAndPositionals) {
    auto export_path = argparser_config_io_path("neko_argparser_command_after_options_export.bin");

    auto config = config_io_parser_config();
    config.configIo->enableExportFormat("binary");
    const auto export_arg = export_path.string();
    const char* argv[]    = {"tool", "serve", "--count", "8", "app", "--export-binary", export_arg.c_str()};

    auto result = parser<ExportPositionalTool>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(std::holds_alternative<ExportPositionalCommand>(*result));
    const auto& serve = std::get<ExportPositionalCommand>(*result);
    EXPECT_EQ(serve.count, 8);
    EXPECT_EQ(serve.root, "app");

    auto exported =
        read_serialized_config<BinarySerializer,
                               NEKO_NAMESPACE::argparser::detail::CommandConfig<ExportPositionalCommand>>(export_path);
    EXPECT_EQ(exported.command, "serve");
    EXPECT_EQ(exported.params.count, 8);
    EXPECT_EQ(exported.params.root, "app");
}

TEST(ArgParser, ImportedFalseBooleanDoesNotTriggerConflicts) {
    auto import_path = argparser_config_io_path("neko_argparser_false_conflict_import.bin");

    RelationshipOptions imported;
    imported.login = false;
    imported.json  = true;
    imported.yaml  = false;
    write_serialized_config<BinarySerializer>(import_path, imported);

    auto config = config_io_parser_config();
    config.configIo->enableImportFormat("binary");
    const auto import_arg = import_path.string();
    const char* argv[]    = {"demo", "--import-binary", import_arg.c_str()};

    auto result = parser<RelationshipOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->login);
    EXPECT_TRUE(result->json);
    EXPECT_FALSE(result->yaml);
}

// clang-format off
struct BuiltinConflictOptions {
    std::string import_json;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("import_json",
                   make_tags<arg_long_name<"import-json">,
                             arg_help<"user owned import-json">>(&BuiltinConflictOptions::import_json));
    };
};
// clang-format on

TEST(ArgParser, BuiltinConfigOptionNamesYieldToUserOptions) {
    auto config = config_io_parser_config();
    config.configIo->enableImportFormat("json");
    config.configIo->enableImportFormat("binary");
    config.configIo->optionNames.push_back({"binary", "import-json", std::nullopt});

    const char* argv[] = {"demo", "--import-json", "user-value"};
    auto result        = parser<BuiltinConflictOptions>(static_cast<int>(std::size(argv)), argv, config);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->import_json, "user-value");

    auto help = format_help<BuiltinConflictOptions>(config);
    EXPECT_NE(help.find("user owned import-json"), std::string::npos);
    EXPECT_EQ(help.find("import options from a JSON file"), std::string::npos);
    EXPECT_EQ(help.find("import options from a binary file"), std::string::npos);
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
