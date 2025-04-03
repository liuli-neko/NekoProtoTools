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
template <typename BufferT = std::vector<char>>
struct PrettyJsonWriter {};

template <typename T, class enable = void>
struct json_output_buffer_type { // NOLINT(readability-identifier-naming)
    using output_buffer_type = void;
};

template <>
struct json_output_buffer_type<std::vector<char>, void> {
    using output_buffer_type = std::vector<char>;
    using wrapper_type       = OutBufferWrapper;
    using writer_type        = JsonWriter<OutBufferWrapper>;
    using char_type          = char;
};

template <typename T>
struct json_output_buffer_type<T, typename std::enable_if<std::is_base_of<std::ostream, T>::value>::type> {
    using output_buffer_type = typename std::remove_reference<T>::type;
    using wrapper_type       = rapidjson::BasicOStreamWrapper<T>;
    using writer_type        = JsonWriter<rapidjson::BasicOStreamWrapper<T>>;
    using char_type          = typename rapidjson::BasicOStreamWrapper<T>::Ch;
};

template <>
struct json_output_buffer_type<OutBufferWrapper, void> {
    using output_buffer_type = std::vector<char>;
    using wrapper_type       = OutBufferWrapper;
    using char_type          = OutBufferWrapper::Ch;
};

template <typename T>
struct json_output_buffer_type<PrettyJsonWriter<T>, void> {
    using output_buffer_type = typename json_output_buffer_type<T>::output_buffer_type;
    using wrapper_type       = typename json_output_buffer_type<T>::wrapper_type;
    using writer_type        = rapidjson::PrettyWriter<wrapper_type>;
    using char_type          = typename json_output_buffer_type<T>::char_type;
};

template <typename T, class enable = void>
struct json_input_buffer_type // NOLINT(readability-identifier-naming)
    : std::false_type {
    using input_buffer_type = void;
};

template <typename T>
struct json_input_buffer_type<T, typename std::enable_if<std::is_base_of<std::istream, T>::value>::type>
    : std::true_type {
    using input_buffer_type = T;
};

template <typename T, class enable = void>
struct is_pretty_json_writer // NOLINT(readability-identifier-naming)
    : std::false_type {};

template <typename T>
struct is_pretty_json_writer<rapidjson::PrettyWriter<T>> : std::true_type {};

class ConstJsonIterator {
public:
    using MemberIterator = JsonValue::ConstMemberIterator;
    using ValueIterator  = JsonValue::ConstValueIterator;

public:
    inline ConstJsonIterator() NEKO_NOEXCEPT : mIndex(0), mType(Null) {};
    inline ConstJsonIterator(MemberIterator begin, MemberIterator end) NEKO_NOEXCEPT : mMemberItBegin(begin),
                                                                                       mMemberItEnd(end),
                                                                                       mMemberIt(begin),
                                                                                       mIndex(0),
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

    inline ConstJsonIterator(ValueIterator begin, ValueIterator end) NEKO_NOEXCEPT : mMemberIt(),
                                                                                     mValueItBegin(begin),
                                                                                     mIndex(0),
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
    inline ConstJsonIterator& operator--() NEKO_NOEXCEPT {
        if (mType == Member) {
            --mMemberIt;
        } else {
            --mIndex;
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
        if (mMemberIt != mMemberItEnd &&
            name == NEKO_STRING_VIEW{mMemberIt->name.GetString(), mMemberIt->name.GetStringLength()}) {
            return &(mMemberIt)->value;
        }
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
    int precision               = rapidjson::PrettyWriter<detail::OutBufferWrapper>::kDefaultMaxDecimalPlaces;
};

namespace detail {
template <typename T, class enable = void>
struct set_json_format_option { // NOLINT(readability-identifier-naming)
    static void setting(T& /* unused */, const JsonOutputFormatOptions& /* unused */) {
        NEKO_LOG_INFO("rapidjson", "No output format options support for this writer({})", _class_name<T>());
    };
};

template <typename T>
struct set_json_format_option<T, typename std::enable_if<is_pretty_json_writer<T>::value>::type> {
    static void setting(T& writer, const JsonOutputFormatOptions& options) {
        writer.SetIndent(options.indentChar, options.indentLength);
        writer.SetFormatOptions(options.formatOptions);
        writer.SetMaxDecimalPlaces(options.precision);
    }
};
} // namespace detail

template <typename BufferT>
class RapidJsonOutputSerializer : public detail::OutputSerializer<RapidJsonOutputSerializer<BufferT>> {
public:
    using WriterType = typename detail::json_output_buffer_type<BufferT>::writer_type;

public:
    explicit RapidJsonOutputSerializer(typename detail::json_output_buffer_type<BufferT>::output_buffer_type& buffer)
        NEKO_NOEXCEPT : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
                        mStream(buffer),
                        mWriter(mStream) {}

    explicit RapidJsonOutputSerializer(typename detail::json_output_buffer_type<BufferT>::output_buffer_type& buffer,
                                       WriterType&& writer) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
          mStream(buffer),
          mWriter(std::move(writer)) {
        mWriter.Reset(mStream);
    }

    explicit RapidJsonOutputSerializer(typename detail::json_output_buffer_type<BufferT>::output_buffer_type& buffer,
                                       const JsonOutputFormatOptions& formatOptions) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
          mStream(buffer),
          mWriter(mStream) {
        detail::set_json_format_option<WriterType>::setting(mWriter, formatOptions);
    }

    RapidJsonOutputSerializer(const RapidJsonOutputSerializer& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
          mStream(other.mStream),
          mWriter(other.mWriter) {}
    RapidJsonOutputSerializer(RapidJsonOutputSerializer&& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidJsonOutputSerializer>(this),
          mStream(std::move(other.mStream)),
          mWriter(std::move(other.mWriter)) {}
    ~RapidJsonOutputSerializer() { end(); }
    template <typename T>
    bool saveValue(SizeTag<T> const& /*unused*/) {
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool saveValue(const T value) NEKO_NOEXCEPT {
        return mWriter.Int(static_cast<int32_t>(value));
    }
    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) < sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool saveValue(const T value) NEKO_NOEXCEPT {
        return mWriter.Uint(static_cast<uint32_t>(value));
    }
    bool saveValue(const int64_t value) NEKO_NOEXCEPT { return mWriter.Int64(value); }
    bool saveValue(const uint64_t value) NEKO_NOEXCEPT { return mWriter.Uint64(value); }
    bool saveValue(const float value) NEKO_NOEXCEPT { return mWriter.Double(value); }
    bool saveValue(const double value) NEKO_NOEXCEPT { return mWriter.Double(value); }
    bool saveValue(const bool value) NEKO_NOEXCEPT { return mWriter.Bool(value); }
    template <typename CharT, typename Traits, typename Alloc>
    bool saveValue(const std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        return mWriter.String(value.c_str(), value.size());
    }
    bool saveValue(const char* value) NEKO_NOEXCEPT { return mWriter.String(value); }
    bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT { return mWriter.Null(); }
#if NEKO_CPP_PLUS >= 17
    template <typename CharT, typename Traits>
    bool saveValue(const std::basic_string_view<CharT, Traits> value) NEKO_NOEXCEPT {
        return mWriter.String(value.data(), value.size());
    }

    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        if constexpr (traits::is_optional<T>::value) {
            if (value.value.has_value()) {
                return mWriter.Key(value.name, value.nameLen) && (*this)(value.value.value());
            }
            return true;
        } else {
            return mWriter.Key(value.name, value.nameLen) && (*this)(value.value);
        }
    }
#else
    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        return mWriter.Key(value.name, value.nameLen) && (*this)(value.value);
    }
#endif
    bool startArray(const std::size_t /*unused*/) NEKO_NOEXCEPT {
        ++mCurrentLevel;
        return mWriter.StartArray();
    }
    bool endArray() NEKO_NOEXCEPT {
        --mCurrentLevel;
        return mWriter.EndArray();
    }
    bool startObject(const std::size_t /*unused*/ = -1) NEKO_NOEXCEPT {
        ++mCurrentLevel;
        return mWriter.StartObject();
    }
    bool endObject() NEKO_NOEXCEPT {
        --mCurrentLevel;
        return mWriter.EndObject();
    }
    bool end() NEKO_NOEXCEPT {
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
    typename detail::json_output_buffer_type<BufferT>::wrapper_type mStream;
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
template <typename BufferT>
class RapidJsonInputSerializer : public detail::InputSerializer<RapidJsonInputSerializer<BufferT>> {

public:
    explicit RapidJsonInputSerializer(const char* buf, std::size_t size) NEKO_NOEXCEPT
        : detail::InputSerializer<RapidJsonInputSerializer>(this),
          mDocument(),
          mItemStack() {
        mDocument.Parse(buf, size);
    }

    explicit RapidJsonInputSerializer(BufferT& stream) NEKO_NOEXCEPT
        : detail::InputSerializer<RapidJsonInputSerializer>(this),
          mDocument(),
          mStream(new rapidjson::BasicIStreamWrapper<BufferT>(stream)),
          mItemStack() {
        mDocument.ParseStream(*mStream);
    }

    ~RapidJsonInputSerializer() NEKO_NOEXCEPT {
        if (mStream) {
            delete mStream;
            mStream = nullptr;
        }
    }

    operator bool() const NEKO_NOEXCEPT { return mDocument.GetParseError() == rapidjson::kParseErrorNone; }

    NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if ((*mCurrentItem).eof()) {
            return {};
        }
        return (*mCurrentItem).name();
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool loadValue(T& value) NEKO_NOEXCEPT {
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
    bool loadValue(T& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsUint()) {
            return false;
        }
        value = static_cast<T>((*mCurrentItem).value().GetUint());
        ++(*mCurrentItem);
        return true;
    }

    template <typename CharT, typename Traits, typename Alloc>
    bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsString()) {
            return false;
        }
        const auto& cvalue = (*mCurrentItem).value();
        value              = std::basic_string<CharT, Traits, Alloc>{cvalue.GetString(), cvalue.GetStringLength()};
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(int64_t& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsInt64()) {
            return false;
        }
        value = (*mCurrentItem).value().GetInt64();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(uint64_t& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsUint64()) {
            return false;
        }
        value = (*mCurrentItem).value().GetUint64();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(float& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsNumber()) {
            return false;
        }
        value = (*mCurrentItem).value().GetDouble();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(double& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsNumber()) {
            return false;
        }
        value = (*mCurrentItem).value().GetDouble();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(bool& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsBool()) {
            return false;
        }
        value = (*mCurrentItem).value().GetBool();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        if (!(*mCurrentItem).value().IsNull()) {
            return false;
        }
        ++(*mCurrentItem);
        return true;
    }

    template <typename T>
    bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mCurrentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
        value.size = (*mCurrentItem).size();
        return true;
    }

#if NEKO_CPP_PLUS >= 17
    template <typename T>
    bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
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
                ret = (*this)(result);
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
            ret = (*this)(value.value);
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
    bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
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
    bool startNode() NEKO_NOEXCEPT {
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

    bool finishNode() NEKO_NOEXCEPT {
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

    void rollbackItem() {
        if (mCurrentItem != nullptr) {
            --(*mCurrentItem);
        }
    }

private:
    RapidJsonInputSerializer& operator=(const RapidJsonInputSerializer&) = delete;
    RapidJsonInputSerializer& operator=(RapidJsonInputSerializer&&)      = delete;

private:
    detail::JsonDocument mDocument;
    std::vector<detail::ConstJsonIterator> mItemStack;
    detail::ConstJsonIterator* mCurrentItem          = nullptr;
    rapidjson::BasicIStreamWrapper<BufferT>* mStream = nullptr;
};

// #######################################################
// JsonSerializer prologue and epilogue
// #######################################################

// #######################################################
// name value pair
template <typename T, typename BufferT>
inline bool prologue(RapidJsonInputSerializer<BufferT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT>
inline bool epilogue(RapidJsonInputSerializer<BufferT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
//  size tag
template <typename T, typename BufferT>
inline bool prologue(RapidJsonInputSerializer<BufferT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT>
inline bool epilogue(RapidJsonInputSerializer<BufferT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
template <
    class T, typename BufferT,
    traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonInputSerializer<BufferT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <
    typename T, typename BufferT,
    traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonInputSerializer<BufferT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <class T, typename WriterT,
          traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                              !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, RapidJsonOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, RapidJsonOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<traits::has_method_const_serialize<T, RapidJsonOutputSerializer<WriterT>>::value ||
                              traits::has_method_serialize<T, RapidJsonOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startObject((std::size_t)-1);
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
template <typename T, typename BufferT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// # std::string
template <typename BufferT, typename CharT, typename Traits, typename Alloc>
inline bool prologue(RapidJsonInputSerializer<BufferT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename BufferT, typename CharT, typename Traits, typename Alloc>
inline bool epilogue(RapidJsonInputSerializer<BufferT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename CharT, typename Traits, typename Alloc, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

#if NEKO_CPP_PLUS >= 17
template <typename CharT, typename Traits, typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
#endif

// #####################################################
// # std::nullptr_t
template <typename BufferT>
inline bool prologue(RapidJsonInputSerializer<BufferT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
template <typename BufferT>
inline bool epilogue(RapidJsonInputSerializer<BufferT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}

template <typename WriterT>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
template <typename WriterT>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
// #####################################################
// # minimal serializable
template <typename T, typename BufferT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidJsonOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// default JsonSerializer type definition
struct RapidJsonSerializer {
    using OutputSerializer = RapidJsonOutputSerializer<std::vector<char>>;
    using InputSerializer  = RapidJsonInputSerializer<std::istream>;
};

NEKO_END_NAMESPACE

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif