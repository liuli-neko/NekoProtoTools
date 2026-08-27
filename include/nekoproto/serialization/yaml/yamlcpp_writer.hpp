#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_YAMLCPP)

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/tags.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nekoproto {
namespace yamlcpp {

class Writer {
public:
    struct OutputNodeType {
        YAML::Node node;
    };

    using OutputArrayType  = OutputNodeType;
    using OutputObjectType = OutputNodeType;
    using OutputValueType  = OutputNodeType;

    Writer() = default;
    explicit Writer(YAML::Node* root) noexcept : root_(root) {}

    void reset(YAML::Node* root) noexcept {
        root_   = root;
        result_ = sa::success();
    }

    auto result() const noexcept -> const sa::Result<void>& { return result_; }

    template <typename Tags>
    auto arrayAsRoot(std::size_t /*size*/, const Tags& tags) -> OutputArrayType {
        if (!resetRoot(YAML::NodeType::Sequence)) {
            return {};
        }
        return applyTags(OutputArrayType{*root_}, tags);
    }

    template <typename Tags>
    auto objectAsRoot(std::size_t /*size*/, const Tags& tags) -> OutputObjectType {
        if (!resetRoot(YAML::NodeType::Map)) {
            return {};
        }
        return applyTags(OutputObjectType{*root_}, tags);
    }

    template <typename Tags>
    auto nullAsRoot(const Tags& tags) -> OutputValueType {
        if (!resetRoot(YAML::NodeType::Null)) {
            return {};
        }
        return applyTags(OutputValueType{*root_}, tags);
    }

    template <typename T, typename Tags>
    auto valueAsRoot(const T& value, const Tags& tags) -> OutputValueType {
        if (root_ == nullptr) {
            remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return {};
        }
        auto node = createScalar(value);
        if (!result_) {
            return {};
        }
        *root_ = node;
        return applyTags(OutputValueType{*root_}, tags);
    }

    template <typename Tags>
    auto addArrayToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) -> OutputArrayType {
        auto node     = createSequence();
        auto inserted = appendToSequence(parent, node);
        return applyTags(OutputArrayType{inserted}, tags);
    }

    template <typename Tags>
    auto addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent, const Tags& tags)
        -> OutputArrayType {
        auto node     = createSequence();
        auto inserted = appendToMapping(parent, name, node);
        return applyTags(OutputArrayType{inserted}, tags);
    }

    template <typename Tags>
    auto addObjectToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) -> OutputObjectType {
        auto node     = createMapping();
        auto inserted = appendToSequence(parent, node);
        return applyTags(OutputObjectType{inserted}, tags);
    }

    template <typename Tags>
    auto addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent, const Tags& tags)
        -> OutputObjectType {
        auto node     = createMapping();
        auto inserted = appendToMapping(parent, name, node);
        return applyTags(OutputObjectType{inserted}, tags);
    }

    template <typename T, typename Tags>
    auto addValueToArray(const T& value, OutputArrayType* parent, const Tags& tags) -> OutputValueType {
        auto node = createScalar(value);
        if (!result_) {
            return {};
        }
        auto inserted = appendToSequence(parent, node);
        return applyTags(OutputValueType{inserted}, tags);
    }

    template <typename T, typename Tags>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent, const Tags& tags)
        -> OutputValueType {
        auto node = createScalar(value);
        if (!result_) {
            return {};
        }
        auto inserted = appendToMapping(parent, name, node);
        return applyTags(OutputValueType{inserted}, tags);
    }

    template <typename Tags>
    auto addNullToArray(OutputArrayType* parent, const Tags& tags) -> OutputValueType {
        auto node     = YAML::Node(YAML::NodeType::Null);
        auto inserted = appendToSequence(parent, node);
        return applyTags(OutputValueType{inserted}, tags);
    }

    template <typename Tags>
    auto addNullToObject(std::string_view name, OutputObjectType* parent, const Tags& tags) -> OutputValueType {
        auto node     = YAML::Node(YAML::NodeType::Null);
        auto inserted = appendToMapping(parent, name, node);
        return applyTags(OutputValueType{inserted}, tags);
    }

    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    void remember(sa::ErrorCode code, std::string message) {
        if (result_) {
            result_ = sa::error(code, std::move(message));
        }
    }

    auto resetRoot(YAML::NodeType::value type) -> bool {
        if (root_ == nullptr) {
            remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return false;
        }
        *root_ = YAML::Node(type);
        return true;
    }

    template <typename Output, typename Tags>
    auto applyTags(Output output, const Tags& tags) -> Output {
        if constexpr (tag_query::has<tag_property::YamlTag>(Tags{})) {
            applyYamlTag(output.node, tag_query::get<tag_property::YamlTag>(tags));
        }
        if constexpr (tag_query::has<tag_property::YamlCollectionStyleProperty>(Tags{})) {
            applyYamlCollectionStyle(output.node, tag_query::get<tag_property::YamlCollectionStyleProperty>(tags));
        }
        return output;
    }

    void applyYamlTag(YAML::Node node, std::string_view tag) {
        if (tag.empty() || !node || !node.IsDefined()) {
            return;
        }
        try {
            node.SetTag(std::string{tag});
        } catch (const YAML::Exception& error) {
            remember(sa::ErrorCode::InvalidType,
                     "Could not apply YAML tag '" + std::string(tag) + "': " + error.what());
        }
    }

    void applyYamlCollectionStyle(YAML::Node node, YamlCollectionStyle style) {
        if (style == YamlCollectionStyle::Any || !node || (!node.IsSequence() && !node.IsMap())) {
            return;
        }
        try {
            switch (style) {
            case YamlCollectionStyle::Any:
                break;
            case YamlCollectionStyle::Flow:
                node.SetStyle(YAML::EmitterStyle::Flow);
                break;
            case YamlCollectionStyle::Block:
                node.SetStyle(YAML::EmitterStyle::Block);
                break;
            }
        } catch (const YAML::Exception& error) {
            remember(sa::ErrorCode::InvalidType, std::string{"Could not apply YAML collection style: "} + error.what());
        }
    }

    auto createSequence() -> YAML::Node { return YAML::Node(YAML::NodeType::Sequence); }

    auto createMapping() -> YAML::Node { return YAML::Node(YAML::NodeType::Map); }

    template <typename T>
    auto createScalar(const T& value) -> YAML::Node {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_floating_point_v<U>) {
            if (!std::isfinite(value)) {
                remember(sa::ErrorCode::InvalidType, "Cannot serialize non-finite floating point value to YAML");
                return {};
            }
        }
        if constexpr (std::is_same_v<U, std::string>) {
            return YAML::Node(value);
        } else if constexpr (std::is_same_v<U, std::string_view>) {
            return YAML::Node(std::string{value});
        } else if constexpr (std::is_same_v<U, bool>) {
            return YAML::Node(value);
        } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
            return YAML::Node(static_cast<long long>(value)); // NOLINT
        } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>) {
            return YAML::Node(static_cast<unsigned long long>(value)); // NOLINT
        } else if constexpr (std::is_floating_point_v<U>) {
            return YAML::Node(static_cast<double>(value));
        } else if constexpr (std::is_enum_v<U>) {
            using I = std::underlying_type_t<U>;
            return createScalar(static_cast<I>(value));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported YAML scalar type");
        }
    }

    auto appendToSequence(OutputArrayType* parent, YAML::Node node) -> YAML::Node {
        if (parent == nullptr || !parent->node || !parent->node.IsSequence()) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML sequence");
            return {};
        }
        parent->node.push_back(node);
        return parent->node[parent->node.size() - 1];
    }

    auto appendToMapping(OutputObjectType* parent, std::string_view name, YAML::Node node) -> YAML::Node {
        if (parent == nullptr || !parent->node || !parent->node.IsMap()) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML mapping");
            return {};
        }
        parent->node[std::string{name}] = node;
        return parent->node[std::string{name}];
    }

private:
    YAML::Node*      root_ = nullptr;
    sa::Result<void> result_;
};

} // namespace yamlcpp

} // namespace nekoproto

#endif
