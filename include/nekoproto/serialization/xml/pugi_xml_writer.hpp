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

NEKO_BEGIN_NAMESPACE
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

    OutputArrayType arrayAsRoot(std::size_t /*size*/) {
        auto root                                 = _appendElement(mDocument, mRootName);
        root.append_attribute(ArrayMarker.data()) = ArrayContainer.data();
        auto placeholder                          = _appendEmptyArray(root, ArrayItem);
        return {std::string{ArrayItem}, root, placeholder};
    }

    OutputObjectType objectAsRoot(std::size_t /*size*/) { return {_appendElement(mDocument, mRootName)}; }

    OutputValueType nullAsRoot() {
        auto root = _appendElement(mDocument, mRootName);
        _markNull(root);
        return {root};
    }

    template <typename T>
    OutputValueType valueAsRoot(const T& value) {
        auto root = _appendElement(mDocument, mRootName);
        _setText(root, _toString(value));
        return {root};
    }

    static OutputArrayType addArrayToArray(std::size_t /*size*/, OutputArrayType* parent) {
        _removePlaceholder(parent);
        auto container                                 = _appendElement(parent->parent, parent->name);
        container.append_attribute(ArrayMarker.data()) = ArrayContainer.data();
        auto placeholder                               = _appendEmptyArray(container, ArrayItem);
        return {std::string{ArrayItem}, container, placeholder};
    }

    static OutputArrayType addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent) {
        auto placeholder = _appendEmptyArray(parent->node, name);
        return {std::string{name}, parent->node, placeholder};
    }

    static OutputObjectType addObjectToArray(std::size_t /*size*/, OutputArrayType* parent) {
        _removePlaceholder(parent);
        return {_appendElement(parent->parent, parent->name)};
    }

    static OutputObjectType addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent) {
        return {_appendElement(parent->node, name)};
    }

    template <typename T>
    static OutputValueType addValueToArray(const T& value, OutputArrayType* parent) {
        _removePlaceholder(parent);
        auto node = _appendElement(parent->parent, parent->name);
        _setText(node, _toString(value));
        return {node};
    }

    template <typename T>
    static OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* parent,
                                            bool isAttribute = false) {
        const auto text = _toString(value);
        if (isAttribute) {
            const auto ownedName = std::string{name};
            parent->node.append_attribute(ownedName.c_str()).set_value(text.c_str());
            return {parent->node};
        }
        if (name == XmlContent) {
            _setText(parent->node, text);
            return {parent->node};
        }
        auto node = _appendElement(parent->node, name);
        _setText(node, text);
        return {node};
    }

    static OutputValueType addNullToArray(OutputArrayType* parent) {
        _removePlaceholder(parent);
        auto node = _appendElement(parent->parent, parent->name);
        _markNull(node);
        return {node};
    }

    static OutputValueType addNullToObject(std::string_view name, OutputObjectType* parent, bool isAttribute = false) {
        if (isAttribute) {
            const auto ownedName = std::string{name};
            parent->node.append_attribute(ownedName.c_str()).set_value("null");
            return {parent->node};
        }
        if (name == XmlContent) {
            return {parent->node};
        }
        auto node = _appendElement(parent->node, name);
        _markNull(node);
        return {node};
    }

    static void addCommentToArray(std::string_view comment, OutputArrayType* parent) {
        _appendComment(parent->parent, comment);
    }

    static void addCommentToObject(std::string_view comment, OutputObjectType* parent) {
        _appendComment(parent->node, comment);
    }

    static void endArray(OutputArrayType* /*array*/) noexcept {}
    static void endObject(OutputObjectType* /*object*/) noexcept {}

    std::string str(std::string_view indent = "    ") const {
        std::ostringstream stream;
        const auto indentation = std::string{indent};
        mDocument.save(stream, indentation.c_str(), pugi::format_default, pugi::encoding_utf8);
        return stream.str();
    }

private:
    static pugi::xml_node _appendElement(pugi::xml_node parent, std::string_view name) {
        const auto ownedName = std::string{name};
        return parent.append_child(ownedName.c_str());
    }

    static pugi::xml_node _appendEmptyArray(pugi::xml_node parent, std::string_view name) {
        auto node                                 = _appendElement(parent, name);
        node.append_attribute(ArrayMarker.data()) = ArrayEmpty.data();
        return node;
    }

    static void _removePlaceholder(OutputArrayType* parent) {
        if (parent->placeholder != nullptr) {
            parent->parent.remove_child(parent->placeholder);
            parent->placeholder = {};
        }
    }

    static void _markNull(pugi::xml_node node) { node.append_attribute(NullMarker.data()) = "true"; }

    static void _setText(pugi::xml_node node, const std::string& value) { node.text().set(value.c_str()); }

    static void _appendComment(pugi::xml_node parent, std::string_view comment) {
        const auto ownedComment = std::string{comment};
        parent.append_child(pugi::node_comment).set_value(ownedComment.c_str());
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
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported XML basic type");
        }
    }

private:
    pugi::xml_document mDocument;
    std::string mRootName;
};

} // namespace xml
NEKO_END_NAMESPACE

#endif
