/**
 * @file rapid_json_serializer.hpp
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

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "private/log.hpp"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <type_traits>
#include <unordered_map>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#include <variant>
#endif

#ifdef _WIN32
#pragma push_macro("GetObject")
#ifdef GetObject
#undef GetObject
#endif
#endif

#include "private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {

using JsonValue       = rapidjson::Value;
using ConstJsonValue  = rapidjson::Value;
using JsonObject      = rapidjson::Value::Object;
using ConstJsonObject = rapidjson::Value::ConstObject;
using JsonArray       = rapidjson::Value::Array;
using ConstJsonArray  = rapidjson::Value::ConstArray;
using JsonDocument    = rapidjson::Document;
using OStreamWrapper  = rapidjson::OStreamWrapper;
template <typename BufferT = OutBufferWrapper>
using JsonWriter = rapidjson::Writer<BufferT>;
template <typename BufferT = OutBufferWrapper>
using PrettyJsonWriter = rapidjson::PrettyWriter<BufferT>;

template <typename T, class enable = void>
struct json_output_wrapper_type { // NOLINT(readability-identifier-naming)
    using output_stream_type = void;
};
template <typename OutputStream, typename SourceEncoding, typename TargetEncoding, typename StackAllocator,
          unsigned writeFlags>
struct json_output_wrapper_type<
    rapidjson::Writer<OutputStream, SourceEncoding, TargetEncoding, StackAllocator, writeFlags>, void> {
    using output_stream_type = OutputStream;
};
template <typename OutputStream, typename SourceEncoding, typename TargetEncoding, typename StackAllocator,
          unsigned writeFlags>
struct json_output_wrapper_type<
    rapidjson::PrettyWriter<OutputStream, SourceEncoding, TargetEncoding, StackAllocator, writeFlags>, void> {
    using output_stream_type = OutputStream;
};

template <typename T, class enable = void>
struct json_output_buffer_type { // NOLINT(readability-identifier-naming)
    using output_buffer_type = void;
};

template <>
struct json_output_buffer_type<OutBufferWrapper, void> {
    using output_buffer_type = std::vector<char>;
    using char_type          = char;
};

template <typename T>
struct json_output_buffer_type<rapidjson::BasicOStreamWrapper<T>, void> {
    using output_buffer_type = T;
    using char_type          = typename T::Ch;
};
class ConstJsonIterator {
public:
    using MemberIterator = JsonValue::ConstMemberIterator;
    using ValueIterator  = JsonValue::ConstValueIterator;

public:
    inline ConstJsonIterator() NEKO_NOEXCEPT : mIndex(0), mType(Null){};
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
            mType = Null;
            return;
        }
    }

    inline ConstJsonIterator(ValueIterator begin, ValueIterator end) NEKO_NOEXCEPT : mValueItBegin(begin),
                                                                                     mIndex(0),
                                                                                     mMemberIt(),
                                                                                     mSize(std::distance(begin, end)),
                                                                                     mType(Value) {
        if (mSize == 0) {
            mType = Null;
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
        return mType == Null || (mType == Value && mIndex >= mSize) || (mType == Member && mMemberIt == mMemberItEnd);
    }
    inline std::size_t size() const NEKO_NOEXCEPT { return mSize; }
    inline const JsonValue& value() NEKO_NOEXCEPT {
        static JsonValue kNull;
        if (eof()) {
            return kNull;
        }
        switch (mType) {
        case Value:
            return mValueItBegin[mIndex];
        case Member:
            return mMemberIt->value;
        default:
            return kNull; // should never reach here, but needed to avoid compiler warning
        }
    }
    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if (mType == Member && mMemberIt != mMemberItEnd) {
            return {mMemberIt->name.GetString(), mMemberIt->name.GetStringLength()};
        }
        return {};
    };
    inline const JsonValue* moveToMember(const NEKO_STRING_VIEW& name) NEKO_NOEXCEPT {
        auto it = mMemberMap.find(name);
        if (it != mMemberMap.end()) {
            mMemberIt = it->second;
            return &(mMemberIt)->value;
        }
        return nullptr;
    };

private:
    MemberIterator mMemberItBegin, mMemberItEnd, mMemberIt;
    ValueIterator mValueItBegin;
    std::size_t mIndex, mSize;
    std::unordered_map<NEKO_STRING_VIEW, MemberIterator> mMemberMap;
    enum Type { Value, Member, Null } mType;
};
} // namespace detail

struct JsonOutputFormatOptions {
public:
    enum class Indent : char {
        Space   = ' ',
        Newline = '\n',
        Tab     = '\t',
    };
    using FormatOptions = rapidjson::PrettyFormatOptions;
    static JsonOutputFormatOptions Default() { // NOLINT(readability-identifier-naming)
        return JsonOutputFormatOptions();
    }
    static JsonOutputFormatOptions Compact() { // NOLINT(readability-identifier-naming)
        return JsonOutputFormatOptions(Indent::Space, 0);
    }
    explicit JsonOutputFormatOptions(Indent indentChar = Indent::Space, uint32_t indentLength = 4,
                                     FormatOptions formatOptions = FormatOptions::kFormatSingleLineArray,
                                     int precision               = detail::JsonWriter<>::kDefaultMaxDecimalPlaces)
        : indentChar(static_cast<char>(indentChar)), indentLength(indentLength), formatOptions(formatOptions),
          precision(precision) {}

    char indentChar             = static_cast<char>(Indent::Space);
    int indentLength            = 4;
    FormatOptions formatOptions = FormatOptions::kFormatDefault;
    int precision               = detail::PrettyJsonWriter<>::kDefaultMaxDecimalPlaces;
};

inline auto make_pretty_json_writer(const JsonOutputFormatOptions& options = JsonOutputFormatOptions::Default())
    NEKO_NOEXCEPT -> detail::PrettyJsonWriter<detail::OutBufferWrapper> {
    auto writer = detail::PrettyJsonWriter<detail::OutBufferWrapper>(0);
    writer.SetIndent(options.indentChar, options.indentLength);
    writer.SetMaxDecimalPlaces(options.precision);
    writer.SetFormatOptions(options.formatOptions);
    return std::move(writer);
}

template <typename WriterT = detail::JsonWriter<>>
class RapidJsonOutputSerializer : public detail::OutputSerializer<RapidJsonOutputSerializer<WriterT>> {
public:
    using WriterType = WriterT;

public:
    explicit inline RapidJsonOutputSerializer(
        typename detail::json_output_buffer_type<
            typename detail::json_output_wrapper_type<WriterType>::output_stream_type>::output_buffer_type& buffer)
        NEKO_NOEXCEPT : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
                        mStream(buffer),
                        mWriter(mStream) {}

    explicit inline RapidJsonOutputSerializer(
        typename detail::json_output_buffer_type<
            typename detail::json_output_wrapper_type<WriterType>::output_stream_type>::output_buffer_type& buffer,
        WriterT&& writer) NEKO_NOEXCEPT : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
                                          mStream(buffer),
                                          mWriter(std::move(writer)) {
        mWriter.Reset(mStream);
    }

    inline RapidJsonOutputSerializer(const RapidJsonOutputSerializer& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
          mStream(other.mStream),
          mWriter(other.mWriter) {}
    inline RapidJsonOutputSerializer(RapidJsonOutputSerializer&& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
          mStream(std::move(other.mStream)),
          mWriter(std::move(other.mWriter)) {}
    inline ~RapidJsonOutputSerializer() { end(); }
    template <typename T>
    inline bool saveValue(SizeTag<T> const& /*unused*/) {
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
    inline bool startArray(const std::size_t /*unused*/) NEKO_NOEXCEPT {
        ++mCurrentLevel;
        return mWriter.StartArray();
    }
    inline bool endArray() NEKO_NOEXCEPT {
        --mCurrentLevel;
        return mWriter.EndArray();
    }
    inline bool startObject(const std::size_t /*unused*/ = -1) NEKO_NOEXCEPT {
        ++mCurrentLevel;
        return mWriter.StartObject();
    }
    inline bool endObject() NEKO_NOEXCEPT {
        --mCurrentLevel;
        return mWriter.EndObject();
    }
    inline bool end() NEKO_NOEXCEPT {
        if (!mWriter.IsComplete() && mCurrentLevel > 0) {
            --mCurrentLevel;
            auto ret = mWriter.EndObject();
            mWriter.Flush();
            return ret && mCurrentLevel == 0;
        }
        return true;
    }

private:
    RapidJsonOutputSerializer& operator=(const RapidJsonOutputSerializer&) = delete;
    RapidJsonOutputSerializer& operator=(RapidJsonOutputSerializer&&)      = delete;

private:
    WriterType mWriter;
    int mCurrentLevel = 0;
    typename detail::json_output_wrapper_type<WriterType>::output_stream_type mStream;
};

/**
 * @brief json input serializer
 * This class provides a convenient template interface to help you parse JSON to CPP objects, you only need to give the
 * variables that need to be assigned to this class through parentheses, and the class will automatically expand the
 * object and array to make it easier for you to iterate through all the values.
 *
 * @note
 * A layer is automatically unfolded when constructing.
 * Like a vector<int> vec, and you have a array json data {1, 2, 3, 4} "json_data"
 * you construct RapidJsonInputSerializer is(json_data)
 * you should not is(vec), because it will try unfold the first node as array.
 * you should call a load function like load(sa, vec). or do it like this:
 * size_t size;
 * is(make_size_tag(size));
 * vec.resize(size);
 * for (auto &v : vec) {
 *     is(v);
 * }
 *
 */
class RapidJsonInputSerializer : public detail::InputSerializer<RapidJsonInputSerializer> {

public:
    inline explicit RapidJsonInputSerializer(const char* buf, std::size_t size) NEKO_NOEXCEPT
        : detail::InputSerializer<RapidJsonInputSerializer>(this),
          mDocument(),
          mItemStack() {
        mDocument.Parse(buf, size);
    }
    inline explicit RapidJsonInputSerializer(rapidjson::IStreamWrapper& stream) NEKO_NOEXCEPT
        : detail::InputSerializer<RapidJsonInputSerializer>(this),
          mDocument(),
          mItemStack() {
        mDocument.ParseStream(stream);
    }
    inline operator bool() const NEKO_NOEXCEPT { return mDocument.GetParseError() == rapidjson::kParseErrorNone; }

    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if ((*mCurrentItem).eof()) {
            return {};
        }
        return (*mCurrentItem).name();
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsInt()) {
            return false;
        }
        value = static_cast<T>((*mCurrentItem).value().GetInt());
        ++(*mCurrentItem);
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) < sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsUint()) {
            return false;
        }
        value = static_cast<T>((*mCurrentItem).value().GetUint());
        ++(*mCurrentItem);
        return true;
    }

    template <typename CharT, typename Traits, typename Alloc>
    inline bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsString()) {
            return false;
        }
        const auto& cvalue = (*mCurrentItem).value();
        value              = std::basic_string<CharT, Traits, Alloc>{cvalue.GetString(), cvalue.GetStringLength()};
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(int64_t& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsInt64()) {
            return false;
        }
        value = (*mCurrentItem).value().GetInt64();
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(uint64_t& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsUint64()) {
            return false;
        }
        value = (*mCurrentItem).value().GetUint64();
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(float& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsNumber()) {
            return false;
        }
        value = (*mCurrentItem).value().GetDouble();
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(double& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsNumber()) {
            return false;
        }
        value = (*mCurrentItem).value().GetDouble();
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(bool& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsBool()) {
            return false;
        }
        value = (*mCurrentItem).value().GetBool();
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsNull()) {
            return false;
        }
        ++(*mCurrentItem);
        return true;
    }

    template <typename T>
    inline bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        value.size = (*mCurrentItem).size();
        return true;
    }

#if NEKO_CPP_PLUS >= 17
    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        const auto& cvalue = (*mCurrentItem).moveToMember({value.name, value.nameLen});
        bool ret           = true;
        if constexpr (traits::is_optional<T>::value) {
            if (nullptr == cvalue || cvalue->IsNull()) {
                value.value.reset();
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_INFO("JsonSerializer", "optional field {} is not find.",
                              std::string(value.name, value.nameLen));
#endif
            } else {
#if defined(NEKO_JSON_MAKE_STR_NONE_TO_NULL)
                // Why would anyone write "None" in json?
                // I've seen it, and it's a disaster.
                if (cvalue->IsString() && std::strcmp(cvalue->GetString(), "None") == 0) {
                    value.value.reset();
#if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_WARN("JsonSerializer", "optional field {} is \"None\".",
                                  std::string(value.name, value.nameLen));
#endif
                    return true;
                }
#endif
                typename traits::is_optional<T>::value_type result;
                ret = operator()(result);
                value.value.emplace(std::move(result));
#if defined(NEKO_VERBOSE_LOGS)
                if (ret) {
                    NEKO_LOG_INFO("JsonSerializer", "optional field {} get value success.",
                                  std::string(value.name, value.nameLen));
                } else {
                    NEKO_LOG_ERROR("JsonSerializer", "optional field {} get value fail.",
                                   std::string(value.name, value.nameLen));
                }
#endif
            }
        } else {
            if (nullptr == cvalue) {
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_ERROR("JsonSerializer", "field {} is not find.", std::string(value.name, value.nameLen));
#endif
                return false;
            }
            ret = operator()(value.value);
#if defined(NEKO_VERBOSE_LOGS)
            if (ret) {
                NEKO_LOG_INFO("JsonSerializer", "field {} get value success.", std::string(value.name, value.nameLen));
            } else {
                NEKO_LOG_ERROR("JsonSerializer", "field {} get value fail.", std::string(value.name, value.nameLen));
            }
#endif
        }
        return ret;
    }
#else
    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        const auto& v = (*mCurrentItem).move_to_member({value.name, value.nameLen});
        if (nullptr == v) {
#if defined(NEKO_VERBOSE_LOGS)
            NEKO_LOG_ERROR("JsonSerializer", "field {} is not find.", std::string(value.name, value.nameLen));
#endif
            return false;
        }
#if defined(NEKO_VERBOSE_LOGS)
        auto ret = operator()(value.value);
        if (ret) {
            NEKO_LOG_INFO("JsonSerializer", "field {} get value success.", std::string(value.name, value.nameLen));
        } else {
            NEKO_LOG_ERROR("JsonSerializer", "field {} get value fail.", std::string(value.name, value.nameLen));
        }
        return ret;
#else
        return operator()(value.value);
#endif
    }
#endif
    inline bool startNode() NEKO_NOEXCEPT {
        if (!mItemStack.empty()) {
            if ((*mCurrentItem).value().IsArray()) {
                mItemStack.emplace_back((*mCurrentItem).value().Begin(), (*mCurrentItem).value().End());
            } else if ((*mCurrentItem).value().IsObject()) {
                mItemStack.emplace_back((*mCurrentItem).value().MemberBegin(), (*mCurrentItem).value().MemberEnd());
            } else {
                return false;
            }
        } else {
            if (mDocument.IsArray()) {
                mItemStack.emplace_back(mDocument.Begin(), mDocument.End());
            } else if (mDocument.IsObject()) {
                mItemStack.emplace_back(mDocument.MemberBegin(), mDocument.MemberEnd());
            } else {
                return false;
            }
        }
        mCurrentItem = &mItemStack.back();
        return true;
    }

    inline bool finishNode() NEKO_NOEXCEPT {
        if (mItemStack.size() >= 2) {
            mItemStack.pop_back();
            mCurrentItem = &mItemStack.back();
            ++(*mCurrentItem);
        } else if (mItemStack.size() == 1) {
            mItemStack.pop_back();
            mCurrentItem = nullptr;
        } else {
            return false;
        }
        return true;
    }

private:
    RapidJsonInputSerializer& operator=(const RapidJsonInputSerializer&) = delete;
    RapidJsonInputSerializer& operator=(RapidJsonInputSerializer&&)      = delete;

private:
    detail::JsonDocument mDocument;
    std::vector<detail::ConstJsonIterator> mItemStack;
    detail::ConstJsonIterator* mCurrentItem;
};

// #######################################################
// JsonSerializer prologue and epilogue
// #######################################################

// #######################################################
// name value pair
template <typename T>
inline bool prologue(RapidJsonInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(RapidJsonInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
//  size tag
template <typename T>
inline bool prologue(RapidJsonInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(RapidJsonInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
template <class T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                       traits::default_value_for_enable>
inline bool prologue(RapidJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <typename T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(RapidJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <class T, typename WriterT,
          traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                              !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, RapidJsonOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, RapidJsonOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<traits::has_method_const_serialize<T, RapidJsonOutputSerializer<WriterT>>::value ||
                              traits::has_method_serialize<T, RapidJsonOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startObject(-1);
}
template <typename T, typename WriterT,
          traits::enable_if_t<traits::has_method_const_serialize<T, RapidJsonOutputSerializer<WriterT>>::value ||
                              traits::has_method_serialize<T, RapidJsonOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.endObject();
}

// #########################################################
// # arithmetic types
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// # std::string
template <typename CharT, typename Traits, typename Alloc>
inline bool prologue(RapidJsonInputSerializer& sa,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc>
inline bool epilogue(RapidJsonInputSerializer& sa,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename CharT, typename Traits, typename Alloc, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

#if NEKO_CPP_PLUS >= 17
template <typename CharT, typename Traits, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
#endif

// #####################################################
// # std::nullptr_t
inline bool prologue(RapidJsonInputSerializer& sa, const std::nullptr_t) NEKO_NOEXCEPT { return true; }
inline bool epilogue(RapidJsonInputSerializer& sa, const std::nullptr_t) NEKO_NOEXCEPT { return true; }

template <typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
template <typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
// #####################################################
// # minimal serializable
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// default JsonSerializer type definition
struct RapidJsonSerializer {
    using OutputSerializer = RapidJsonOutputSerializer<detail::JsonWriter<detail::OutBufferWrapper>>;
    using InputSerializer  = RapidJsonInputSerializer;
};

NEKO_END_NAMESPACE

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif