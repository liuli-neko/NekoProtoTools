#pragma once

#include "nekoproto/global/global.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace json {

struct RawValue {
    std::string text;
};

class TextWriter {
private:
    struct Node {
        enum class Kind { Null, Scalar, String, Raw, Array, Object };

        Kind kind = Kind::Null;
        std::string value;
        std::vector<Node> array;

        std::vector<std::string> objectNames;
        std::vector<Node> objectValues;

        auto emplaceObject(const std::string& name, Node&& node) {
            objectNames.push_back(name);
            objectValues.push_back(std::move(node));
            return std::pair<std::string_view, Node&>(objectNames.back(), objectValues.back());
        }

        void reserveObject(size_t size) {
            objectNames.reserve(size);
            objectValues.reserve(size);
        }

        std::pair<std::string_view, const Node&> object(int index) const {
            return {objectNames[index], objectValues[index]};
        }
    };

public:
    using RawValueType = RawValue;

    struct OutputArrayType {
        Node* value;
    };

    struct OutputObjectType {
        Node* value;
    };

    struct OutputValueType {
        Node* value;
    };

    void reset() { mRoot = Node{}; }

    OutputArrayType arrayAsRoot(std::size_t size) {
        _initializeArray(mRoot, size);
        return {&mRoot};
    }

    OutputObjectType objectAsRoot(std::size_t size) {
        _initializeObject(mRoot, size);
        return {&mRoot};
    }

    OutputValueType nullAsRoot() {
        mRoot = Node{};
        return {&mRoot};
    }

    template <typename T>
    OutputValueType valueAsRoot(const T& value) {
        _setValue(mRoot, value);
        return {&mRoot};
    }

    static OutputArrayType addArrayToArray(std::size_t size, OutputArrayType* parent) {
        auto& child = parent->value->array.emplace_back();
        _initializeArray(child, size);
        return {&child};
    }

    static OutputArrayType addArrayToObject(std::string_view name, std::size_t size, OutputObjectType* parent) {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        _initializeArray(child, size);
        return {&child};
    }

    static OutputObjectType addObjectToArray(std::size_t size, OutputArrayType* parent) {
        auto& child = parent->value->array.emplace_back();
        _initializeObject(child, size);
        return {&child};
    }

    static OutputObjectType addObjectToObject(std::string_view name, std::size_t size, OutputObjectType* parent) {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        _initializeObject(child, size);
        return {&child};
    }

    template <typename T>
    OutputValueType addValueToArray(const T& value, OutputArrayType* parent) {
        auto& child = parent->value->array.emplace_back();
        _setValue(child, value);
        return {&child};
    }

    template <typename T>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        _setValue(child, value);
        return {&child};
    }

    static OutputValueType addNullToArray(OutputArrayType* parent) {
        auto& child = parent->value->array.emplace_back();
        return {&child};
    }

    static OutputValueType addNullToObject(std::string_view name, OutputObjectType* parent) {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        return {&child};
    }

    std::string str() const {
        std::string output;
        _render(mRoot, output);
        return output;
    }

private:
    static void _initializeArray(Node& node, std::size_t size) {
        node      = Node{};
        node.kind = Node::Kind::Array;
        if (size != static_cast<std::size_t>(-1)) {
            node.array.reserve(size);
        }
    }

    static void _initializeObject(Node& node, std::size_t size) {
        node      = Node{};
        node.kind = Node::Kind::Object;
        if (size != static_cast<std::size_t>(-1)) {
            node.reserveObject(size);
        }
    }

    static std::string _escape(std::string_view value) {
        std::string output;
        output.reserve(value.size() + 2);
        for (const unsigned char ch : value) {
            switch (ch) {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (ch < 0x20U) {
                    constexpr char KHex[] = "0123456789abcdef";
                    output += "\\u00";
                    output.push_back(KHex[(ch >> 4U) & 0x0FU]);
                    output.push_back(KHex[ch & 0x0FU]);
                } else {
                    output.push_back(static_cast<char>(ch));
                }
                break;
            }
        }
        return output;
    }

    template <typename T>
    static std::string _numberToString(T value) {
        if constexpr (std::is_floating_point_v<T>) {
            constexpr int KJsonFloatPrecision = std::numeric_limits<T>::digits10;

            std::ostringstream stream;
            stream << std::setprecision(KJsonFloatPrecision) << value;
            return stream.str();
        } else {
            char buffer[128];
            auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
            if (error == std::errc{}) {
                return {buffer, end};
            }

            std::ostringstream stream;
            stream << value;
            return stream.str();
        }
    }

    template <typename T>
    static void _setValue(Node& node, const T& value) {
        using U = std::remove_cvref_t<T>;
        node    = Node{};
        if constexpr (std::is_same_v<U, RawValue>) {
            node.kind  = Node::Kind::Raw;
            node.value = value.text;
        } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            node.kind  = Node::Kind::String;
            node.value = std::string{value};
        } else if constexpr (std::is_same_v<U, bool>) {
            node.kind  = Node::Kind::Scalar;
            node.value = value ? "true" : "false";
        } else if constexpr (std::is_integral_v<U>) {
            node.kind  = Node::Kind::Scalar;
            node.value = _numberToString(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            node.kind = Node::Kind::Scalar;
            if (std::isfinite(value)) {
                node.value = _numberToString(value);
            } else {
                node.kind = Node::Kind::Null;
            }
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported JSON text value type");
        }
    }

    static void _render(const Node& node, std::string& output) {
        switch (node.kind) {
        case Node::Kind::Null:
            output += "null";
            break;
        case Node::Kind::Scalar:
        case Node::Kind::Raw:
            output += node.value;
            break;
        case Node::Kind::String:
            output.push_back('"');
            output += _escape(node.value);
            output.push_back('"');
            break;
        case Node::Kind::Array:
            output.push_back('[');
            for (std::size_t i = 0; i < node.array.size(); ++i) {
                if (i != 0) {
                    output.push_back(',');
                }
                _render(node.array[i], output);
            }
            output.push_back(']');
            break;
        case Node::Kind::Object:
            output.push_back('{');
            for (std::size_t i = 0; i < node.objectNames.size(); ++i) {
                if (i != 0) {
                    output.push_back(',');
                }
                output.push_back('"');
                output += _escape(node.object(i).first);
                output += "\":";
                _render(node.object(i).second, output);
            }
            output.push_back('}');
            break;
        }
    }

private:
    Node mRoot;
};
} // namespace json
NEKO_END_NAMESPACE
