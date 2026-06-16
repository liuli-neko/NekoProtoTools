#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "nekoproto/global/log.hpp"

#include <limits>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <type_traits>

#ifdef _WIN32
#pragma push_macro("GetObject")
#ifdef GetObject
#undef GetObject
#endif
#endif

#include "nekoproto/serialization/error.hpp"

NEKO_BEGIN_NAMESPACE
namespace rapid {
#if NEKO_CPP_PLUS < 20
namespace detail {
template <class T, class = void>
struct has_from_json_obj : std::false_type {};

template <class T>
struct has_from_json_obj<T, std::void_t<decltype(T::from_json_obj(std::declval<Reader::InputValueType>()))>>
    : std::true_type {};
} // namespace detail
#endif

struct Reader {
    using InputArrayType  = rapidjson::Value::ConstArray;
    using InputObjectType = rapidjson::Value::ConstObject;
    using InputValueType  = const rapidjson::Value*;

    template <class T>
    static constexpr bool HasCustomConstructor =
#if NEKO_CPP_PLUS >= 20
        requires(InputValueType var) { T::from_json_obj(var); };
#else
        detail::has_from_json_obj<T>::value;
#endif

    static sa::Result<InputValueType> getFieldFromArray(const size_t idx, const InputArrayType& array) noexcept {
        if (idx >= array.Size()) {
            return sa::error(sa::ErrorCode::InvalidIndex, "Index " + std::to_string(idx) + " out of range");
        }
        return &array[idx];
    }

    static sa::Result<InputValueType> getFieldFromObject(const std::string_view name,
                                                         const InputObjectType& object) noexcept {
        auto it = object.FindMember(rapidjson::Value(name.data(), name.size()));
        if (it != object.MemberEnd()) {
            return &it->value;
        }
        return sa::error(sa::ErrorCode::InvalidField, "Field " + std::string(name) + " not found");
    }

    static std::size_t arraySize(const InputArrayType& array) noexcept { return array.Size(); }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) noexcept {
        return &array[static_cast<rapidjson::SizeType>(index)];
    }

    static std::size_t objectSize(const InputObjectType& object) noexcept { return object.MemberCount(); }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) noexcept {
        return getFieldFromObject(name, object);
    }

    template <typename Fn>
    static bool forEachObjectMember(const InputObjectType& object, Fn&& fn) {
        for (const auto& member : object) {
            std::string_view name{member.name.GetString(), member.name.GetStringLength()};
            if (!fn(name, &member.value)) {
                return false;
            }
        }
        return true;
    }

    static bool isEmpty(const InputValueType& value) noexcept { return value == nullptr || value->IsNull(); }

    static sa::Result<std::string> toRawString(InputValueType value) noexcept {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!value->Accept(writer)) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not dump raw JSON value");
        }
        return std::string{buffer.GetString(), buffer.GetSize()};
    }

    template <typename CharT, typename Traits>
    static sa::Result<std::basic_string_view<CharT, Traits>> toStringView(InputValueType value) noexcept {
        if (value == nullptr || !value->IsString()) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected string");
        }
        return std::basic_string_view<CharT, Traits>{reinterpret_cast<const CharT*>(value->GetString()),
                                                     value->GetStringLength()};
    }

    template <typename T>
    static sa::Result<T> toBasicType(InputValueType value) noexcept {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
            if (!value->IsString()) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected string");
            }
            return std::string{value->GetString(), value->GetStringLength()};
        } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            if (!value->IsBool()) {
                return sa::error(sa::ErrorCode::InvalidType, "Expected bool");
            }
            return value->GetBool();
        } else if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>) {
            if (value->IsNumber()) {
                return value->GetDouble();
            }
            return sa::error(sa::ErrorCode::InvalidType, "Expected float or double");
        } else if constexpr (std::is_unsigned_v<std::remove_cvref_t<T>>) {
            uint64_t val = 0;
            if (value->IsUint()) {
                val = value->GetUint();
            } else if (value->IsUint64()) {
                val = value->GetUint64();
            } else {
                return sa::error(sa::ErrorCode::InvalidType, "Expected unsigned int");
            }
            using U = std::remove_cvref_t<T>;
            if (val > std::numeric_limits<U>::max()) {
                return sa::error(sa::ErrorCode::InvalidType, "Unsigned integer out of range");
            }
            return static_cast<U>(val);
        } else if constexpr (std::is_integral_v<std::remove_cvref_t<T>>) {
            int64_t val = 0;
            if (value->IsInt()) {
                val = value->GetInt();
            } else if (value->IsInt64()) {
                val = value->GetInt64();
            } else {
                return sa::error(sa::ErrorCode::InvalidType, "Expected integer");
            }
            using I = std::remove_cvref_t<T>;
            if (val < std::numeric_limits<I>::min() || val > std::numeric_limits<I>::max()) {
                return sa::error(sa::ErrorCode::InvalidType, "integer out of range");
            }
            return static_cast<I>(val);
        } else {
            static_assert(std::is_same_v<std::remove_cvref_t<T>, bool>, "Unsupported type");
        }
    }

    template <class ArrayReader>
    sa::Error readArray(const ArrayReader& reader, const InputArrayType& value) const noexcept {
        const auto size = value.Size();
        for (rapidjson::SizeType i = 0; i < size; ++i) {
            const auto err = reader.read(&value[i]);
            if (err) {
                return err;
            }
        }
        return sa::error(sa::ErrorCode::Ok);
    }

    template <class ObjectReader>
    sa::Error readObject(const ObjectReader& reader, const InputObjectType& value) const noexcept {
        for (const auto& member : value) {
            std::string name = {member.name.GetString(), member.name.GetStringLength()};
            const auto err   = reader.read(name, &member.value);
            if (err) {
                return err;
            }
        }
        return sa::error(sa::ErrorCode::Ok);
    }

    static sa::Result<InputArrayType> toArray(InputValueType value) noexcept {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        if (!value->IsArray()) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast to array!");
        }
        return value->GetArray();
    }

    static sa::Result<InputObjectType> toObject(InputValueType value) noexcept {
        if (value == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "value is null");
        }
        if (!value->IsObject()) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast to object!");
        }
        return value->GetObject();
    }

    template <class T>
    static sa::Result<T> useCustomConstructor(InputValueType value) noexcept {
        try {
            return T::from_json_obj(value);
        } catch (const std::exception& e) {
            return sa::error(sa::ErrorCode::InvalidType, e.what());
        }
    }
};
} // namespace rapid
NEKO_END_NAMESPACE
#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif
#endif
