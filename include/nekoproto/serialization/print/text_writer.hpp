#pragma once

#include "nekoproto/global/global.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nekoproto {
namespace print {

struct Reader {
    struct InputArrayType {};
    struct InputObjectType {};
    using InputValueType = std::nullptr_t;
};

class TextWriter {
private:
    struct Node {
        enum class Kind { Null, Scalar, String, Array, Object };

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

        auto object(int index) const -> std::pair<std::string_view, const Node&> {
            return {objectNames[index], objectValues[index]};
        }
    };

public:
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

    auto arrayAsRoot(std::size_t size) -> OutputArrayType {
        initializeArray(mRoot, size);
        return {&mRoot};
    }

    auto objectAsRoot(std::size_t size) -> OutputObjectType {
        initializeObject(mRoot, size);
        return {&mRoot};
    }

    auto nullAsRoot() -> OutputValueType {
        mRoot = Node{};
        return {&mRoot};
    }

    template <typename T>
    auto valueAsRoot(const T& value) -> OutputValueType {
        setValue(mRoot, value);
        return {&mRoot};
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

    static auto addObjectToObject(std::string_view name, std::size_t size, OutputObjectType* parent) -> OutputObjectType {
        auto& child = parent->value->emplaceObject(std::string{name}, Node{}).second;
        initializeObject(child, size);
        return {&child};
    }

    template <typename T>
    static auto addValueToArray(const T& value, OutputArrayType* parent) -> OutputValueType {
        auto& child = parent->value->array.emplace_back();
        setValue(child, value);
        return {&child};
    }

    template <typename T>
    static auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) -> OutputValueType {
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
        render(mRoot, output);
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
        output.reserve(value.size());
        for (const char ch : value) {
            switch (ch) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
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
                output.push_back(ch);
                break;
            }
        }
        return output;
    }

    template <typename T>
    static auto numberToString(T value) -> std::string {
        if constexpr (std::is_floating_point_v<T>) {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<T>::digits10) << value;
            return stream.str();
        } else {
            char buffer[128];
            const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
            if (error == std::errc{}) {
                return {buffer, end};
            }
            return {};
        }
    }

    template <typename T>
    static void setValue(Node& node, const T& value) {
        using U = std::remove_cvref_t<T>;
        node    = Node{};
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            node.kind  = Node::Kind::String;
            node.value = std::string{value};
        } else if constexpr (std::is_same_v<U, bool>) {
            node.kind  = Node::Kind::Scalar;
            node.value = value ? "true" : "false";
        } else if constexpr (std::is_integral_v<U>) {
            node.kind  = Node::Kind::Scalar;
            node.value = numberToString(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            node.kind  = std::isfinite(value) ? Node::Kind::Scalar : Node::Kind::Null;
            node.value = numberToString(value);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported printable value type");
        }
    }

    static void render(const Node& node, std::string& output) {
        switch (node.kind) {
        case Node::Kind::Null:
            output += "null";
            break;
        case Node::Kind::Scalar:
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
                    output += ", ";
                }
                render(node.array[i], output);
            }
            output.push_back(']');
            break;
        case Node::Kind::Object:
            output.push_back('{');
            for (std::size_t i = 0; i < node.objectNames.size(); ++i) {
                if (i != 0) {
                    output += ", ";
                }
                output += node.object(i).first;
                output += " = ";
                render(node.object(i).second, output);
            }
            output.push_back('}');
            break;
        }
    }

private:
    Node mRoot;
};

} // namespace print
} // namespace nekoproto
