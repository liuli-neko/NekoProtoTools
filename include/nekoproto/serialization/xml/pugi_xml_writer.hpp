#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"

#if defined(NEKO_PROTO_ENABLE_PUGIXML)

#include <pugixml.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace nekoproto {
namespace xml {

inline constexpr std::string_view k_array_marker    = "neko-array";
inline constexpr std::string_view k_array_container = "container";
inline constexpr std::string_view k_array_empty     = "empty";
inline constexpr std::string_view k_null_marker     = "nil";
inline constexpr std::string_view k_xml_content     = "xml_content";
inline constexpr std::string_view k_array_item      = "item";

class Writer {
public:
    struct OutputArrayType {
        std::string    name;
        pugi::xml_node parent;
        pugi::xml_node placeholder;
    };

    struct OutputObjectType {
        pugi::xml_node node;
    };

    struct OutputValueType {
        pugi::xml_node node;
    };

    using OutputVarType = OutputValueType;

    explicit Writer(std::string root_name = "root") : root_name_(std::move(root_name)) { reset(root_name_); }

    void reset(std::string root_name) {
        document_.reset();
        root_name_                               = std::move(root_name);
        auto declaration                         = document_.append_child(pugi::node_declaration);
        declaration.append_attribute("version")  = "1.0";
        declaration.append_attribute("encoding") = "UTF-8";
    }

    auto arrayAsRoot(std::size_t /*size*/) -> OutputArrayType {
        auto root                                   = appendElement(document_, root_name_);
        root.append_attribute(k_array_marker.data()) = k_array_container.data();
        auto placeholder                            = appendEmptyArray(root, k_array_item);
        return {std::string{k_array_item}, root, placeholder};
    }

    auto objectAsRoot(std::size_t /*size*/) -> OutputObjectType { return {appendElement(document_, root_name_)}; }

    auto nullAsRoot() -> OutputValueType {
        auto root = appendElement(document_, root_name_);
        markNull(root);
        return {root};
    }

    template <typename T>
    auto valueAsRoot(const T& value) -> OutputValueType {
        auto root = appendElement(document_, root_name_);
        setText(root, toString(value));
        return {root};
    }

    static auto addArrayToArray(std::size_t /*size*/, OutputArrayType* parent) -> OutputArrayType {
        removePlaceholder(parent);
        auto container                                   = appendElement(parent->parent, parent->name);
        container.append_attribute(k_array_marker.data()) = k_array_container.data();
        auto placeholder                                 = appendEmptyArray(container, k_array_item);
        return {std::string{k_array_item}, container, placeholder};
    }

    static auto addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent)
        -> OutputArrayType {
        auto placeholder = appendEmptyArray(parent->node, name);
        return {std::string{name}, parent->node, placeholder};
    }

    static auto addObjectToArray(std::size_t /*size*/, OutputArrayType* parent) -> OutputObjectType {
        removePlaceholder(parent);
        return {appendElement(parent->parent, parent->name)};
    }

    static auto addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent)
        -> OutputObjectType {
        return {appendElement(parent->node, name)};
    }

    template <typename T>
    static auto addValueToArray(const T& value, OutputArrayType* parent) -> OutputValueType {
        removePlaceholder(parent);
        auto node = appendElement(parent->parent, parent->name);
        setText(node, toString(value));
        return {node};
    }

    template <typename T>
    static auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent,
                                 bool is_attribute = false) -> OutputValueType {
        const auto text = toString(value);
        if (is_attribute) {
            const auto owned_name = std::string{name};
            parent->node.append_attribute(owned_name.c_str()).set_value(text.c_str());
            return {parent->node};
        }
        if (name == k_xml_content) {
            setText(parent->node, text);
            return {parent->node};
        }
        auto node = appendElement(parent->node, name);
        setText(node, text);
        return {node};
    }

    static auto addNullToArray(OutputArrayType* parent) -> OutputValueType {
        removePlaceholder(parent);
        auto node = appendElement(parent->parent, parent->name);
        markNull(node);
        return {node};
    }

    static auto addNullToObject(std::string_view name, OutputObjectType* parent, bool is_attribute = false)
        -> OutputValueType {
        if (is_attribute) {
            const auto owned_name = std::string{name};
            parent->node.append_attribute(owned_name.c_str()).set_value("null");
            return {parent->node};
        }
        if (name == k_xml_content) {
            return {parent->node};
        }
        auto node = appendElement(parent->node, name);
        markNull(node);
        return {node};
    }

    static void addCommentToArray(std::string_view comment, OutputArrayType* parent) {
        appendComment(parent->parent, comment);
    }

    static void addCommentToObject(std::string_view comment, OutputObjectType* parent) {
        appendComment(parent->node, comment);
    }

    static void endArray(OutputArrayType* /*array*/) noexcept {}
    static void endObject(OutputObjectType* /*object*/) noexcept {}

    auto str(std::string_view indent = "    ") const -> std::string {
        std::ostringstream stream;
        const auto         indentation = std::string{indent};
        document_.save(stream, indentation.c_str(), pugi::format_default, pugi::encoding_utf8);
        return stream.str();
    }

private:
    static auto appendElement(pugi::xml_node parent, std::string_view name) -> pugi::xml_node {
        const auto owned_name = std::string{name};
        return parent.append_child(owned_name.c_str());
    }

    static auto appendEmptyArray(pugi::xml_node parent, std::string_view name) -> pugi::xml_node {
        auto node                                   = appendElement(parent, name);
        node.append_attribute(k_array_marker.data()) = k_array_empty.data();
        return node;
    }

    static void removePlaceholder(OutputArrayType* parent) {
        if (parent->placeholder != nullptr) {
            parent->parent.remove_child(parent->placeholder);
            parent->placeholder = {};
        }
    }

    static void markNull(pugi::xml_node node) { node.append_attribute(k_null_marker.data()) = "true"; }

    static void setText(pugi::xml_node node, const std::string& value) { node.text().set(value.c_str()); }

    static void appendComment(pugi::xml_node parent, std::string_view comment) {
        const auto owned_comment = std::string{comment};
        parent.append_child(pugi::node_comment).set_value(owned_comment.c_str());
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
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported XML basic type");
        }
    }

private:
    pugi::xml_document document_;
    std::string        root_name_;
};

} // namespace xml
} // namespace nekoproto

#endif
