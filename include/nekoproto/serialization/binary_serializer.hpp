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

#include "nekoproto/global/global.hpp"
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
    bool saveValue(SizeTag<T> const& size) NEKO_NOEXCEPT {
        return saveValue(size.size);
    }
    template <typename T>
        requires std::is_unsigned_v<T> && (sizeof(T) <= sizeof(uint64_t)) && (!std::is_enum_v<T>)
    bool saveValue(const T value) NEKO_NOEXCEPT {
        mBuffer.push_back(0x00U);
        return detail::IntegerEncoder::encode(value, mBuffer, 1) == 0;
    }
    template <typename T>
        requires std::is_signed_v<T> && (sizeof(T) <= sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool saveValue(const T value) NEKO_NOEXCEPT {
        if (value < 0) {
            mBuffer.push_back(0x80U);
            return detail::IntegerEncoder::encode(-value, mBuffer, 1) == 0;
        }
        mBuffer.push_back(0x00U);
        return detail::IntegerEncoder::encode(value, mBuffer, 1) == 0;
    }
    bool saveValue(const float value) NEKO_NOEXCEPT {
        mBuffer.resize(mBuffer.size() + sizeof(float), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(float), &value, sizeof(float));
        return true;
    }
    bool saveValue(const double value) NEKO_NOEXCEPT {
        mBuffer.resize(mBuffer.size() + sizeof(double), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(double), &value, sizeof(double));
        return true;
    }
    bool saveValue(const bool value) NEKO_NOEXCEPT {
        mBuffer.push_back(value ? 1 : 0);
        return true;
    }
    bool saveValue(const std::string& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save string({})", value);
        saveValue(value.size());
        mBuffer.insert(mBuffer.end(), value.begin(), value.end());
        return true;
    }
    bool saveValue(const char* value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save string({})", value);
        std::size_t size = strlen(value);
        saveValue(size);
        mBuffer.insert(mBuffer.end(), value, value + size);
        return true;
    }
    bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT { return saveValue("null"); }
    bool saveValue(const std::string_view value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save string_view({})", value);
        saveValue(value.size());
        mBuffer.insert(mBuffer.end(), value.begin(), value.end());
        return true;
    }
    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "save name({}) value pair", NEKO_STRING_VIEW{value.name, value.nameLen});
        return this->operator()(NEKO_STRING_VIEW{value.name, value.nameLen}) && this->operator()(value.value);
    }
    // as serializer(make_size_tag(size));
    bool startArray(const std::size_t size) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "startArray: {}", size);
        return saveValue(make_size_tag(size));
    }
    bool endArray() { // NOLINT(readability-convert-member-functions-to-static)
        NEKO_LOG_INFO("BinarySerializer", "endArray");
        return true;
    }
    bool startObject(const std::size_t size) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "startObject: {}", size);
        return saveValue(make_size_tag(size));
    }
    bool endObject() NEKO_NOEXCEPT { // NOLINT(readability-convert-member-functions-to-static)
        NEKO_LOG_INFO("BinarySerializer", "endObject");
        return true;
    }
    bool end() NEKO_NOEXCEPT { // NOLINT(readability-convert-member-functions-to-static)
        return true;
    }
    bool push(char value) NEKO_NOEXCEPT {
        mBuffer.push_back(value);
        return true;
    }
    std::size_t size() const { return mBuffer.size(); }

private:
    BinaryOutputSerializer& operator=(const BinaryOutputSerializer&) = delete;
    BinaryOutputSerializer& operator=(BinaryOutputSerializer&&)      = delete;

private:
    BufferType& mBuffer;
};

class BinaryInputSerializer : public detail::InputSerializer<BinaryInputSerializer> {
public:
    explicit BinaryInputSerializer(const char* buf, const std::size_t size) NEKO_NOEXCEPT
        : InputSerializer<BinaryInputSerializer>(this),
          mBuffer(buf),
          mSize(size),
          mOffset(0) {}
    operator bool() const NEKO_NOEXCEPT { return true; }
    std::string name() NEKO_NOEXCEPT {
        std::string ret;
        if (loadValue(ret)) {
            return ret;
        }
        return {};
    }

    bool loadValue(std::string& value) NEKO_NOEXCEPT {
        std::size_t size = value.size();
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

    template <typename T>
        requires std::is_signed_v<T> && (sizeof(T) <= sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mOffset >= mSize) {
            return false;
        }
        auto flags = mBuffer[mOffset] & 0x80;
        int ret    = detail::IntegerDecoder::decode(reinterpret_cast<const uint8_t*>(mBuffer + mOffset),
                                                    (int)(mSize - mOffset), value, 1);
        if (ret <= 0) {
            return false;
        }
        mOffset += ret;
        if (flags) {
            value = -value;
        }
        NEKO_LOG_INFO("BinarySerializer", "load signed int {}", value);
        return true;
    }
    template <typename T>
        requires std::is_unsigned_v<T> && (sizeof(T) <= sizeof(uint64_t)) && (!std::is_enum_v<T>)
    bool loadValue(T& value) NEKO_NOEXCEPT {
        if (mOffset >= mSize) {
            return false;
        }
        auto flags = mBuffer[mOffset] & 0x80;
        if (flags) {
            return false;
        }
        auto ret = detail::IntegerDecoder::decode(reinterpret_cast<const uint8_t*>(mBuffer + mOffset),
                                                  (int)(mSize - mOffset), value, 1);
        if (ret <= 0) {
            return false;
        }
        mOffset += ret;
        NEKO_LOG_INFO("BinarySerializer", "load unsigned int {}", value);
        return true;
    }

    bool loadValue(float& value) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "load null");
        if (mOffset + sizeof(float) > mSize) {
            return false;
        }
        memcpy(&value, mBuffer + mOffset, sizeof(float));
        mOffset += sizeof(float);
        NEKO_LOG_INFO("BinarySerializer", "load float {}", value);
        return true;
    }

    bool loadValue(double& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(double) > mSize) {
            return false;
        }
        memcpy(&value, mBuffer + mOffset, sizeof(double));
        mOffset += sizeof(double);
        NEKO_LOG_INFO("BinarySerializer", "load double {}", value);
        return true;
    }

    bool loadValue(bool& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(bool) > mSize) {
            return false;
        }
        memcpy(&value, mBuffer + mOffset, sizeof(bool));
        mOffset += sizeof(bool);
        NEKO_LOG_INFO("BinarySerializer", "load bool {}", value);
        return true;
    }

    bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        NEKO_LOG_INFO("BinarySerializer", "load null");
        std::string value;
        auto ret = loadValue(value);
        return ret && value == "null";
    }

    template <typename T>
    bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        if (mOffset >= mSize) {
            return false;
        }
        auto flags = mBuffer[mOffset] & 0x80;
        if (flags) {
            return false;
        }
        auto ret = detail::IntegerDecoder::decode(reinterpret_cast<const uint8_t*>(mBuffer + mOffset),
                                                  (int)(mSize - mOffset), value.size, 1);
        if (ret <= 0) {
            return false;
        }
        mOffset += ret;
        NEKO_LOG_INFO("BinarySerializer", "load size tag {}", value.size);
        return true;
    }

    template <typename T>
    bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
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
    bool pop(char& value) NEKO_NOEXCEPT {
        if (mOffset + 1 > mSize) {
            return false;
        }
        value = mBuffer[mOffset];
        mOffset += 1;
        return true;
    }
    std::size_t offset() const { return mOffset; }

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

template <typename T, std::size_t Size = sizeof(T)>
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

template <typename T, std::size_t Size>
inline bool FixedLengthField<T, Size>::save(BinaryOutputSerializer& serializer) const {
    NEKO_CONSTEXPR_IF(Size != sizeof(T)) {
        auto size = serializer.size();
        auto ret  = serializer.saveValue(value);
        while (ret && serializer.size() - size < Size) {
            ret = serializer.push(0);
        }
        return ret;
    }
    else {
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
}

template <typename T, std::size_t Size>
inline bool FixedLengthField<T, Size>::load(BinaryInputSerializer& serializer) {
    NEKO_CONSTEXPR_IF(Size != sizeof(T)) {
        auto size = serializer.offset();
        auto ret  = serializer.loadValue(value);
        char tmp;
        while (ret && serializer.offset() - size < Size) {
            ret = serializer.pop(tmp);
        }
        return ret;
    }
    else {
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
}

NEKO_END_NAMESPACE
