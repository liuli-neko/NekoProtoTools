#pragma once

#include <stdint.h>
#include <vector>
#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

#ifdef _WIN32
inline int16_t htobe16(const int16_t value) { return _byteswap_ushort(value); }
inline int16_t be16toh(const int16_t value) { return _byteswap_ushort(value); }
inline uint16_t htobe16(const uint16_t value) { return _byteswap_ushort(value); }
inline uint16_t be16toh(const uint16_t value) { return _byteswap_ushort(value); }
inline int32_t htobe32(const int32_t value) { return _byteswap_ulong(value); }
inline int32_t be32toh(const int32_t value) { return _byteswap_ulong(value); }
inline uint32_t htobe32(const uint32_t value) { return _byteswap_ulong(value); }
inline uint32_t be32toh(const uint32_t value) { return _byteswap_ulong(value); }
inline int64_t htobe64(const int64_t value) { return _byteswap_uint64(value); }
inline int64_t be64toh(const int64_t value) { return _byteswap_uint64(value); }
inline uint64_t htobe64(const uint64_t value) { return _byteswap_uint64(value); }
inline uint64_t be64toh(const uint64_t value) { return _byteswap_uint64(value); }
#endif

template <typename T, class enable = void>
struct BinaryConvert;

class BinarySerializer {
public:
    void startSerialize();
    inline bool endSerialize(std::vector<char>* data);
    template <typename T>
    bool insert(const char* name, const T& value);
    bool startDeserialize(const std::vector<char>& data);
    bool endDeserialize();
    template <typename T>
    bool get(const char* name, T* value);

private:
    std::vector<char> mData;
    int mOffset = 0;
};

inline void BinarySerializer::startSerialize() {
    mData.clear();
    mData.reserve(1024);
}

inline bool BinarySerializer::endSerialize(std::vector<char>* data) {
    data->swap(mData);
    mData.clear();
    return true;
}

template <typename T>
bool BinarySerializer::insert(const char* name, const T& value) {
    return BinaryConvert<T>::toBinaryArray(mData, value);
}

inline bool BinarySerializer::startDeserialize(const std::vector<char>& data) {
    mData = data;
    mOffset = 0;
    return true;
}

inline bool BinarySerializer::endDeserialize() {
    if (mOffset != mData.size()) {
        CS_LOG_WARN("binary data deserialize warning, read size{} != buf szie{}.", mOffset, mData.size());
        // std::cout << "mOffset: " << mOffset << ", mData.size(): " << mData.size() << std::endl;
    }
    mData.clear();
    return true;
}

template <typename T>
bool BinarySerializer::get(const char* name, T* value) {
    return BinaryConvert<T>::fromBinaryArray(value, mData, mOffset);
}

template <>
struct BinaryConvert<uint8_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const uint8_t value) {
        buf.push_back(value);
        return true;
    }
    static bool fromBinaryArray(uint8_t* dst, const std::vector<char>& buf, int& offset_byte) {
        *dst = buf[offset_byte];
        offset_byte += 1;
        return true;
    }
};

template <>
struct BinaryConvert<int8_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const int8_t value) {
        buf.push_back(value);
        return true;
    }
    static bool fromBinaryArray(int8_t* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 1) {
            return false;
        }
        *dst = buf[offset_byte];
    }
};

template <>
struct BinaryConvert<uint16_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const uint16_t value) {
        auto nv = htobe16(value);
        buf.push_back(nv & 0xFF);
        buf.push_back((nv >> 8) & 0xFF);
        return true;
    }
    static bool fromBinaryArray(uint16_t* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 2) {
            return false;
        }
        uint16_t value = 0;
        value |= buf[offset_byte] & 0xFF;
        value |= (buf[offset_byte + 1] & 0xFF) << 8;
        *dst = be16toh(value);
        offset_byte += 2;
        return true;
    }
};

template <>
struct BinaryConvert<int16_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const int16_t value) {
        auto nv = htobe16(value);
        buf.push_back(nv & 0xFF);
        buf.push_back((nv >> 8) & 0xFF);
        return true;
    }
    static bool fromBinaryArray(int16_t* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 2) {
            return false;
        }
        int16_t value = 0;
        value |= buf[offset_byte] & 0xFF;
        value |= (buf[offset_byte + 1] & 0xFF) << 8;
        *dst = be16toh(value);
        offset_byte += 2;
        return true;
    }
};

template <>
struct BinaryConvert<uint32_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const uint32_t value) {
        auto nv = htobe32(value);
        buf.resize(buf.size() + 4, 0);
        memcpy(buf.data() + buf.size() - 4, &nv, 4);
        return true;
    }
    static bool fromBinaryArray(uint32_t* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 4) {
            return false;
        }
        uint32_t value = 0;
        memcpy(&value, buf.data() + offset_byte, 4);
        *dst = be32toh(value);
        offset_byte += 4;
        return true;
    }
};

template <>
struct BinaryConvert<int32_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const int32_t value) {
        auto nv = htobe32(value);
        buf.resize(buf.size() + 4, 0);
        memcpy(buf.data() + buf.size() - 4, &nv, 4);
        return true;
    }
    static bool fromBinaryArray(int32_t* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 4) {
            return false;
        }
        int32_t value = 0;
        memcpy(&value, buf.data() + offset_byte, 4);
        *dst = be32toh(value);
        offset_byte += 4;
        return true;
    }
};

template <>
struct BinaryConvert<uint64_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const uint64_t value) {
        auto nv = htobe64(value);
        buf.resize(buf.size() + 8, 0);
        memcpy(buf.data() + buf.size() - 8, &nv, 8);
        return true;
    }
    static bool fromBinaryArray(uint64_t* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 8) {
            return false;
        }
        uint64_t value = 0;
        memcpy(&value, buf.data() + offset_byte, 8);
        *dst = be64toh(value);
        offset_byte += 8;
        return true;
    }
};

template <>
struct BinaryConvert<int64_t, void> {
    static bool toBinaryArray(std::vector<char>& buf, const int64_t value) {
        auto nv = htobe64(value);
        buf.resize(buf.size() + 8, 0);
        memcpy(buf.data() + buf.size() - 8, &nv, 8);
        return true;
    }
    static bool fromBinaryArray(int64_t* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 8) {
            return false;
        }
        int64_t value = 0;
        memcpy(&value, buf.data() + offset_byte, 8);
        *dst = be64toh(value);
        offset_byte += 8;
        return true;
    }
};

template <>
struct BinaryConvert<std::string, void> {
    static bool toBinaryArray(std::vector<char>& buf, const std::string& value) {
        buf.resize(buf.size() + value.size() + 8, 0);
        uint64_t len = htobe64((uint64_t)value.size());
        memcpy(buf.data() + buf.size() - 8 - value.size(), &len, 8);
        memcpy(buf.data() + buf.size() - value.size(), value.data(), value.size());
        return true;
    }
    static bool fromBinaryArray(std::string* dst, const std::vector<char>& buf, int& offset_byte) {
        if (buf.size() < offset_byte + 8) {
            return false;
        }
        uint64_t len = 0;
        memcpy(&len, buf.data() + offset_byte, 8);
        len = be64toh(len);
        if (buf.size() < offset_byte + len + 8) {
            return false;
        }
        *dst = std::string(buf.data() + offset_byte + 4, len);
        offset_byte += len + 8;
        return true;
    }
};

CS_PROTO_END_NAMESPACE