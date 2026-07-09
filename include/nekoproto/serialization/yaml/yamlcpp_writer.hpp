#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_YAMLCPP)

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

NEKO_BEGIN_NAMESPACE
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
    explicit Writer(YAML::Node* root) noexcept : mRoot(root) {}

    void reset(YAML::Node* root) noexcept {
        mRoot   = root;
        mResult = sa::success();
    }

    const sa::Result<void>& result() const noexcept { return mResult; }

    template <typename Tags>
    OutputArrayType arrayAsRoot(std::size_t /*size*/, const Tags& tags) {
        if (!_resetRoot(YAML::NodeType::Sequence)) {
            return {};
        }
        return _applyTags(OutputArrayType{*mRoot}, tags);
    }

    template <typename Tags>
    OutputObjectType objectAsRoot(std::size_t /*size*/, const Tags& tags) {
        if (!_resetRoot(YAML::NodeType::Map)) {
            return {};
        }
        return _applyTags(OutputObjectType{*mRoot}, tags);
    }

    template <typename Tags>
    OutputValueType nullAsRoot(const Tags& tags) {
        if (!_resetRoot(YAML::NodeType::Null)) {
            return {};
        }
        return _applyTags(OutputValueType{*mRoot}, tags);
    }

    template <typename T, typename Tags>
    OutputValueType valueAsRoot(const T& value, const Tags& tags) {
        if (mRoot == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return {};
        }
        auto node = _createScalar(value);
        if (!mResult) {
            return {};
        }
        *mRoot = node;
        return _applyTags(OutputValueType{*mRoot}, tags);
    }

    template <typename Tags>
    OutputArrayType addArrayToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) {
        auto node = _createSequence();
        auto inserted = _appendToSequence(parent, node);
        return _applyTags(OutputArrayType{inserted}, tags);
    }

    template <typename Tags>
    OutputArrayType addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent,
                                     const Tags& tags) {
        auto node = _createSequence();
        auto inserted = _appendToMapping(parent, name, node);
        return _applyTags(OutputArrayType{inserted}, tags);
    }

    template <typename Tags>
    OutputObjectType addObjectToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) {
        auto node = _createMapping();
        auto inserted = _appendToSequence(parent, node);
        return _applyTags(OutputObjectType{inserted}, tags);
    }

    template <typename Tags>
    OutputObjectType addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent,
                                       const Tags& tags) {
        auto node = _createMapping();
        auto inserted = _appendToMapping(parent, name, node);
        return _applyTags(OutputObjectType{inserted}, tags);
    }

    template <typename T, typename Tags>
    OutputValueType addValueToArray(const T& value, OutputArrayType* parent, const Tags& tags) {
        auto node = _createScalar(value);
        if (!mResult) {
            return {};
        }
        auto inserted = _appendToSequence(parent, node);
        return _applyTags(OutputValueType{inserted}, tags);
    }

    template <typename T, typename Tags>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* parent,
                                     const Tags& tags) {
        auto node = _createScalar(value);
        if (!mResult) {
            return {};
        }
        auto inserted = _appendToMapping(parent, name, node);
        return _applyTags(OutputValueType{inserted}, tags);
    }

    template <typename Tags>
    OutputValueType addNullToArray(OutputArrayType* parent, const Tags& tags) {
        auto node = YAML::Node(YAML::NodeType::Null);
        auto inserted = _appendToSequence(parent, node);
        return _applyTags(OutputValueType{inserted}, tags);
    }

    template <typename Tags>
    OutputValueType addNullToObject(std::string_view name, OutputObjectType* parent, const Tags& tags) {
        auto node = YAML::Node(YAML::NodeType::Null);
        auto inserted = _appendToMapping(parent, name, node);
        return _applyTags(OutputValueType{inserted}, tags);
    }

    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    void _remember(sa::ErrorCode code, std::string message) {
        if (mResult) {
            mResult = sa::error(code, std::move(message));
        }
    }

    bool _resetRoot(YAML::NodeType::value type) {
        if (mRoot == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return false;
        }
        *mRoot = YAML::Node(type);
        return true;
    }

    template <typename Output, typename Tags>
    Output _applyTags(Output output, const Tags& tags) {
        if constexpr (tag_query::has<tag_property::yaml_tag>(Tags{})) {
            _applyYamlTag(output.node, tag_query::get<tag_property::yaml_tag>(tags));
        }
        if constexpr (tag_query::has<tag_property::yaml_collection_style>(Tags{})) {
            _applyYamlCollectionStyle(output.node, tag_query::get<tag_property::yaml_collection_style>(tags));
        }
        return output;
    }

    void _applyYamlTag(YAML::Node node, std::string_view tag) {
        if (tag.empty() || !node || !node.IsDefined()) {
            return;
        }
        try {
            node.SetTag(std::string{tag});
        } catch (const YAML::Exception& error) {
            _remember(sa::ErrorCode::InvalidType, "Could not apply YAML tag '" + std::string(tag) + "': " + error.what());
        }
    }

    void _applyYamlCollectionStyle(YAML::Node node, YamlCollectionStyle style) {
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
            _remember(sa::ErrorCode::InvalidType, std::string{"Could not apply YAML collection style: "} + error.what());
        }
    }

    YAML::Node _createSequence() { return YAML::Node(YAML::NodeType::Sequence); }

    YAML::Node _createMapping() { return YAML::Node(YAML::NodeType::Map); }

    template <typename T>
    YAML::Node _createScalar(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_floating_point_v<U>) {
            if (!std::isfinite(value)) {
                _remember(sa::ErrorCode::InvalidType, "Cannot serialize non-finite floating point value to YAML");
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
            return _createScalar(static_cast<I>(value));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported YAML scalar type");
        }
    }

    YAML::Node _appendToSequence(OutputArrayType* parent, YAML::Node node) {
        if (parent == nullptr || !parent->node || !parent->node.IsSequence()) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML sequence");
            return {};
        }
        parent->node.push_back(node);
        return parent->node[parent->node.size() - 1];
    }

    YAML::Node _appendToMapping(OutputObjectType* parent, std::string_view name, YAML::Node node) {
        if (parent == nullptr || !parent->node || !parent->node.IsMap()) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML mapping");
            return {};
        }
        parent->node[std::string{name}] = node;
        return parent->node[std::string{name}];
    }

private:
    YAML::Node* mRoot = nullptr;
    sa::Result<void> mResult;
};

} // namespace yamlcpp

NEKO_END_NAMESPACE

#endif
