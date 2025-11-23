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
#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "nekoproto/global/log.hpp"
#include "nekoproto/global/reflect.hpp"

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

#include "nekoproto/serialization/private/helpers.hpp"

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
        NEKO_LOG_INFO("rapidjson", "No output format options support for this writer({})", class_nameof<T>);
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

class RapidJsonValue {
public:
    RapidJsonValue() = default;
    explicit RapidJsonValue(const JsonValue& value) {
        mValue = std::make_shared<JsonDocument>();
        mValue->CopyFrom(value, mValue->GetAllocator());
    }
    auto hasValue() const -> bool { return mValue != nullptr; }
    operator bool() const { return hasValue(); }
    auto nativeValue() const -> const JsonValue& { return *mValue; }
    auto nativeValue() -> JsonValue& { return *mValue; }
    auto isObject() const -> bool { return mValue && mValue->IsObject(); }
    auto isArray() const -> bool { return mValue && mValue->IsArray(); }
    auto isString() const -> bool { return mValue && mValue->IsString(); }
    auto isNumber() const -> bool { return mValue && mValue->IsNumber(); }
    auto isBool() const -> bool { return mValue && mValue->IsBool(); }
    auto isNull() const -> bool { return mValue && mValue->IsNull(); }

    template <typename T>
    auto value(T& value) const -> bool {
        if (mValue && mValue->template Is<T>()) {
            value = mValue->template Get<T>();
            return true;
        }
        return false;
    }

    auto size() const -> std::size_t {
        if (isArray()) {
            return mValue->Size();
        }
        if (isObject()) {
            return mValue->MemberCount();
        }
        return 0;
    }

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    auto operator[](const T& name) const -> RapidJsonValue {
        if (isObject()) {
            auto view  = std::string_view(name);
            auto value = mValue->FindMember(JsonValue(view.data(), view.size()));
            if (value != mValue->MemberEnd()) {
                return RapidJsonValue(value->value);
            }
        }
        return RapidJsonValue();
    }

    auto operator[](std::size_t index) const -> RapidJsonValue {
        if (isArray()) {
            if (index < mValue->Size()) {
                return RapidJsonValue(mValue->GetArray()[(int)index]);
            }
        }
        if (isObject()) {
            if (index < mValue->MemberCount()) {
                return RapidJsonValue((mValue->MemberBegin() + index)->value);
            }
        }
        return RapidJsonValue();
    }

private:
    std::shared_ptr<JsonDocument> mValue;
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

    template <typename T>
        requires std::is_signed_v<T> && (sizeof(T) < sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool saveValue(const T value) NEKO_NOEXCEPT {
        return mWriter.Int(static_cast<int32_t>(value));
    }

    template <typename T>
        requires std::is_unsigned_v<T> && (sizeof(T) < sizeof(uint64_t)) && (!std::is_enum_v<T>)
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
        return mWriter.String(value.c_str(), (int)value.size());
    }
    bool saveValue(const char* value) NEKO_NOEXCEPT { return mWriter.String(value); }
    bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT { return mWriter.Null(); }
    bool saveValue(const detail::RapidJsonValue& value) NEKO_NOEXCEPT {
        if (value.hasValue()) {
            return value.nativeValue().Accept(mWriter);
        }
        return saveValue(std::nullptr_t{});
    }
    template <typename CharT, typename Traits>
    bool saveValue(const std::basic_string_view<CharT, Traits> value) NEKO_NOEXCEPT {
        return mWriter.String(value.data(), (int)value.size());
    }
    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("JsonSerializer", "saveValue: {}", std::string_view{value.name, value.nameLen});
        if constexpr (traits::optional_like_type<T>::value) {
            if (traits::optional_like_type<T>::has_value(value.value)) {
                return mWriter.Key(value.name, (int)value.nameLen) &&
                       (*this)(traits::optional_like_type<T>::get_value(value.value));
            }
            return true;
        } else {
            return mWriter.Key(value.name, (int)value.nameLen) && (*this)(value.value);
        }
    }
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
    typename detail::json_output_buffer_type<BufferT>::wrapper_type mStream;
    WriterType mWriter;
    int mCurrentLevel = 0;
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
        mLastResult = mDocument.GetParseError() == rapidjson::kParseErrorNone;
    }

    explicit RapidJsonInputSerializer(const detail::RapidJsonValue& value) NEKO_NOEXCEPT
        : detail::InputSerializer<RapidJsonInputSerializer>(this),
          mDocument(),
          mItemStack() {
        mLastResult = value.hasValue();
        if (mLastResult) {
            mDocument.CopyFrom(value.nativeValue(), mDocument.GetAllocator());
        }
    }

    explicit RapidJsonInputSerializer(BufferT& stream) NEKO_NOEXCEPT
        : detail::InputSerializer<RapidJsonInputSerializer>(this),
          mDocument(),
          mItemStack(),
          mStream(new rapidjson::BasicIStreamWrapper<BufferT>(stream)) {
        mDocument.ParseStream(*mStream);
        mLastResult = mDocument.GetParseError() == rapidjson::kParseErrorNone;
    }

    ~RapidJsonInputSerializer() NEKO_NOEXCEPT {
        if (mStream) {
            delete mStream;
            mStream = nullptr;
        }
    }

    operator bool() const NEKO_NOEXCEPT { return mLastResult; }

    NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return {};
        }
        if ((*mCurrentItem).eof()) {
            return {};
        }
        return (*mCurrentItem).name();
    }

    template <typename T>
        requires std::is_signed_v<T> && (sizeof(T) < sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsInt();
        if (!mLastResult) {
            return false;
        }
        value = static_cast<T>((*mCurrentItem).value().GetInt());
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(detail::RapidJsonValue& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            value = detail::RapidJsonValue(mDocument);
        } else {
            value = detail::RapidJsonValue((*mCurrentItem).value());
            ++(*mCurrentItem);
        }
        mLastResult = true;
        return true;
    }

    template <typename T>
        requires std::is_unsigned_v<T> && (sizeof(T) < sizeof(uint64_t)) && (!std::is_enum_v<T>)
    bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsUint();
        if (!mLastResult) {
            return false;
        }
        value = static_cast<T>((*mCurrentItem).value().GetUint());
        ++(*mCurrentItem);
        return true;
    }

    template <typename CharT, typename Traits, typename Alloc>
    bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsString();
        if (!mLastResult) {
            return false;
        }
        const auto& cvalue = (*mCurrentItem).value();
        value              = std::basic_string<CharT, Traits, Alloc>{cvalue.GetString(), cvalue.GetStringLength()};
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(int64_t& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsInt64();
        if (!mLastResult) {
            return false;
        }
        value = (*mCurrentItem).value().GetInt64();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(uint64_t& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsUint64();
        if (!mLastResult) {
            return false;
        }
        value = (*mCurrentItem).value().GetUint64();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(float& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsNumber();
        if (!mLastResult) {
            return false;
        }
        value = static_cast<float>((*mCurrentItem).value().GetDouble());
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(double& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsNumber();
        if (!mLastResult) {
            return false;
        }
        value = (*mCurrentItem).value().GetDouble();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(bool& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsBool();
        if (!mLastResult) {
            return false;
        }
        value = (*mCurrentItem).value().GetBool();
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().IsNull();
        if (!mLastResult) {
            return false;
        }
        ++(*mCurrentItem);
        return true;
    }

    template <typename T>
    bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        value.size = static_cast<uint32_t>((*mCurrentItem).size());
        return true;
    }

    template <typename T>
    bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        const auto& cvalue = (*mCurrentItem).moveToMember({value.name, value.nameLen});
        mLastResult        = true;
        if constexpr (traits::optional_like_type<T>::value) {
            if (nullptr == cvalue || cvalue->IsNull()) {
                traits::optional_like_type<T>::set_null(value.value);
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_INFO("JsonSerializer", "optional field {} is not find.",
                              std::string(value.name, value.nameLen));
#endif
            } else {
#if defined(NEKO_JSON_MAKE_STR_NONE_TO_NULL)
                // Why would anyone write "None" in json?
                // I've seen it, and it's a disaster.
                if (cvalue->IsString() && std::strcmp(cvalue->GetString(), "None") == 0) {
                    traits::optional_like_type<T>::set_null(value.value);
#if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_WARN("JsonSerializer", "optional field {} is \"None\".",
                                  std::string(value.name, value.nameLen));
#endif
                    return true;
                }
#endif
                if constexpr (std::is_default_constructible_v<typename traits::optional_like_type<T>::type>) {
                    typename traits::optional_like_type<T>::type temp;
                    mLastResult = (*this)(temp);
                    traits::optional_like_type<T>::set_value(value.value, std::move(temp));
                } else {
                    mLastResult = (*this)(traits::optional_like_type<T>::get_value(value.value));
                }
#if defined(NEKO_VERBOSE_LOGS)
                if (mLastResult) {
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
                if constexpr (is_skipable<T>::value) {
                    mLastResult = true;
                    return true;
                } else {
#if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_ERROR("JsonSerializer", "field {} is not find.", std::string(value.name, value.nameLen));
#endif
                    mLastResult = false;
                    return false;
                }
            }
            mLastResult = (*this)(value.value);
#if defined(NEKO_VERBOSE_LOGS)
            if (mLastResult) {
                NEKO_LOG_INFO("JsonSerializer", "field {} get value success.", std::string(value.name, value.nameLen));
            } else {
                NEKO_LOG_ERROR("JsonSerializer", "field {} get value fail.", std::string(value.name, value.nameLen));
            }
#endif
        }
        return mLastResult;
    }
    bool startNode() NEKO_NOEXCEPT {
        if (!mItemStack.empty()) {
            if (mCurrentItem == nullptr) {
                return false;
            }
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
            if (mLastResult) {
                ++(*mCurrentItem);
            } else {
                mLastResult = true;
            }
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

    bool isArray() {
        if (mItemStack.empty()) {
            return mDocument.IsArray();
        }
        if (mCurrentItem == nullptr) {
            return false;
        }
        return (*mCurrentItem).value().IsArray();
    }

    bool isObject() {
        if (mItemStack.empty()) {
            return mDocument.IsObject();
        }
        if (mCurrentItem == nullptr) {
            return false;
        }
        return (*mCurrentItem).value().IsObject();
    }

private:
    RapidJsonInputSerializer& operator=(const RapidJsonInputSerializer&) = delete;
    RapidJsonInputSerializer& operator=(RapidJsonInputSerializer&&)      = delete;

private:
    detail::JsonDocument mDocument;
    std::vector<detail::ConstJsonIterator> mItemStack;
    detail::ConstJsonIterator* mCurrentItem          = nullptr;
    rapidjson::BasicIStreamWrapper<BufferT>* mStream = nullptr;
    bool mLastResult                                 = true;
};

template <>
struct is_minimal_serializable<detail::RapidJsonValue> : std::true_type {};

template <typename WriterT>
inline bool save(RapidJsonOutputSerializer<WriterT>& sa, const detail::RapidJsonValue& value) NEKO_NOEXCEPT {
    return sa.saveValue(value);
}

template <typename BufferT>
inline bool load(RapidJsonInputSerializer<BufferT>& sa, detail::RapidJsonValue& value) NEKO_NOEXCEPT {
    return sa.loadValue(value);
}

// #####################################################
// default JsonSerializer type definition
struct RapidJsonSerializer {
    using OutputSerializer = RapidJsonOutputSerializer<std::vector<char>>;
    using InputSerializer  = RapidJsonInputSerializer<std::istream>;
    using JsonValue        = detail::RapidJsonValue;
};

NEKO_END_NAMESPACE

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif