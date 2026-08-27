#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_LIBFYAML)

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/tags.hpp"

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

namespace nekoproto {
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
    explicit Writer(fy_document* document) noexcept : document_(document) {}

    void reset(fy_document* document) noexcept {
        document_ = document;
        result_   = sa::success();
        tag_storage_.clear();
    }

    auto result() const noexcept -> const sa::Result<void>& { return result_; }

    template <typename Tags>
    auto arrayAsRoot(std::size_t /*size*/, const Tags& tags) -> OutputArrayType {
        auto node = createSequence();
        setRoot(node);
        return applyTags(OutputArrayType{node}, tags);
    }

    template <typename Tags>
    auto objectAsRoot(std::size_t /*size*/, const Tags& tags) -> OutputObjectType {
        auto node = createMapping();
        setRoot(node);
        return applyTags(OutputObjectType{node}, tags);
    }

    template <typename Tags>
    auto nullAsRoot(const Tags& tags) -> OutputValueType {
        auto node = createNull();
        setRoot(node);
        return applyTags(OutputValueType{node}, tags);
    }

    template <typename T, typename Tags>
    auto valueAsRoot(const T& value, const Tags& tags) -> OutputValueType {
        auto node = createScalar(value);
        setRoot(node);
        return applyTags(OutputValueType{node}, tags);
    }

    template <typename Tags>
    auto addArrayToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) -> OutputArrayType {
        auto node = createSequence();
        appendToSequence(parent, node);
        return applyTags(OutputArrayType{node}, tags);
    }

    template <typename Tags>
    auto addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent, const Tags& tags)
        -> OutputArrayType {
        auto node = createSequence();
        appendToMapping(parent, name, node);
        return applyTags(OutputArrayType{node}, tags);
    }

    template <typename Tags>
    auto addObjectToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) -> OutputObjectType {
        auto node = createMapping();
        appendToSequence(parent, node);
        return applyTags(OutputObjectType{node}, tags);
    }

    template <typename Tags>
    auto addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent, const Tags& tags)
        -> OutputObjectType {
        auto node = createMapping();
        appendToMapping(parent, name, node);
        return applyTags(OutputObjectType{node}, tags);
    }

    template <typename T, typename Tags>
    auto addValueToArray(const T& value, OutputArrayType* parent, const Tags& tags) -> OutputValueType {
        auto node = createScalar(value);
        appendToSequence(parent, node);
        return applyTags(OutputValueType{node}, tags);
    }

    template <typename T, typename Tags>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent, const Tags& tags)
        -> OutputValueType {
        auto node = createScalar(value);
        appendToMapping(parent, name, node);
        return applyTags(OutputValueType{node}, tags);
    }

    template <typename Tags>
    auto addNullToArray(OutputArrayType* parent, const Tags& tags) -> OutputValueType {
        auto node = createNull();
        appendToSequence(parent, node);
        return applyTags(OutputValueType{node}, tags);
    }

    template <typename Tags>
    auto addNullToObject(std::string_view name, OutputObjectType* parent, const Tags& tags) -> OutputValueType {
        auto node = createNull();
        appendToMapping(parent, name, node);
        return applyTags(OutputValueType{node}, tags);
    }

    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    auto remember(sa::ErrorCode code, std::string message) -> sa::Result<void> {
        result_ = sa::error(code, std::move(message));
        return result_;
    }

    template <typename Output, typename Tags>
    auto applyTags(Output output, const Tags& tags) -> Output {
        if constexpr (tag_query::has<tag_property::YamlTag>(Tags{})) {
            applyYamlTag(output.node, tag_query::get<tag_property::YamlTag>(tags));
        }
        if constexpr (tag_query::has<tag_property::YamlAnchor>(Tags{})) {
            applyYamlAnchor(output.node, tag_query::get<tag_property::YamlAnchor>(tags));
        }
        if constexpr (tag_query::has<tag_property::YamlScalarStyleProperty>(Tags{})) {
            applyYamlScalarStyle(output.node, tag_query::get<tag_property::YamlScalarStyleProperty>(tags));
        }
        if constexpr (tag_query::has<tag_property::YamlCollectionStyleProperty>(Tags{})) {
            applyYamlCollectionStyle(output.node, tag_query::get<tag_property::YamlCollectionStyleProperty>(tags));
        }
        return output;
    }

    void applyYamlTag(fy_node* node, std::string_view tag) {
        if (tag.empty()) {
            return;
        }
        if (node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot apply YAML tag to an empty node");
            return;
        }
        tag_storage_.emplace_back(tag);
        const auto& ownedTag = tag_storage_.back();
        if (fy_node_set_tag(node, ownedTag.data(), ownedTag.size()) != 0) {
            remember(sa::ErrorCode::InvalidType, "Could not apply YAML tag '" + std::string(tag) + "'");
        }
    }

    void applyYamlAnchor(fy_node* node, std::string_view anchor) {
        if (anchor.empty()) {
            return;
        }
        if (node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot apply YAML anchor to an empty node");
            return;
        }
        if (fy_node_set_anchor_copy(node, anchor.data(), anchor.size()) != 0) {
            remember(sa::ErrorCode::InvalidType, "Could not apply YAML anchor '" + std::string(anchor) + "'");
        }
    }

    void applyYamlScalarStyle(fy_node* node, YamlScalarStyle style) {
        if (style == YamlScalarStyle::Any) {
            return;
        }
        if (node == nullptr || !fy_node_is_scalar(node)) {
            remember(sa::ErrorCode::InvalidType, "YAML scalar style can only be applied to scalar nodes");
            return;
        }
        const auto requested = toLibfyamlStyle(style);
        const auto actual    = fy_node_set_style(node, requested);
        if (actual != requested) {
            remember(sa::ErrorCode::InvalidType, "Could not apply requested YAML scalar style");
        }
    }

    void applyYamlCollectionStyle(fy_node* node, YamlCollectionStyle style) {
        if (style == YamlCollectionStyle::Any) {
            return;
        }
        if (node == nullptr || (!fy_node_is_sequence(node) && !fy_node_is_mapping(node))) {
            remember(sa::ErrorCode::InvalidType,
                     "YAML collection style can only be applied to sequence or mapping nodes");
            return;
        }
        const auto requested = toLibfyamlStyle(style);
        const auto actual    = fy_node_set_style(node, requested);
        if (actual != requested) {
            remember(sa::ErrorCode::InvalidType, "Could not apply requested YAML collection style");
        }
    }

    static auto toLibfyamlStyle(YamlScalarStyle style) noexcept -> fy_node_style {
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

    static auto toLibfyamlStyle(YamlCollectionStyle style) noexcept -> fy_node_style {
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

    auto createSequence() -> fy_node* {
        if (document_ == nullptr) {
            remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        auto* node = fy_node_create_sequence(document_);
        if (node == nullptr) {
            remember(sa::ErrorCode::Unknown, "Could not create YAML sequence node");
        }
        return node;
    }

    auto createMapping() -> fy_node* {
        if (document_ == nullptr) {
            remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        auto* node = fy_node_create_mapping(document_);
        if (node == nullptr) {
            remember(sa::ErrorCode::Unknown, "Could not create YAML mapping node");
        }
        return node;
    }

    auto createNull() -> fy_node* {
        if (document_ == nullptr) {
            remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        auto* node = fy_node_build_from_string(document_, "null", 4);
        if (node == nullptr) {
            remember(sa::ErrorCode::Unknown, "Could not create YAML null node");
        }
        return node;
    }

    template <typename T>
    auto createScalar(const T& value) -> fy_node* {
        if (document_ == nullptr) {
            remember(sa::ErrorCode::InvalidType, "YAML document is not initialized");
            return nullptr;
        }
        if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>) {
            if (!std::isfinite(value)) {
                remember(sa::ErrorCode::InvalidType, "Cannot serialize non-finite floating point value to YAML");
                return nullptr;
            }
        }
        const auto text = toString(value);
        auto* node      = fy_node_create_scalar_copy(document_, text.data(), text.size());
        if (node == nullptr) {
            remember(sa::ErrorCode::Unknown, "Could not create YAML scalar node");
            return nullptr;
        }
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string> ||
                      std::is_same_v<std::remove_cvref_t<T>, std::string_view>) {
            fy_node_set_style(node, FYNS_DOUBLE_QUOTED);
        }
        return node;
    }

    void setRoot(fy_node* node) {
        if (node == nullptr) {
            return;
        }
        if (fy_document_set_root(document_, node) != 0) {
            remember(sa::ErrorCode::Unknown, "Could not set YAML document root");
        }
    }

    void appendToSequence(OutputArrayType* parent, fy_node* node) {
        if (parent == nullptr || parent->node == nullptr || node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML sequence");
            return;
        }
        if (fy_node_sequence_append(parent->node, node) != 0) {
            remember(sa::ErrorCode::InvalidType, "Could not append YAML sequence item");
        }
    }

    void appendToMapping(OutputObjectType* parent, std::string_view name, fy_node* node) {
        if (parent == nullptr || parent->node == nullptr || node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty YAML mapping");
            return;
        }
        auto* key = fy_node_create_scalar_copy(document_, name.data(), name.size());
        if (key == nullptr) {
            remember(sa::ErrorCode::Unknown, "Could not create YAML mapping key '" + std::string(name) + "'");
            return;
        }
        fy_node_set_style(key, FYNS_PLAIN);
        if (fy_node_mapping_append(parent->node, key, node) != 0) {
            remember(sa::ErrorCode::InvalidField, "Could not append YAML mapping field '" + std::string(name) + "'");
        }
    }

    template <typename T>
    static auto toString(const T& value) -> std::string {
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
            return toString(static_cast<I>(value));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported YAML scalar type");
        }
    }

private:
    fy_document*            document_ = nullptr;
    sa::Result<void>        result_;
    std::deque<std::string> tag_storage_;
};

} // namespace yaml

} // namespace nekoproto

#endif
