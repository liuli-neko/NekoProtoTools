#pragma once

#include "cc_proto_json_serializer.hpp"

#include <string>
#include <vector>

CS_PROTO_BEGIN_NAMESPACE

/// ================== Base 64 ==========================
struct Base64Covert {
  constexpr static const char Table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static inline uint8_t QueryTable(uint8_t ch) noexcept {
    // search on table
    return strchr(Table, ch) - Table;
  }
  static std::vector<char> Encode(const std::vector<char> &str) {
    // encode string to base64
    auto s = (const uint8_t *)(str.data());
    return Encode(s, strlen((char *)s));
  }
  static std::vector<char> Encode(const uint8_t *str) {
    return Encode(str, strlen((char *)str));
  }
  static std::vector<char> Encode(const uint8_t *str, size_t datalen) {
    std::vector<uint8_t> buf;
    Encode(str, datalen, buf);
    return std::vector<char>((char *)(buf.data()),
                               (char *)(buf.data()) + buf.size());
  }
  static void Encode(const uint8_t *data, size_t datalen,
                     std::vector<uint8_t> &buf) {
    auto cptr = data;
    auto table = Table;
    buf.resize(((datalen + 2) / 3) * 4);
    auto bufptr = buf.data();
    while (datalen > 2) {
      // encode
      // 3 chars to 4 base64 chars
      // xxxxxx|xx xxxx|xxxx xx|xxxxxx
      bufptr[0] = table[(cptr[0] >> 2) & 0x3f];  // get highest 6 bits
      bufptr[1] =
          table[((cptr[0] << 4) | (cptr[1] >> 4)) & 0x3f];  // get middle 6 bits
      bufptr[2] = table[((cptr[1] << 2) | (cptr[2] >> 6)) & 0x3f];
      bufptr[3] = table[cptr[2] & 0x3f];

      // move raw data pointer to next 3 chars
      cptr += 3;
      bufptr += 4;
      datalen -= 3;
    }
    switch (datalen) {
      case 2: {
        // only 2 chars
        // xxxxxx|xx xxxx|xxxx 00|000000
        bufptr[0] = table[(cptr[0] >> 2) & 0x3f];
        bufptr[1] = table[((cptr[0] << 4) | (cptr[1] >> 4)) & 0x3f];
        bufptr[2] = table[(cptr[1] & 0x0f) << 2];
        bufptr[3] = '=';
        break;
      }
      case 1: {
        // only 1 char
        // xxxxxx|xx 0000|00 00|000000
        bufptr[0] = table[(cptr[0] >> 2) & 0x3f];
        bufptr[1] = table[(cptr[1] << 4) & 0x3f];
        bufptr[2] = '=';
        bufptr[3] = '=';
        break;
      }
    }
  }
  static std::vector<char> Decode(const std::vector<char> &str) {
    auto s = (const uint8_t *)(str.data());
    return Decode(s, str.size());
  }
  static std::vector<char> Decode(const uint8_t *str) {
    return Decode(str, strlen((char *)str));
  }
  static std::vector<char> Decode(const uint8_t *str, size_t datalen) {
    std::vector<uint8_t> buf;
    Decode(str, datalen, buf);
    return std::vector<char>((char *)(buf.data()),
                               (char *)(buf.data()) + buf.size());
  }
  static bool Decode(const uint8_t *data, size_t datalen,
                     std::vector<uint8_t> &buf) {
    if ((datalen % 4) != 0) {
      LOG("Bad Base64 String len(%zu)", datalen);
      return false;
    }
    buf.resize(datalen / 4 * 3);

    auto cptr = data;
    while (data[datalen - 1] == '=') {
      --datalen;  // remove '='
    }

    uint8_t array[4];
    auto bufptr = buf.data();
    while (datalen > 3) {
      array[0] = QueryTable(cptr[0]);
      array[1] = QueryTable(cptr[1]);
      array[2] = QueryTable(cptr[2]);
      array[3] = QueryTable(cptr[3]);
      bufptr[0] = ((array[0] << 2) | (array[1] >> 4));
      bufptr[1] = ((array[1] << 4) | (array[2] >> 2));
      bufptr[2] = ((array[2] << 6) | array[3]);
      cptr += 4;
      bufptr += 3;
      datalen -= 4;
    }
    CS_PROTO_ASSERT(datalen != 1, "Bad Base64 String");
    switch (datalen) {
      case 2: {
        array[0] = QueryTable(cptr[0]);
        array[1] = QueryTable(cptr[1]);
        bufptr[0] = ((array[0] << 2) | (array[1] >> 4));
        break;
      }
      case 3: {
        array[0] = QueryTable(cptr[0]);
        array[1] = QueryTable(cptr[1]);
        array[2] = QueryTable(cptr[2]);
        bufptr[0] = ((array[0] << 2) | (array[1] >> 4));
        bufptr[1] = ((array[1] << 4) | (array[2] >> 2));
        break;
      }
    }
    return true;
  }
};
/// ===================end base 64=========================
template <typename T>
struct JsonConvert<T, typename std::enable_if<!std::is_same<
                          typename T::SerializerType, JsonSerializer>::value>::type> {
  static void toJsonValue(JsonWriter &writer, const T &value) {
    auto data = Base64Covert::Encode(value.serialize());
    writer.String(data.data(), data.size(), true);
  }
  static bool fromJsonValue(T *dst, const JsonValue &value) {
    if (!value.IsString() || dst == nullptr) {
      return false;
    }
    dst->deserialize(
        Base64Covert::Decode(reinterpret_cast<const uint8_t *>(value.GetString()), value.GetStringLength()));
    return true;
  }
};

CS_PROTO_END_NAMESPACE