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
#include <rapidjson/rapidjson.h>
#include <rapidjson/writer.h>
#include <type_traits>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#include <variant>
#endif

#include "serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

using JsonValue    = rapidjson::Value;
using JsonDocument = rapidjson::Document;

template <typename WriterT, typename ValueT, typename T, class enable = void>
struct JsonConvert;

template <typename ValueType, class enable>
class JsonDeserializer;

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

template <typename BufferType = std::vector<char>, typename Writer = OutBufferWrapper>
class JsonInputSerializer;

namespace {
inline auto makeJsonBufferWrapper(std::vector<char>& data) -> OutBufferWrapper { return OutBufferWrapper(&data); }
auto makeJsonBufferWrapper(OutBufferWrapper& bufferWrapper) -> OutBufferWrapper& { return bufferWrapper; }

template <typename Writer>
inline bool saveValue(FieldSize const& value, Writer& writer) {
    (void)value;
    (void)writer;
    return true;
}

template <typename Writer>
inline bool saveValue(const int16_t value, Writer& writer) {
    return writer.Int(value);
}

template <typename Writer>
inline bool saveValue(const uint16_t value, Writer& writer) {
    return writer.Uint(value);
}

template <typename Writer>
inline bool saveValue(const int32_t value, Writer& writer) {
    return writer.Int(value);
}

template <typename Writer>
inline bool saveValue(const uint32_t value, Writer& writer) {
    return writer.Uint(value);
}

template <typename Writer>
inline bool saveValue(const int64_t value, Writer& writer) {
    return writer.Int64(value);
}

template <typename Writer>
inline bool saveValue(const uint64_t value, Writer& writer) {
    return writer.Uint64(value);
}

template <typename Writer>
inline bool saveValue(const float value, Writer& writer) {
    return writer.Double(value);
}

template <typename Writer>
inline bool saveValue(const double value, Writer& writer) {
    return writer.Double(value);
}

template <typename Writer>
inline bool saveValue(const bool value, Writer& writer) {
    return writer.Bool(value);
}

template <typename Writer>
inline bool saveValue(const std::string& value, Writer& writer) {
    return writer.String(value.c_str(), value.size());
}

template <typename Writer>
inline bool saveValue(const char* value, Writer& writer) {
    return writer.String(value);
}

#if NEKO_CPP_PLUS >= 17
template <typename Writer>
inline bool saveValue(const std::string_view value, Writer& writer) {
    return writer.String(value.data(), value.size());
}
#endif

template <typename JsonValue>
inline bool loadValue(std::string& value, const JsonValue& json) {
    if (!json.IsString())
        return false;
    value = {json.GetString(), json.GetStringLength()};
    return true;
}

template <typename JsonValue>
inline bool loadValue(int16_t& value, const JsonValue& json) {
    if (!json.IsInt())
        return false;
    value = json.GetInt();
    return true;
}

template <typename JsonValue>
inline bool loadValue(int32_t& value, const JsonValue& json) {
    if (!json.IsInt())
        return false;
    value = json.GetInt();
    return true;
}

template <typename JsonValue>
inline bool loadValue(int64_t& value, const JsonValue& json) {
    if (!json.IsInt64())
        return false;
    value = json.GetInt64();
    return true;
}

template <typename JsonValue>
inline bool loadValue(uint16_t& value, const JsonValue& json) {
    if (!json.IsUint())
        return false;
    value = json.GetUint();
    return true;
}

template <typename JsonValue>
inline bool loadValue(uint32_t& value, const JsonValue& json) {
    if (!json.IsUint())
        return false;
    value = json.GetUint();
    return true;
}

template <typename JsonValue>
inline bool loadValue(uint64_t& value, const JsonValue& json) {
    if (!json.IsUint64())
        return false;
    value = json.GetUint64();
    return true;
}

template <typename JsonValue>
inline bool loadValue(float& value, const JsonValue& json) {
    if (!json.IsNumber())
        return false;
    value = json.GetFloat();
    return true;
}

template <typename JsonValue>
inline bool loadValue(double& value, const JsonValue& json) {
    if (!json.IsNumber())
        return false;
    value = json.GetDouble();
    return true;
}

template <typename JsonValue>
inline bool loadValue(bool& value, const JsonValue& json) {
    if (!json.IsBool())
        return false;
    value = json.GetBool();
    return true;
}

template <typename JsonValue>
inline bool loadValue(FieldSize& value, const JsonValue& json) {
    if (!json.IsArray())
        return false;
    value.size = json.Size();
    return true;
}

} // namespace

template <typename BufferType, typename Writer>
class JsonInputSerializer {
public:
    using BufferWrapperType = decltype(makeJsonBufferWrapper(std::declval<BufferType&>()));
    using WriterType        = Writer;

private:
    template <typename T, typename G, class enable = void>
    struct can_save : std::false_type {};

    template <typename T, typename G>
    struct can_save<
        T, G,
        typename std::enable_if<std::is_same<
            decltype(::NekoProto::saveValue(std::declval<const T&>(), std::declval<G&>())), bool>::value>::type>
        : std::true_type {};

    template <typename T, class enable = void>
    struct selector {
        template <char = 0>
        static inline bool processImp(JsonInputSerializer& self, const T& value) {
            return save(self, value);
        }
    };
    template <typename T>
    struct selector<NamedField<T>, void> {
        template <char = 0>
        static inline bool processImp(JsonInputSerializer& self, const NamedField<T>& value) {
            return self.saveNamedValue<T>(value);
        }
    };
    template <typename T>
    struct selector<T, typename std::enable_if<can_save<T, WriterType>::value>::type> {
        template <char = 0>
        static inline bool processImp(JsonInputSerializer& self, const T& value) {
            return self.saveValue(value);
        }
    };

    template <typename T>
    struct selector<T, typename std::enable_if<can_serialize<T>::value>::type> {
        template <char = 0>
        static inline bool processImp(JsonInputSerializer& self, const T& value) {
            self.startObject();
            auto ret = value.serialize(self);
            self.endObject();
            return ret;
        }
    };

public:
    JsonInputSerializer(BufferType& out) NEKO_NOEXCEPT : mBuffer(makeJsonBufferWrapper(out)), mWriter(mBuffer) {
        mWriter.StartObject();
    }
    JsonInputSerializer(const JsonInputSerializer& other) NEKO_NOEXCEPT : mBuffer(other.mBuffer),
                                                                          mWriter(other.mWriter) {}
    JsonInputSerializer(JsonInputSerializer&& other) NEKO_NOEXCEPT : mBuffer(std::move(other.mBuffer)),
                                                                     mWriter(std::move(other.mWriter)) {}
    template <typename T>
    inline bool saveValue(const T& value) {
        return ::NekoProto::saveValue(value, mWriter);
    }
    template <typename T>
    inline bool saveNamedValue(const NamedField<T>& value) {
        return mWriter.Key(value.name, value.nameLen) && this->operator()(value.value);
    }

    bool startArray(const std::size_t size) { return mWriter.StartArray(); }
    bool endArray() { return mWriter.EndArray(); }
    bool startObject() { return mWriter.StartObject(); }
    bool endObject() { return mWriter.EndObject(); }
    template <typename T>
    bool operator()(const T& value) {
        return selector<T>::processImp(*this, value);
    }

    bool end() {
        auto ret = mWriter.EndObject();
        mWriter.Flush();
        return ret;
    }

private:
    JsonInputSerializer& operator=(const JsonInputSerializer&) = delete;
    JsonInputSerializer& operator=(JsonInputSerializer&&)      = delete;

private:
    WriterType mWriter;
    BufferWrapperType mBuffer;
};

template <typename ValueType = JsonDocument, class enable = void>
class JsonDeserializer {
public:
    using NodeType = JsonDocument;

    template <typename T, typename G, class enable1 = void>
    struct can_load : std::false_type {};

    template <typename T, typename G>
    struct can_load<
        T, G,
        typename std::enable_if<std::is_same<
            decltype(::NekoProto::loadValue(std::declval<T&>(), std::declval<const G&>())), bool>::value>::type>
        : std::true_type {};

    template <typename U, typename T, class enable2 = void>
    struct selector {
        template <char = 0>
        static inline bool doLoad(U& self, T& value) {
            return load(self, value);
        }
    };

    template <typename U>
    struct selector<U, FieldSize, void> {
        template <char = 0>
        static inline bool doLoad(U& self, FieldSize& value) {
            value.size = self.size();
            return true;
        }
    };

    template <typename U, typename T>
    struct selector<U, NamedField<T>, void> {
        template <char = 0>
        static inline bool doLoad(U& self, const NamedField<T>& value) {
            return self.loadNamedValue(value);
        }
    };
    template <typename U, typename T>
    struct selector<U, T, typename std::enable_if<can_load<T, NodeType>::value>::type> {
        template <char = 0>
        static inline bool doLoad(U& self, T& value) {
            return self.loadValue(value);
        }
    };

    template <typename U, typename T>
    struct selector<U, T, typename std::enable_if<can_serialize<T>::value>::type> {
        template <char = 0>
        static inline bool doLoad(U& self, T& value) {
            return value.deserialize(self);
        }
    };

public:
    JsonDeserializer(const std::vector<char>& buf) {
        mNode.Parse(buf.data(), buf.size());
        mIter = mNode.MemberBegin();
    }
    JsonDeserializer(const char* buf, std::size_t size) {
        mNode.Parse(buf, size);
        mIter = mNode.MemberBegin();
    }
    JsonDeserializer(rapidjson::IStreamWrapper& stream) {
        mNode.ParseStream(stream);
        mIter = mNode.MemberBegin();
    }
    JsonDeserializer(JsonDeserializer&& other) NEKO_NOEXCEPT : mNode(std::move(other.mNode)),
                                                               mIter(std::move(other.mIter)) {}

    operator bool() const { return mNode.GetParseError() == rapidjson::kParseErrorNone; }
    std::size_t size() const { return mNode.Size(); }
    std::string name() const {
        if (mIter == mNode.MemberEnd())
            return {};
        return {mIter->name.GetString(), mIter->name.GetStringLength()};
    }
    template <typename T>
    bool loadValue(T& value) const {
        if (mIter == mNode.MemberEnd())
            return false;
        return ::NekoProto::loadValue(value, mIter->value);
    }

    template <typename T>
    bool loadNamedValue(const NamedField<T>& value) const {
        return load(value, mNode);
    }

    template <typename T>
    bool operator()(T&& values) {
        auto ret = (selector<JsonDeserializer, T>::doLoad(*this, std::forward<T>(values)));
        ++mIter;
        return ret;
    }

private:
    JsonDeserializer& operator=(const JsonDeserializer&) = delete;
    JsonDeserializer& operator=(JsonDeserializer&&)      = delete;

private:
    JsonDocument mNode;
    JsonDocument::ConstMemberIterator mIter;
};

template <>
class JsonDeserializer<JsonValue, void> {
public:
    using NodeType = JsonValue;

public:
    JsonDeserializer(const NodeType& node) NEKO_NOEXCEPT : mNode(node) {}
    operator bool() const { return true; }
    std::string name() const { return {}; }
    std::size_t size() const { return mNode.Size(); }
    template <typename T>
    bool loadValue(T& value) const {
        return ::NekoProto::loadValue(value, mNode);
    }
    template <typename T>
    bool loadNamedValue(const NamedField<T>& value) const {
        return load(value, mNode);
    }

    template <typename T>
    bool operator()(T&& value) {
        return JsonDeserializer<JsonDocument>::selector<JsonDeserializer, T>::doLoad(*this, std::forward<T>(value));
    }

private:
    JsonDeserializer& operator=(const JsonDeserializer&) = delete;
    JsonDeserializer& operator=(JsonDeserializer&&)      = delete;

private:
    const NodeType& mNode;
};

template <>
class JsonDeserializer<JsonValue::Object, void> {
public:
    using NodeType = JsonValue;

public:
    JsonDeserializer(const NodeType& node) NEKO_NOEXCEPT : mNode(node), mIter(mNode.MemberBegin()) {}
    operator bool() const { return true; }
    std::string name() const {
        if (mIter == mNode.MemberEnd())
            return {};
        return {mIter->name.GetString(), mIter->name.GetStringLength()};
    }
    std::size_t size() const { return mNode.Size(); }
    template <typename T>
    bool loadValue(T& value) const {
        if (mIter == mNode.MemberEnd())
            return false;
        return ::NekoProto::loadValue(value, mIter->value);
    }
    template <typename T>
    bool loadNamedValue(const NamedField<T>& value) const {
        return load(value, mNode);
    }

    template <typename T>
    bool operator()(T&& value) {
        auto ret = JsonDeserializer<JsonDocument>::selector<JsonDeserializer, T>::doLoad(*this, std::forward<T>(value));
        ++mIter;
        return ret;
    }

private:
    JsonDeserializer& operator=(const JsonDeserializer&) = delete;
    JsonDeserializer& operator=(JsonDeserializer&&)      = delete;

private:
    const NodeType& mNode;
    JsonValue::ConstMemberIterator mIter;
};

template <>
class JsonDeserializer<JsonDocument::ConstArray, void> {
public:
    using NodeType = JsonDocument::ConstArray;

public:
    JsonDeserializer(const NodeType& node) NEKO_NOEXCEPT : mNode(node), mIter(node.begin()) {}
    std::size_t size() const { return mNode.Size(); }
    std::string name() const { return {}; }

    template <typename T>
    bool loadValue(T& value) const {
        return ::NekoProto::loadValue(value, *mIter);
    }
    template <typename T, uint8_t c = 1>
    bool loadNamedValue(const NamedField<T>& value) const {
        static_assert(c, "loadNamedValue is not supported for Array");
        return false;
    }

    template <typename T>
    bool operator()(T&& value) {
        if (std::is_same<T, FieldSize>::value) {
            return JsonDeserializer<JsonDocument>::selector<JsonDeserializer, T>::doLoad(*this, std::forward<T>(value));
        } else {
            auto ret =
                JsonDeserializer<JsonDocument>::selector<JsonDeserializer, T>::doLoad(*this, std::forward<T>(value));
            ++mIter;
            return ret;
        }
    }

private:
    JsonDeserializer& operator=(const JsonDeserializer&) = delete;
    JsonDeserializer& operator=(JsonDeserializer&&)      = delete;

private:
    const NodeType& mNode;
    JsonDocument::Array::ConstValueIterator mIter;
};

struct JsonSerializer {
    template <typename T = std::vector<char>, typename U = JsonWriter<OutBufferWrapper>>
    using InputSerializer = JsonInputSerializer<T, U>;
    template <typename T = JsonDocument>
    using OutputSerializer = JsonDeserializer<T>;
};

template <typename T, class enable = void>
struct _get_refer {
    using type = T&;
    static T& get(T& value) { return value; }
};
template <typename T>
struct _get_refer<T&, void> {
    using type = T&;
    static T& get(T& value) { return value; }
};
template <typename T>
struct _get_refer<T*, void> {
    using type = T&;
    static T& get(T* value) { return *value; }
};

template <typename T, typename JsonValueT>
inline bool load(const NamedField<T>& value, const JsonValueT& json) {
    std::unique_ptr<char[]> buf(new char[value.nameLen + 1]{0});
    std::memcpy(buf.get(), value.name, value.nameLen);
    auto it = json.FindMember(buf.get());
    if (it != json.MemberEnd() && it->value.IsArray()) {
        return JsonDeserializer<JsonDocument::ConstArray>(it->value.GetArray())(_get_refer<T>::get(value.value));
    } else if (it != json.MemberEnd() && it->value.IsObject()) {
        return JsonDeserializer<JsonValue::Object>(it->value)(_get_refer<T>::get(value.value));
    }
    return JsonDeserializer<JsonValue>(it->value)(_get_refer<T>::get(value.value));
}

NEKO_END_NAMESPACE