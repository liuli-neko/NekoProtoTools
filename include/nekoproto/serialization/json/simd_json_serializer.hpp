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
#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)
#include <iomanip>
#include <simdjson.h>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "nekoproto/global/log.hpp"
#include "nekoproto/serialization/private/helpers.hpp"
#include "nekoproto/serialization/types/enum.hpp"

#ifdef _WIN32
#pragma push_macro("GetObject")
#ifdef GetObject
#undef GetObject
#endif
#endif

NEKO_BEGIN_NAMESPACE

namespace detail {
namespace simd {

using RawJsonValue  = simdjson::dom::element;
using JsonValue     = simdjson::simdjson_result<simdjson::dom::element>;
using RawJsonObject = simdjson::dom::object;
using JsonObject    = simdjson::simdjson_result<simdjson::dom::object>;
using RawJsonArray  = simdjson::dom::array;
using JsonArray     = simdjson::simdjson_result<simdjson::dom::array>;
using JsonDocument  = simdjson::dom::document;
using JsonParser    = simdjson::dom::parser;

template <typename T, class enable = void>
struct json_output_buffer_type { // NOLINT(readability-identifier-naming)
    using output_buffer_type = void;
};

class ConstJsonIterator {
public:
    using MemberIterator = simdjson::dom::object::iterator;
    using ValueIterator  = simdjson::dom::array::iterator;

public:
    ConstJsonIterator() NEKO_NOEXCEPT : mType(Null) {};
    ConstJsonIterator(const JsonObject& object) NEKO_NOEXCEPT : mMemberIndex(0), mSize(object.size()), mType(Member) {
        if (mSize == 0) {
            mType = Null;
            return;
        }
        for (auto iter = object.begin(); iter != object.end(); ++iter) {
            mMembers.push_back(iter);
            mMemberMap.emplace(iter.key(), mMembers.size() - 1);
        }
    }

    ConstJsonIterator(const JsonArray& array) NEKO_NOEXCEPT : mValueIndex(0), mSize(array.size()), mType(Value) {
        if (mSize == 0) {
            mType = Null;
            return;
        }
        for (auto iter = array.begin(); iter != array.end(); ++iter) {
            mValues.push_back(iter);
        }
    }

    ~ConstJsonIterator() = default;

    ConstJsonIterator& operator++() NEKO_NOEXCEPT {
        if (mType == Member) {
            ++mMemberIndex;
        } else {
            ++mValueIndex;
        }
        return *this;
    }

    ConstJsonIterator& operator--() NEKO_NOEXCEPT {
        if (mType == Member) {
            --mMemberIndex;
        } else {
            --mValueIndex;
        }
        return *this;
    }

    bool eof() const NEKO_NOEXCEPT {
        return mType == Null || (mType == Value && mValueIndex == (int)mValues.size()) ||
               (mType == Member && mMemberIndex >= (int)mMembers.size());
    }
    std::size_t size() const NEKO_NOEXCEPT { return mSize; }
    JsonValue value() NEKO_NOEXCEPT {
        NEKO_ASSERT(!eof(), "JsonSerializer", "JsonInputSerializer get next value called on end of this json object");
        switch (mType) {
        case Value:
            return *mValues[mValueIndex];
        case Member:
            return mMembers[mMemberIndex].value();
        default:
            return JsonValue(
                simdjson::error_code::NO_SUCH_FIELD); // should never reach here, but needed to avoid compiler warning
        }
    }
    NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if (mType == Member && mMemberIndex < (int)mMembers.size()) {
            return mMembers[mMemberIndex].key();
        }
        return {};
    };
    JsonValue moveToMember(const NEKO_STRING_VIEW& name) NEKO_NOEXCEPT {
        if (mType != Member) {
            return JsonValue(simdjson::error_code::NO_SUCH_FIELD);
        }
        if (mMemberIndex < (int)mMembers.size() && mMembers[mMemberIndex].key() == name) {
            return mMembers[mMemberIndex].value();
        }
        auto it = mMemberMap.find(name);
        if (it != mMemberMap.end()) {
            mMemberIndex = it->second;
            return mMembers[mMemberIndex].value();
        }
        return JsonValue(simdjson::error_code::NO_SUCH_FIELD);
    }

private:
    std::unordered_map<NEKO_STRING_VIEW, int> mMemberMap;
    std::vector<MemberIterator> mMembers;
    std::vector<ValueIterator> mValues;
    int mMemberIndex = 0;
    int mValueIndex  = 0;
    std::size_t mSize;
    enum Type { Value, Member, Null } mType;
};

template <typename Ch = char>
class VectorStreamBuf : public std::streambuf {
public:
    VectorStreamBuf(std::vector<Ch>& vec) : mVec(vec) {}

protected:
    int_type overflow(int_type ch = traits_type::eof()) override {
        if (ch != traits_type::eof()) {
            mVec.push_back(static_cast<Ch>(ch));
        }
        return ch;
    }

private:
    std::vector<Ch>& mVec;
};

template <typename T>
struct json_output_buffer_type<std::vector<T>, void> {
    using output_stream_buffer_type = VectorStreamBuf<T>;
    using output_buffer_stream      = std::basic_ostream<T>;
    using char_type                 = T;

    static output_stream_buffer_type makeOutputBuffer(std::vector<T>& vec) { return VectorStreamBuf<T>(vec); }
    static output_buffer_stream makeOutputBufferStream(output_stream_buffer_type& buf) {
        return std::basic_ostream<T>(&buf);
    }
};

template <typename T>
struct json_output_buffer_type<T, typename std::enable_if<std::is_base_of<std::ostream, T>::value>::type> {
    using output_stream_buffer_type = typename std::remove_reference<T>::type&;
    using output_buffer_stream      = typename std::remove_reference<T>::type;
    using char_type                 = typename T::char_type;

    static output_buffer_stream& makeOutputBuffer(output_stream_buffer_type stream) { return stream; }
    static output_buffer_stream makeOutputBufferStream(output_stream_buffer_type stream) { return std::move(stream); }
};

class SimdJsonValue {
public:
    SimdJsonValue() = default;
    explicit SimdJsonValue(const RawJsonValue& value) { mValue = std::make_shared<RawJsonValue>(value); }
    explicit SimdJsonValue(const JsonValue& value) {
        if (value.error() == simdjson::error_code::SUCCESS) {
            mValue = std::make_shared<RawJsonValue>(value.value_unsafe());
        }
    }
    auto hasValue() const -> bool { return mValue != nullptr; }
    operator bool() const { return hasValue(); }
    auto nativeValue() const -> const RawJsonValue& { return *mValue; }
    auto nativeValue() -> RawJsonValue& { return *mValue; }
    auto isObject() const -> bool { return mValue && mValue->is_object(); }
    auto isArray() const -> bool { return mValue && mValue->is_array(); }
    auto isString() const -> bool { return mValue && mValue->is_string(); }
    auto isNumber() const -> bool { return mValue && mValue->is_number(); }
    auto isBool() const -> bool { return mValue && mValue->is_bool(); }
    auto isNull() const -> bool { return mValue && mValue->is_null(); }

    template <typename T>
    auto value(T& value) const -> bool {
        if (mValue) {
            return mValue->get<T>(value) == simdjson::error_code::SUCCESS;
        }
        return false;
    }

    auto size() const -> std::size_t {
        if (isArray()) {
            return mValue->get_array().size();
        }
        if (isObject()) {
            return mValue->get_object().size();
        }
        return 0;
    }

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    auto operator[](const T& name) const -> SimdJsonValue {
        if (isObject()) {
            auto value = mValue->get_object().at_key(std::string_view(name));
            if (value.error() == simdjson::error_code::SUCCESS) {
                return SimdJsonValue(value.value());
            }
        }
        return {};
    }

    auto operator[](std::size_t index) const -> SimdJsonValue {
        if (isArray() && index < size()) {
            auto array = mValue->get_array();
            if (array.error() == simdjson::error_code::SUCCESS) {
                return SimdJsonValue(array.value().at(index));
            }
            return {};
        }
        if (isObject() && index < size()) {
            auto obj = mValue->get_object();
            if (obj.error() == simdjson::error_code::SUCCESS) {
                auto item = obj.begin();
                while (index > 0) {
                    index--;
                    item++;
                }
                return SimdJsonValue(item.value());
            }
        }
        return {};
    }

private:
    std::shared_ptr<RawJsonValue> mValue;
};
} // namespace simd
} // namespace detail

template <typename BufferT>
class SimdJsonOutputSerializer : public detail::OutputSerializer<SimdJsonOutputSerializer<BufferT>> {
private:
    enum State { Null, ObjectStart, InObject, ArrayStart, InArray, AfterKey };
    using OutputStreamBuf = typename detail::simd::json_output_buffer_type<BufferT>::output_stream_buffer_type;
    using OutputStream    = typename detail::simd::json_output_buffer_type<BufferT>::output_buffer_stream;

public:
    explicit SimdJsonOutputSerializer(BufferT& buffer) NEKO_NOEXCEPT
        : detail::OutputSerializer<SimdJsonOutputSerializer>(this),
          mBuffer(detail::simd::json_output_buffer_type<BufferT>::makeOutputBuffer(buffer)),
          mStream(detail::simd::json_output_buffer_type<BufferT>::makeOutputBufferStream(mBuffer)) {
        mStateStack.emplace_back(State::Null);
    }

    SimdJsonOutputSerializer(const SimdJsonOutputSerializer& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<SimdJsonOutputSerializer>(this),
          mBuffer(other.mBuffer),
          mStream(&mBuffer) {
        mStateStack.emplace_back(State::Null);
    }
    SimdJsonOutputSerializer(SimdJsonOutputSerializer&& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<SimdJsonOutputSerializer>(this),
          mBuffer(other.mBuffer),
          mStream(&mBuffer) {
        mStateStack.emplace_back(State::Null);
    }
    ~SimdJsonOutputSerializer() { end(); }
    template <typename T>
    bool saveValue(SizeTag<T> const& /*unused*/) {
        return true;
    }

    template <typename T>
        requires std::is_signed_v<T> && (sizeof(T) <= sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool saveValue(const T value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save signed in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    template <typename T>
        requires std::is_unsigned_v<T> && (sizeof(T) <= sizeof(uint64_t)) && (!std::is_enum_v<T>)
    bool saveValue(const T value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save unsigned in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool saveValue(const float value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << std::setprecision(6) << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save float in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool saveValue(const double value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << std::setprecision(15) << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save double in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool saveValue(const bool value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << (value ? "true" : "false");
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save bool in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    template <typename CharT, typename Traits, typename Alloc>
    bool saveValue(const std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "\"" << value << "\"";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save string in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool saveValue(const char* value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "\"" << value << "\"";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save str in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "null";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save nullptr_t in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }

    bool saveValue(const detail::simd::JsonObject& object) NEKO_NOEXCEPT {
        auto ret = startObject(object.size());
        for (const auto& [key, value] : object) {
            ret = ret && _writeKey(key);
            ret = ret && saveValue(value);
        }
        ret = ret && endObject();
        return ret;
    }

    bool saveValue(const detail::simd::JsonArray& array) NEKO_NOEXCEPT {
        auto ret = startArray(array.size());
        for (const auto& value : array) {
            ret = ret && saveValue(value);
        }
        ret = ret && endArray();
        return ret;
    }

    bool saveValue(const detail::simd::RawJsonValue& value) NEKO_NOEXCEPT {
        if (value.is_bool()) {
            return saveValue(value.get_bool());
        }
        if (value.is_int64()) {
            return saveValue(value.get_int64());
        }
        if (value.is_uint64()) {
            return saveValue(value.get_uint64());
        }
        if (value.is_double()) {
            return saveValue(value.get_double());
        }
        if (value.is_string()) {
            return saveValue(value.get_string());
        }
        if (value.is_null()) {
            return saveValue(std::nullptr_t{});
        }
        if (value.is_array()) {
            return saveValue(value.get_array());
        }
        if (value.is_object()) {
            return saveValue(value.get_object());
        }
        return false;
    }

    bool saveValue(const detail::simd::SimdJsonValue& value) NEKO_NOEXCEPT {
        if (!value.hasValue()) {
            if (_updateSeparatorAndState()) {
                mStream << "null";
                return true;
            }
            NEKO_LOG_WARN("JsonSerializer", "save SimdJsonValue in Invalid state {}",
                          Reflect<State>::name(mStateStack.back()));
            return false;
        }

        return saveValue(value.nativeValue());
    }

    template <typename CharT, typename Traits>
    bool saveValue(const std::basic_string_view<CharT, Traits> value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "\"" << value << "\"";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save string in Invalid state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }

    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        if (mStateStack.back() == State::AfterKey) {
            NEKO_LOG_WARN("JsonSerializer", "save key {} in warning state {}", std::string{value.name, value.nameLen},
                          Reflect<State>::name(mStateStack.back()));
            return false;
        }
        if constexpr (traits::optional_like_type<T>::value) {
            if (traits::optional_like_type<T>::has_value(value.value)) {
                _writeKey({value.name, value.nameLen});
                return (*this)(traits::optional_like_type<T>::get_value(value.value));
            }
            return true;
        } else {
            _writeKey({value.name, value.nameLen});
            return (*this)(value.value);
        }
    }

    bool startArray(const std::size_t /*unused*/) NEKO_NOEXCEPT {
        if (mStateStack.back() == State::AfterKey) {
            mStateStack.push_back(State::ArrayStart);
            mStream << ":";
            mStream << "[";
            return true;
        }
        if (mStateStack.back() == State::InArray) {
            mStateStack.back() = State::InArray;
            mStateStack.push_back(State::ArrayStart);
            mStream << ",";
            mStream << "[";
            return true;
        }
        if (mStateStack.back() == State::ArrayStart) {
            mStateStack.back() = State::InArray;
            mStateStack.push_back(State::ArrayStart);
            mStream << "[";
            return true;
        }
        if (mStateStack.back() == State::Null) {
            mStateStack.push_back(State::ArrayStart);
            mStream << "[";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "startArray called in wrong state {}",
                      Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool endArray() NEKO_NOEXCEPT {
        if (mStateStack.back() == State::InArray || mStateStack.back() == State::ArrayStart) {
            mStateStack.pop_back();
            mStream << "]";
            if (mStateStack.back() == State::AfterKey) {
                mStateStack.pop_back();
            }
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "endArray called in wrong state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool startObject(const std::size_t /*unused*/) NEKO_NOEXCEPT {
        if (mStateStack.back() == State::AfterKey) {
            mStateStack.push_back(State::ObjectStart);
            mStream << ":";
            mStream << "{";
            return true;
        }
        if (mStateStack.back() == State::Null) {
            mStateStack.push_back(State::ObjectStart);
            mStream << "{";
            return true;
        }
        if (mStateStack.back() == State::InArray) {
            mStateStack.push_back(State::ObjectStart);
            mStream << ",{";
            return true;
        }
        if (mStateStack.back() == State::ArrayStart) {
            mStateStack.back() = State::InArray;
            mStateStack.push_back(State::ObjectStart);
            mStream << "{";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "startObject called in wrong state {}",
                      Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool endObject() NEKO_NOEXCEPT {
        if (mStateStack.back() == State::InObject || mStateStack.back() == State::ObjectStart) {
            mStateStack.pop_back();
            mStream << "}";
            if (mStateStack.back() == State::AfterKey) {
                mStateStack.pop_back();
            }
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "endObject called in wrong state {}", Reflect<State>::name(mStateStack.back()));
        return false;
    }
    bool end() NEKO_NOEXCEPT {
        if (mStateStack.empty()) {
            return true;
        }
        if (mStateStack.back() == State::InObject || mStateStack.back() == State::ObjectStart) {
            mStateStack.pop_back();
            mStream << "}";
        } else if (mStateStack.back() == State::InArray || mStateStack.back() == State::ArrayStart) {
            mStream << "]";
        }
        if (mStateStack.size() > 0 && mStateStack.back() == State::Null) {
            mStateStack.pop_back();
        }
        if (mStateStack.size() != 0) {
            NEKO_LOG_WARN("JsonSerializer", "end called in wrong state {}", int(mStateStack.back()));
            return false;
        }
        mStream.flush();
        return true;
    }

private:
    bool _updateSeparatorAndState() {
        if (mStateStack.back() == State::AfterKey) {
            mStateStack.pop_back();
            mStream << ":";
            return true;
        }
        if (mStateStack.back() == State::ArrayStart) {
            mStateStack.back() = State::InArray;
            return true;
        }
        if (mStateStack.back() == State::InArray) {
            mStream << ",";
            return true;
        }
        return false;
    }
    bool _writeKey(const NEKO_STRING_VIEW& key) {
        if (mStateStack.back() == State::ArrayStart) {
            mStateStack.back() = State::InArray;
        }
        if (mStateStack.back() == State::InArray) {
            mStream << ",";
        }
        if (mStateStack.back() == State::InObject) {
            mStream << ",";
        }
        if (mStateStack.back() == State::ObjectStart) {
            mStateStack.back() = State::InObject;
        }
        mStream << '"' << key << '"';
        mStateStack.push_back(State::AfterKey);
        return true;
    }

private:
    SimdJsonOutputSerializer& operator=(const SimdJsonOutputSerializer&) = delete;
    SimdJsonOutputSerializer& operator=(SimdJsonOutputSerializer&&)      = delete;
    std::vector<State> mStateStack;
    OutputStreamBuf mBuffer;
    OutputStream mStream;
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
 * you construct JsonInputSerializer is(json_data)
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
class SimdJsonInputSerializer : public detail::InputSerializer<SimdJsonInputSerializer> {

public:
    explicit SimdJsonInputSerializer(const detail::simd::SimdJsonValue& value) NEKO_NOEXCEPT
        : detail::InputSerializer<SimdJsonInputSerializer>(this),
          mItemStack() {
        mLastResult = value.hasValue();
        if (!mLastResult) {
            NEKO_LOG_INFO("JsonSerializer", "simdjson parser error: empty buffer");
            return;
        }
        mRoot = value.nativeValue();
    }

    explicit SimdJsonInputSerializer(const char* buf, std::size_t size) NEKO_NOEXCEPT
        : detail::InputSerializer<SimdJsonInputSerializer>(this),
          mItemStack() {
        while (size > 0 && buf[size - 1] == '\0') {
            size--;
        }
        if (size == 0) {
            NEKO_LOG_INFO("JsonSerializer", "simdjson parser error: empty buffer");
            mLastResult = true;
            return;
        }
        auto error = mParser.parse(buf, size).get(mRoot);
        if (error != 0U) {
            NEKO_LOG_INFO("JsonSerializer", "simdjson parser error: {}", simdjson::error_message(error));
            mLastResult = false;
            return;
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
        requires std::is_signed_v<T> && (sizeof(T) <= sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        int64_t ret;
        mLastResult = (*mCurrentItem).value().get_int64().get(ret) == simdjson::error_code::SUCCESS;
        if (mLastResult) {
            value = static_cast<T>(ret);
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    template <typename T>
        requires std::is_unsigned_v<T> && (sizeof(T) <= sizeof(uint64_t)) && (!std::is_enum_v<T>)
    bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        uint64_t ret;
        mLastResult = (*mCurrentItem).value().get_uint64().get(ret) == simdjson::error_code::SUCCESS;
        if (mLastResult) {
            value = static_cast<T>(ret);
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    template <typename CharT, typename Traits, typename Alloc>
    bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().is_string();
        if (!mLastResult) {
            return false;
        }
        const auto& cvalue = (*mCurrentItem).value();
        value              = std::basic_string<CharT, Traits, Alloc>(cvalue.get_string().take_value());
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(float& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        double ret;
        mLastResult = (*mCurrentItem).value().get_double().get(ret) == simdjson::error_code::SUCCESS;
        if (mLastResult) {
            value = static_cast<float>(ret);
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    bool loadValue(double& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().get_double().get(value) == simdjson::error_code::SUCCESS;
        if (mLastResult) {
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    bool loadValue(bool& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().get_bool().get(value) == simdjson::error_code::SUCCESS;
        if (mLastResult) {
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = (*mCurrentItem).value().is_null();
        if (!mLastResult) {
            return false;
        }
        ++(*mCurrentItem);
        return true;
    }

    bool loadValue(detail::simd::SimdJsonValue& value) NEKO_NOEXCEPT {
        if (mCurrentItem == nullptr) {
            return false;
        }
        mLastResult = true;
        value       = detail::simd::SimdJsonValue((*mCurrentItem).value().value());
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
            if (cvalue.error() != simdjson::error_code::SUCCESS || cvalue.is_null()) {
                traits::optional_like_type<T>::set_null(value.value);
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_INFO("JsonSerializer", "optional field {} is not find.",
                              std::string(value.name, value.nameLen));
#endif
            } else {
#if defined(NEKO_JSON_MAKE_STR_NONE_TO_NULL)
                // Why would anyone write "None" in json?
                // I've seen it, and it's a disaster.
                if (cvalue->is_string() && cvalue->get_string() == "None") {
                    traits::optional_like_type<T>::set_null(value.value);
#if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_WARN("JsonSerializer", "optional field {} is \"None\".",
                                  std::string(value.name, value.nameLen));
#endif
                    return true;
                }
#endif
                typename traits::optional_like_type<T>::type result;
                mLastResult = operator()(result);
                traits::optional_like_type<T>::set_value(value.value, std::move(result));
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
            if (cvalue.error() != simdjson::error_code::SUCCESS) {
                if constexpr (is_skipable<T>::value) {
                    mLastResult = true;
                    return true;
                } else {
#if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_ERROR("JsonSerializer", "field {} is not find. error {}",
                                   std::string(value.name, value.nameLen), simdjson::error_message(cvalue.error()));
#endif
                    mLastResult = false;
                    return false;
                }
            }
            mLastResult = operator()(value.value);
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
        if (mItemStack.empty()) {
            if (mRoot.is_array()) {
                mItemStack.emplace_back(mRoot.get_array());
            } else if (mRoot.is_object()) {
                mItemStack.emplace_back(mRoot.get_object());
            } else {
                return false;
            }
        } else if (mCurrentItem != nullptr) {
            if ((*mCurrentItem).value().is_array()) {
                mItemStack.emplace_back((*mCurrentItem).value().get_array());
            } else if ((*mCurrentItem).value().is_object()) {
                mItemStack.emplace_back((*mCurrentItem).value().get_object());
            } else {
                return false;
            }
        } else {
            return false;
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
            return true;
        }
        if (mItemStack.size() == 1) {
            mItemStack.pop_back();
            mCurrentItem = nullptr;
            return true;
        }
        return false;
    }

    void rollbackItem() {
        if (mCurrentItem != nullptr) {
            --(*mCurrentItem);
        }
    }

    bool isArray() {
        if (mItemStack.empty()) {
            return mRoot.is_array();
        }
        if (mCurrentItem == nullptr) {
            return false;
        }
        return (*mCurrentItem).value().is_array();
    }

    bool isObject() {
        if (mItemStack.empty()) {
            return mRoot.is_object();
        }
        if (mCurrentItem == nullptr) {
            return false;
        }
        return (*mCurrentItem).value().is_object();
    }

private:
    SimdJsonInputSerializer& operator=(const SimdJsonInputSerializer&) = delete;
    SimdJsonInputSerializer& operator=(SimdJsonInputSerializer&&)      = delete;

private:
    std::vector<detail::simd::ConstJsonIterator> mItemStack;
    detail::simd::ConstJsonIterator* mCurrentItem;
    detail::simd::RawJsonValue mRoot;
    detail::simd::JsonParser mParser;
    bool mLastResult = true;
};

template <>
struct is_minimal_serializable<detail::simd::SimdJsonValue> : std::true_type {};

template <typename BufferT>
inline bool save(SimdJsonOutputSerializer<BufferT>& sa, const detail::simd::SimdJsonValue& value) NEKO_NOEXCEPT {
    return sa.saveValue(value);
}

inline bool load(SimdJsonInputSerializer& sa, detail::simd::SimdJsonValue& value) NEKO_NOEXCEPT {
    return sa.loadValue(value);
}

// #####################################################
// default JsonSerializer type definition
struct SimdJsonSerializer {
    using OutputSerializer = SimdJsonOutputSerializer<std::vector<char>>;
    using InputSerializer  = SimdJsonInputSerializer;
    using JsonValue        = detail::simd::SimdJsonValue;
};

NEKO_END_NAMESPACE

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif