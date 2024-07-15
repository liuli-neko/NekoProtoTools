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
#include <simdjson.h>
#include <vector>

#include "private/global.hpp"
#include "private/helpers.hpp"
#include "private/log.hpp"

#ifdef GetObject
#undef GetObject
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
struct json_output_wrapper_type {
    using output_stream_type = void;
};
template <typename T, class enable = void>
struct json_output_buffer_type {
    using output_buffer_type = void;
};
template <>
struct json_output_buffer_type<OutBufferWrapper, void> {
    using output_buffer_type = std::vector<char>;
    using char_type          = char;
};

class ConstJsonIterator {
public:
    using MemberIterator = simdjson::dom::object::iterator;
    using ValueIterator  = simdjson::dom::array::iterator;

public:
    inline ConstJsonIterator() NEKO_NOEXCEPT : mType(Null_){};
    inline ConstJsonIterator(const JsonObject& object) NEKO_NOEXCEPT : mSize(object.size()), mType(Member) {
        if (mSize == 0) {
            mType = Null_;
            return;
        }
        for (auto iter = object.begin(); iter != object.end(); ++iter) {
            mMembers.emplace(iter.key(), iter);
        }
        mMemberIt = mMembers.begin();
    }

    inline ConstJsonIterator(const JsonArray& array) NEKO_NOEXCEPT : mValueItBegin(array.begin()),
                                                                     mValueItEnd(array.end()),
                                                                     mValueIt(array.begin()),
                                                                     mSize(array.size()),
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
            ++mValueIt;
        }
        return *this;
    }
    inline bool eof() const NEKO_NOEXCEPT {
        if (mType == Null_ || (mType == Value && mValueIt == mValueItEnd) ||
            (mType == Member && mMemberIt == mMembers.end())) {
            return true;
        }
        return false;
    }
    inline std::size_t size() const NEKO_NOEXCEPT { return mSize; }
    inline JsonValue value() NEKO_NOEXCEPT {
        NEKO_ASSERT(!eof(), "JsonInputSerializer get next value called on end of this json object");
        switch (mType) {
        case Value:
            return *mValueIt;
        case Member:
            return (mMemberIt->second).value();
        default:
            return JsonValue(
                simdjson::error_code::NO_SUCH_FIELD); // should never reach here, but needed to avoid compiler warning
        }
    }
    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if (mType == Member && mMemberIt != mMembers.end()) {
            return mMemberIt->first;
        } else {
            return {};
        }
    };
    inline JsonValue move_to_member(const NEKO_STRING_VIEW& name) NEKO_NOEXCEPT {
        if (mType != Member) {
            return JsonValue(simdjson::error_code::NO_SUCH_FIELD);
        }
        auto it = mMembers.find(name);
        if (it != mMembers.end()) {
            mMemberIt = it;
            return (mMemberIt->second).value();
        }
        return JsonValue(simdjson::error_code::NO_SUCH_FIELD);
    }

private:
    std::unordered_map<NEKO_STRING_VIEW, MemberIterator>::iterator mMemberIt;
    std::unordered_map<NEKO_STRING_VIEW, MemberIterator> mMembers;
    ValueIterator mValueItBegin, mValueItEnd, mValueIt;
    std::size_t mSize;
    enum Type { Value, Member, Null_ } mType;
};
} // namespace simd
} // namespace detail

class SimdJsonOutputSerializer : public detail::OutputSerializer<SimdJsonOutputSerializer> {
public:
public:
    template <typename T>
    explicit inline SimdJsonOutputSerializer(T& buffer) NEKO_NOEXCEPT
        : detail::OutputSerializer<SimdJsonOutputSerializer>(this) {}

    inline SimdJsonOutputSerializer(const SimdJsonOutputSerializer& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<SimdJsonOutputSerializer>(this) {}
    inline SimdJsonOutputSerializer(SimdJsonOutputSerializer&& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<SimdJsonOutputSerializer>(this) {}
    inline ~SimdJsonOutputSerializer() { end(); }
    template <typename T>
    inline bool saveValue(SizeTag<T> const&) {
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool saveValue(const T value) NEKO_NOEXCEPT {
        return false;
    }
    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) < sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool saveValue(const T value) NEKO_NOEXCEPT {
        return false;
    }
    inline bool saveValue(const int64_t value) NEKO_NOEXCEPT { return false; }
    inline bool saveValue(const uint64_t value) NEKO_NOEXCEPT { return false; }
    inline bool saveValue(const float value) NEKO_NOEXCEPT { return false; }
    inline bool saveValue(const double value) NEKO_NOEXCEPT { return false; }
    inline bool saveValue(const bool value) NEKO_NOEXCEPT { return false; }
    template <typename CharT, typename Traits, typename Alloc>
    inline bool saveValue(const std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        return false;
    }
    inline bool saveValue(const char* value) NEKO_NOEXCEPT { return false; }
    inline bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT { return false; }
    template <typename CharT, typename Traits>
    inline bool saveValue(const std::basic_string_view<CharT, Traits> value) NEKO_NOEXCEPT {
        return false;
    }

    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        return false;
    }

    inline bool startArray(const std::size_t) NEKO_NOEXCEPT { return false; }
    inline bool endArray() NEKO_NOEXCEPT { return false; }
    inline bool startObject(const std::size_t) NEKO_NOEXCEPT { return false; }
    inline bool endObject() NEKO_NOEXCEPT { return false; }
    inline bool end() NEKO_NOEXCEPT { return false; }

private:
    SimdJsonOutputSerializer& operator=(const SimdJsonOutputSerializer&) = delete;
    SimdJsonOutputSerializer& operator=(SimdJsonOutputSerializer&&)      = delete;
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
 * is(makeSizeTag(size));
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
        if (error) {
            NEKO_LOG_INFO("simdjson parser error: {}", simdjson::error_message(error));
            mParserError = true;
            return;
        }
    }
    inline operator bool() const NEKO_NOEXCEPT { return !mParserError; }

    inline NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        if ((*mCurrentItem).eof())
            return {};
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
        const auto& v = (*mCurrentItem).value();
        value         = std::basic_string<CharT, Traits, Alloc>(v.get_string().take_value());
        ++(*mCurrentItem);
        return true;
    }

    inline bool loadValue(float& value) NEKO_NOEXCEPT {
        double ret;
        if (!(*mCurrentItem).value().get_double().get(ret)) {
            value = ret;
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    inline bool loadValue(double& value) NEKO_NOEXCEPT {
        if (!(*mCurrentItem).value().get_double().get(value)) {
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    inline bool loadValue(bool& value) NEKO_NOEXCEPT {
        if (!(*mCurrentItem).value().get_bool().get(value)) {
            ++(*mCurrentItem);
            return true;
        }
        return false;
    }

    inline bool loadValue(std::nullptr_t&) NEKO_NOEXCEPT {
        if (!(*mCurrentItem).value().is_null())
            return false;
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
        const auto& v = (*mCurrentItem).move_to_member({value.name, value.nameLen});
        bool ret      = true;
        if constexpr (traits::is_optional<T>::value) {
            if (v.error() != simdjson::error_code::SUCCESS || v.is_null()) {
                value.value.reset();
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_INFO("optional field {} is not find.", std::string(value.name, value.nameLen));
#endif
            } else {
#if defined(NEKO_JSON_MAKE_STR_NONE_TO_NULL)
                // Why would anyone write "None" in json?
                // I've seen it, and it's a disaster.
                if (v->is_string() && v->get_string() == "None") {
                    value.value.reset();
#if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_WARN("optional field {} is \"None\".", std::string(value.name, value.nameLen));
#endif
                    return true;
                }
#endif
                typename traits::is_optional<T>::value_type result;
                ret = operator()(result);
                value.value.emplace(std::move(result));
#if defined(NEKO_VERBOSE_LOGS)
                if (ret) {
                    NEKO_LOG_INFO("optional field {} get value success.", std::string(value.name, value.nameLen));
                } else {
                    NEKO_LOG_ERROR("optional {} field {} get value fail.", NEKO_PRETTY_FUNCTION_NAME,
                                   std::string(value.name, value.nameLen));
                }
#endif
            }
        } else {
            if (v.error() != simdjson::error_code::SUCCESS) {
#if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_ERROR("{} field {} is not find. error {}", NEKO_PRETTY_FUNCTION_NAME,
                               std::string(value.name, value.nameLen), simdjson::error_message(v.error()));
#endif
                return false;
            }
            ret = operator()(value.value);
#if defined(NEKO_VERBOSE_LOGS)
            if (ret) {
                NEKO_LOG_INFO("field {} get value success.", std::string(value.name, value.nameLen));
            } else {
                NEKO_LOG_ERROR("{} field {} get value fail.", NEKO_PRETTY_FUNCTION_NAME,
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
        } else if (mItemStack.size() == 1) {
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
inline bool prologue(SimdJsonInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(SimdJsonInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T>
inline bool prologue(SimdJsonOutputSerializer& sa, const NameValuePair<T>&) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(SimdJsonOutputSerializer& sa, const NameValuePair<T>&) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
//  size tag
template <typename T>
inline bool prologue(SimdJsonInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(SimdJsonInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T>
inline bool prologue(SimdJsonOutputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(SimdJsonOutputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
template <class T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                       traits::default_value_for_enable>
inline bool prologue(SimdJsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <typename T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(SimdJsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <class T, traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                                       !is_minimal_serializable<T>::valueT,
                                       !traits::has_method_const_serialize<T, SimdJsonOutputSerializer>::value> =
                       traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::valueT,
                                          !traits::has_method_const_serialize<T, SimdJsonOutputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, traits::enable_if_t<traits::has_method_const_serialize<T, SimdJsonOutputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return sa.startObject(-1);
}
template <typename T, traits::enable_if_t<traits::has_method_const_serialize<T, SimdJsonOutputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return sa.endObject();
}

// #########################################################
// # arithmetic types
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// # std::string
template <typename CharT, typename Traits, typename Alloc>
inline bool prologue(SimdJsonInputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc>
inline bool epilogue(SimdJsonInputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}

template <typename CharT, typename Traits, typename Alloc>
inline bool prologue(SimdJsonOutputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc>
inline bool epilogue(SimdJsonOutputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>&) NEKO_NOEXCEPT {
    return true;
}

#if NEKO_CPP_PLUS >= 17
template <typename CharT, typename Traits>
inline bool prologue(SimdJsonOutputSerializer& sa, const std::basic_string_view<CharT, Traits>&) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits>
inline bool epilogue(SimdJsonOutputSerializer& sa, const std::basic_string_view<CharT, Traits>&) NEKO_NOEXCEPT {
    return true;
}
#endif

// #####################################################
// # std::nullptr_t
inline bool prologue(SimdJsonInputSerializer& sa, const std::nullptr_t&) NEKO_NOEXCEPT { return true; }
inline bool epilogue(SimdJsonInputSerializer& sa, const std::nullptr_t&) NEKO_NOEXCEPT { return true; }

inline bool prologue(SimdJsonOutputSerializer& sa, const std::nullptr_t&) NEKO_NOEXCEPT { return true; }
inline bool epilogue(SimdJsonOutputSerializer& sa, const std::nullptr_t&) NEKO_NOEXCEPT { return true; }
// #####################################################
// # minimal serializable
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonInputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(SimdJsonOutputSerializer& sa, const T&) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// default JsonSerializer type definition
struct SimdJsonSerializer {
    using OutputSerializer = SimdJsonOutputSerializer;
    using InputSerializer  = SimdJsonInputSerializer;
};

NEKO_END_NAMESPACE