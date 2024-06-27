/**
 * @file proto_json_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "private/global.hpp"

#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <type_traits>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#include <variant>
#endif

NEKO_BEGIN_NAMESPACE

using JsonValue    = rapidjson::Value;
using JsonDocument = rapidjson::Document;

template <typename WriterT, typename ValueT, typename T, class enable = void>
struct JsonConvert;

// TODO: make it to be a interface ?
class OutBufferWrapper {
public:
    using Ch = char;

    OutBufferWrapper() NEKO_NOEXCEPT;
    OutBufferWrapper(std::vector<Ch>* vec) NEKO_NOEXCEPT;
    void setVector(std::vector<Ch>* vec) NEKO_NOEXCEPT;
    void Put(Ch c) NEKO_NOEXCEPT;
    void Flush() NEKO_NOEXCEPT;
    const Ch* GetString() const NEKO_NOEXCEPT;
    std::size_t GetSize() const NEKO_NOEXCEPT;
    void Clear() NEKO_NOEXCEPT;

private:
    std::shared_ptr<std::vector<Ch>> mVec;
};

inline OutBufferWrapper::OutBufferWrapper() NEKO_NOEXCEPT : mVec(new std::vector<Ch>()) {}
inline OutBufferWrapper::OutBufferWrapper(std::vector<Ch>* vec) NEKO_NOEXCEPT : mVec(vec, [](std::vector<Ch>* ptr) {}) {
}
inline void OutBufferWrapper::setVector(std::vector<Ch>* vec) NEKO_NOEXCEPT {
    if (vec != nullptr) {
        mVec.reset(vec, [](std::vector<Ch>* ptr) {});
    } else {
        mVec.reset(new std::vector<Ch>(), [](std::vector<Ch>* ptr) { delete ptr; });
    }
}
inline void OutBufferWrapper::Put(Ch c) NEKO_NOEXCEPT { mVec->push_back(c); }
inline void OutBufferWrapper::Flush() NEKO_NOEXCEPT {}
inline const OutBufferWrapper::Ch* OutBufferWrapper::GetString() const NEKO_NOEXCEPT { return mVec->data(); }
inline std::size_t OutBufferWrapper::GetSize() const NEKO_NOEXCEPT { return mVec->size(); }
inline void OutBufferWrapper::Clear() NEKO_NOEXCEPT { mVec->clear(); }

template <typename BufferT = OutBufferWrapper>
using JsonWriter = rapidjson::Writer<BufferT>;

class JsonSerializer {
public:
    using ValueType  = JsonValue;
    using WriterType = JsonWriter<>;

public:
    JsonSerializer() NEKO_NOEXCEPT;
    JsonSerializer(const JsonSerializer&) NEKO_NOEXCEPT;
    JsonSerializer(JsonSerializer&& other) NEKO_NOEXCEPT;
    explicit JsonSerializer(WriterType* writer) NEKO_NOEXCEPT;
    explicit JsonSerializer(const JsonValue& root) NEKO_NOEXCEPT;
    JsonSerializer& operator=(const JsonSerializer&) NEKO_NOEXCEPT;
    JsonSerializer& operator=(JsonSerializer&& other) NEKO_NOEXCEPT;
    ~JsonSerializer() NEKO_NOEXCEPT = default;

    /**
     * @brief start serialize
     * calling this function before traversing all fields to initialize the writer.
     * can serialize multiple objects to one buffer.
     *
     * @param[out] data the buf to output
     */
    void startSerialize(std::vector<char>* data) NEKO_NOEXCEPT;
    void startSerialize(const NEKO_STRING_VIEW& filename) NEKO_NOEXCEPT;
    /**
     * @brief end serialize
     *  while calling this function after traversing all fields.
     *  if success serialize all fields, return true, otherwise return false.
     *
     * @return true
     * @return false
     */
    bool endSerialize() NEKO_NOEXCEPT;
    /**
     * @brief insert value
     * for all fields, this function will be called by original class.
     * if success serialize this field, return true, otherwise return false.
     * @tparam T the type of value
     * @param[in] name the field name
     * @param[in] value the field value
     * @return true
     * @return false
     */
    template <typename T>
    bool insert(const char* name, const size_t len, const T& value);

    /**
     * @brief start deserialize
     *  while calling this function before traversing all fields,
     * can clear the last time deserialize states in this function and initialize the deserializor.
     * @param[in] data the buf to input
     * @return true
     * @return false
     */
    bool startDeserialize(const std::vector<char>& data) NEKO_NOEXCEPT;
    bool startDeserialize(const NEKO_STRING_VIEW& filename) NEKO_NOEXCEPT;
    /**
     * @brief end deserialize
     *  while calling this function after traversing all fields.
     *  if success deserialize all fields, return true, otherwise return false.
     * @return true
     * @return false
     */
    inline bool endDeserialize() NEKO_NOEXCEPT;
    /**
     * @brief get value
     * for all fields, this function will be called by original class.
     * if success deserialize this field, return true, otherwise return false.
     * @tparam T the type of value
     * @param[in] name the field name
     * @param[out] value the field value
     * @return true
     * @return false
     */
    template <typename T>
    bool get(const char* name, const size_t len, T* value);

private:
    JsonDocument mDocument;
    const JsonValue& mRoot;
    OutBufferWrapper mBuffer;
    std::unique_ptr<WriterType, void (*)(WriterType*)> mWriter;
};

inline JsonSerializer::JsonSerializer() NEKO_NOEXCEPT : mDocument(),
                                                        mRoot(JsonValue()),
                                                        mBuffer(),
                                                        mWriter(nullptr, [](WriterType* writer) {}) {}

inline JsonSerializer::JsonSerializer(const JsonSerializer&) NEKO_NOEXCEPT
    : mDocument(),
      mRoot(JsonValue()),
      mBuffer(),
      mWriter(nullptr, [](WriterType* writer) {}) {}

inline JsonSerializer::JsonSerializer(JsonSerializer&& other) NEKO_NOEXCEPT
    : mDocument(),
      mRoot(JsonValue()),
      mBuffer(),
      mWriter(nullptr, [](WriterType* writer) {}) {}

inline JsonSerializer::JsonSerializer(WriterType* writer) NEKO_NOEXCEPT
    : mDocument(),
      mRoot(JsonValue()),
      mBuffer(),
      mWriter(std::unique_ptr<WriterType, void (*)(WriterType*)>(writer, [](WriterType* writer) {})) {}

inline JsonSerializer::JsonSerializer(const JsonValue& root) NEKO_NOEXCEPT
    : mDocument(),
      mRoot(root),
      mBuffer(),
      mWriter(nullptr, [](WriterType* writer) {}) {}

inline JsonSerializer& JsonSerializer::operator=(const JsonSerializer&) NEKO_NOEXCEPT { return *this; }

inline JsonSerializer& JsonSerializer::operator=(JsonSerializer&& other) NEKO_NOEXCEPT { return *this; }

inline void JsonSerializer::startSerialize(std::vector<char>* data) NEKO_NOEXCEPT {
    if (!mWriter) {
        mBuffer.Clear();
        mBuffer.setVector(data);
        mWriter = std::unique_ptr<WriterType, void (*)(WriterType*)>(new WriterType(mBuffer),
                                                                     [](WriterType* writer) { delete writer; });
    }
    mWriter->StartObject();
}

inline void startSerialize(const NEKO_STRING_VIEW& filename) NEKO_NOEXCEPT {
    // TODO:
    NEKO_LOG_ERROR("not implemented");
}

inline bool JsonSerializer::endSerialize() NEKO_NOEXCEPT {
    mWriter->EndObject();
    if (!mWriter || !mWriter->IsComplete()) {
        mBuffer.Clear();
        return false;
    }
    mBuffer.setVector(nullptr);
    return true;
}

#if NEKO_CPP_PLUS >= 17
namespace {
template <typename T, class enable = void>
struct is_optional : std::false_type {
    using value_type = std::remove_reference_t<T>;
};

template <typename T>
struct is_optional<std::optional<T>, void> : std::true_type {
    using value_type = std::remove_reference_t<T>;
};
} // namespace

template <typename T>
bool JsonSerializer::insert(const char* name, const size_t len, const T& value) {
    if constexpr (is_optional<T>::value) {
        if (!value.has_value()) {
            return true;
        }
        if (!mWriter->Key(name, len)) {
            return false;
        }
        return JsonConvert<WriterType, ValueType, typename is_optional<T>::value_type>::toJsonValue(*mWriter, *value);
    } else {
        if (!mWriter->Key(name, len)) {
            return false;
        }
        return JsonConvert<WriterType, ValueType, T>::toJsonValue(*mWriter, value);
    }
}

template <typename T>
bool JsonSerializer::get(const char* name, const size_t len, T* value) {
    NEKO_ASSERT(value != nullptr, "{} value is null in get value function.", std::string(name, len));
    if constexpr (is_optional<T>::value) {
        std::string name_str(name, len);
        if (!mDocument.IsNull() && mDocument.HasMember(name_str.c_str())) {
            typename is_optional<T>::value_type tmp;
            if (JsonConvert<WriterType, ValueType, typename is_optional<T>::value_type>::fromJsonValue(
                    &tmp, mDocument[name_str.c_str()])) {
                *value = std::move(tmp);
                return true;
            }
            // this is a optional field, so, even a type error is not considered an error.
            NEKO_LOG_WARN("{} value is error type in json.", std::string(name, len));
        }
        if (!mRoot.IsNull() && mRoot.HasMember(name_str.c_str())) {
            typename is_optional<T>::value_type tmp;
            if (JsonConvert<WriterType, ValueType, typename is_optional<T>::value_type>::fromJsonValue(
                    &tmp, mRoot[name_str.c_str()])) {
                *value = std::move(tmp);
                return true;
            }
            // this is a optional field, so, even a type error is not considered an error.
            NEKO_LOG_WARN("{} value is error type in json.", std::string(name, len));
        }
        (*value).reset();
        return true;
    } else {
        std::string name_str(name, len);
        if (!mDocument.IsNull() && mDocument.HasMember(name_str.c_str())) {
            if (JsonConvert<WriterType, ValueType, T>::fromJsonValue(value, mDocument[name_str.c_str()])) {
                return true;
            }
            // For non-optional fields, continued use is undefined behavior because the value cannot be obtained
            // correctly.
            NEKO_LOG_WARN("{} value is error type in json.", std::string(name, len));
            return false;
        }
        if (!mRoot.IsNull() && mRoot.HasMember(name_str.c_str())) {
            if (JsonConvert<WriterType, ValueType, T>::fromJsonValue(value, mRoot[name_str.c_str()])) {
                return true;
            }
            // For non-optional fields, continued use is undefined behavior because the value cannot be obtained
            // correctly.
            NEKO_LOG_WARN("{} value is error type in json.", std::string(name, len));
            return false;
        }
        // For non-optional fields, continued use is undefined behavior because the value cannot be obtained
        // correctly.
        NEKO_LOG_WARN("{} value is not find in json.", std::string(name, len));
        return false;
    }
}
#else
template <typename T>
bool JsonSerializer::insert(const char* name, const size_t len, const T& value) {
    if (!mWriter->Key(name, len)) {
        return false;
    }
    return JsonConvert<WriterType, ValueType, T>::toJsonValue(*mWriter, value);
}

template <typename T>
bool JsonSerializer::get(const char* name, const size_t len, T* value) {
    std::string name_str(name, len);
    if (!mDocument.IsNull() && mDocument.HasMember(name_str.c_str())) {
        return JsonConvert<WriterType, ValueType, T>::fromJsonValue(value, mDocument[name_str.c_str()]);
    }
    if (!mRoot.IsNull() && mRoot.HasMember(name_str.c_str())) {
        return JsonConvert<WriterType, ValueType, T>::fromJsonValue(value, mRoot[name_str.c_str()]);
    }
    return false;
}
#endif

inline bool JsonSerializer::startDeserialize(const NEKO_STRING_VIEW& filename) NEKO_NOEXCEPT {
    std::ifstream file(filename.data(), std::ios::binary);
    if (!file.is_open()) {
        NEKO_LOG_ERROR("open file {} failed", filename.data());
        return false;
    }
    rapidjson::IStreamWrapper stream(file);
    mDocument.ParseStream(stream);
    if (mDocument.HasParseError()) {
        NEKO_LOG_ERROR("parse error {}", (int)mDocument.GetParseError());
        return false;
    }
    return true;
}

inline bool JsonSerializer::startDeserialize(const std::vector<char>& data) NEKO_NOEXCEPT {
    mDocument.Parse(data.data(), data.size());
    if (mDocument.HasParseError()) {
        NEKO_LOG_ERROR("parse error {}", (int)mDocument.GetParseError());
        return false;
    }
    return true;
}

inline bool JsonSerializer::endDeserialize() NEKO_NOEXCEPT {
    mDocument.SetNull();
    mDocument.GetAllocator().Clear();
    return true;
}

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, int, void> {
    static bool toJsonValue(WriterT& writer, const int value) { return writer.Int(value); }
    static bool fromJsonValue(int* dst, const ValueT& value) {
        if (!value.IsInt() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetInt();
        return true;
    }
};

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, unsigned int, void> {
    static bool toJsonValue(WriterT& writer, const unsigned int value) { return writer.Uint(value); }
    static bool fromJsonValue(unsigned int* dst, const ValueT& value) {
        if (!value.IsUint() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetUint();
        return true;
    }
};

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, int64_t, void> {
    static bool toJsonValue(WriterT& writer, const int64_t value) { return writer.Int64(value); }
    static bool fromJsonValue(int64_t* dst, const ValueT& value) {
        if (!value.IsInt64() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetInt64();
        return true;
    }
};

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, uint64_t, void> {
    static bool toJsonValue(WriterT& writer, const uint64_t value) { return writer.Uint64(value); }
    static bool fromJsonValue(uint64_t* dst, const ValueT& value) {
        if (!value.IsUint64() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetUint64();
        return true;
    }
};

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, std::string, void> {
    static bool toJsonValue(WriterT& writer, const std::string& value) {
        return writer.String(value.c_str(), value.size(), true);
    }
    static bool fromJsonValue(std::string* dst, const ValueT& value) {
        if (!value.IsString() || dst == nullptr) {
            return false;
        }
        (*dst) = std::string(value.GetString(), value.GetStringLength());
        return true;
    }
};

#if NEKO_CPP_PLUS >= 20
template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, std::u8string, void> {
    static bool toJsonValue(WriterT& writer, const std::u8string value) {
        return writer.String(reinterpret_cast<const char*>(value.data()), value.size(), true);
    }
    static bool fromJsonValue(std::u8string* dst, const ValueT& value) {
        if (!value.IsString() || dst == nullptr) {
            return false;
        }
        (*dst) = std::u8string(reinterpret_cast<const char8_t*>(value.GetString()), value.GetStringLength());
        return true;
    }
};
#endif
template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, std::vector<T>, void> {
    static bool toJsonValue(WriterT& writer, const std::vector<T>& value) {
        bool ret = writer.StartArray();
        for (auto& v : value) {
            ret = JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, v) && ret;
        }
        ret = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(std::vector<T>* dst, const ValueT& value) {
        if (!value.IsArray() || dst == nullptr) {
            return false;
        }
        dst->clear();
        for (auto& v : value.GetArray()) {
            T t;
            if (!JsonConvert<WriterT, ValueT, T>::fromJsonValue(&t, v)) {
                return false;
            }
            dst->push_back(t);
        }
        return true;
    }
};

template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<
    WriterT, ValueT, T,
    typename std::enable_if<std::is_same<typename T::ProtoType::SerializerType, JsonSerializer>::value>::type> {
    static bool toJsonValue(WriterT& writer, const T& value) {
        auto jsonS = JsonSerializer(&writer);
        writer.StartObject();
        auto ret = value.serialize(jsonS);
        writer.EndObject();
        return ret;
    }
    static bool fromJsonValue(T* dst, const ValueT& value) {
        if (!value.IsObject() || dst == nullptr) {
            return false;
        }
        auto jsonS = JsonSerializer(value);
        return dst->deserialize(jsonS);
    }
};

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, bool, void> {
    static bool toJsonValue(WriterT& writer, const bool value) { return writer.Bool(value); }
    static bool fromJsonValue(bool* dst, const ValueT& value) {
        if (!value.IsBool() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetBool();
        return true;
    }
};

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, double, void> {
    static bool toJsonValue(WriterT& writer, const double value) { return writer.Double(value); }
    static bool fromJsonValue(double* dst, const ValueT& value) {
        if (!value.IsDouble() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetDouble();
        return true;
    }
};

template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, float, void> {
    static bool toJsonValue(WriterT& writer, const float value) { return writer.Double(value); }
    static bool fromJsonValue(float* dst, const ValueT& value) {
        if (!value.IsDouble() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetDouble();
        return true;
    }
};

#if NEKO_CPP_PLUS >= 17
template <typename WriterT, typename ValueT, typename... Ts>
struct JsonConvert<WriterT, ValueT, std::variant<Ts...>, void> {
    template <typename T, size_t N>
    static bool toJsonValueImp(WriterT& writer, const std::variant<Ts...>& value) {
        if (value.index() != N)
            return false;
        return JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, std::get<N>(value));
    }

    template <size_t... Ns>
    static bool unfoldToJsonValue(WriterT& writer, const std::variant<Ts...>& value, std::index_sequence<Ns...>) {
        return (toJsonValueImp<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(writer, value) || ...);
    }

    static bool toJsonValue(WriterT& writer, const std::variant<Ts...>& value) {
        return unfoldToJsonValue(writer, value, std::make_index_sequence<sizeof...(Ts)>());
    }

    template <typename T, size_t N>
    static bool fromJsonValueImp(std::variant<Ts...>* dst, const ValueT& value) {
        T tmp = {};
        if (JsonConvert<WriterT, ValueT, T>::fromJsonValue(&tmp, value)) {
            *dst = tmp;
            return true;
        }
        return false;
    }

    template <size_t... Ns>
    static bool unfoldFromJsonValue(std::variant<Ts...>* dst, const ValueT& value, std::index_sequence<Ns...>) {
        return (fromJsonValueImp<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(dst, value) || ...);
    }

    static bool fromJsonValue(std::variant<Ts...>* dst, const ValueT& value) {
        NEKO_ASSERT(dst != nullptr, "[qBittorrent] std::variant dst is nullptr in from json value");
        return unfoldFromJsonValue(dst, value, std::make_index_sequence<sizeof...(Ts)>());
    }
};
#endif

NEKO_END_NAMESPACE