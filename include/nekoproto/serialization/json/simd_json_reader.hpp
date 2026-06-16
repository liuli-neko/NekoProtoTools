#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)

#include "nekoproto/serialization/error.hpp"

#include <limits>
#include <memory>
#include <simdjson.h>
#include <string>
#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
namespace detail::simd {

using JsonParser = simdjson::dom::parser;

struct InputValue {
    simdjson::dom::element value;
    std::shared_ptr<JsonParser> owner;
};

struct InputArray {
    simdjson::dom::array value;
    std::shared_ptr<JsonParser> owner;
};

struct InputObject {
    simdjson::dom::object value;
    std::shared_ptr<JsonParser> owner;
};

struct Reader {
    using InputArrayType  = InputArray;
    using InputObjectType = InputObject;
    using InputValueType  = InputValue;

    static std::size_t arraySize(const InputArrayType& array) noexcept { return array.value.size(); }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) noexcept {
        return {array.value.at(index).value_unsafe(), array.owner};
    }

    static std::size_t objectSize(const InputObjectType& object) noexcept { return object.value.size(); }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) noexcept {
        auto value = object.value.at_key(name);
        if (value.error() != simdjson::SUCCESS) {
            return sa::error(sa::ErrorCode::InvalidField,
                             "Field '" + std::string(name) + "' not found: " +
                                 simdjson::error_message(value.error()));
        }
        return InputValueType{value.value_unsafe(), object.owner};
    }

    template <typename Fn>
    static bool forEachObjectMember(const InputObjectType& object, Fn&& fn) {
        for (const auto field : object.value) {
            if (!fn(field.key, InputValueType{field.value, object.owner})) {
                return false;
            }
        }
        return true;
    }

    static bool isEmpty(const InputValueType& value) noexcept { return value.value.is_null(); }

    static sa::Result<std::string> toRawString(const InputValueType& value) noexcept {
        try {
            return simdjson::minify(value.value);
        } catch (const simdjson::simdjson_error& error) {
            return sa::error(sa::ErrorCode::ParseError, error.what());
        }
    }

    template <typename CharT, typename Traits>
    static sa::Result<std::basic_string_view<CharT, Traits>> toStringView(const InputValueType& value) noexcept {
        auto string = value.value.get_string();
        if (string.error() != simdjson::SUCCESS) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected string");
        }
        const auto view = string.value_unsafe();
        return std::basic_string_view<CharT, Traits>{reinterpret_cast<const CharT*>(view.data()), view.size()};
    }

    template <typename T>
    static sa::Result<T> toBasicType(const InputValueType& input) noexcept {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string>) {
            auto value = input.value.get_string();
            if (value.error() != simdjson::SUCCESS) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected string");
            }
            return std::string{value.value_unsafe()};
        } else if constexpr (std::is_same_v<U, bool>) {
            auto value = input.value.get_bool();
            if (value.error() != simdjson::SUCCESS) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected bool");
            }
            return value.value_unsafe();
        } else if constexpr (std::is_floating_point_v<U>) {
            auto value = input.value.get_double();
            if (value.error() != simdjson::SUCCESS) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected float or double");
            }
            return static_cast<U>(value.value_unsafe());
        } else if constexpr (std::is_unsigned_v<U>) {
            auto value = input.value.get_uint64();
            if (value.error() != simdjson::SUCCESS) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected unsigned integer");
            }
            if (value.value_unsafe() > std::numeric_limits<U>::max()) {
                return sa::error(sa::ErrorCode::InvalidType, "Unsigned integer out of range");
            }
            return static_cast<U>(value.value_unsafe());
        } else if constexpr (std::is_integral_v<U>) {
            auto value = input.value.get_int64();
            if (value.error() != simdjson::SUCCESS) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected integer");
            }
            if (value.value_unsafe() < std::numeric_limits<U>::min() ||
                value.value_unsafe() > std::numeric_limits<U>::max()) {
                return sa::error(sa::ErrorCode::InvalidType, "Integer out of range");
            }
            return static_cast<U>(value.value_unsafe());
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported simdjson basic type");
        }
    }

    static sa::Result<InputArrayType> toArray(const InputValueType& input) noexcept {
        auto value = input.value.get_array();
        if (value.error() != simdjson::SUCCESS) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected array");
        }
        return InputArrayType{value.value_unsafe(), input.owner};
    }

    static sa::Result<InputObjectType> toObject(const InputValueType& input) noexcept {
        auto value = input.value.get_object();
        if (value.error() != simdjson::SUCCESS) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected object");
        }
        return InputObjectType{value.value_unsafe(), input.owner};
    }
};

} // namespace detail::simd
NEKO_END_NAMESPACE

#endif
