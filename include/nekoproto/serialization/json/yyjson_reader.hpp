#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_YYJSON)

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <yyjson.h>

#include "nekoproto/serialization/error.hpp"

namespace nekoproto {
namespace detail::yyjson {

#if NEKO_CPP_PLUS < 20
template <class T, class = void>
struct HasFromJsonObj : std::false_type {};

template <class T>
struct HasFromJsonObj<T, std::void_t<decltype(T::from_json_obj(std::declval<const yyjson_val*>()))>>
    : std::true_type {};
#endif

struct Reader {
    using InputArrayType  = const yyjson_val*;
    using InputObjectType = const yyjson_val*;
    using InputValueType  = const yyjson_val*;

    template <class T>
    static constexpr bool HasCustomConstructor =
#if NEKO_CPP_PLUS >= 20
        requires(InputValueType var) { T::from_json_obj(var); };
#else
        HasFromJsonObj<T>::value;
#endif

    static auto getFieldFromArray(const size_t idx, const InputArrayType array) noexcept -> sa::Result<InputValueType> {
        if (array == nullptr || !yyjson_is_arr(const_cast<yyjson_val*>(array))) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected array");
        }
        const size_t size = yyjson_arr_size(const_cast<yyjson_val*>(array));
        if (idx >= size) {
            return sa::error(sa::ErrorCode::InvalidIndex, "Index " + std::to_string(idx) + " out of range");
        }
        return yyjson_arr_get(const_cast<yyjson_val*>(array), idx);
    }

    static auto getFieldFromObject(const std::string_view name, const InputObjectType object) noexcept -> sa::Result<InputValueType> {
        if (object == nullptr || !yyjson_is_obj(const_cast<yyjson_val*>(object))) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected object");
        }
        auto* val = yyjson_obj_getn(const_cast<yyjson_val*>(object), name.data(), name.size());
        if (val != nullptr) {
            return val;
        }
        return sa::error(sa::ErrorCode::InvalidField, "Field " + std::string(name) + " not found");
    }

    static auto arraySize(const InputArrayType array) noexcept -> std::size_t {
        if (array == nullptr || !yyjson_is_arr(const_cast<yyjson_val*>(array))) {
            return 0;
        }
        return yyjson_arr_size(const_cast<yyjson_val*>(array));
    }

    static auto arrayElement(const InputArrayType array, std::size_t index) noexcept -> InputValueType {
        if (array == nullptr) {
            return nullptr;
        }
        return yyjson_arr_get(const_cast<yyjson_val*>(array), index);
    }

    template <typename Fn>
    static auto forEachArrayElement(const InputArrayType array, Fn&& fn) -> bool {
        if (array == nullptr || !yyjson_is_arr(const_cast<yyjson_val*>(array))) {
            return false;
        }
        size_t idx = 0;
        size_t max = 0;
        yyjson_val* val = nullptr;
        yyjson_arr_foreach(const_cast<yyjson_val*>(array), idx, max, val) {
            if (!fn(val)) {
                return false;
            }
        }
        return true;
    }

    static auto objectSize(const InputObjectType object) noexcept -> std::size_t {
        if (object == nullptr || !yyjson_is_obj(const_cast<yyjson_val*>(object))) {
            return 0;
        }
        return yyjson_obj_size(const_cast<yyjson_val*>(object));
    }

    static auto objectField(const InputObjectType object, std::string_view name) noexcept -> sa::Result<InputValueType> {
        return getFieldFromObject(name, object);
    }

    template <typename Fn>
    static auto forEachObjectMember(const InputObjectType object, Fn&& fn) -> bool {
        if (object == nullptr || !yyjson_is_obj(const_cast<yyjson_val*>(object))) {
            return false;
        }
        yyjson_val* key = nullptr;
        yyjson_obj_iter iter = yyjson_obj_iter_with(const_cast<yyjson_val*>(object));
        while ((key = yyjson_obj_iter_next(&iter))) {
            yyjson_val* val = yyjson_obj_iter_get_val(key);
            std::string_view name{yyjson_get_str(key), yyjson_get_len(key)};
            if (!fn(name, val)) {
                return false;
            }
        }
        return true;
    }

    static auto isEmpty(const InputValueType value) noexcept -> bool {
        return value == nullptr || yyjson_is_null(const_cast<yyjson_val*>(value));
    }

    static auto toRawString(InputValueType value) noexcept -> sa::Result<std::string> {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        size_t len = 0;
        char* str = yyjson_val_write(value, 0, &len);
        if (str == nullptr) {
            return sa::error(sa::ErrorCode::ParseError, "Failed to serialize yyjson value to raw string");
        }
        std::string result(str, len);
        free(str);
        return result;
    }

    template <typename CharT, typename Traits>
    static auto toStringView(InputValueType value) noexcept -> sa::Result<std::basic_string_view<CharT, Traits>> {
        if (value == nullptr || !yyjson_is_str(const_cast<yyjson_val*>(value))) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected string");
        }
        const char* str = yyjson_get_str(const_cast<yyjson_val*>(value));
        const size_t len = yyjson_get_len(const_cast<yyjson_val*>(value));
        return std::basic_string_view<CharT, Traits>{reinterpret_cast<const CharT*>(str), len};
    }

    template <typename T>
    static auto toBasicType(InputValueType value) noexcept -> sa::Result<T> {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        using U = std::remove_cvref_t<T>;
        auto* val = const_cast<yyjson_val*>(value);
        if constexpr (std::is_same_v<U, std::string>) {
            if (!yyjson_is_str(val)) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected string");
            }
            return std::string{yyjson_get_str(val), yyjson_get_len(val)};
        } else if constexpr (std::is_same_v<U, bool>) {
            if (!yyjson_is_bool(val)) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected bool");
            }
            return yyjson_get_bool(val);
        } else if constexpr (std::is_floating_point_v<U>) {
            if (yyjson_is_num(val)) {
                return static_cast<U>(yyjson_get_num(val));
            }
            return sa::error(sa::ErrorCode::InvalidType, "Expected float or double");
        } else if constexpr (std::is_unsigned_v<U>) {
            if (yyjson_is_uint(val)) {
                uint64_t num = yyjson_get_uint(val);
                if (num > std::numeric_limits<U>::max()) {
                    return sa::error(sa::ErrorCode::InvalidType, "Unsigned integer out of range");
                }
                return static_cast<U>(num);
            }
            if (yyjson_is_sint(val)) {
                int64_t num = yyjson_get_sint(val);
                if (num < 0 || static_cast<uint64_t>(num) > std::numeric_limits<U>::max()) {
                    return sa::error(sa::ErrorCode::InvalidType, "Unsigned integer out of range");
                }
                return static_cast<U>(num);
            }
            return sa::error(sa::ErrorCode::InvalidType, "Expected unsigned integer");
        } else if constexpr (std::is_integral_v<U>) {
            if (yyjson_is_sint(val)) {
                int64_t num = yyjson_get_sint(val);
                if (num < std::numeric_limits<U>::min() || num > std::numeric_limits<U>::max()) {
                    return sa::error(sa::ErrorCode::InvalidType, "Integer out of range");
                }
                return static_cast<U>(num);
            }
            if (yyjson_is_uint(val)) {
                uint64_t num = yyjson_get_uint(val);
                if (num > static_cast<uint64_t>(std::numeric_limits<U>::max())) {
                    return sa::error(sa::ErrorCode::InvalidType, "Integer out of range");
                }
                return static_cast<U>(num);
            }
            return sa::error(sa::ErrorCode::InvalidType, "Expected integer");
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported type in yyjson toBasicType");
        }
    }

    static auto toArray(InputValueType value) noexcept -> sa::Result<InputArrayType> {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        if (!yyjson_is_arr(const_cast<yyjson_val*>(value))) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast to array!");
        }
        return value;
    }

    static auto toObject(InputValueType value) noexcept -> sa::Result<InputObjectType> {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        if (!yyjson_is_obj(const_cast<yyjson_val*>(value))) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast to object!");
        }
        return value;
    }

    template <class T>
    static auto useCustomConstructor(InputValueType value) noexcept -> sa::Result<T> {
        try {
            return T::from_json_obj(value);
        } catch (const std::exception& e) {
            return sa::error(sa::ErrorCode::InvalidType, e.what());
        }
    }
};

} // namespace detail::yyjson
} // namespace nekoproto

#endif
