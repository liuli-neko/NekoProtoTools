#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/to_string.hpp"

NEKO_USE_NAMESPACE

namespace {
struct Printable {
    int id = 0;
    std::string text;
    std::vector<int> values;
    std::optional<int> omitted;

    NEKO_SERIALIZER(id, text, values, omitted)
};

struct MetadataNameCollision {
    int names = 0;
    std::vector<int> values;
    int field_tags = 0;

    NEKO_SERIALIZER(names, values, field_tags)
};
} // namespace

TEST(ToString, UsesCommonParserAndHumanReadableWriter) {
    const Printable value{.id = 7, .text = "hello\nworld", .values = {1, 2, 3}, .omitted = std::nullopt};
    EXPECT_EQ(serializable_to_string(value), R"({id = 7, text = "hello\nworld", values = [1, 2, 3]})");
}

TEST(ToString, SupportsRootValuesAndNulls) {
    EXPECT_EQ(serializable_to_string(std::vector<std::string>{"a", "b"}), R"(["a", "b"])");
    EXPECT_EQ(serializable_to_string(std::optional<int>{}), "null");
}

TEST(ToString, AllowsMemberNamesMatchingMacroMetadata) {
    const MetadataNameCollision value{.names = 4, .values = {5, 6}, .field_tags = 7};
    EXPECT_EQ(serializable_to_string(value), R"({names = 4, values = [5, 6], field_tags = 7})");
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
