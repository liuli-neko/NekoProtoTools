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

#include "private/detail/integer.hpp"
#include "private/global.hpp"
#include "private/helpers.hpp"

NEKO_BEGIN_NAMESPACE

inline int16_t htobe(const int16_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return htobe16(value);
#endif
}
inline int16_t betoh(const int16_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return be16toh(value);
#endif
}
inline uint16_t htobe(const uint16_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return htobe16(value);
#endif
}
inline uint16_t betoh(const uint16_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return be16toh(value);
#endif
}
inline int32_t htobe(const int32_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return htobe32(value);
#endif
}
inline int32_t betoh(const int32_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return be32toh(value);
#endif
}
inline uint32_t htobe(const uint32_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return htobe32(value);
#endif
}
inline uint32_t betoh(const uint32_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return be32toh(value);
#endif
}
inline int64_t htobe(const int64_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return htobe64(value);
#endif
}
inline int64_t betoh(const int64_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return be64toh(value);
#endif
}
inline uint64_t htobe(const uint64_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return htobe64(value);
#endif
}
inline uint64_t betoh(const uint64_t value) NEKO_NOEXCEPT {
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return be64toh(value);
#endif
}

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
        uint32_t size = value.size();
        saveValue(makeSizeTag(size));
        mBuffer.insert(mBuffer.end(), value.begin(), value.end());
        return true;
    }
    inline bool saveValue(const char* value) NEKO_NOEXCEPT {
        uint32_t size = strlen(value);
        saveValue(makeSizeTag(size));
        mBuffer.insert(mBuffer.end(), value, value + size);
        return true;
    }
#if NEKO_CPP_PLUS >= 17
    inline bool saveValue(const std::string_view value) NEKO_NOEXCEPT {
        uint32_t size = value.size();
        saveValue(makeSizeTag(size));
        mBuffer.insert(mBuffer.end(), value.begin(), value.end());
        return true;
    }
#endif
    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        return saveValue(NEKO_STRING_VIEW{value.name, value.nameLen}) && this->operator()(value.value);
    }
    // as serializer(makeSizeTag(size));
    inline bool startArray(const std::size_t size) NEKO_NOEXCEPT { return saveValue(makeSizeTag(size)); }
    inline bool endArray() { return true; }
    inline bool startObject(const std::size_t size) NEKO_NOEXCEPT { return saveValue(makeSizeTag(size)); }
    inline bool endObject() NEKO_NOEXCEPT { return true; }
    inline bool end() NEKO_NOEXCEPT { return true; }
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
        if (!loadValue(makeSizeTag(size))) {
            return false;
        }
        if (size > mSize - mOffset) {
            return false;
        }
        value.resize(size);
        std::copy(mBuffer + mOffset, mBuffer + mOffset + size, value.begin());
        mOffset += size;
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_integral<T>::value, sizeof(T) <= sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    inline bool loadValue(T& value) NEKO_NOEXCEPT {
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

    template <typename T>
    inline bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        return loadValue(value.size);
    }

    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        std::string name;
        if (!loadValue(name)) {
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
    template <typename Serializer>
    bool save(Serializer& serializer) const;
    template <typename Serializer>
    bool load(Serializer& serializer);
    Type value;
};

template <class T, std::size_t Size = sizeof(T)>
inline FixedLengthField<T> makeFixedLengthField(T&& value) NEKO_NOEXCEPT {
    return FixedLengthField<T, Size>{std::forward<T>(value)};
}

struct BinarySerializer {
    using OutputSerializer = BinaryOutputSerializer;
    using InputSerializer  = BinaryInputSerializer;
};

template <typename T, size_t Size>
template <typename Serializer>
inline bool FixedLengthField<T, Size>::save(Serializer& serializer) const {
    if (Size != sizeof(T)) {
        auto size = serializer.size();
        auto ret  = serializer.saveValue(value);
        while (ret && serializer.size() - size < Size) {
            ret = serializer.push(0);
        }
        return ret;
    } else {
        char buffer[Size];
        memcpy(buffer, &value, Size);
        for (auto& c : buffer) {
            if (!serializer.push(c)) {
                return false;
            }
        }
        return true;
    }
}

template <typename T, size_t Size>
template <typename Serializer>
inline bool FixedLengthField<T, Size>::load(Serializer& serializer) {
    if (Size != sizeof(T)) {
        auto size = serializer.offset();
        auto ret  = serializer.loadValue(value);
        char tmp;
        while (ret && serializer.offset() - size < Size) {
            ret = serializer.pop(tmp);
        }
        return ret;
    } else {
        char buffer[Size];
        for (auto& c : buffer) {
            if (!serializer.pop(c)) {
                return false;
            }
        }
        memcpy(&value, buffer, Size);
        return true;
    }
}

NEKO_END_NAMESPACE
