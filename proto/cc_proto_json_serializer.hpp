#pragma once

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <vector>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

using JsonValue = rapidjson::Value;
using JsonDocument = rapidjson::Document;

template <typename WriterT, typename ValueT, typename T, class enable = void>
struct JsonConvert;

class VectorBuffer {
public:
    using Ch = char;

    inline VectorBuffer() : mVec(new std::vector<Ch>()), mIsOwner(true) {}
    inline VectorBuffer(std::vector<Ch>* vec) : mVec(vec), mIsOwner(false) {}
    inline void setVector(std::vector<Ch>* vec) { 
        if (vec != nullptr) {
            if (mIsOwner) {
                delete mVec;
                mIsOwner = false;
            }
            mVec = vec;
        } else {
            if (!mIsOwner) {
                mVec = new std::vector<Ch>();
                mIsOwner = true;
            }
        }
    }
    inline void Put(Ch c) {
        mVec->push_back(c);
    }
    inline void Flush() {}
    inline const Ch* GetString() const {
        return mVec->data();
    }
    inline std::size_t GetSize() const {
        return mVec->size();
    }
    inline void Clear() {
        mVec->clear();
    }

private:
    std::vector<Ch>* mVec = nullptr;
    bool mIsOwner = false;
};

template<typename BufferT = VectorBuffer>
using JsonWriter = rapidjson::Writer<BufferT>;

template<typename BufferT = VectorBuffer>
class JsonSerializer {
public:
    using ValueType = JsonValue;
    using BufferType = BufferT;
    using WriterType = JsonWriter<BufferT>;

public:
    JsonSerializer();
    JsonSerializer(const JsonSerializer&);
    JsonSerializer(JsonSerializer&& other);
    JsonSerializer(WriterType* writer);
    JsonSerializer(const JsonValue& root);
    JsonSerializer& operator=(const JsonSerializer&);
    JsonSerializer& operator=(JsonSerializer&& other);
    ~JsonSerializer() = default;

    /**
     * @brief start serialize
     *  while calling this function before traversing all fields,
     * can clear the buffer in this function or initialize the buffer and serializor.
     *
     * @param[out] data the buf to output
     */
    void startSerialize(std::vector<char>* data);
    /**
     * @brief end serialize
     *  while calling this function after traversing all fields.
     *  if success serialize all fields, return true, otherwise return false.
     *
     * @return true
     * @return false
     */
    bool endSerialize();
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
    const JsonValue& mRoot;
    VectorBuffer mBuffer;
    std::unique_ptr<WriterType, void (*)(WriterType*)> mWriter;
};

template<typename BufferT>
inline JsonSerializer<BufferT>::JsonSerializer()
    : mDocument(), mRoot(JsonValue()), mBuffer(), mWriter(nullptr, [](WriterType* writer) {}) {}

template<typename BufferT>
inline JsonSerializer<BufferT>::JsonSerializer(const JsonSerializer&)
    : mDocument(), mRoot(JsonValue()), mBuffer(), mWriter(nullptr, [](WriterType* writer) {}) {}

template<typename BufferT>
inline JsonSerializer<BufferT>::JsonSerializer(JsonSerializer&& other)
    : mDocument(), mRoot(JsonValue()), mBuffer(), mWriter(nullptr, [](WriterType* writer) {}) {}

template<typename BufferT>
inline JsonSerializer<BufferT>::JsonSerializer(WriterType* writer)
    : mDocument(),
      mRoot(JsonValue()),
      mBuffer(),
      mWriter(std::unique_ptr<WriterType, void (*)(WriterType*)>(writer, [](WriterType* writer) {})) {}

template<typename BufferT>
inline JsonSerializer<BufferT>::JsonSerializer(const JsonValue& root)
    : mDocument(), mRoot(root), mBuffer(), mWriter(nullptr, [](WriterType* writer) {}) {}

template<typename BufferT>
inline JsonSerializer<BufferT>& JsonSerializer<BufferT>::operator=(const JsonSerializer&) { return *this; }

template<typename BufferT>
inline JsonSerializer<BufferT>& JsonSerializer<BufferT>::operator=(JsonSerializer&& other) { return *this; }

template<typename BufferT>
inline void JsonSerializer<BufferT>::startSerialize(std::vector<char> *data) {
    if (!mWriter) {
        mBuffer.Clear();
        mBuffer.setVector(data);
        mWriter = std::unique_ptr<WriterType, void (*)(WriterType*)>(new WriterType(mBuffer),
                                                                     [](WriterType* writer) { delete writer; });
    }
    mWriter->StartObject();
}

template<typename BufferT>
inline bool JsonSerializer<BufferT>::endSerialize() {
    mWriter->EndObject();
    if (!mWriter || !mWriter->IsComplete()) {
        mBuffer.Clear();
        return false;
    }
    mBuffer.setVector(nullptr);
    return true;
}

template<typename BufferT>
template <typename T>
bool JsonSerializer<BufferT>::insert(const char* name, const size_t len, const T& value) {
    if (!mWriter->Key(name, len)) {
        return false;
    }
    return JsonConvert<WriterType, ValueType, T>::toJsonValue(*mWriter, value);
}

template<typename BufferT>
inline bool JsonSerializer<BufferT>::startDeserialize(const std::vector<char>& data) {
    mDocument.Parse(data.data(), data.size());
    if (mDocument.HasParseError()) {
        CS_LOG_ERROR("parse error {}", (int)mDocument.GetParseError());
        return false;
    }
    return true;
}

template<typename BufferT>
inline bool JsonSerializer<BufferT>::endDeserialize() {
    mDocument.SetNull();
    mDocument.GetAllocator().Clear();
    return true;
}

template<typename BufferT>
template <typename T>
bool JsonSerializer<BufferT>::get(const char* name, const size_t len, T* value) {
    std::string name_str(name, len);
    if (!mDocument.IsNull() && mDocument.HasMember(name_str.c_str())) {
        return JsonConvert<WriterType, ValueType, T>::fromJsonValue(value, mDocument[name_str.c_str()]);
    }
    if (!mRoot.IsNull() && mRoot.HasMember(name_str.c_str())) {
        return JsonConvert<WriterType, ValueType, T>::fromJsonValue(value, mRoot[name_str.c_str()]);
    }
    return false;
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

#if CS_CPP_PLUS >= 20
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
struct JsonConvert<WriterT, ValueT, T, typename std::enable_if<std::is_same<typename T::SerializerType, JsonSerializer<typename T::SerializerType::BufferType>>::value>::type> {
    static bool toJsonValue(WriterT& writer, const T& value) {
        auto jsonS = typename T::SerializerType(&writer);
        writer.StartObject();
        auto ret = value.serialize(jsonS);
        writer.EndObject();
        return ret;
    }
    static bool fromJsonValue(T* dst, const ValueT& value) {
        if (!value.IsObject() || dst == nullptr) {
            return false;
        }
        auto jsonS = typename T::SerializerType(value);
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

CS_PROTO_END_NAMESPACE