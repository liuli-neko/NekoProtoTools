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

inline constexpr std::string_view ArrayMarker    = "neko-array";
inline constexpr std::string_view ArrayContainer = "container";
inline constexpr std::string_view ArrayEmpty     = "empty";
inline constexpr std::string_view NullMarker     = "nil";
inline constexpr std::string_view XmlContent     = "xml_content";
inline constexpr std::string_view ArrayItem      = "item";

class Writer {
public:
    struct OutputArrayType {
        std::string name;
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

    explicit Writer(std::string rootName = "root") : mRootName(std::move(rootName)) { reset(mRootName); }

    void reset(std::string rootName) {
        mDocument.reset();
        mRootName                                = std::move(rootName);
        auto declaration                         = mDocument.append_child(pugi::node_declaration);
        declaration.append_attribute("version")  = "1.0";
        declaration.append_attribute("encoding") = "UTF-8";
    }

    auto arrayAsRoot(std::size_t /*size*/) -> OutputArrayType {
        auto root                                 = appendElement(mDocument, mRootName);
        root.append_attribute(ArrayMarker.data()) = ArrayContainer.data();
        auto placeholder                          = appendEmptyArray(root, ArrayItem);
        return {std::string{ArrayItem}, root, placeholder};
    }

    auto objectAsRoot(std::size_t /*size*/) -> OutputObjectType { return {appendElement(mDocument, mRootName)}; }

    auto nullAsRoot() -> OutputValueType {
        auto root = appendElement(mDocument, mRootName);
        markNull(root);
        return {root};
    }

    template <typename T>
    auto valueAsRoot(const T& value) -> OutputValueType {
        auto root = appendElement(mDocument, mRootName);
        setText(root, toString(value));
        return {root};
    }

    static auto addArrayToArray(std::size_t /*size*/, OutputArrayType* parent) -> OutputArrayType {
        removePlaceholder(parent);
        auto container                                 = appendElement(parent->parent, parent->name);
        container.append_attribute(ArrayMarker.data()) = ArrayContainer.data();
        auto placeholder                               = appendEmptyArray(container, ArrayItem);
        return {std::string{ArrayItem}, container, placeholder};
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
                                 bool isAttribute = false) -> OutputValueType {
        const auto text = toString(value);
        if (isAttribute) {
            const auto ownedName = std::string{name};
            parent->node.append_attribute(ownedName.c_str()).set_value(text.c_str());
            return {parent->node};
        }
        if (name == XmlContent) {
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

    static auto addNullToObject(std::string_view name, OutputObjectType* parent, bool isAttribute = false)
        -> OutputValueType {
        if (isAttribute) {
            const auto ownedName = std::string{name};
            parent->node.append_attribute(ownedName.c_str()).set_value("null");
            return {parent->node};
        }
        if (name == XmlContent) {
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
        const auto indentation = std::string{indent};
        mDocument.save(stream, indentation.c_str(), pugi::format_default, pugi::encoding_utf8);
        return stream.str();
    }

private:
    static auto appendElement(pugi::xml_node parent, std::string_view name) -> pugi::xml_node {
        const auto ownedName = std::string{name};
        return parent.append_child(ownedName.c_str());
    }

    static auto appendEmptyArray(pugi::xml_node parent, std::string_view name) -> pugi::xml_node {
        auto node                                 = appendElement(parent, name);
        node.append_attribute(ArrayMarker.data()) = ArrayEmpty.data();
        return node;
    }

    static void removePlaceholder(OutputArrayType* parent) {
        if (parent->placeholder != nullptr) {
            parent->parent.remove_child(parent->placeholder);
            parent->placeholder = {};
        }
    }

    static void markNull(pugi::xml_node node) { node.append_attribute(NullMarker.data()) = "true"; }

    static void setText(pugi::xml_node node, const std::string& value) { node.text().set(value.c_str()); }

    static void appendComment(pugi::xml_node parent, std::string_view comment) {
        const auto ownedComment = std::string{comment};
        parent.append_child(pugi::node_comment).set_value(ownedComment.c_str());
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
    pugi::xml_document mDocument;
    std::string mRootName;
};

} // namespace xml
} // namespace nekoproto

#endif
