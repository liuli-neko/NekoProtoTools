#pragma once

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <vector>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

using JsonValue = rapidjson::Value;
using JsonDocument = rapidjson::Document;
using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

template <typename T, class enable = void>
struct JsonConvert;

class JsonSerializer {
public:
    JsonSerializer() = default;
    JsonSerializer(const JsonSerializer&);
    JsonSerializer(JsonSerializer&& other);
    JsonSerializer& operator=(const JsonSerializer&);
    JsonSerializer& operator=(JsonSerializer&& other);
    ~JsonSerializer() = default;

    /**
     * @brief start serialize
     *  while calling this function before traversing all fields, 
     * can clear the buffer in this function or initialize the buffer and serializor.
     */
    void startSerialize();
    /**
     * @brief end serialize
     *  while calling this function after traversing all fields.
     *  if success serialize all fields, return true, otherwise return false.
     * 
     * @param[out] data the buf to output
     * @return true 
     * @return false 
     */
    bool endSerialize(std::vector<char>* data);
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
    bool startDeserialize(const std::vector<char>& data);
    /**
     * @brief start deserialize
     * if want to start with a custom value, can't overload this function.
     * @param[in] data the value to input
     * @return true 
     * @return false 
     */
    bool startDeserialize(const JsonValue& data);
    /**
     * @brief end deserialize
     *  while calling this function after traversing all fields.
     *  if success deserialize all fields, return true, otherwise return false.
     * @return true 
     * @return false
     */
    inline bool endDeserialize();
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
    rapidjson::StringBuffer mBuffer;
    std::unique_ptr<JsonWriter> mWriter;
};

inline JsonSerializer::JsonSerializer(const JsonSerializer&) {}

inline JsonSerializer::JsonSerializer(JsonSerializer&& other) {}

inline JsonSerializer& JsonSerializer::operator=(const JsonSerializer&) { return *this; }

inline JsonSerializer& JsonSerializer::operator=(JsonSerializer&& other) { return *this; }

inline void JsonSerializer::startSerialize() {
    if (!mWriter) {
        mBuffer.Clear();
        mWriter.reset(new JsonWriter(mBuffer));
    }
    mWriter->StartObject();
}

inline bool JsonSerializer::endSerialize(std::vector<char>* data) {
    mWriter->EndObject();
    if (!mWriter || !mWriter->IsComplete()) {
        mBuffer.Clear();
        return false;
    }
    if (data) {
        (*data) = std::vector<char>(mBuffer.GetString(), mBuffer.GetString() + mBuffer.GetSize());
    }
    mBuffer.Clear();
    return true;
}

template <typename T>
bool JsonSerializer::insert(const char* name,const size_t len, const T& value) {
    if (!mWriter->Key(name, len)) {
        return false;
    }
    return JsonConvert<T>::toJsonValue(*mWriter, value);
}

inline bool JsonSerializer::startDeserialize(const std::vector<char>& data) {
    mDocument.Parse(data.data(), data.size());
    if (mDocument.HasParseError()) {
        CS_LOG_ERROR("parse error %d", (int)mDocument.GetParseError());
        return false;
    }
    return true;
}

inline bool JsonSerializer::startDeserialize(const JsonValue& data) {
    mDocument.CopyFrom(data, mDocument.GetAllocator());
    return true;
}

inline bool JsonSerializer::endDeserialize() {
    mDocument.SetNull();
    mDocument.GetAllocator().Clear();
    return true;
}

template <typename T>
bool JsonSerializer::get(const char* name, const size_t len, T* value) {
    std::string name_str(name, len);
    if (!mDocument.HasMember(name_str.c_str())) {
        return false;
    }
    return JsonConvert<T>::fromJsonValue(value, mDocument[name_str.c_str()]);
}

template <>
struct JsonConvert<int, void> {
    static bool toJsonValue(JsonWriter& writer, const int value) { return writer.Int(value); }
    static bool fromJsonValue(int* dst, const JsonValue& value) {
        if (!value.IsInt() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetInt();
        return true;
    }
};

template <>
struct JsonConvert<unsigned int, void> {
    static bool toJsonValue(JsonWriter& writer, const unsigned int value) { return writer.Uint(value); }
    static bool fromJsonValue(unsigned int* dst, const JsonValue& value) {
        if (!value.IsUint() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetUint();
        return true;
    }
};

template <>
struct JsonConvert<int64_t, void> {
    static bool toJsonValue(JsonWriter& writer, const int64_t value) { return writer.Int64(value); }
    static bool fromJsonValue(int64_t* dst, const JsonValue& value) {
        if (!value.IsInt64() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetInt64();
        return true;
    }
};

template <>
struct JsonConvert<uint64_t, void> {
    static bool toJsonValue(JsonWriter& writer, const uint64_t value) { return writer.Uint64(value); }
    static bool fromJsonValue(uint64_t* dst, const JsonValue& value) {
        if (!value.IsUint64() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetUint64();
        return true;
    }
};

template <>
struct JsonConvert<std::string, void> {
    static bool toJsonValue(JsonWriter& writer, const std::string& value) {
        return writer.String(value.c_str(), value.size(), true);
    }
    static bool fromJsonValue(std::string* dst, const JsonValue& value) {
        if (!value.IsString() || dst == nullptr) {
            return false;
        }
        (*dst) = std::string(value.GetString(), value.GetStringLength());
        return true;
    }
};

#if CS_CPP_PLUS >= 20
template <>
struct JsonConvert<std::u8string, void> {
    static bool toJsonValue(JsonWriter& writer, const std::u8string value) {
        return writer.String(reinterpret_cast<const char *>(value.data()), value.size(), true);
    }
    static bool fromJsonValue(std::u8string* dst, const JsonValue& value) {
        if (!value.IsString() || dst == nullptr) {
            return false;
        }
        (*dst) = std::u8string(reinterpret_cast<const char8_t *>(value.GetString()), value.GetStringLength());
        return true;
    }
};
#endif
template <typename T>
struct JsonConvert<std::vector<T>, void> {
    static bool toJsonValue(JsonWriter& writer, const std::vector<T>& value) {
        bool ret = writer.StartArray();
        for (auto& v : value) {
            ret = JsonConvert<T>::toJsonValue(writer, v) && ret;
        }
        ret = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(std::vector<T>* dst, const JsonValue& value) {
        if (!value.IsArray() || dst == nullptr) {
            return false;
        }
        dst->clear();
        for (auto& v : value.GetArray()) {
            T t;
            if (!JsonConvert<T>::fromJsonValue(&t, v)) {
                return false;
            }
            dst->push_back(t);
        }
        return true;
    }
};

template <typename T>
struct JsonConvert<T, typename std::enable_if<std::is_same<typename T::SerializerType, JsonSerializer>::value>::type> {
    static bool toJsonValue(JsonWriter& writer, const T& value) {
        auto data = value.toData();
        return writer.RawValue(data.data(), data.size(), rapidjson::kObjectType);
    }
    static bool fromJsonValue(T* dst, const JsonValue& value) {
        if (!value.IsObject() || dst == nullptr) {
            return false;
        }
        return dst->fromData(value);
    }
};

template <>
struct JsonConvert<bool, void> {
    static bool toJsonValue(JsonWriter& writer, const bool value) { return writer.Bool(value); }
    static bool fromJsonValue(bool* dst, const JsonValue& value) {
        if (!value.IsBool() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetBool();
        return true;
    }
};

template <>
struct JsonConvert<double, void> {
    static bool toJsonValue(JsonWriter& writer, const double value) { return writer.Double(value); }
    static bool fromJsonValue(double* dst, const JsonValue& value) {
        if (!value.IsDouble() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetDouble();
        return true;
    }
};

template <>
struct JsonConvert<float, void> {
    static bool toJsonValue(JsonWriter& writer, const float value) { return writer.Double(value); }
    static bool fromJsonValue(float* dst, const JsonValue& value) {
        if (!value.IsDouble() || dst == nullptr) {
            return false;
        }
        (*dst) = value.GetDouble();
        return true;
    }
};

CS_PROTO_END_NAMESPACE