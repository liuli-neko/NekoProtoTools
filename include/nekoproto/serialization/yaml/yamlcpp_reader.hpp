#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_YAMLCPP)

#include "nekoproto/serialization/error.hpp"

#include <yaml-cpp/yaml.h>

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
namespace yamlcpp {

class Reader {
public:
    struct InputArrayType {
        YAML::Node node;
    };

    struct InputObjectType {
        YAML::Node node;
    };

    using InputValueType = YAML::Node;

    static std::size_t arraySize(const InputArrayType& array) noexcept { return array.node.size(); }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) {
        if (!array.node || !array.node.IsSequence() || index >= array.node.size()) {
            return {};
        }
        return array.node[index];
    }

    static std::size_t objectSize(const InputObjectType& object) noexcept { return object.node.size(); }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) {
        if (!object.node || !object.node.IsMap()) {
            return sa::error(sa::ErrorCode::InvalidType, "YAML mapping is empty");
        }
        auto node = object.node[std::string{name}];
        if (node && node.IsDefined()) {
            return node;
        }
        return sa::error(sa::ErrorCode::InvalidField, "Field '" + std::string{name} + "' not found");
    }

    template <typename Fn>
    static bool forEachObjectMember(const InputObjectType& object, Fn&& fn) {
        if (!object.node || !object.node.IsMap()) {
            return false;
        }
        for (auto it = object.node.begin(); it != object.node.end(); ++it) {
            auto key = _scalarView(it->first);
            if (!key) {
                return false;
            }
            if (!fn(key.value(), it->second)) {
                return false;
            }
        }
        return true;
    }

    static bool isEmpty(InputValueType value) noexcept {
        return !value || !value.IsDefined() || value.IsNull();
    }

    template <typename CharT, typename Traits>
    static sa::Result<std::basic_string_view<CharT, Traits>> toStringView(InputValueType value) {
        static_assert(sizeof(CharT) == sizeof(char), "YAML string views require byte-sized characters");
        auto text = _scalarView(value);
        if (!text) {
            return text.error();
        }
        return std::basic_string_view<CharT, Traits>{reinterpret_cast<const CharT*>(text.value().data()),
                                                     text.value().size()};
    }

    template <typename T>
    static sa::Result<T> toBasicType(InputValueType value) {
        using U = std::remove_cvref_t<T>;
        auto text = _scalarView(value);
        if (!text) {
            return text.error();
        }
        if constexpr (std::is_same_v<U, std::string>) {
            return std::string{text.value()};
        } else if constexpr (std::is_same_v<U, bool>) {
            if (text.value() == "true" || text.value() == "True" || text.value() == "TRUE" || text.value() == "1") {
                return true;
            }
            if (text.value() == "false" || text.value() == "False" || text.value() == "FALSE" ||
                text.value() == "0") {
                return false;
            }
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast '" + std::string{text.value()} + "' to boolean");
        } else if constexpr (std::is_integral_v<U>) {
            U parsed{};
            const auto [end, error] =
                std::from_chars(text.value().data(), text.value().data() + text.value().size(), parsed);
            if (error != std::errc{} || end != text.value().data() + text.value().size()) {
                return sa::error(sa::ErrorCode::InvalidType, "Could not cast '" + std::string{text.value()} + "' to integer");
            }
            return parsed;
        } else if constexpr (std::is_floating_point_v<U>) {
            const auto owned = std::string{text.value()};
            char* end        = nullptr;
            errno            = 0;
            const long double parsed = std::strtold(owned.c_str(), &end);
            if (errno == ERANGE || end != owned.c_str() + owned.size() || !std::isfinite(parsed) ||
                parsed < static_cast<long double>(std::numeric_limits<U>::lowest()) ||
                parsed > static_cast<long double>(std::numeric_limits<U>::max())) {
                return sa::error(sa::ErrorCode::InvalidType,
                                 "Could not cast '" + owned + "' to floating point value");
            }
            return static_cast<U>(parsed);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported YAML scalar type");
        }
    }

    static sa::Result<InputArrayType> toArray(InputValueType value) {
        if (!value || !value.IsSequence()) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast YAML node to sequence");
        }
        return InputArrayType{value};
    }

    static sa::Result<InputObjectType> toObject(InputValueType value) {
        if (!value || !value.IsMap()) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast YAML node to mapping");
        }
        return InputObjectType{value};
    }

private:
    static sa::Result<std::string_view> _scalarView(InputValueType value) {
        if (isEmpty(value)) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected scalar, got null");
        }
        if (!value.IsScalar()) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected YAML scalar");
        }
        const auto& scalar = value.Scalar();
        return std::string_view{scalar.data(), scalar.size()};
    }
};

} // namespace yamlcpp
NEKO_END_NAMESPACE

#endif
