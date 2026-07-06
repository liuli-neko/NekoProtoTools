#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_LIBFYAML)

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <libfyaml.h>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
namespace yaml {

class Writer {
public:
    struct OutputNodeType {
        fy_node* node = nullptr;
    };

    using OutputArrayType  = OutputNodeType;
    using OutputObjectType = OutputNodeType;
    using OutputValueType  = OutputNodeType;

    Writer() = default;
    explicit Writer(fy_document* document) noexcept : mDocument(document) {}

    void reset(fy_document* document) noexcept {
        mDocument = document;
        mResult   = sa::success();
        mTagStorage.clear();
    }

    const sa::Result<void>& result() const noexcept { return mResult; }

    template <typename Tags>
    OutputArrayType arrayAsRoot(std::size_t /*size*/, const Tags& tags) {
        auto node = _createSequence();
        _setRoot(node);
        return _applyTags(OutputArrayType{node}, tags);
    }

    template <typename Tags>
    OutputObjectType objectAsRoot(std::size_t /*size*/, const Tags& tags) {
        auto node = _createMapping();
        _setRoot(node);
        return _applyTags(OutputObjectType{node}, tags);
    }

    template <typename Tags>
    OutputValueType nullAsRoot(const Tags& tags) {
        auto node = _createNull();
        _setRoot(node);
        return _applyTags(OutputValueType{node}, tags);
    }

    template <typename T, typename Tags>
    OutputValueType valueAsRoot(const T& value, const Tags& tags) {
        auto node = _createScalar(value);
        _setRoot(node);
        return _applyTags(OutputValueType{node}, tags);
    }

    template <typename Tags>
    OutputArrayType addArrayToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) {
        auto node = _createSequence();
        _appendToSequence(parent, node);
        return _applyTags(OutputArrayType{node}, tags);
    }

    template <typename Tags>
    OutputArrayType addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent,
                                     const Tags& tags) {
        auto node = _createSequence();
        _appendToMapping(parent, name, node);
        return _applyTags(OutputArrayType{node}, tags);
    }

    template <typename Tags>
    OutputObjectType addObjectToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) {
        auto node = _createMapping();
        _appendToSequence(parent, node);
        return _applyTags(OutputObjectType{node}, tags);
    }

    template <typename Tags>
    OutputObjectType addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent,
                                       const Tags& tags) {
        auto node = _createMapping();
        _appendToMapping(parent, name, node);
        return _applyTags(OutputObjectType{node}, tags);
    }

    template <typename T, typename Tags>
    OutputValueType addValueToArray(const T& value, OutputArrayType* parent, const Tags& tags) {
        auto node = _createScalar(value);
        _appendToSequence(parent, node);
        return _applyTags(OutputValueType{node}, tags);
    }

    template <typename T, typename Tags>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* parent,
                                     const Tags& tags) {
        auto node = _createScalar(value);
        _appendToMapping(parent, name, node);
        return _applyTags(OutputValueType{node}, tags);
    }

    template <typename Tags>
    OutputValueType addNullToArray(OutputArrayType* parent, const Tags& tags) {
        auto node = _createNull();
        _appendToSequence(parent, node);
        return _applyTags(OutputValueType{node}, tags);
    }

    template <typename Tags>
    OutputValueType addNullToObject(std::string_view name, OutputObjectType* parent, const Tags& tags) {
        auto node = _createNull();
        _appendToMapping(parent, name, node);
        return _applyTags(OutputValueType{node}, tags);
    }

    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    sa::Result<void> _remember(sa::ErrorCode code, std::string message) {
        mResult = sa::error(code, std::move(message));
        return mResult;
    }

    template <typename Output, typename Tags>
    Output _applyTags(Output output, const Tags& tags) {
        if constexpr (tag_query::has<tag_property::yaml_tag>(Tags{})) {
            _applyYamlTag(output.node, tag_query::get<tag_property::yaml_tag>(tags));
        }
        if constexpr (tag_query::has<tag_property::yaml_anchor>(Tags{})) {
            _applyYamlAnchor(output.node, tag_query::get<tag_property::yaml_anchor>(tags));
        }
        if constexpr (tag_query::has<tag_property::yaml_scalar_style>(Tags{})) {
            _applyYamlScalarStyle(output.node, tag_query::get<tag_property::yaml_scalar_style>(tags));
        }
        if constexpr (tag_query::has<tag_property::yaml_collection_style>(Tags{})) {
            _applyYamlCollectionStyle(output.node, tag_query::get<tag_property::yaml_collection_style>(tags));
        }
        return output;
    }

    void _applyYamlTag(fy_node* node, std::string_view tag) {
        if (tag.empty()) {
            return;
        }
        if (node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot apply YAML tag to an empty node");
            return;
        }
        mTagStorage.emplace_back(tag);
        const auto& ownedTag = mTagStorage.back();
        if (fy_node_set_tag(node, ownedTag.data(), ownedTag.size()) != 0) {
            _remember(sa::ErrorCode::InvalidType, "Could not apply YAML tag '" + std::string(tag) + "'");
        }
    }

    void _applyYamlAnchor(fy_node* node, std::string_view anchor) {
        if (anchor.empty()) {
            return;
        }
        if (node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot apply YAML anchor to an empty node");
            return;
        }
        if (fy_node_set_anchor_copy(node, anchor.data(), anchor.size()) != 0) {
            _remember(sa::ErrorCode::InvalidType, "Could not apply YAML anchor '" + std::string(anchor) + "'");
        }
    }

    void _applyYamlScalarStyle(fy_node* node, YamlScalarStyle style) {
        if (style == YamlScalarStyle::Any) {
            return;
        }
        if (node == nullptr || !fy_node_is_scalar(node)) {
            _remember(sa::ErrorCode::InvalidType, "YAML scalar style can only be applied to scalar nodes");
            return;
        }
        const auto requested = _toLibfyamlStyle(style);
        const auto actual    = fy_node_set_style(node, requested);
        if (actual != requested) {
            _remember(sa::ErrorCode::InvalidType, "Could not apply requested YAML scalar style");
        }
    }

    void _applyYamlCollectionStyle(fy_node* node, YamlCollectionStyle style) {
        if (style == YamlCollectionStyle::Any) {
            return;
        }
        if (node == nullptr || (!fy_node_is_sequence(node) && !fy_node_is_mapping(node))) {
            _remember(sa::ErrorCode::InvalidType, "YAML collection style can only be applied to sequence or mapping nodes");
            return;
        }
        const auto requested = _toLibfyamlStyle(style);
        const auto actual    = fy_node_set_style(node, requested);
        if (actual != requested) {
            _remember(sa::ErrorCode::InvalidType, "Could not apply requested YAML collection style");
        }
    }

    static fy_node_style _toLibfyamlStyle(YamlScalarStyle style) noexcept {
        switch (style) {
        case YamlScalarStyle::Any:
            return FYNS_ANY;
        case YamlScalarStyle::Plain:
            return FYNS_PLAIN;
        case YamlScalarStyle::SingleQuoted:
            return FYNS_SINGLE_QUOTED;
        case YamlScalarStyle::DoubleQuoted:
            return FYNS_DOUBLE_QUOTED;
        case YamlScalarStyle::Literal:
            return FYNS_LITERAL;
        case YamlScalarStyle::Folded:
            return FYNS_FOLDED;
        }
        return FYNS_ANY;
    }

    static fy_node_style _toLibfyamlStyle(YamlCollectionStyle style) noexcept {
        switch (style) {
        case YamlCollectionStyle::Any:
            return FYNS_ANY;
        case YamlCollectionStyle::Flow:
            return FYNS_FLOW;
        case YamlCollectionStyle::Block:
            return FYNS_BLOCK;
        }
        return FYNS_ANY;
    }

    fy_node* _createSequence() {
        if (mDocument == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        auto* node = fy_node_create_sequence(mDocument);
        if (node == nullptr) {
            _remember(sa::ErrorCode::Unknown, "Could not create YAML sequence node");
        }
        return node;
    }

    fy_node* _createMapping() {
        if (mDocument == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        auto* node = fy_node_create_mapping(mDocument);
        if (node == nullptr) {
            _remember(sa::ErrorCode::Unknown, "Could not create YAML mapping node");
        }
        return node;
    }

    fy_node* _createNull() {
        if (mDocument == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        auto* node = fy_node_build_from_string(mDocument, "null", 4);
        if (node == nullptr) {
            _remember(sa::ErrorCode::Unknown, "Could not create YAML null node");
        }
        return node;
    }

    template <typename T>
    fy_node* _createScalar(const T& value) {
        if (mDocument == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>) {
            if (!std::isfinite(value)) {
                _remember(sa::ErrorCode::InvalidType, "Cannot serialize non-finite floating point value to YAML");
                return nullptr;
            }
        }
        const auto text = _toString(value);
        auto* node      = fy_node_create_scalar_copy(mDocument, text.data(), text.size());
        if (node == nullptr) {
            _remember(sa::ErrorCode::Unknown, "Could not create YAML scalar node");
            return nullptr;
        }
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string> ||
                      std::is_same_v<std::remove_cvref_t<T>, std::string_view>) {
            fy_node_set_style(node, FYNS_DOUBLE_QUOTED);
        }
        return node;
    }

    void _setRoot(fy_node* node) {
        if (node == nullptr) {
            return;
        }
        if (fy_document_set_root(mDocument, node) != 0) {
            _remember(sa::ErrorCode::Unknown, "Could not set YAML document root");
        }
    }

    void _appendToSequence(OutputArrayType* parent, fy_node* node) {
        if (parent == nullptr || parent->node == nullptr || node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML sequence");
            return;
        }
        if (fy_node_sequence_append(parent->node, node) != 0) {
            _remember(sa::ErrorCode::InvalidType, "Could not append YAML sequence item");
        }
    }

    void _appendToMapping(OutputObjectType* parent, std::string_view name, fy_node* node) {
        if (parent == nullptr || parent->node == nullptr || node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML mapping");
            return;
        }
        auto* key = fy_node_create_scalar_copy(mDocument, name.data(), name.size());
        if (key == nullptr) {
            _remember(sa::ErrorCode::Unknown, "Could not create YAML mapping key '" + std::string(name) + "'");
            return;
        }
        fy_node_set_style(key, FYNS_PLAIN);
        if (fy_node_mapping_append(parent->node, key, node) != 0) {
            _remember(sa::ErrorCode::InvalidField, "Could not append YAML mapping field '" + std::string(name) + "'");
        }
    }

    template <typename T>
    static std::string _toString(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string>) {
            return value;
        } else if constexpr (std::is_same_v<U, std::string_view>) {
            return std::string{value};
        } else if constexpr (std::is_same_v<U, bool>) {
            return value ? "true" : "false";
        } else if constexpr (std::is_integral_v<U>) {
            char buffer[128];
            const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
            if (error == std::errc{}) {
                return {buffer, end};
            }
            return {};
        } else if constexpr (std::is_floating_point_v<U>) {
            if (!std::isfinite(value)) {
                return {};
            }
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<U>::max_digits10) << value;
            return stream.str();
        } else if constexpr (std::is_enum_v<U>) {
            using I = std::underlying_type_t<U>;
            return _toString(static_cast<I>(value));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported YAML scalar type");
        }
    }

private:
    fy_document* mDocument = nullptr;
    sa::Result<void> mResult;
    std::deque<std::string> mTagStorage;
};

} // namespace yaml

NEKO_END_NAMESPACE

#endif
