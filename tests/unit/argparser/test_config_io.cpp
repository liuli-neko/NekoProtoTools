#include <gtest/gtest.h>

#include "nekoproto/argparser/config_io.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace nekoproto;

struct PublicConfigValue {
    int count = 0;
    std::string name;
    std::vector<int> ports;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("count", &PublicConfigValue::count, "name", &PublicConfigValue::name, "ports",
                   &PublicConfigValue::ports);
    };
};

void expectConfigEq(const PublicConfigValue& lhs, const PublicConfigValue& rhs) {
    EXPECT_EQ(lhs.count, rhs.count);
    EXPECT_EQ(lhs.name, rhs.name);
    EXPECT_EQ(lhs.ports, rhs.ports);
}

auto sampleConfig() -> PublicConfigValue { return {.count = 7, .name = "public-config-io", .ports = {8000, 8080}}; }

TEST(ArgParserConfigIo, RuntimeFormatEncodesAndDecodesBytes) {
    const auto original = sampleConfig();

    auto encoded = argparser::config_io::encode(original, "binary");
    ASSERT_TRUE(encoded.has_value()) << encoded.error().message;

    auto decoded =
        argparser::config_io::decode<PublicConfigValue>(std::span<const char>{encoded->data(), encoded->size()}, "bin");
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message;
    expectConfigEq(*decoded, original);
}

TEST(ArgParserConfigIo, TypedFormatEncodesAndDecodesBytes) {
    const auto original = sampleConfig();

    auto encoded = argparser::config_io::encode(original, argparser::config_io::formats::binary);
    ASSERT_TRUE(encoded.has_value()) << encoded.error().message;

    auto decoded = argparser::config_io::decode<PublicConfigValue>(
        std::span<const char>{encoded->data(), encoded->size()}, argparser::config_io::formats::binary);
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message;
    expectConfigEq(*decoded, original);
}

TEST(ArgParserConfigIo, StreamSaveAndLoadRoundTrip) {
    const auto original = sampleConfig();
    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);

    auto saved = argparser::config_io::save(original, stream, "binary");
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    stream.seekg(0);
    auto loaded = argparser::config_io::load<PublicConfigValue>(stream, "bin");
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    expectConfigEq(*loaded, original);
}

TEST(ArgParserConfigIo, FileSaveAndLoadInferFormatFromExtension) {
    const auto original = sampleConfig();
    const auto path     = std::filesystem::temp_directory_path() / "neko_argparser_public_config_io.bin";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    auto saved = argparser::config_io::save(original, path);
    ASSERT_TRUE(saved.has_value()) << saved.error().message;

    auto loaded = argparser::config_io::load<PublicConfigValue>(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    expectConfigEq(*loaded, original);

    std::filesystem::remove(path, ignored);
}

TEST(ArgParserConfigIo, UnknownRuntimeFormatReturnsConfigIoError) {
    auto result = argparser::config_io::decode<PublicConfigValue>(std::string_view{}, "not-a-format");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code,
              argparser::config_io::makeErrorCode(argparser::config_io::ConfigIoErrorCode::UnknownFormat));
    EXPECT_EQ(result.error().format, "not-a-format");
}

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
TEST(ArgParserConfigIo, TextBackendUsesTheSamePublicApi) {
    const auto original = sampleConfig();

    auto encoded = argparser::config_io::encode(original, argparser::config_io::formats::json);
    ASSERT_TRUE(encoded.has_value()) << encoded.error().message;
    EXPECT_FALSE(encoded->empty());

    auto decoded = argparser::config_io::decode<PublicConfigValue>(
        std::span<const char>{encoded->data(), encoded->size()}, "json");
    ASSERT_TRUE(decoded.has_value()) << decoded.error().message;
    expectConfigEq(*decoded, original);
}
#endif

#include "../common/common_main.cpp.in" // IWYU pragma: export
