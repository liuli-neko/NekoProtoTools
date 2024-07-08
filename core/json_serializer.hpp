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

#if NEKO_CPP_PLUS >= 17
#include <optional>
#endif

#include "serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

using JsonValue       = rapidjson::Value;
using ConstJsonValue  = rapidjson::Value;
using JsonObject      = rapidjson::Value::Object;
using ConstJsonObject = rapidjson::Value::ConstObject;
using JsonArray       = rapidjson::Value::Array;
using ConstJsonArray  = rapidjson::Value::ConstArray;
using JsonDocument    = rapidjson::Document;

template <typename WriterT, typename ValueT, typename T, class enable = void>
struct JsonConvert;

template <typename ValueType, class enable>
class JsonDeserializer;

template <typename BufferT = detail::OutBufferWrapper>
using JsonWriter = rapidjson::Writer<BufferT>;

template <typename JsonIteratorType, class enable>
class JsonInputSerializer;

namespace detail {
inline auto makeJsonBufferWrapper(std::vector<char>& data) -> OutBufferWrapper { return OutBufferWrapper(&data); }
auto makeJsonBufferWrapper(OutBufferWrapper& bufferWrapper) -> OutBufferWrapper& { return bufferWrapper; }
} // namespace detail

template <typename BufferType, typename Writer>
class JsonOutputSerializer : public detail::OutputSerializer<JsonOutputSerializer<BufferType, Writer>> {
public:
    using BufferWrapperType = decltype(detail::makeJsonBufferWrapper(std::declval<BufferType&>()));
    using WriterType        = Writer;

public:
    JsonOutputSerializer(BufferType& out) NEKO_NOEXCEPT : detail::OutputSerializer<JsonOutputSerializer>(this),
                                                          mBuffer(detail::makeJsonBufferWrapper(out)),
                                                          mWriter(mBuffer) {
        mWriter.StartObject();
    }
    JsonOutputSerializer(const JsonOutputSerializer& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<JsonOutputSerializer>(this),
          mBuffer(other.mBuffer),
          mWriter(other.mWriter) {}
    JsonOutputSerializer(JsonOutputSerializer&& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<JsonOutputSerializer>(this),
          mBuffer(std::move(other.mBuffer)),
          mWriter(std::move(other.mWriter)) {}
    template <typename T>
    inline bool saveValue(SizeTag<T> const&) {
        return true;
    }
    inline bool saveValue(const int8_t value) { return mWriter.Int(value); }
    inline bool saveValue(const uint8_t value) { return mWriter.Uint(value); }
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
    inline bool saveValue(const std::nullptr_t) { return mWriter.Null(); }
#if NEKO_CPP_PLUS >= 17
    inline bool saveValue(const std::string_view value) { return mWriter.String(value.data(), value.size()); }

    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) {
        if constexpr (traits::is_optional<T>::value) {
            if (value.value.has_value())
                return mWriter.Key(value.name, value.nameLen) && this->operator()(value.value.value());
        } else {
            return mWriter.Key(value.name, value.nameLen) && this->operator()(value.value);
        }
    }
#else
    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) {
        return mWriter.Key(value.name, value.nameLen) && this->operator()(value.value);
    }
#endif
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
template <typename JsonType = ConstJsonValue, class enable1 = void>
class ConstJsonIterator {};

template <>
class ConstJsonIterator<ConstJsonValue, void> {
    using NodeType = ConstJsonValue;

public:
    ConstJsonIterator(const NodeType& node) : mNode(node) {}
    ConstJsonIterator() : mNode(NodeType()){};
    ConstJsonIterator& operator++() { return *this; }
    bool eof() const { return true; }
    std::size_t size() const { return 1; }
    const JsonValue& value() const { return mNode; }
    std::string name() const { return std::string(); }
    const JsonValue* operator->() const { return &mNode; }
    const JsonValue* member(const std::string& name) const { return nullptr; }
    ConstJsonIterator& operator=(const JsonDocument& node) {
        NEKO_ASSERT(false, "ConstJsonIterator::operator=(const JsonDocument&) is not allowed");
        return *this;
    }

private:
    ConstJsonIterator(const ConstJsonIterator&)            = delete;
    ConstJsonIterator& operator=(const ConstJsonIterator&) = delete;

private:
    const NodeType& mNode;
};

template <>
class ConstJsonIterator<ConstJsonArray, void> {
    using NodeType = ConstJsonArray;

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
    ConstJsonIterator& operator++() {
        ++mIndex;
        return *this;
    }
    bool eof() const { return mIndex == mEnd; }
    std::size_t size() const { return mSize; }
    const JsonValue& value() {
        static JsonValue empty;
        if (eof()) {
            return empty;
        }
        return *(mIndex++);
    }
    std::string name() const { return std::string(); }
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
class ConstJsonIterator<ConstJsonObject, void> {
    using NodeType = ConstJsonObject;

public:
    ConstJsonIterator(const NodeType& node) {
        mNameMap.reserve(node.MemberCount());
        for (auto member = node.MemberBegin(); member != node.MemberEnd(); ++member) {
            mNameMap.emplace(std::string{member->name.GetString(), member->name.GetStringLength()}, member);
        }
        mIndex = mNameMap.begin();
    }
    ConstJsonIterator() : mNameMap(), mIndex(mNameMap.begin()) {}
    ConstJsonIterator& operator=(const NodeType& node) {
        mNameMap.reserve(node.MemberCount());
        for (auto member = node.MemberBegin(); member != node.MemberEnd(); ++member) {
            mNameMap.emplace(std::string{member->name.GetString(), member->name.GetStringLength()}, member);
        }
        mIndex = mNameMap.begin();
        return *this;
    }
    ConstJsonIterator& operator=(const JsonDocument& node) {
        mNameMap.reserve(node.MemberCount());
        for (auto member = node.MemberBegin(); member != node.MemberEnd(); ++member) {
            mNameMap.emplace(std::string{member->name.GetString(), member->name.GetStringLength()}, member);
        }
        mIndex = mNameMap.begin();
        return *this;
    }
    ConstJsonIterator& operator++() {
        ++mIndex;
        return *this;
    }
    bool eof() const { return mIndex == mNameMap.end(); }
    std::size_t size() const { return mNameMap.size(); }
    std::string name() const {
        if (mIndex == mNameMap.end())
            return std::string();
        return mIndex->first;
    }
    const JsonValue& value() {
        const auto& it = mIndex->second;
        ++mIndex;
        return it->value;
    }
    const JsonValue* operator->() { return &((mIndex->second)->value); }
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

template <typename JsonValueType = ConstJsonObject, class enable = void>
class JsonInputSerializer : public detail::InputSerializer<JsonInputSerializer<JsonValueType, enable>> {
public:
    using NodeType = detail::ConstJsonIterator<JsonValueType>;

public:
    JsonInputSerializer(const std::vector<char>& buf)
        : detail::InputSerializer<JsonInputSerializer<JsonValueType, enable>>(this), mDocument(), mIter() {
        mDocument.Parse(buf.data(), buf.size());
        mIter = mDocument;
    }
    JsonInputSerializer(const char* buf, std::size_t size)
        : detail::InputSerializer<JsonInputSerializer<JsonValueType, enable>>(this), mDocument(), mIter() {
        mDocument.Parse(buf, size);
        mIter = mDocument;
    }
    JsonInputSerializer(rapidjson::IStreamWrapper& stream)
        : detail::InputSerializer<JsonInputSerializer<JsonValueType, enable>>(this), mDocument(), mIter() {
        mDocument.ParseStream(stream);
        mIter = mDocument;
    }
    JsonInputSerializer(const JsonValueType& value)
        : detail::InputSerializer<JsonInputSerializer<JsonValueType, enable>>(this), mDocument(), mIter(value) {}
    JsonInputSerializer(JsonInputSerializer&& other) NEKO_NOEXCEPT
        : detail::InputSerializer<JsonInputSerializer<JsonValueType, enable>>(this),
          mDocument(std::move(other.mDocument)),
          mIter(std::move(other.mIter)) {}
    operator bool() const { return mDocument.GetParseError() == rapidjson::kParseErrorNone; }
    std::string name() const {
        if (mIter.eof())
            return {};
        return {mIter.name()};
    }

    inline bool loadValue(std::string& value) {
        if (!mIter->IsString())
            return false;
        const auto& v = mIter.value();
        value         = std::string(v.GetString(), v.GetStringLength());
        return true;
    }

    inline bool loadValue(int8_t& value) {
        if (!mIter->IsInt())
            return false;
        value = mIter.value().GetInt();
        return true;
    }

    inline bool loadValue(int16_t& value) {
        if (!mIter->IsInt())
            return false;
        value = mIter.value().GetInt();
        return true;
    }

    inline bool loadValue(int32_t& value) {
        if (!mIter->IsInt())
            return false;
        value = mIter.value().GetInt();
        return true;
    }

    inline bool loadValue(int64_t& value) {
        if (!mIter->IsInt64())
            return false;
        value = mIter.value().GetInt64();
        return true;
    }

    inline bool loadValue(uint8_t& value) {
        if (!mIter->IsUint())
            return false;
        value = mIter.value().GetUint();
        return true;
    }

    inline bool loadValue(uint16_t& value) {
        if (!mIter->IsUint())
            return false;
        value = mIter.value().GetUint();
        return true;
    }

    inline bool loadValue(uint32_t& value) {
        if (!mIter->IsUint())
            return false;
        value = mIter.value().GetUint();
        return true;
    }

    inline bool loadValue(uint64_t& value) {
        if (!mIter->IsUint64())
            return false;
        value = mIter.value().GetUint64();
        return true;
    }

    inline bool loadValue(float& value) {
        if (!mIter->IsNumber())
            return false;
        value = mIter.value().GetDouble();
        return true;
    }

    inline bool loadValue(double& value) {
        if (!mIter->IsNumber())
            return false;
        value = mIter.value().GetDouble();
        return true;
    }

    inline bool loadValue(bool& value) {
        if (!mIter->IsBool())
            return false;
        value = mIter.value().GetBool();
        return true;
    }

    template <typename T>
    inline bool loadValue(const SizeTag<T>& value) {
        value.size = mIter.size();
        return true;
    }

#if NEKO_CPP_PLUS >= 17
    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) {
        const auto& v = mIter.member({value.name, value.nameLen});
        bool ret      = true;
        if constexpr (traits::is_optional<T>::value) {
            if (nullptr == v) {
                value.value.reset();
            } else if (v->IsArray()) {
                typename traits::is_optional<T>::value_type result;
                ret = JsonInputSerializer<ConstJsonArray>(v->GetArray())(result);
                value.value.emplace(std::move(result));
            } else if (v->IsObject()) {
                typename traits::is_optional<T>::value_type result;
                ret = JsonInputSerializer<ConstJsonObject>(v->GetObject())(result);
                value.value.emplace(std::move(result));
            } else {
                typename traits::is_optional<T>::value_type result;
                ret = JsonInputSerializer<ConstJsonValue>(*v)(result);
                value.value.emplace(std::move(result));
            }
        } else {
            if (nullptr == v) {
                return false;
            }
            if (v->IsArray()) {
                ret = JsonInputSerializer<ConstJsonArray>(v->GetArray())(value.value);
            } else if (v->IsObject()) {
                ret = JsonInputSerializer<ConstJsonObject>(v->GetObject())(value.value);
            } else {
                ret = JsonInputSerializer<ConstJsonValue>(*v)(value.value);
            }
        }
        return ret;
    }
#else
    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) {
        const auto& v = mIter.member({value.name, value.nameLen});
        if (nullptr == v) {
            return false;
        }
        bool ret = true;
        if (v->IsArray()) {
            ret = JsonInputSerializer<ConstJsonArray>(v->GetArray())(value.value);
        } else if (v->IsObject()) {
            ret = JsonInputSerializer<ConstJsonObject>(v->GetObject())(value.value);
        } else {
            ret = JsonInputSerializer<ConstJsonValue>(*v)(value.value);
        }
        return ret;
    }
#endif

private:
    JsonInputSerializer& operator=(const JsonInputSerializer&) = delete;
    JsonInputSerializer& operator=(JsonInputSerializer&&)      = delete;

private:
    JsonDocument mDocument;
    NodeType mIter;
};

struct JsonSerializer {
    using OutputSerializer = JsonOutputSerializer<std::vector<char>, JsonWriter<detail::OutBufferWrapper>>;
    using InputSerializer  = JsonInputSerializer<ConstJsonObject>;
};

NEKO_END_NAMESPACE