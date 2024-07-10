/**
 * @file json_serializer.hpp
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

namespace detail {
inline auto makeJsonBufferWrapper(std::vector<char>& data) NEKO_NOEXCEPT->OutBufferWrapper {
    return OutBufferWrapper(&data);
}
inline auto makeJsonBufferWrapper(OutBufferWrapper& bufferWrapper) NEKO_NOEXCEPT->OutBufferWrapper& {
    return bufferWrapper;
}
} // namespace detail

template <typename BufferType, typename Writer>
class JsonOutputSerializer : public detail::OutputSerializer<JsonOutputSerializer<BufferType, Writer>> {
public:
    using BufferWrapperType = decltype(detail::makeJsonBufferWrapper(std::declval<BufferType&>()));
    using WriterType        = Writer;

public:
    inline JsonOutputSerializer(BufferType& out) NEKO_NOEXCEPT : detail::OutputSerializer<JsonOutputSerializer>(this),
                                                                 mBuffer(detail::makeJsonBufferWrapper(out)),
                                                                 mWriter(mBuffer) {
        mWriter.StartObject();
    }
    inline JsonOutputSerializer(const JsonOutputSerializer& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<JsonOutputSerializer>(this),
          mBuffer(other.mBuffer),
          mWriter(other.mWriter) {}
    inline JsonOutputSerializer(JsonOutputSerializer&& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<JsonOutputSerializer>(this),
          mBuffer(std::move(other.mBuffer)),
          mWriter(std::move(other.mWriter)) {}
    inline ~JsonOutputSerializer() { end(); }
    template <typename T>
    inline bool saveValue(SizeTag<T> const&) {
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool saveValue(const T value) NEKO_NOEXCEPT {
        return mWriter.Int(static_cast<int32_t>(value));
    }
    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) < sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool saveValue(const T value) NEKO_NOEXCEPT {
        return mWriter.Uint(static_cast<uint32_t>(value));
    }
    inline bool saveValue(const int64_t value) NEKO_NOEXCEPT { return mWriter.Int64(value); }
    inline bool saveValue(const uint64_t value) NEKO_NOEXCEPT { return mWriter.Uint64(value); }
    inline bool saveValue(const float value) NEKO_NOEXCEPT { return mWriter.Double(value); }
    inline bool saveValue(const double value) NEKO_NOEXCEPT { return mWriter.Double(value); }
    inline bool saveValue(const bool value) NEKO_NOEXCEPT { return mWriter.Bool(value); }
    template <typename CharT, typename Traits, typename Alloc>
    inline bool saveValue(const std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        return mWriter.String(value.c_str(), value.size());
    }
    inline bool saveValue(const char* value) NEKO_NOEXCEPT { return mWriter.String(value); }
    inline bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT { return mWriter.Null(); }
#if NEKO_CPP_PLUS >= 17
    template <typename CharT, typename Traits>
    inline bool saveValue(const std::basic_string_view<CharT, Traits> value) NEKO_NOEXCEPT {
        return mWriter.String(value.data(), value.size());
    }

    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        if constexpr (traits::is_optional<T>::value) {
            if (value.value.has_value()) {
                return mWriter.Key(value.name, value.nameLen) && (*this)(value.value.value());
            }
        } else {
            return mWriter.Key(value.name, value.nameLen) && (*this)(value.value);
        }
        return true;
    }
#else
    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        return mWriter.Key(value.name, value.nameLen) && (*this)(value.value);
    }
#endif
    inline bool startArray(const std::size_t) NEKO_NOEXCEPT { return mWriter.StartArray(); }
    inline bool endArray() NEKO_NOEXCEPT { return mWriter.EndArray(); }
    inline bool startObject(const std::size_t) NEKO_NOEXCEPT { return mWriter.StartObject(); }
    inline bool endObject() NEKO_NOEXCEPT { return mWriter.EndObject(); }
    inline bool end() NEKO_NOEXCEPT {
        if (!mWriter.IsComplete()) {
            auto ret = mWriter.EndObject();
            mWriter.Flush();
            return ret;
        }
        return true;
    }

private:
    JsonOutputSerializer& operator=(const JsonOutputSerializer&) = delete;
    JsonOutputSerializer& operator=(JsonOutputSerializer&&)      = delete;

private:
    WriterType mWriter;
    BufferWrapperType mBuffer;
};

namespace detail {
class ConstJsonIterator {
public:
    using MemberIterator = JsonValue::ConstMemberIterator;
    using ValueIterator  = JsonValue::ConstValueIterator;

public:
    inline ConstJsonIterator() NEKO_NOEXCEPT : mIndex(0), mType(Null_){};
    inline ConstJsonIterator(MemberIterator begin, MemberIterator end) NEKO_NOEXCEPT : mMemberItBegin(begin),
                                                                                       mMemberItEnd(end),
                                                                                       mIndex(0),
                                                                                       mMemberIt(begin),
                                                                                       mSize(0),
                                                                                       mType(Member) {
        for (auto member = mMemberItBegin; member != mMemberItEnd; ++member) {
            mMemberMap.emplace(NEKO_STRING_VIEW{member->name.GetString(), member->name.GetStringLength()}, member);
            ++mSize;
        }
        if (mSize == 0) {
            mType = Null_;
            return;
        }
    }

    inline ConstJsonIterator(ValueIterator begin, ValueIterator end) NEKO_NOEXCEPT : mValueItBegin(begin),
                                                                                     mIndex(0),
                                                                                     mMemberIt(),
                                                                                     mSize(std::distance(begin, end)),
                                                                                     mType(Value) {
        if (mSize == 0) {
            mType = Null_;
        }
    }

    inline ~ConstJsonIterator() = default;

    inline ConstJsonIterator& operator++() NEKO_NOEXCEPT {
        if (mType == Member) {
            ++mMemberIt;
        } else {
            ++mIndex;
        }
        return *this;
    }
    inline bool eof() const NEKO_NOEXCEPT {
        if (mType == Null_ || (mType == Value && mIndex >= mSize) || (mType == Member && mMemberIt == mMemberItEnd)) {
            return true;
        }
        return false;
    }
    inline std::size_t size() const NEKO_NOEXCEPT { return mSize; }
    inline const JsonValue& value() NEKO_NOEXCEPT {
        NEKO_ASSERT(!eof(), "JsonInputSerializer get next value called on end of this json object");
        switch (mType) {
        case Value:
            return mValueItBegin[mIndex];
        case Member:
            return mMemberIt->value;
        }
        return mValueItBegin[mIndex]; // should never reach here, but needed to avoid compiler warning
    }
    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if (mType == Member && mMemberIt != mMemberItEnd) {
            return {mMemberIt->name.GetString(), mMemberIt->name.GetStringLength()};
        } else {
            return {};
        }
    };
    inline const JsonValue* move_to_member(const NEKO_STRING_VIEW& name) NEKO_NOEXCEPT {
        auto it = mMemberMap.find(name);
        if (it != mMemberMap.end()) {
            mMemberIt = it->second;
            return &(mMemberIt)->value;
        } else {
            return nullptr;
        }
    };

private:
    MemberIterator mMemberItBegin, mMemberItEnd, mMemberIt;
    ValueIterator mValueItBegin;
    std::size_t mIndex, mSize;
    std::unordered_map<NEKO_STRING_VIEW, MemberIterator> mMemberMap;
    enum Type { Value, Member, Null_ } mType;
};

} // namespace detail

class JsonInputSerializer : public detail::InputSerializer<JsonInputSerializer> {

public:
    inline JsonInputSerializer(const std::vector<char>& buf) NEKO_NOEXCEPT
        : detail::InputSerializer<JsonInputSerializer>(this),
          mDocument(),
          mItemStack() {
        mDocument.Parse(buf.data(), buf.size());
        if (mDocument.IsArray()) {
            mItemStack.emplace_back(mDocument.Begin(), mDocument.End());
        } else if (mDocument.IsObject()) {
            mItemStack.emplace_back(mDocument.MemberBegin(), mDocument.MemberEnd());
        }
    }
    inline JsonInputSerializer(const char* buf, std::size_t size) NEKO_NOEXCEPT
        : detail::InputSerializer<JsonInputSerializer>(this),
          mDocument(),
          mItemStack() {
        mDocument.Parse(buf, size);
        if (mDocument.IsArray()) {
            mItemStack.emplace_back(mDocument.Begin(), mDocument.End());
        } else if (mDocument.IsObject()) {
            mItemStack.emplace_back(mDocument.MemberBegin(), mDocument.MemberEnd());
        }
    }
    inline JsonInputSerializer(rapidjson::IStreamWrapper& stream) NEKO_NOEXCEPT
        : detail::InputSerializer<JsonInputSerializer>(this),
          mDocument(),
          mItemStack() {
        mDocument.ParseStream(stream);
        if (mDocument.IsArray()) {
            mItemStack.emplace_back(mDocument.Begin(), mDocument.End());
        } else if (mDocument.IsObject()) {
            mItemStack.emplace_back(mDocument.MemberBegin(), mDocument.MemberEnd());
        }
    }
    inline operator bool() const NEKO_NOEXCEPT { return mDocument.GetParseError() == rapidjson::kParseErrorNone; }

    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if (mItemStack.empty() || mItemStack.back().eof())
            return {};
        return mItemStack.back().name();
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsInt()) {
            return false;
        }
        value = static_cast<T>(mItemStack.back().value().GetInt());
        ++mItemStack.back();
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) < sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsUint()) {
            return false;
        }
        value = static_cast<T>(mItemStack.back().value().GetUint());
        ++mItemStack.back();
    }

    template <typename CharT, typename Traits, typename Alloc>
    inline bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsString()) {
            return false;
        }
        const auto& v = mItemStack.back().value();
        value         = std::basic_string<CharT, Traits, Alloc>{v.GetString(), v.GetStringLength()};
        ++mItemStack.back();
        return true;
    }

    inline bool loadValue(int64_t& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsInt64())
            return false;
        value = mItemStack.back().value().GetInt64();
        ++mItemStack.back();
        return true;
    }

    inline bool loadValue(uint64_t& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsUint64())
            return false;
        value = mItemStack.back().value().GetUint64();
        ++mItemStack.back();
        return true;
    }

    inline bool loadValue(float& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsNumber())
            return false;
        value = mItemStack.back().value().GetDouble();
        ++mItemStack.back();
        return true;
    }

    inline bool loadValue(double& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsNumber())
            return false;
        value = mItemStack.back().value().GetDouble();
        ++mItemStack.back();
        return true;
    }

    inline bool loadValue(bool& value) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsBool())
            return false;
        value = mItemStack.back().value().GetBool();
        ++mItemStack.back();
        return true;
    }

    inline bool loadValue(std::nullptr_t&) NEKO_NOEXCEPT {
        if (mItemStack.empty() || !mItemStack.back().value().IsNull())
            return false;
        ++mItemStack.back();
        return true;
    }

    template <typename T>
    inline bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        value.size = mItemStack.back().size();
        return true;
    }

#if NEKO_CPP_PLUS >= 17
    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        const auto& v = mItemStack.back().move_to_member({value.name, value.nameLen});
        bool ret      = true;
        if constexpr (traits::is_optional<T>::value) {
            if (nullptr == v) {
                value.value.reset();
            } else {
                typename traits::is_optional<T>::value_type result;
                ret = operator()(result);
                value.value.emplace(std::move(result));
            }
        } else {
            if (nullptr == v) {
                return false;
            }
            ret = operator()(value.value);
        }
        return ret;
    }
#else
    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        const auto& v = mItemStack.back().move_to_member({value.name, value.nameLen});
        if (nullptr == v) {
            return false;
        }
        return operator()(value.value);
    }
#endif
    inline bool startNode() NEKO_NOEXCEPT {
        if (mItemStack.back().value().IsArray()) {
            mItemStack.emplace_back(mItemStack.back().value().Begin(), mItemStack.back().value().End());
        } else if (mItemStack.back().value().IsObject()) {
            mItemStack.emplace_back(mItemStack.back().value().MemberBegin(), mItemStack.back().value().MemberEnd());
        } else {
            return false;
        }
        return true;
    }

    inline bool finishNode() NEKO_NOEXCEPT {
        if (mItemStack.size() < 2) {
            return false;
        }
        mItemStack.pop_back();
        ++mItemStack.back();
        return true;
    }

private:
    JsonInputSerializer& operator=(const JsonInputSerializer&) = delete;
    JsonInputSerializer& operator=(JsonInputSerializer&&)      = delete;

private:
    JsonDocument mDocument;
    std::vector<detail::ConstJsonIterator> mItemStack;
};

// #######################################################
// JsonSerializer prologue and epilogue
// #######################################################

// #######################################################
// name value pair
template <typename T>
inline bool prologue(JsonInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(JsonInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferType, typename Writer>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa, const NameValuePair<T>&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferType, typename Writer>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa, const NameValuePair<T>&) NEKO_NOEXCEPT {
    return true;
}

//#########################################################
// size tag
template <typename T>
inline bool prologue(JsonInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(JsonInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferType, typename Writer>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferType, typename Writer>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
template <class T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                       traits::default_value_for_enable>
inline bool prologue(JsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <typename T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(JsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <class T, typename BufferType, typename Writer,
          traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                              !is_minimal_serializable<T>::value,
                              !traits::has_method_const_serialize<T, JsonOutputSerializer<BufferType, Writer>>::value> =
              traits::default_value_for_enable>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferType, typename Writer,
          traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value,
                              !traits::has_method_const_serialize<T, JsonOutputSerializer<BufferType, Writer>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferType, typename Writer,
          traits::enable_if_t<traits::has_method_const_serialize<T, JsonOutputSerializer<BufferType, Writer>>::value> =
              traits::default_value_for_enable>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return sa.startObject(-1);
}
template <typename T, typename BufferType, typename Writer,
          traits::enable_if_t<traits::has_method_const_serialize<T, JsonOutputSerializer<BufferType, Writer>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return sa.endObject();
}

// #########################################################
// # arithmetic types
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(JsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(JsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferType, typename Writer,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferType, typename Writer,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// # std::string
template <typename CharT, typename Traits, typename Alloc>
inline bool prologue(JsonInputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc>
inline bool epilogue(JsonInputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}

template <typename CharT, typename Traits, typename Alloc, typename BufferType, typename Writer>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa,
                     const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc, typename BufferType, typename Writer>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa,
                     const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}

#if NEKO_CPP_PLUS >= 17
template <typename CharT, typename Traits, typename BufferType, typename Writer>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa,
                     const std::basic_string_view<CharT, Traits>&) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename BufferType, typename Writer>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa,
                     const std::basic_string_view<CharT, Traits>&) NEKO_NOEXCEPT {
    return true;
}
#endif

// #####################################################
// # std::nullptr_t
inline bool prologue(JsonInputSerializer& sa, const std::nullptr_t&) NEKO_NOEXCEPT { return true; }
inline bool epilogue(JsonInputSerializer& sa, const std::nullptr_t&) NEKO_NOEXCEPT { return true; }

template <typename BufferType, typename Writer>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa, const std::nullptr_t&) NEKO_NOEXCEPT {
    return true;
}
template <typename BufferType, typename Writer>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa, const std::nullptr_t&) NEKO_NOEXCEPT {
    return true;
}
// #####################################################
// # minimal serializable
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(JsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(JsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferType, typename Writer,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferType, typename Writer,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(JsonOutputSerializer<BufferType, Writer>& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// default JsonSerializer type definition
struct JsonSerializer {
    using OutputSerializer = JsonOutputSerializer<std::vector<char>, JsonWriter<detail::OutBufferWrapper>>;
    using InputSerializer  = JsonInputSerializer;
};

NEKO_END_NAMESPACE