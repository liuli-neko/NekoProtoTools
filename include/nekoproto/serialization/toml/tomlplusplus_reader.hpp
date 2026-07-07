#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)

#if defined(TOML_EXCEPTIONS) && TOML_EXCEPTIONS
#error "NekoProto TOML backend requires toml++ with TOML_EXCEPTIONS=0"
#endif
#ifndef TOML_EXCEPTIONS
#define TOML_EXCEPTIONS 0
#endif

#include "nekoproto/serialization/error.hpp"

#include <toml++/toml.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace tomlplusplus {

class Reader {
public:
    struct InputArrayType {
        const toml::array* node = nullptr;
    };

    struct InputObjectType {
        const toml::table* node = nullptr;
    };

    using InputValueType = const toml::node*;

    static std::size_t arraySize(const InputArrayType& array) noexcept {
        return array.node == nullptr ? 0U : array.node->size();
    }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) noexcept {
        if (array.node == nullptr || index >= array.node->size()) {
            return nullptr;
        }
        return &(*array.node)[index];
    }

    static std::size_t objectSize(const InputObjectType& object) noexcept {
        return object.node == nullptr ? 0U : object.node->size();
    }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) noexcept {
        if (object.node == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "TOML table is empty");
        }
        auto it = object.node->find(name);
        if (it != object.node->end()) {
            return &it->second;
        }
        return sa::error(sa::ErrorCode::InvalidField, "Field '" + std::string{name} + "' not found");
    }

    template <typename Fn>
    static bool forEachObjectMember(const InputObjectType& object, Fn&& fn) {
        if (object.node == nullptr) {
            return false;
        }
        for (const auto& [key, value] : *object.node) {
            if (!fn(key.str(), &value)) {
                return false;
            }
        }
        return true;
    }

    static bool isEmpty(InputValueType value) noexcept { return value == nullptr; }

    template <typename CharT, typename Traits>
    static sa::Result<std::basic_string_view<CharT, Traits>> toStringView(InputValueType value) noexcept {
        static_assert(sizeof(CharT) == sizeof(char), "TOML string views require byte-sized characters");
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected TOML string, got empty node");
        }
        auto text = value->value<std::string_view>();
        if (!text) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected TOML string");
        }
        return std::basic_string_view<CharT, Traits>{reinterpret_cast<const CharT*>(text->data()), text->size()};
    }

    template <typename T>
    static sa::Result<T> toBasicType(InputValueType value) noexcept {
        using U = std::remove_cvref_t<T>;
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "TOML node is empty");
        }
        if constexpr (std::is_same_v<U, std::string>) {
            auto text = value->value<std::string>();
            if (!text) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected TOML string");
            }
            return std::move(*text);
        } else if constexpr (std::is_same_v<U, bool>) {
            auto boolean = value->value_exact<bool>();
            if (!boolean) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected TOML boolean");
            }
            return *boolean;
        } else if constexpr (std::is_integral_v<U>) {
            auto integer = value->value_exact<std::int64_t>();
            if (!integer) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected TOML integer");
            }
            if constexpr (std::is_unsigned_v<U>) {
                if (*integer < 0 || static_cast<std::uint64_t>(*integer) > std::numeric_limits<U>::max()) {
                    return sa::error(sa::ErrorCode::InvalidType, "TOML integer is out of range");
                }
            } else {
                if (*integer < static_cast<std::int64_t>(std::numeric_limits<U>::lowest()) ||
                    *integer > static_cast<std::int64_t>(std::numeric_limits<U>::max())) {
                    return sa::error(sa::ErrorCode::InvalidType, "TOML integer is out of range");
                }
            }
            return static_cast<U>(*integer);
        } else if constexpr (std::is_floating_point_v<U>) {
            auto number = value->value<double>();
            if (!number || !std::isfinite(*number) ||
                *number < static_cast<double>(std::numeric_limits<U>::lowest()) ||
                *number > static_cast<double>(std::numeric_limits<U>::max())) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected TOML floating point value");
            }
            return static_cast<U>(*number);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported TOML basic type");
        }
    }

    static sa::Result<InputArrayType> toArray(InputValueType value) noexcept {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "TOML node is empty");
        }
        auto* array = value->as_array();
        if (array == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast TOML node to array");
        }
        return InputArrayType{array};
    }

    static sa::Result<InputObjectType> toObject(InputValueType value) noexcept {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "TOML node is empty");
        }
        auto* table = value->as_table();
        if (table == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast TOML node to table");
        }
        return InputObjectType{table};
    }
};

} // namespace tomlplusplus

NEKO_END_NAMESPACE

#endif
