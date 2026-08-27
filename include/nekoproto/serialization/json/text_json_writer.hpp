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

namespace nekoproto {
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

        std::vector<std::string> object_names;
        std::vector<Node>        object_values;

        auto emplaceObject(const std::string& name, Node&& node) {
            object_names.push_back(name);
            object_values.push_back(std::move(node));
            return std::pair<std::string_view, Node&>(object_names.back(), object_values.back());
        }

        void reserveObject(size_t size) {
            object_names.reserve(size);
            object_values.reserve(size);
        }

        auto object(int index) const -> std::pair<std::string_view, const Node&> {
            return {object_names[index], object_values[index]};
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

    void reset() { root_ = Node{}; }

    auto arrayAsRoot(std::size_t size) -> OutputArrayType {
        initializeArray(root_, size);
        return {&root_};
    }

    auto objectAsRoot(std::size_t size) -> OutputObjectType {
        initializeObject(root_, size);
        return {&root_};
    }

    auto nullAsRoot() -> OutputValueType {
        root_ = Node{};
        return {&root_};
    }

    template <typename T>
    auto valueAsRoot(const T& value) -> OutputValueType {
        setValue(root_, value);
        return {&root_};
    }

    static auto addArrayToArray(std::size_t size, OutputArrayType* parent) -> OutputArrayType {
        auto& child = parent->value->array.emplace_back();
        initializeArray(child, size);
        return {&child};
    }

    static auto addArrayToObject(std::string_view name, std::size_t size, OutputObjectType* parent) -> OutputArrayType {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        initializeArray(child, size);
        return {&child};
    }

    static auto addObjectToArray(std::size_t size, OutputArrayType* parent) -> OutputObjectType {
        auto& child = parent->value->array.emplace_back();
        initializeObject(child, size);
        return {&child};
    }

    static auto addObjectToObject(std::string_view name, std::size_t size, OutputObjectType* parent)
        -> OutputObjectType {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        initializeObject(child, size);
        return {&child};
    }

    template <typename T>
    auto addValueToArray(const T& value, OutputArrayType* parent) -> OutputValueType {
        auto& child = parent->value->array.emplace_back();
        setValue(child, value);
        return {&child};
    }

    template <typename T>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) -> OutputValueType {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        setValue(child, value);
        return {&child};
    }

    static auto addNullToArray(OutputArrayType* parent) -> OutputValueType {
        auto& child = parent->value->array.emplace_back();
        return {&child};
    }

    static auto addNullToObject(std::string_view name, OutputObjectType* parent) -> OutputValueType {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        return {&child};
    }

    auto str() const -> std::string {
        std::string output;
        render(root_, output);
        return output;
    }

private:
    static void initializeArray(Node& node, std::size_t size) {
        node      = Node{};
        node.kind = Node::Kind::Array;
        if (size != static_cast<std::size_t>(-1)) {
            node.array.reserve(size);
        }
    }

    static void initializeObject(Node& node, std::size_t size) {
        node      = Node{};
        node.kind = Node::Kind::Object;
        if (size != static_cast<std::size_t>(-1)) {
            node.reserveObject(size);
        }
    }

    static auto escape(std::string_view value) -> std::string {
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
    static auto numberToString(T value) -> std::string {
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
    static void setValue(Node& node, const T& value) {
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
            node.value = numberToString(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            node.kind = Node::Kind::Scalar;
            if (std::isfinite(value)) {
                node.value = numberToString(value);
            } else {
                node.kind = Node::Kind::Null;
            }
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported JSON text value type");
        }
    }

    static void render(const Node& node, std::string& output) {
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
            output += escape(node.value);
            output.push_back('"');
            break;
        case Node::Kind::Array:
            output.push_back('[');
            for (std::size_t i = 0; i < node.array.size(); ++i) {
                if (i != 0) {
                    output.push_back(',');
                }
                render(node.array[i], output);
            }
            output.push_back(']');
            break;
        case Node::Kind::Object:
            output.push_back('{');
            for (std::size_t i = 0; i < node.object_names.size(); ++i) {
                if (i != 0) {
                    output.push_back(',');
                }
                output.push_back('"');
                output += escape(node.object(i).first);
                output += "\":";
                render(node.object(i).second, output);
            }
            output.push_back('}');
            break;
        }
    }

private:
    Node root_;
};
} // namespace json
} // namespace nekoproto
