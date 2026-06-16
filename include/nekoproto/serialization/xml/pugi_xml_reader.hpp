#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_PUGIXML)

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/xml/pugi_xml_writer.hpp"

#include <pugixml.hpp>

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

NEKO_BEGIN_NAMESPACE
namespace xml {

class Reader {
public:
    struct InputArrayType {
        pugi::xml_node first;
        std::string name;
        bool empty = false;
    };

    struct InputObjectType {
        pugi::xml_node node;
    };

    struct InputValueType {
        std::variant<pugi::xml_node, pugi::xml_attribute> value = pugi::xml_node{};
        bool unwrapArrayContainer                               = false;

        InputValueType() = default;
        InputValueType(pugi::xml_node node, bool unwrap = false) : value(node), unwrapArrayContainer(unwrap) {}
        InputValueType(pugi::xml_attribute attribute) : value(attribute) {}
    };

    static std::size_t arraySize(const InputArrayType& array) noexcept {
        if (array.empty || !array.first) {
            return 0;
        }
        std::size_t size = 0;
        for (auto node = array.first; node != nullptr; node = node.next_sibling(array.name.c_str())) {
            ++size;
        }
        return size;
    }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) noexcept {
        auto node = array.first;
        while ((node != nullptr) && index > 0) {
            node = node.next_sibling(array.name.c_str());
            --index;
        }
        return InputValueType{node, true};
    }

    static std::size_t objectSize(const InputObjectType& object) noexcept {
        std::size_t size = 0;
        for (auto child = object.node.first_child(); child != nullptr; child = child.next_sibling()) {
            if (child.type() == pugi::node_element) {
                ++size;
            }
        }
        for (auto attribute = object.node.first_attribute(); attribute != nullptr; attribute = attribute.next_attribute()) {
            if (!_isInternalAttribute(attribute.name())) {
                ++size;
            }
        }
        return size;
    }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) noexcept {
        if (name == XmlContent) {
            return InputValueType{object.node};
        }
        const auto ownedName = std::string{name};
        if (auto child = object.node.child(ownedName.c_str())) {
            return InputValueType{child};
        }
        if (auto attribute = object.node.attribute(ownedName.c_str())) {
            return InputValueType{attribute};
        }
        return sa::error(sa::ErrorCode::InvalidField, "Field '" + std::string{name} + "' not found");
    }

    template <typename Fn>
    static bool forEachObjectMember(const InputObjectType& object, Fn&& fn) {
        for (auto child = object.node.first_child(); child; child = child.next_sibling()) {
            if (child.type() == pugi::node_element && !fn(std::string_view{child.name()}, InputValueType{child})) {
                return false;
            }
        }
        for (auto attribute = object.node.first_attribute(); attribute; attribute = attribute.next_attribute()) {
            if (!_isInternalAttribute(attribute.name()) &&
                !fn(std::string_view{attribute.name()}, InputValueType{attribute})) {
                return false;
            }
        }
        return true;
    }

    static bool isEmpty(const InputValueType& input) noexcept {
        return std::visit(
            [](const auto& value) {
                using U = std::remove_cvref_t<decltype(value)>;
                if (!value) {
                    return true;
                }
                if constexpr (std::is_same_v<U, pugi::xml_node>) {
                    const auto marker = value.attribute(NullMarker.data());
                    return marker && std::string_view{marker.value()} == "true";
                } else {
                    return std::string_view{value.value()} == "null";
                }
            },
            input.value);
    }

    template <typename CharT, typename Traits>
    static sa::Result<std::basic_string_view<CharT, Traits>> toStringView(const InputValueType& input) noexcept {
        static_assert(sizeof(CharT) == sizeof(char), "XML string views require byte-sized characters");
        if (isEmpty(input)) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected string, got null");
        }
        const auto text = _valueView(input);
        return std::basic_string_view<CharT, Traits>{reinterpret_cast<const CharT*>(text.data()), text.size()};
    }

    template <typename T>
    static sa::Result<T> toBasicType(const InputValueType& input) noexcept {
        using U = std::remove_cvref_t<T>;
        if (isEmpty(input)) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected value, got null");
        }
        const auto text = _valueView(input);
        if constexpr (std::is_same_v<U, std::string>) {
            return std::string{text};
        } else if constexpr (std::is_same_v<U, bool>) {
            if (text == "true" || text == "1") {
                return true;
            }
            if (text == "false" || text == "0") {
                return false;
            }
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast '" + std::string{text} + "' to boolean");
        } else if constexpr (std::is_integral_v<U>) {
            U value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size()) {
                return sa::error(sa::ErrorCode::InvalidType, "Could not cast '" + std::string{text} + "' to integer");
            }
            return value;
        } else if constexpr (std::is_floating_point_v<U>) {
            const auto owned         = std::string{text};
            char* end                = nullptr;
            errno                    = 0;
            const long double parsed = std::strtold(owned.c_str(), &end);
            if (errno == ERANGE || end != owned.c_str() + owned.size() || !std::isfinite(parsed) ||
                parsed < static_cast<long double>(std::numeric_limits<U>::lowest()) ||
                parsed > static_cast<long double>(std::numeric_limits<U>::max())) {
                return sa::error(sa::ErrorCode::InvalidType, "Could not cast '" + owned + "' to floating point value");
            }
            return static_cast<U>(parsed);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported XML basic type");
        }
    }

    static sa::Result<InputArrayType> toArray(const InputValueType& input) noexcept {
        auto node = _asNode(input);
        if (!node) {
            return node.error();
        }

        const auto marker = node.value().attribute(ArrayMarker.data());
        if ((marker != nullptr) && std::string_view{marker.value()} == ArrayEmpty) {
            return InputArrayType{{}, {}, true};
        }
        if ((marker != nullptr) && std::string_view{marker.value()} == ArrayContainer && input.unwrapArrayContainer) {
            auto first = node.value().first_child();
            while ((first != nullptr) && first.type() != pugi::node_element) {
                first = first.next_sibling();
            }
            if (!first) {
                return InputArrayType{{}, {}, true};
            }
            const auto firstMarker = first.attribute(ArrayMarker.data());
            if ((firstMarker != nullptr) && std::string_view{firstMarker.value()} == ArrayEmpty) {
                return InputArrayType{{}, {}, true};
            }
            return InputArrayType{first, first.name(), false};
        }
        return InputArrayType{node.value(), node.value().name(), false};
    }

    static sa::Result<InputObjectType> toObject(const InputValueType& input) noexcept {
        auto node = _asNode(input);
        if (!node) {
            return node.error();
        }
        return InputObjectType{node.value()};
    }

private:
    static bool _isInternalAttribute(std::string_view name) noexcept {
        return name == ArrayMarker || name == NullMarker;
    }

    static std::string_view _valueView(const InputValueType& input) noexcept {
        return std::visit(
            [](const auto& value) -> std::string_view {
                using U = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<U, pugi::xml_node>) {
                    const auto* text = value.child_value();
                    return {text, std::strlen(text)};
                } else {
                    const auto* text = value.value();
                    return {text, std::strlen(text)};
                }
            },
            input.value);
    }

    static sa::Result<pugi::xml_node> _asNode(const InputValueType& input) noexcept {
        if (const auto* node = std::get_if<pugi::xml_node>(&input.value)) {
            if (*node != nullptr) {
                return *node;
            }
            return sa::error(sa::ErrorCode::InvalidType, "XML node is empty");
        }
        const auto& attribute = std::get<pugi::xml_attribute>(input.value);
        return sa::error(sa::ErrorCode::InvalidType, "Field '" + std::string{attribute.name()} + "' is an attribute");
    }
};

} // namespace xml
NEKO_END_NAMESPACE

#endif
