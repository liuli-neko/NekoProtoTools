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
#ifdef GetObject
#undef GetObject
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

template <typename JsonIteratorType, class enable>
class JsonInputSerializer;

namespace {
inline auto makeJsonBufferWrapper(std::vector<char>& data) -> OutBufferWrapper { return OutBufferWrapper(&data); }
auto makeJsonBufferWrapper(OutBufferWrapper& bufferWrapper) -> OutBufferWrapper& { return bufferWrapper; }
} // namespace

template <typename BufferType, typename Writer>
class JsonOutputSerializer : public OutputSerializer<JsonOutputSerializer<BufferType, Writer>> {
public:
    using BufferWrapperType = decltype(makeJsonBufferWrapper(std::declval<BufferType&>()));
    using WriterType        = Writer;

public:
    JsonOutputSerializer(BufferType& out) NEKO_NOEXCEPT : OutputSerializer<JsonOutputSerializer>(this),
                                                          mBuffer(makeJsonBufferWrapper(out)),
                                                          mWriter(mBuffer) {
        mWriter.StartObject();
    }
    JsonOutputSerializer(const JsonOutputSerializer& other) NEKO_NOEXCEPT
        : OutputSerializer<JsonOutputSerializer>(this),
          mBuffer(other.mBuffer),
          mWriter(other.mWriter) {}
    JsonOutputSerializer(JsonOutputSerializer&& other) NEKO_NOEXCEPT : OutputSerializer<JsonOutputSerializer>(this),
                                                                       mBuffer(std::move(other.mBuffer)),
                                                                       mWriter(std::move(other.mWriter)) {}
    template <typename T>
    inline bool saveValue(SizeTag<T> const&) {
        return true;
    }
    inline bool saveValue(const int16_t value) { return mWriter.Int(value); }
    inline bool saveValue(const uint16_t value) { return mWriter.Uint(value); }
    inline bool saveValue(const int32_t value) { return mWriter.Int(value); }
    inline bool saveValue(const uint32_t value) { return mWriter.Uint(value); }
    inline bool saveValue(const int64_t value) { return mWriter.Int64(value); }
    inline bool saveValue(const uint64_t value) { return mWriter.Uint64(value); }
    inline bool saveValue(const float value) { return mWriter.Double(value); }
    inline bool saveValue(const double value) { return mWriter.Double(value); }
    inline bool saveValue(const bool value) { return mWriter.Bool(value); }
    inline bool saveValue(const std::string& value) { return mWriter.String(value.c_str(), value.size()); }
    inline bool saveValue(const char* value) { return mWriter.String(value); }
#if NEKO_CPP_PLUS >= 17
    inline bool saveValue(const std::string_view value) { return mWriter.String(value.data(), value.size()); }
#endif
    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) {
        return mWriter.Key(value.name, value.nameLen) && this->operator()(value.value);
    }
    bool startArray(const std::size_t size) { return mWriter.StartArray(); }
    bool endArray() { return mWriter.EndArray(); }
    bool startObject() { return mWriter.StartObject(); }
    bool endObject() { return mWriter.EndObject(); }
    bool end() {
        auto ret = mWriter.EndObject();
        mWriter.Flush();
        return ret;
    }

private:
    JsonOutputSerializer& operator=(const JsonOutputSerializer&) = delete;
    JsonOutputSerializer& operator=(JsonOutputSerializer&&)      = delete;

private:
    WriterType mWriter;
    BufferWrapperType mBuffer;
};

namespace detail {
template <typename JsonType = JsonValue, class enable1 = void>
class ConstJsonIterator {
    using NodeType = JsonType;

public:
    ConstJsonIterator(const NodeType& node) : mNode(node) {}
    const JsonValue& operator*() const { return mNode; }
    ConstJsonIterator& operator++() { return *this; }
    bool eof() const { return true; }
    std::size_t size() const { return 1; }
    const JsonValue& value() const { return mNode; }
    std::string name() const { return ""; }
    const JsonValue* operator->() const { return &mNode; }
    const JsonValue* member(const std::string& name) const { return nullptr; }

private:
    ConstJsonIterator(const ConstJsonIterator&)            = delete;
    ConstJsonIterator& operator=(const ConstJsonIterator&) = delete;

private:
    const NodeType& mNode;
};

template <>
class ConstJsonIterator<JsonValue::Array, void> {
    using NodeType = JsonValue::Array;

public:
    ConstJsonIterator(const NodeType& node) : mIndex(node.begin()), mEnd(node.end()), mSize(node.Size()) {}
    ConstJsonIterator() : mIndex(nullptr), mEnd(nullptr), mSize(0) {}
    ConstJsonIterator& operator=(const NodeType& node) {
        mIndex = node.begin();
        mEnd   = node.end();
        mSize  = node.Size();
        return *this;
    };
    ConstJsonIterator& operator=(const JsonDocument& node) {
        const auto& array = node.GetArray();
        mIndex            = array.begin();
        mEnd              = array.end();
        mSize             = array.Size();
        return *this;
    }
    const JsonValue& operator*() {
        const auto& it = mIndex;
        ++mIndex;
        return *it;
    }
    ConstJsonIterator& operator++() {
        ++mIndex;
        return *this;
    }
    bool eof() const { return mIndex == mEnd; }
    std::size_t size() const { return mSize; }
    const JsonValue& value() {
        const auto& it = mIndex;
        ++mIndex;
        return *it;
    }
    std::string name() const { return ""; }
    const JsonValue* operator->() const { return mIndex; }
    const JsonValue* member(const std::string& name) const { return nullptr; }

private:
    ConstJsonIterator(const ConstJsonIterator&)            = delete;
    ConstJsonIterator& operator=(const ConstJsonIterator&) = delete;

private:
    typename NodeType::ConstValueIterator mIndex;
    typename NodeType::ConstValueIterator mEnd;
    std::size_t mSize;
};

template <>
class ConstJsonIterator<JsonValue::Object, void> {
    using NodeType = JsonValue::Object;

public:
    ConstJsonIterator(const NodeType& node) {
        mNameMap.reserve(node.MemberCount());
        for (const auto& member : node) {
            mNameMap.emplace(std::string{member.name.GetString(), member.name.GetStringLength()}, member);
        }
        mIndex = mNameMap.begin();
    }
    ConstJsonIterator() : mNameMap(), mIndex() {}
    ConstJsonIterator& operator=(const NodeType& node) {
        mNameMap.reserve(node.MemberCount());
        for (const auto& member : node) {
            mNameMap.emplace(std::string{member.name.GetString(), member.name.GetStringLength()}, member);
        }
        mIndex = mNameMap.begin();
        return *this;
    }
    ConstJsonIterator& operator=(const JsonDocument& node) {
        const auto& object = node.GetObject();
        mNameMap.reserve(object.MemberCount());
        for (const auto& member : object) {
            mNameMap.emplace(std::string{member.name.GetString(), member.name.GetStringLength()}, member);
        }
        mIndex = mNameMap.begin();
        return *this;
    }
    const JsonValue& operator*() {
        const auto& it = mIndex->second;
        ++mIndex;
        return (it->value);
    }
    ConstJsonIterator& operator++() {
        ++mIndex;
        return *this;
    }
    bool eof() const { return mIndex == mNameMap.end(); }
    std::size_t size() const { return mNameMap.size(); }
    std::string name() const { return mIndex->first; }
    const JsonValue& value() {
        const auto& it = mIndex->second;
        ++mIndex;
        return it->value;
    }
    const JsonValue* operator->() const { return &((mIndex->second)->value); }
    const JsonValue* member(const std::string& name) const {
        auto it = mNameMap.find(name);
        return (it != mNameMap.end()) ? &((it->second)->value) : nullptr;
    }

private:
    ConstJsonIterator(const ConstJsonIterator&)            = delete;
    ConstJsonIterator& operator=(const ConstJsonIterator&) = delete;

private:
    std::unordered_map<std::string, typename NodeType::ConstMemberIterator> mNameMap;
    std::unordered_map<std::string, typename NodeType::ConstMemberIterator>::const_iterator mIndex;
};
} // namespace detail

template <typename JsonIteratorType = detail::ConstJsonIterator<JsonValue::Object>, class enable = void>
class JsonInputSerializer : public InputSerializer<JsonInputSerializer<JsonIteratorType, enable>> {
public:
    using NodeType = JsonIteratorType;

public:
    JsonInputSerializer(const std::vector<char>& buf)
        : InputSerializer<JsonInputSerializer<JsonIteratorType, enable>>(this) {
        mDocument.Parse(buf.data(), buf.size());
        mIter = mDocument;
    }
    JsonInputSerializer(const char* buf, std::size_t size)
        : InputSerializer<JsonInputSerializer<JsonIteratorType, enable>>(this) {
        mDocument.Parse(buf, size);
        mIter = mDocument;
    }
    JsonInputSerializer(rapidjson::IStreamWrapper& stream)
        : InputSerializer<JsonInputSerializer<JsonIteratorType, enable>>(this) {
        mDocument.ParseStream(stream);
        mIter = mDocument;
    }
    JsonInputSerializer(const JsonValue& value)
        : InputSerializer<JsonInputSerializer<JsonIteratorType, enable>>(this), mDocument(), mIter(value) {}
    JsonInputSerializer(const JsonValue::Array& array)
        : InputSerializer<JsonInputSerializer<JsonIteratorType, enable>>(this), mDocument(), mIter(array) {}
    JsonInputSerializer(const JsonValue::Object& object)
        : InputSerializer<JsonInputSerializer<JsonIteratorType, enable>>(this), mDocument(), mIter(object) {}
    JsonInputSerializer(JsonInputSerializer&& other) NEKO_NOEXCEPT
        : InputSerializer<JsonInputSerializer<JsonIteratorType, enable>>(this),
          mDocument(std::move(other.mDocument)),
          mIter(std::move(other.mIter)) {}
    operator bool() const { return mDocument.GetParseError() == rapidjson::kParseErrorNone; }
    std::size_t size() const { return mIter.Size(); }
    std::string name() const {
        if (mIter.eof())
            return {};
        return {mIter.name()};
    }

    inline bool loadValue(std::string& value) {
        if (!mItem->IsString())
            return false;
        value = {mIter->GetString(), mIter->GetStringLength()};
        return true;
    }

    inline bool loadValue(int16_t& value) {
        if (!mIter->IsInt())
            return false;
        value = mIter->GetInt();
        return true;
    }

    inline bool loadValue(int32_t& value) {
        if (!mIter->IsInt())
            return false;
        value = mIter->GetInt();
        return true;
    }

    inline bool loadValue(int64_t& value) {
        if (!mIter->IsInt64())
            return false;
        value = mIter->GetInt64();
        return true;
    }

    inline bool loadValue(uint16_t& value) {
        if (!mIter->IsUint())
            return false;
        value = mIter->GetUint();
        return true;
    }

    inline bool loadValue(uint32_t& value) {
        if (!mIter->IsUint())
            return false;
        value = mIter->GetUint();
        return true;
    }

    inline bool loadValue(uint64_t& value) {
        if (!mIter->IsUint64())
            return false;
        value = mIter->GetUint64();
        return true;
    }

    inline bool loadValue(float& value) {
        if (!mIter->IsNumber())
            return false;
        value = mIter->GetFloat();
        return true;
    }

    inline bool loadValue(double& value) {
        if (!mIter->IsNumber())
            return false;
        value = mIter->GetDouble();
        return true;
    }

    inline bool loadValue(bool& value) {
        if (!mIter->IsBool())
            return false;
        value = mIter->GetBool();
        return true;
    }

    template <typename T>
    inline bool loadValue(SizeTag<T>& value) {
        if (!mIter->IsArray())
            return false;
        value.size = mIter->Size();
        return true;
    }

    template <typename T>
    inline bool loadValue(NameValuePair<T>& value) {
        const auto& v = mIter->member({value.name, value.nameLen});
        if (nullptr == v)
            return false;
        if (v.IsArray()) {
            return JsonInputSerializer<detail::ConstJsonIterator<JsonValue::Array>>(v.GetArray())(value.value);
        } else if (v.IsObject()) {
            return JsonInputSerializer<detail::ConstJsonIterator<JsonValue::Object>>(v.GetObject())(value.value);
        }
        return JsonInputSerializer<detail::ConstJsonIterator<JsonValue>>(v)(value.value);
    }

private:
    JsonInputSerializer& operator=(const JsonInputSerializer&) = delete;
    JsonInputSerializer& operator=(JsonInputSerializer&&)      = delete;

private:
    JsonDocument mDocument;
    NodeType mIter;
};

struct JsonSerializer {
    template <typename T = std::vector<char>, typename U = JsonWriter<OutBufferWrapper>>
    using OutputSerializer = JsonOutputSerializer<T, U>;
    template <typename T = detail::ConstJsonIterator<JsonValue::Object>>
    using InputSerializer = JsonInputSerializer<T>;
};

template <typename SerializerT, typename T>
inline bool save(SerializerT& serializer, const SizeTag<T>& value) {
    return serializer.saveValue(value);
}

template <typename SerializerT, typename T>
inline bool save(SerializerT& serializer, const NameValuePair<T>& value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const int16_t value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const uint16_t value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const int32_t value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const uint32_t value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const int64_t value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const uint64_t value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const float value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const double value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const bool value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const std::string& value) {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const char* value) {
    return serializer.saveValue(value);
}

#if NEKO_CPP_PLUS >= 17
template <typename SerializerT>
inline bool save(SerializerT& serializer, const std::string_view value) {
    return serializer.saveValue(value);
}
#endif

template <typename SerializerT>
inline bool load(SerializerT& serializer, std::string& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, int16_t& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, int32_t& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, int64_t& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, uint16_t& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, uint32_t& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, uint64_t& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, float& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, double& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, bool& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT, typename T>
inline bool load(SerializerT& serializer, SizeTag<T>& value) {
    return serializer.loadValue(value);
}

template <typename SerializerT, typename T>
inline bool load(SerializerT& serializer, const NameValuePair<T>& value) {
    return serializer.loadValue(value);
}

NEKO_END_NAMESPACE