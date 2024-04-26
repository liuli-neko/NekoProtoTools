#pragma once

#include <vector>
#include <stdint.h>
#include <arpa/inet.h>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

template <typename T, class enable = void>
struct BinaryConvert;

class BinarySerializer {
 public:
  inline void startSerialize() {
    mData.clear();
    mData.reserve(1024);
  }
  inline bool endSerialize(std::vector<char> *data) {
    data->swap(mData);
    mData.clear();
    return true;
  }

  template <typename T>
  bool insert(const char *name, const T &value) {
    return BinaryConvert<T>::toBinaryArray(mData, value);
  }

  inline bool startDeserialize(const std::vector<char> &data) {
    mData = data;
    mOffset = 0;
    return true;
  }

  inline bool endDeserialize() {
    if (mOffset != mData.size()) {
      CS_LOG_WARN("binary data deserialize warning, read size{} != buf szie{}.", mOffset, mData.size());
      // std::cout << "mOffset: " << mOffset << ", mData.size(): " << mData.size() << std::endl;
    }
    mData.clear();
    return true;
  }

  template <typename T>
  bool get(const char *name, T *value) {
    return BinaryConvert<T>::fromBinaryArray(value, mData, mOffset);
  }

 private:
    std::vector<char> mData;
    int mOffset = 0;
};

template <>
struct BinaryConvert<uint8_t, void> {
  static bool toBinaryArray(std::vector<char> &buf, const uint8_t value) {
    buf.push_back(value);
    return true;
  }
  static bool fromBinaryArray(uint8_t *dst, const std::vector<char> &buf, int &offset_byte) {
    *dst = buf[offset_byte];
    offset_byte += 1;
    return true;
  }
};

template <>
struct BinaryConvert<int8_t, void> {
  static bool toBinaryArray(std::vector<char> &buf, const int8_t value) {
    buf.push_back(value);
    return true;
  }
  static bool fromBinaryArray(int8_t *dst, const std::vector<char> &buf, int &offset_byte) {
    if (buf.size() < offset_byte + 1) {
        return false;
    }
    *dst = buf[offset_byte];
  }
};

template <>
struct BinaryConvert<uint16_t, void> {
  static bool toBinaryArray(std::vector<char> &buf, const uint16_t value) {
    auto nv = htobe16(value);
    buf.push_back(nv & 0xFF);
    buf.push_back((nv >> 8) & 0xFF);
    return true;
  }
  static bool fromBinaryArray(uint16_t *dst, const std::vector<char> &buf, int &offset_byte) {
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
  static bool toBinaryArray(std::vector<char> &buf, const int16_t value) {
    auto nv = htobe16(value);
    buf.push_back(nv & 0xFF);
    buf.push_back((nv >> 8) & 0xFF);
    return true;
  }
  static bool fromBinaryArray(int16_t *dst, const std::vector<char> &buf, int &offset_byte) {
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
  static bool toBinaryArray(std::vector<char> &buf, const uint32_t value) {
    auto nv = htobe32(value);
    buf.resize(buf.size() + 4, 0);
    memcpy(buf.data() + buf.size() - 4, &nv, 4);
    return true;
  }
  static bool fromBinaryArray(uint32_t *dst, const std::vector<char> &buf, int &offset_byte) {
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
  static bool toBinaryArray(std::vector<char> &buf, const int32_t value) {
    auto nv = htobe32(value);
    buf.resize(buf.size() + 4, 0);
    memcpy(buf.data() + buf.size() - 4, &nv, 4);
    return true;
  }
  static bool fromBinaryArray(int32_t *dst, const std::vector<char> &buf, int &offset_byte) {
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
  static bool toBinaryArray(std::vector<char> &buf, const uint64_t value) {
    auto nv = htobe64(value);
    buf.resize(buf.size() + 8, 0);
    memcpy(buf.data() + buf.size() - 8, &nv, 8);
    return true;
  }
  static bool fromBinaryArray(uint64_t *dst, const std::vector<char> &buf, int &offset_byte) {
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
  static bool toBinaryArray(std::vector<char> &buf, const int64_t value) {
    auto nv = htobe64(value);
    buf.resize(buf.size() + 8, 0);
    memcpy(buf.data() + buf.size() - 8, &nv, 8);
    return true;
  }
  static bool fromBinaryArray(int64_t *dst, const std::vector<char> &buf, int &offset_byte) {
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
  static bool toBinaryArray(std::vector<char> &buf, const std::string &value) {
    buf.resize(buf.size() + value.size(), 0);
    memcpy(buf.data() + buf.size() - value.size(), value.data(), value.size());
    return true;
  }
  static bool fromBinaryArray(std::string *dst, const std::vector<char> &buf, int &offset_byte) {
    if (buf.size() < offset_byte + 1) {
        return false;
    }
    int len = offset_byte;
    while (len < buf.size() && buf[len] != 0) {
      ++len;
    }
    if (buf.size() <= len) {
      return false;
    }
    *dst = std::string(buf.data() + offset_byte, len - offset_byte);
    offset_byte = len + 1;
    return true;
  }
};

CS_PROTO_END_NAMESPACE