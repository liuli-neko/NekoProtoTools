/**
 * @file binary_data.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "../json_serializer.hpp"

#include <string>
#include <vector>

NEKO_BEGIN_NAMESPACE

/// ================== Base 64 ==========================
struct Base64Covert {
#if NEKO_CPP_PLUS >= 17
    constexpr static const char Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#else
    static const char Table[65];
#endif
    static inline uint8_t QueryTable(uint8_t ch) noexcept {
        // search on table
        return strchr(Table, ch) - Table;
    }
    static std::vector<char> Encode(const std::vector<char>& str) {
        // encode string to base64
        auto s = (str.data());
        return Encode(str.data(), str.size());
    }
    static std::vector<char> Encode(const char* str) { return Encode(str, strlen(str)); }
    static std::vector<char> Encode(const char* str, size_t datalen) {
        std::vector<uint8_t> buf;
        Encode(reinterpret_cast<const uint8_t*>(str), datalen, buf);
        return std::vector<char>((char*)(buf.data()), (char*)(buf.data()) + buf.size());
    }
    static void Encode(const uint8_t* data, size_t datalen, std::vector<uint8_t>& buf) {
        auto cptr  = data;
        auto table = Table;
        buf.resize(((datalen + 2) / 3) * 4, '=');
        auto bufptr = buf.data();
        while (datalen > 2) {
            // encode
            // 3 chars to 4 base64 chars
            // xxxxxx|xx xxxx|xxxx xx|xxxxxx
            bufptr[0] = table[(cptr[0] >> 2) & 0x3f];                    // get highest 6 bits
            bufptr[1] = table[((cptr[0] << 4) | (cptr[1] >> 4)) & 0x3f]; // get middle 6 bits
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
            bufptr += 4;
            break;
        }
        case 1: {
            // only 1 char
            // xxxxxx|xx 0000|00 00|000000
            bufptr[0] = table[(cptr[0] >> 2) & 0x3f];
            bufptr[1] = table[(cptr[0] << 4) & 0x3f];
            bufptr += 4;
            break;
        }
        }
        NEKO_ASSERT(bufptr == buf.data() + buf.size(), "BinarySerializer", "Bad Base64 String");
    }
    static std::vector<char> Decode(const std::vector<char>& str) { return Decode(str.data(), str.size()); }
    static std::vector<char> Decode(const char* str) { return Decode(str, strlen(str)); }
    static std::vector<char> Decode(const char* str, size_t datalen) {
        std::vector<uint8_t> buf;
        Decode(reinterpret_cast<const uint8_t*>(str), datalen, buf);
        return std::vector<char>((char*)(buf.data()), (char*)(buf.data()) + buf.size());
    }
    static bool Decode(const uint8_t* data, size_t datalen, std::vector<uint8_t>& buf) {
        if ((datalen % 4) != 0) {
            NEKO_LOG_ERROR("proto", "Bad Base64 String len({}), data({:.{}s})", datalen,
                           reinterpret_cast<const char*>(data), datalen);
            return false;
        }

        int len   = (datalen / 4) * 3;
        auto cptr = data;
        while (data[datalen - 1] == '=') {
            --datalen; // remove '='
            --len;
        }
        buf.resize(len, '=');

        uint8_t array[4];
        auto bufptr = buf.data();
        while (datalen > 3) {
            array[0]  = QueryTable(cptr[0]);
            array[1]  = QueryTable(cptr[1]);
            array[2]  = QueryTable(cptr[2]);
            array[3]  = QueryTable(cptr[3]);
            bufptr[0] = ((array[0] << 2) | (array[1] >> 4));
            bufptr[1] = ((array[1] << 4) | (array[2] >> 2));
            bufptr[2] = ((array[2] << 6) | array[3]);
            cptr += 4;
            bufptr += 3;
            datalen -= 4;
        }
        NEKO_ASSERT(datalen != 1, "BinarySerializer", "Bad Base64 String");
        switch (datalen) {
        case 2: {
            array[0]  = QueryTable(cptr[0]);
            array[1]  = QueryTable(cptr[1]);
            bufptr[0] = ((array[0] << 2) | (array[1] >> 4));
            bufptr += 1;
            break;
        }
        case 3: {
            array[0]  = QueryTable(cptr[0]);
            array[1]  = QueryTable(cptr[1]);
            array[2]  = QueryTable(cptr[2]);
            bufptr[0] = ((array[0] << 2) | (array[1] >> 4));
            bufptr[1] = ((array[1] << 4) | (array[2] >> 2));
            bufptr += 2;
            break;
        }
        }
        NEKO_ASSERT(bufptr == buf.data() + buf.size(), "BinarySerializer", "Bad Base64 String");
        return true;
    }
};
#if NEKO_CPP_PLUS < 17
const char Base64Covert::Table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#endif
/// ===================end base 64=========================

template <class T>
struct BinaryData {
private:
    // Store a reference if passed an lvalue reference, otherwise
    // make a copy of the data
    using Type = T;

public:
    Type* data;
    std::size_t size;

    // for InputSerializer, please ensure that the remaining valid space of the data block is greater than or equal to
    // the length of the obtained data, and the size value is not used in the obtained data.
    // for OutputSerializer, while write data of size to JSON
    BinaryData(Type* data, std::size_t size) : data(data), size(size) {}
    BinaryData(Type* data) : data(data), size(0) {}
};

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const BinaryData<T>& value) {
    std::vector<uint8_t> buf;
    Base64Covert::Encode(reinterpret_cast<const uint8_t*>(value.data), value.size, buf);
    buf.push_back('\0');
    return sa(reinterpret_cast<const char*>(buf.data()));
}

template <typename Serializer, typename T>
inline bool load(Serializer& sa, BinaryData<T>& value) {
    std::string sv;
    if (!sa(sv)) {
        return false;
    }
    std::vector<uint8_t> buf;
    auto ret = Base64Covert::Decode(reinterpret_cast<const uint8_t*>(sv.data()), sv.size(), buf);
    memcpy(value.data, buf.data(), buf.size());
    return ret;
}

template <typename T>
struct is_minimal_serializable<BinaryData<T>, void> : std::true_type {};

NEKO_END_NAMESPACE