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

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)
#include <iomanip>
#include <simdjson.h>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "private/helpers.hpp"
#include "private/log.hpp"
#include "types/enum.hpp"

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
    inline ConstJsonIterator() NEKO_NOEXCEPT : mType(Null) {};
    inline ConstJsonIterator(const JsonObject& object) NEKO_NOEXCEPT : mSize(object.size()),
                                                                       mType(Member),
                                                                       mMemberIndex(0) {
        if (mSize == 0) {
            mType = Null;
            return;
        }
        for (auto iter = object.begin(); iter != object.end(); ++iter) {
            mMembers.push_back(iter);
            mMemberMap.emplace(iter.key(), mMembers.size() - 1);
        }
    }

    inline ConstJsonIterator(const JsonArray& array) NEKO_NOEXCEPT : mValueItBegin(array.begin()),
                                                                     mValueItEnd(array.end()),
                                                                     mValueIt(array.begin()),
                                                                     mSize(array.size()),
                                                                     mType(Value) {
        if (mSize == 0) {
            mType = Null;
        }
    }

    inline ~ConstJsonIterator() = default;

    inline ConstJsonIterator& operator++() NEKO_NOEXCEPT {
        if (mType == Member) {
            ++mMemberIndex;
        } else {
            ++mValueIt;
        }
        return *this;
    }
    inline bool eof() const NEKO_NOEXCEPT {
        return mType == Null || (mType == Value && mValueIt == mValueItEnd) ||
               (mType == Member && mMemberIndex >= mMembers.size());
    }
    inline std::size_t size() const NEKO_NOEXCEPT { return mSize; }
    inline JsonValue value() NEKO_NOEXCEPT {
        NEKO_ASSERT(!eof(), "JsonSerializer", "JsonInputSerializer get next value called on end of this json object");
        switch (mType) {
        case Value:
            return *mValueIt;
        case Member:
            return mMembers[mMemberIndex].value();
        default:
            return JsonValue(
                simdjson::error_code::NO_SUCH_FIELD); // should never reach here, but needed to avoid compiler warning
        }
    }
    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if (mType == Member && mMemberIndex < mMembers.size()) {
            return mMembers[mMemberIndex].key();
        }
        return {};
    };
    inline JsonValue moveToMember(const NEKO_STRING_VIEW& name) NEKO_NOEXCEPT {
        if (mType != Member) {
            return JsonValue(simdjson::error_code::NO_SUCH_FIELD);
        }
        if (mMemberIndex < mMembers.size() && mMembers[mMemberIndex].key() == name) {
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
    int mMemberIndex = 0;
    ValueIterator mValueItBegin, mValueItEnd, mValueIt;
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

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) <= sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool saveValue(const T value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save signed in Invalid state {}", detail::enum_to_string(mStateStack.back()));
        return false;
    }
    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) <= sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool saveValue(const T value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save unsigned in Invalid state {}",
                      detail::enum_to_string(mStateStack.back()));
        return false;
    }
    bool saveValue(const float value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << std::setprecision(6) << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save float in Invalid state {}", detail::enum_to_string(mStateStack.back()));
        return false;
    }
    bool saveValue(const double value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << std::setprecision(15) << value;
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save double in Invalid state {}", detail::enum_to_string(mStateStack.back()));
        return false;
    }
    bool saveValue(const bool value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << (value ? "true" : "false");
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save bool in Invalid state {}", detail::enum_to_string(mStateStack.back()));
        return false;
    }
    template <typename CharT, typename Traits, typename Alloc>
    bool saveValue(const std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "\"" << value << "\"";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save string in Invalid state {}", detail::enum_to_string(mStateStack.back()));
        return false;
    }
    bool saveValue(const char* value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "\"" << value << "\"";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save str in Invalid state {}", detail::enum_to_string(mStateStack.back()));
        return false;
    }
    bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "null";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save nullptr_t in Invalid state {}",
                      detail::enum_to_string(mStateStack.back()));
        return false;
    }
#if NEKO_CPP_PLUS >= 17
    template <typename CharT, typename Traits>
    bool saveValue(const std::basic_string_view<CharT, Traits> value) NEKO_NOEXCEPT {
        if (_updateSeparatorAndState()) {
            mStream << "\"" << value << "\"";
            return true;
        }
        NEKO_LOG_WARN("JsonSerializer", "save string in Invalid state {}", detail::enum_to_string(mStateStack.back()));
        return false;
    }

    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        if (mStateStack.back() == State::AfterKey) {
            NEKO_LOG_WARN("JsonSerializer", "save key {} in warning state {}", std::string{value.name, value.nameLen},
                          detail::enum_to_string(mStateStack.back()));
            return false;
        }
        if constexpr (traits::is_optional<T>::value) {
            if (value.value.has_value()) {
                _writeKey({value.name, value.nameLen});
                return (*this)(value.value.value());
            }
        } else {
            _writeKey({value.name, value.nameLen});
            return (*this)(value.value);
        }
        return true;
    }
#else
    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        if (mStateStack.back() == State::AfterKey) {
            NEKO_LOG_WARN("JsonSerializer", "save key {} in warning state {}", std::string{value.name, value.nameLen},
                          detail::enum_to_string(mStateStack.back()));
            return false;
        }
        _writeKey({value.name, value.nameLen});
        return (*this)(value.value);
    }
#endif
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
        NEKO_LOG_WARN("JsonSerializer", "startArray called in wrong state {}",
                      detail::enum_to_string(mStateStack.back()));
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
        NEKO_LOG_WARN("JsonSerializer", "endArray called in wrong state {}",
                      detail::enum_to_string(mStateStack.back()));
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
                      detail::enum_to_string(mStateStack.back()));
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
        NEKO_LOG_WARN("JsonSerializer", "endObject called in wrong state {}",
                      detail::enum_to_string(mStateStack.back()));
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
        if (mStateStack.back() == State::Null) {
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
    inline explicit SimdJsonInputSerializer(const char* buf, std::size_t size) NEKO_NOEXCEPT
        : detail::InputSerializer<SimdJsonInputSerializer>(this),
          mItemStack() {
        auto error = mParser.parse(buf, size).get(mRoot);
        if (error != 0U) {
            NEKO_LOG_INFO("JsonSerializer", "simdjson parser error: {}", simdjson::error_message(error));
            mParserError = true;
            return;
        }
    }
    inline operator bool() const NEKO_NOEXCEPT { return !mParserError; }

    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if ((*mCurrentItem).eof()) {
            return {};
        }
        return (*mCurrentItem).name();
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) <= sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
        int64_t ret;
        if (!(*mCurrentItem).value().get_int64().get(ret)) {
            value = static_cast<T>(ret);
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) <= sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
        uint64_t ret;
        if (!(*mCurrentItem).value().get_uint64().get(ret)) {
            value = static_cast<T>(ret);
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    template <typename CharT, typename Traits, typename Alloc>
    inline bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        if (!(*mCurrentItem).value().is_string()) {
            return false;
        }
        const auto& cvalue = (*mCurrentItem).value();
        value              = std::basic_string<CharT, Traits, Alloc>(cvalue.get_string().take_value());
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(float& value) NEKO_NOEXCEPT {
        double ret;
        if ((*mCurrentItem).value().get_double().get(ret) == 0U) {
            value = ret;
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    inline bool loadValue(double& value) NEKO_NOEXCEPT {
        if ((*mCurrentItem).value().get_double().get(value) == 0U) {
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    inline bool loadValue(bool& value) NEKO_NOEXCEPT {
        if ((*mCurrentItem).value().get_bool().get(value) == 0U) {
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    inline bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        if (!(*mCurrentItem).value().is_null()) {
            return false;
        }
        ++(*mCurrentItem);
        return true;
    }

    template <typename T>
    inline bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        value.size = (*mCurrentItem).size();
        return true;
    }

    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        const auto& cvalue = (*mCurrentItem).moveToMember({value.name, value.nameLen});
        bool ret           = true;
        if constexpr (traits::is_optional<T>::value) {
            if (cvalue.error() != simdjson::error_code::SUCCESS || cvalue.is_null()) {
                value.value.reset();
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_INFO("JsonSerializer", "optional field {} is not find.",
                              std::string(value.name, value.nameLen));
#endif
            } else {
#if defined(NEKO_JSON_MAKE_STR_NONE_TO_NULL)
                // Why would anyone write "None" in json?
                // I've seen it, and it's a disaster.
                if (cvalue->is_string() && cvalue->get_string() == "None") {
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
                    NEKO_LOG_ERROR("JsonSerializer", "optional {} field {} get value fail.", NEKO_PRETTY_FUNCTION_NAME,
                                   std::string(value.name, value.nameLen));
                }
#endif
            }
        } else {
            if (cvalue.error() != simdjson::error_code::SUCCESS) {
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_ERROR("JsonSerializer", "{} field {} is not find. error {}", NEKO_PRETTY_FUNCTION_NAME,
                               std::string(value.name, value.nameLen), simdjson::error_message(cvalue.error()));
#endif
                return false;
            }
            ret = operator()(value.value);
#if defined(NEKO_VERBOSE_LOGS)
            if (ret) {
                NEKO_LOG_INFO("JsonSerializer", "field {} get value success.", std::string(value.name, value.nameLen));
            } else {
                NEKO_LOG_ERROR("JsonSerializer", "{} field {} get value fail.", NEKO_PRETTY_FUNCTION_NAME,
                               std::string(value.name, value.nameLen));
            }
#endif
        }
        return ret;
    }
    inline bool startNode() NEKO_NOEXCEPT {
        if (mItemStack.empty()) {
            if (mRoot.is_array()) {
                mItemStack.emplace_back(mRoot.get_array());
            } else if (mRoot.is_object()) {
                mItemStack.emplace_back(mRoot.get_object());
            } else {
                return false;
            }
        } else {
            if ((*mCurrentItem).value().is_array()) {
                mItemStack.emplace_back((*mCurrentItem).value().get_array());
            } else if ((*mCurrentItem).value().is_object()) {
                mItemStack.emplace_back((*mCurrentItem).value().get_object());
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
            return true;
        }
        if (mItemStack.size() == 1) {
            mItemStack.pop_back();
            mCurrentItem = nullptr;
            return true;
        }
        return false;
    }

private:
    SimdJsonInputSerializer& operator=(const SimdJsonInputSerializer&) = delete;
    SimdJsonInputSerializer& operator=(SimdJsonInputSerializer&&)      = delete;

private:
    std::vector<detail::simd::ConstJsonIterator> mItemStack;
    detail::simd::ConstJsonIterator* mCurrentItem;
    detail::simd::RawJsonValue mRoot;
    detail::simd::JsonParser mParser;
    bool mParserError = false;
};

// #######################################################
// JsonSerializer prologue and epilogue
// #######################################################

// #######################################################
// name value pair
template <typename T>
inline bool prologue(SimdJsonInputSerializer& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(SimdJsonInputSerializer& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferT>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
//  size tag
template <typename T>
inline bool prologue(SimdJsonInputSerializer& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(SimdJsonInputSerializer& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferT>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
template <class T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                       traits::default_value_for_enable>
inline bool prologue(SimdJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <typename T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(SimdJsonInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <class T, typename BufferT,
          traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                              !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, SimdJsonOutputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferT,
          traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, SimdJsonOutputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferT,
          traits::enable_if_t<traits::has_method_const_serialize<T, SimdJsonOutputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startObject(-1);
}
template <typename T, typename BufferT,
          traits::enable_if_t<traits::has_method_const_serialize<T, SimdJsonOutputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.endObject();
}

// #########################################################
// # arithmetic types
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonInputSerializer& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonInputSerializer& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// # std::string
template <typename CharT, typename Traits, typename Alloc>
inline bool prologue(SimdJsonInputSerializer& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc>
inline bool epilogue(SimdJsonInputSerializer& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename CharT, typename Traits, typename Alloc, typename BufferT>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc, typename BufferT>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

#if NEKO_CPP_PLUS >= 17
template <typename CharT, typename Traits, typename BufferT>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename BufferT>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
#endif

// #####################################################
// # std::nullptr_t
inline bool prologue(SimdJsonInputSerializer& /*unused*/, const std::nullptr_t& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
inline bool epilogue(SimdJsonInputSerializer& /*unused*/, const std::nullptr_t& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename BufferT>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const std::nullptr_t& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename BufferT>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const std::nullptr_t& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
// #####################################################
// # minimal serializable
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonInputSerializer& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonInputSerializer& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename BufferT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// default JsonSerializer type definition
struct SimdJsonSerializer {
    using OutputSerializer = SimdJsonOutputSerializer<std::vector<char>>;
    using InputSerializer  = SimdJsonInputSerializer;
};

NEKO_END_NAMESPACE

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif