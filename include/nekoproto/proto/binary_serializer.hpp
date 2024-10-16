/**
 * @file binary_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <stdint.h>

#include <vector>
#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include "private/global.hpp"
#include "private/helpers.hpp"
#include "private/integer.hpp"

NEKO_BEGIN_NAMESPACE

inline int16_t htobe(const int16_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "htobe16");
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return htobe16(value);
#endif
}
inline int16_t betoh(const int16_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "be16toh");
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return be16toh(value);
#endif
}
inline uint16_t htobe(const uint16_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "htobe16");
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return htobe16(value);
#endif
}
inline uint16_t betoh(const uint16_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "be16toh");
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return be16toh(value);
#endif
}
inline int32_t htobe(const int32_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "htobe32");
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return htobe32(value);
#endif
}
inline int32_t betoh(const int32_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "be32toh");
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return be32toh(value);
#endif
}
inline uint32_t htobe(const uint32_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "htobe32");
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return htobe32(value);
#endif
}
inline uint32_t betoh(const uint32_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "be32toh");
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return be32toh(value);
#endif
}
inline int64_t htobe(const int64_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "htobe64");
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return htobe64(value);
#endif
}
inline int64_t betoh(const int64_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "be64toh");
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return be64toh(value);
#endif
}
inline uint64_t htobe(const uint64_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "htobe64");
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return htobe64(value);
#endif
}
inline uint64_t betoh(const uint64_t value) NEKO_NOEXCEPT {
    // NEKO_LOG_INFO("binaryserializer", "be64toh");
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return be64toh(value);
#endif
}
/**
 * @brief output filed for binary format
 * Although empty fields are supported, empty data can be artificially constructed,
 *  have ambiguities that are difficult to erase, or require significant additional
 * overhead, so it is not recommended to set optional fields in binary fields.
 *
 * @note: we don't make special deals with empty fields ambiguities.
 *
 */
class BinaryOutputSerializer : public detail::OutputSerializer<BinaryOutputSerializer> {
public:
    using BufferType = std::vector<char>;

public:
    explicit BinaryOutputSerializer(BufferType& out) NEKO_NOEXCEPT : OutputSerializer<BinaryOutputSerializer>(this),
                                                                     mBuffer(out) {}
    BinaryOutputSerializer(const BinaryOutputSerializer& other) NEKO_NOEXCEPT
        : OutputSerializer<BinaryOutputSerializer>(this),
          mBuffer(other.mBuffer) {}
    BinaryOutputSerializer(BinaryOutputSerializer&& other) NEKO_NOEXCEPT
        : OutputSerializer<BinaryOutputSerializer>(this),
          mBuffer(other.mBuffer) {}
    template <typename T>
    inline bool saveValue(SizeTag<T> const& size) NEKO_NOEXCEPT {
        return saveValue(size.size);
    }
    template <typename T, traits::enable_if_t<std::is_integral<T>::value, sizeof(T) <= sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool saveValue(const T value) NEKO_NOEXCEPT {
        if (value < 0) {
            mBuffer.push_back(0x80);
            return detail::IntegerEncoder::encode(-value, mBuffer, 1) == 0;
        }
        mBuffer.push_back(0x00);
        return detail::IntegerEncoder::encode(value, mBuffer, 1) == 0;
    }
    inline bool saveValue(const float value) NEKO_NOEXCEPT {
        mBuffer.resize(mBuffer.size() + sizeof(float), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(float), &value, sizeof(float));
        return true;
    }
    inline bool saveValue(const double value) NEKO_NOEXCEPT {
        mBuffer.resize(mBuffer.size() + sizeof(double), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(double), &value, sizeof(double));
        return true;
    }
    inline bool saveValue(const bool value) NEKO_NOEXCEPT {
        mBuffer.push_back(value ? 1 : 0);
        return true;
    }
    inline bool saveValue(const std::string& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save string({})", value);
        mBuffer.insert(mBuffer.end(), value.begin(), value.end());
        return true;
    }
    inline bool saveValue(const char* value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save string({})", value);
        uint32_t size = strlen(value);
        mBuffer.insert(mBuffer.end(), value, value + size);
        return true;
    }
    inline bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT { return saveValue("null"); }
#if NEKO_CPP_PLUS >= 17
    inline bool saveValue(const std::string_view value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save string_view({})", value);
        mBuffer.insert(mBuffer.end(), value.begin(), value.end());
        return true;
    }
#endif
    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save name({}) value pair", NEKO_STRING_VIEW{value.name, value.nameLen});
        return this->operator()(NEKO_STRING_VIEW{value.name, value.nameLen}) && this->operator()(value.value);
    }
    // as serializer(make_size_tag(size));
    inline bool startArray(const std::size_t size) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "startArray: {}", size);
        return saveValue(make_size_tag(size));
    }
    inline bool endArray() { // NOLINT(readability-convert-member-functions-to-static)
        NEKO_LOG_INFO("BinarySerializer", "endArray");
        return true;
    }
    inline bool startObject(const std::size_t size) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "startObject: {}", size);
        return saveValue(make_size_tag(size));
    }
    inline bool endObject() NEKO_NOEXCEPT { // NOLINT(readability-convert-member-functions-to-static)
        NEKO_LOG_INFO("BinarySerializer", "endObject");
        return true;
    }
    inline bool end() NEKO_NOEXCEPT { // NOLINT(readability-convert-member-functions-to-static)
        return true;
    }
    inline bool push(char value) NEKO_NOEXCEPT {
        mBuffer.push_back(value);
        return true;
    }
    inline std::size_t size() const { return mBuffer.size(); }

private:
    BinaryOutputSerializer& operator=(const BinaryOutputSerializer&) = delete;
    BinaryOutputSerializer& operator=(BinaryOutputSerializer&&)      = delete;

private:
    BufferType& mBuffer;
};

class BinaryInputSerializer : public detail::InputSerializer<BinaryInputSerializer> {
public:
    inline explicit BinaryInputSerializer(const char* buf, const std::size_t size) NEKO_NOEXCEPT
        : InputSerializer<BinaryInputSerializer>(this),
          mBuffer(buf),
          mSize(size),
          mOffset(0) {}
    inline operator bool() const NEKO_NOEXCEPT { return true; }
    inline std::string name() NEKO_NOEXCEPT {
        std::string ret;
        if (loadValue(ret)) {
            return ret;
        }
        return {};
    }

    inline bool loadValue(std::string& value) NEKO_NOEXCEPT {
        uint32_t size = value.size();
        if (!loadValue(make_size_tag(size))) {
            return false;
        }
        if (size > mSize - mOffset) {
            return false;
        }
        value.resize(size);
        std::copy(mBuffer + mOffset, mBuffer + mOffset + size, value.begin());
        NEKO_LOG_INFO("BinarySerializer", "Load string: {}", value);
        mOffset += size;
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_integral<T>::value, sizeof(T) <= sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mOffset >= mSize) {
            return false;
        }
        auto flags = mBuffer[mOffset] & 0x80;
        int ret = detail::IntegerDecoder::decode(reinterpret_cast<const uint8_t*>(mBuffer + mOffset), mSize - mOffset,
                                                 value, 1);
        if (ret <= 0) {
            return false;
        }
        mOffset += ret;
        if (flags) {
            value = -value;
        }
        return true;
    }

    inline bool loadValue(float& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "load null");
        if (mOffset + sizeof(float) > mSize) {
            return false;
        }
        memcpy(&value, mBuffer + mOffset, sizeof(float));
        mOffset += sizeof(float);
        return true;
    }

    inline bool loadValue(double& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(double) > mSize) {
            return false;
        }
        memcpy(&value, mBuffer + mOffset, sizeof(double));
        mOffset += sizeof(double);
        return true;
    }

    inline bool loadValue(bool& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(bool) > mSize) {
            return false;
        }
        memcpy(&value, mBuffer + mOffset, sizeof(bool));
        mOffset += sizeof(bool);
        return true;
    }

    inline bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "load null");
        std::string value;
        auto ret = loadValue(value);
        return ret && value == "null";
    }

    template <typename T>
    inline bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        if (!loadValue(value.size)) {
            return false;
        }
        NEKO_LOG_INFO("BinarySerializer", "load size tag {}", value.size);
        return true;
    }

    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "load name({}) value pair", std::string(value.name, value.nameLen));
        std::string name;
        if (!operator()(name)) {
            return false;
        }
        if (name.size() != value.nameLen || memcmp(name.data(), value.name, value.nameLen) != 0) {
            return false;
        }
        return operator()(value.value);
    }
    inline bool pop(char& value) NEKO_NOEXCEPT {
        if (mOffset + 1 > mSize) {
            return false;
        }
        value = mBuffer[mOffset];
        mOffset += 1;
        return true;
    }
    inline std::size_t offset() const { return mOffset; }

    bool startNode() { // NOLINT(readability-convert-member-functions-to-static)
        NEKO_LOG_INFO("BinarySerializer", "start node");
        return true;
    };

    bool finishNode() { // NOLINT(readability-convert-member-functions-to-static)
        NEKO_LOG_INFO("BinarySerializer", "finish node");
        return true;
    }

private:
    BinaryInputSerializer& operator=(const BinaryInputSerializer&) = delete;
    BinaryInputSerializer& operator=(BinaryInputSerializer&&)      = delete;

private:
    const char* mBuffer;
    std::size_t mSize;
    std::size_t mOffset = 0;
};

template <typename T, size_t Size = sizeof(T)>
struct FixedLengthField {
private:
    using Type = typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type;

public:
    FixedLengthField(T&& value) : value(std::forward<T>(value)) {}
    bool save(BinaryOutputSerializer& serializer) const;
    bool load(BinaryInputSerializer& serializer);
    Type value;
};

template <class T, std::size_t Size = sizeof(T)>
inline FixedLengthField<T> make_fixed_length_field(T&& value) NEKO_NOEXCEPT {
    return FixedLengthField<T, Size>{std::forward<T>(value)};
}

struct BinarySerializer {
    using OutputSerializer = BinaryOutputSerializer;
    using InputSerializer  = BinaryInputSerializer;
};

template <typename T, size_t Size>
inline bool FixedLengthField<T, Size>::save(BinaryOutputSerializer& serializer) const {
    if (Size != sizeof(T)) {
        auto size = serializer.size();
        auto ret  = serializer.saveValue(value);
        while (ret && serializer.size() - size < Size) {
            ret = serializer.push(0);
        }
        return ret;
    }
    char buffer[Size];
    if (std::is_integral<typename std::remove_reference<typename std::remove_const<T>::type>::type>::value) {
        auto tmp = htobe(value);
        memcpy(buffer, &tmp, Size);
    } else {
        memcpy(buffer, &value, Size);
    }
    for (auto& ch : buffer) {
        if (!serializer.push(ch)) {
            return false;
        }
    }
    return true;
}

template <typename T, size_t Size>
inline bool FixedLengthField<T, Size>::load(BinaryInputSerializer& serializer) {
    if (Size != sizeof(T)) {
        auto size = serializer.offset();
        auto ret  = serializer.loadValue(value);
        char tmp;
        while (ret && serializer.offset() - size < Size) {
            ret = serializer.pop(tmp);
        }
        return ret;
    }
    char buffer[Size];
    for (auto& ch : buffer) {
        if (!serializer.pop(ch)) {
            return false;
        }
    }
    memcpy(&value, buffer, Size);
    if (std::is_integral<typename std::remove_reference<typename std::remove_const<T>::type>::type>::value) {
        value = betoh(value);
    }
    return true;
}

// #######################################################
// BinarySerializer prologue and epilogue
// #######################################################

// #######################################################
// name value pair
template <typename T>
inline bool prologue(BinaryInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(BinaryInputSerializer& sa, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T>
inline bool prologue(BinaryOutputSerializer& sa, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(BinaryOutputSerializer& sa, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
//  size tag
template <typename T>
inline bool prologue(BinaryInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(BinaryInputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T>
inline bool prologue(BinaryOutputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(BinaryOutputSerializer& sa, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
//  FixedSizeTag
template <typename T>
inline bool prologue(BinaryInputSerializer& sa, const FixedLengthField<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(BinaryInputSerializer& sa, const FixedLengthField<T>& value) NEKO_NOEXCEPT {
    return true;
}

template <typename T>
inline bool prologue(BinaryOutputSerializer& sa, const FixedLengthField<T>& value) NEKO_NOEXCEPT {
    return true;
}
template <typename T>
inline bool epilogue(BinaryOutputSerializer& sa, const FixedLengthField<T>& value) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
template <class T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value,
                                       !traits::has_method_const_serialize<T, BinaryOutputSerializer>::value> =
                       traits::default_value_for_enable>
inline bool prologue(BinaryInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <typename T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value,
                                          !traits::has_method_const_serialize<T, BinaryOutputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(BinaryInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <class T, traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                                       !is_minimal_serializable<T>::valueT,
                                       !traits::has_method_const_serialize<T, BinaryOutputSerializer>::value> =
                       traits::default_value_for_enable>
inline bool prologue(BinaryOutputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startObject(1);
}

template <typename T, traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::valueT,
                                          !traits::has_method_const_serialize<T, BinaryOutputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(BinaryOutputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.endObject();
}

template <typename T, traits::enable_if_t<traits::has_method_const_serialize<T, BinaryOutputSerializer>::value ||
                                          traits::has_method_serialize<T, BinaryOutputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool prologue(BinaryOutputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<traits::has_method_const_serialize<T, BinaryOutputSerializer>::value ||
                                          traits::has_method_serialize<T, BinaryOutputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(BinaryOutputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, traits::enable_if_t<traits::has_method_const_serialize<T, BinaryInputSerializer>::value ||
                                          traits::has_method_serialize<T, BinaryInputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool prologue(BinaryInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<traits::has_method_const_serialize<T, BinaryInputSerializer>::value ||
                                          traits::has_method_serialize<T, BinaryInputSerializer>::value> =
                          traits::default_value_for_enable>
inline bool epilogue(BinaryInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// # arithmetic types
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(BinaryInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(BinaryInputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(BinaryOutputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(BinaryOutputSerializer& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// # std::string
template <typename CharT, typename Traits, typename Alloc>
inline bool prologue(BinaryInputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <typename CharT, typename Traits, typename Alloc>
inline bool epilogue(BinaryInputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <typename CharT, typename Traits, typename Alloc>
inline bool prologue(BinaryOutputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>& str) NEKO_NOEXCEPT {
    return sa.startObject(str.size());
}
template <typename CharT, typename Traits, typename Alloc>
inline bool epilogue(BinaryOutputSerializer& sa, const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return sa.endObject();
}

#if NEKO_CPP_PLUS >= 17
template <typename CharT, typename Traits>
inline bool prologue(BinaryOutputSerializer& sa, const std::basic_string_view<CharT, Traits>& str) NEKO_NOEXCEPT {
    return sa.startObject(str.size());
}
template <typename CharT, typename Traits>
inline bool epilogue(BinaryOutputSerializer& sa, const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return sa.endObject();
}
#endif

NEKO_END_NAMESPACE
