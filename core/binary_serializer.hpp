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

NEKO_BEGIN_NAMESPACE

#ifdef _WIN32
inline int16_t htobe16(const int16_t value) NEKO_NOEXCEPT { return _byteswap_ushort(value); }
inline int16_t be16toh(const int16_t value) NEKO_NOEXCEPT { return _byteswap_ushort(value); }
inline uint16_t htobe16(const uint16_t value) NEKO_NOEXCEPT { return _byteswap_ushort(value); }
inline uint16_t be16toh(const uint16_t value) NEKO_NOEXCEPT { return _byteswap_ushort(value); }
inline int32_t htobe32(const int32_t value) NEKO_NOEXCEPT { return _byteswap_ulong(value); }
inline int32_t be32toh(const int32_t value) NEKO_NOEXCEPT { return _byteswap_ulong(value); }
inline uint32_t htobe32(const uint32_t value) NEKO_NOEXCEPT { return _byteswap_ulong(value); }
inline uint32_t be32toh(const uint32_t value) NEKO_NOEXCEPT { return _byteswap_ulong(value); }
inline int64_t htobe64(const int64_t value) NEKO_NOEXCEPT { return _byteswap_uint64(value); }
inline int64_t be64toh(const int64_t value) NEKO_NOEXCEPT { return _byteswap_uint64(value); }
inline uint64_t htobe64(const uint64_t value) NEKO_NOEXCEPT { return _byteswap_uint64(value); }
inline uint64_t be64toh(const uint64_t value) NEKO_NOEXCEPT { return _byteswap_uint64(value); }
#endif

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
        mBuffer.push_back('S');
        mBuffer.push_back(':');
        saveValue(size.size);
        return true;
    }
    inline bool saveValue(const int8_t value) NEKO_NOEXCEPT {
        mBuffer.push_back(value);
        return true;
    }
    inline bool saveValue(const uint8_t value) NEKO_NOEXCEPT {
        mBuffer.push_back(value);
        return true;
    }
    inline bool saveValue(const int16_t value) NEKO_NOEXCEPT {
        auto nv = htobe16(value);
        mBuffer.resize(mBuffer.size() + sizeof(int16_t), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(int16_t), &nv, sizeof(int16_t));
        return true;
    }
    inline bool saveValue(const uint16_t value) NEKO_NOEXCEPT {
        auto nv = htobe16(value);
        mBuffer.resize(mBuffer.size() + sizeof(uint16_t), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(uint16_t), &nv, sizeof(uint16_t));
        return true;
    }
    inline bool saveValue(const int32_t value) NEKO_NOEXCEPT {
        auto nv = htobe32(value);
        mBuffer.resize(mBuffer.size() + sizeof(int32_t), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(int32_t), &nv, sizeof(int32_t));
        return true;
    }
    inline bool saveValue(const uint32_t value) NEKO_NOEXCEPT {
        auto nv = htobe32(value);
        mBuffer.resize(mBuffer.size() + sizeof(uint32_t), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(uint32_t), &nv, sizeof(uint32_t));
        return true;
    }
    inline bool saveValue(const int64_t value) NEKO_NOEXCEPT {
        auto nv = htobe64(value);
        mBuffer.resize(mBuffer.size() + sizeof(int64_t), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(int64_t), &nv, sizeof(int64_t));
        return true;
    }
    inline bool saveValue(const uint64_t value) NEKO_NOEXCEPT {
        auto nv = htobe64(value);
        mBuffer.resize(mBuffer.size() + sizeof(uint64_t), 0);
        memcpy(mBuffer.data() + mBuffer.size() - sizeof(uint64_t), &nv, sizeof(uint64_t));
        return true;
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

    inline bool loadValue(int8_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(int8_t) > mSize) {
            return false;
        }
        value = mBuffer[mOffset];
        mOffset += sizeof(int8_t);
        return true;
    }

    inline bool loadValue(int16_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(int16_t) > mSize) {
            return false;
        }
        int16_t tmp;
        memcpy(&tmp, mBuffer + mOffset, sizeof(int16_t));
        mOffset += sizeof(int16_t);
        value = be16toh(tmp);
        return true;
    }

    inline bool loadValue(int32_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(int32_t) > mSize) {
            return false;
        }
        int32_t tmp;
        memcpy(&tmp, mBuffer + mOffset, sizeof(int32_t));
        mOffset += sizeof(int32_t);
        value = be32toh(tmp);
        return true;
    }

    inline bool loadValue(int64_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(int64_t) > mSize) {
            return false;
        }
        int64_t tmp;
        memcpy(&tmp, mBuffer + mOffset, sizeof(int64_t));
        mOffset += sizeof(int64_t);
        value = be64toh(tmp);
        return true;
    }

    inline bool loadValue(uint8_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(uint8_t) > mSize) {
            return false;
        }
        value = mBuffer[mOffset];
        mOffset += sizeof(uint8_t);
        return true;
    }

    inline bool loadValue(uint16_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(uint16_t) > mSize) {
            return false;
        }
        uint16_t tmp;
        memcpy(&tmp, mBuffer + mOffset, sizeof(uint16_t));
        mOffset += sizeof(uint16_t);
        value = be16toh(tmp);
        return true;
    }

    inline bool loadValue(uint32_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(uint32_t) > mSize) {
            return false;
        }
        uint32_t tmp;
        memcpy(&tmp, mBuffer + mOffset, sizeof(uint32_t));
        mOffset += sizeof(uint32_t);
        value = be32toh(tmp);
        return true;
    }

    inline bool loadValue(uint64_t& value) NEKO_NOEXCEPT {
        if (mOffset + sizeof(uint64_t) > mSize) {
            return false;
        }
        uint64_t tmp;
        memcpy(&tmp, mBuffer + mOffset, sizeof(uint64_t));
        mOffset += sizeof(uint64_t);
        value = be64toh(tmp);
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
        if (mOffset + 2 > mSize) {
            return false;
        }
        if (mBuffer[mOffset] != 'S' || mBuffer[mOffset + 1] != ':') {
            return false;
        }
        mOffset += 2;
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

private:
    BinaryInputSerializer& operator=(const BinaryInputSerializer&) = delete;
    BinaryInputSerializer& operator=(BinaryInputSerializer&&)      = delete;

private:
    const char* mBuffer;
    std::size_t mSize;
    std::size_t mOffset = 0;
};

struct BinarySerializer {
    using OutputSerializer = BinaryOutputSerializer;
    using InputSerializer  = BinaryInputSerializer;
};

NEKO_END_NAMESPACE