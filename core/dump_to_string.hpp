#pragma once

#include "private/helpers.hpp"
#include "serializer_base.hpp"

#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <string_view>
#include <variant>
#endif
#ifndef NEKO_SERIALIZABLE_TO_STRING_ENABLE
#define NEKO_SERIALIZABLE_TO_STRING_ENABLE 1
#endif

NEKO_BEGIN_NAMESPACE

class PrintSerializer : public detail::OutputSerializer<PrintSerializer> {
public:
    using BufferType = std::ostream;

public:
    PrintSerializer(BufferType& out) NEKO_NOEXCEPT : OutputSerializer<PrintSerializer>(this), mBuffer(out) {
        mBuffer << "{";
    }
    PrintSerializer(const PrintSerializer& other) NEKO_NOEXCEPT : OutputSerializer<PrintSerializer>(this),
                                                                  mBuffer(other.mBuffer) {}
    PrintSerializer(PrintSerializer&& other) NEKO_NOEXCEPT : OutputSerializer<PrintSerializer>(this),
                                                             mBuffer(other.mBuffer) {}
    template <typename T>
    inline bool saveValue(SizeTag<T> const& size) {
        mBuffer << "[size : " << size << "]";
        return true;
    }
    inline bool saveValue(const int8_t value) {
        mBuffer << static_cast<int>(value) << ", ";
        return true;
    }
    inline bool saveValue(const uint8_t value) {
        mBuffer << static_cast<int>(value) << ", ";
        return true;
    }
    inline bool saveValue(const int16_t value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const uint16_t value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const int32_t value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const uint32_t value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const int64_t value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const uint64_t value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const float value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const double value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const bool value) {
        mBuffer << (value ? "true" : "false") << ", ";
        return true;
    }
    inline bool saveValue(const std::string& value) {
        mBuffer << value << ", ";
        return true;
    }
    inline bool saveValue(const char* value) {
        mBuffer << value << ", ";
        return true;
    }
#if NEKO_CPP_PLUS >= 17
    inline bool saveValue(const std::string_view value) {
        mBuffer << value << ", ";
        return true;
    }
#endif
    template <typename T>
    inline bool saveValue(const NameValuePair<T>& value) {
        mBuffer << NEKO_STRING_VIEW{value.name, value.name_len} << " = ";
        return this->operator()(value.value);
    }
    // as serializer(makeSizeTag(size));
    bool startArray(const std::size_t size) {
        mBuffer << "[";
        return true;
    }
    bool endArray() {
        mBuffer << "], ";
        return true;
    }
    bool startObject() {
        mBuffer << "{";
        return true;
    }
    bool endObject() {
        mBuffer << "}, ";
        return true;
    }
    bool end() {
        mBuffer << "}";
        return true;
    }

private:
    PrintSerializer& operator=(const PrintSerializer&) = delete;
    PrintSerializer& operator=(PrintSerializer&&)      = delete;

private:
    BufferType& mBuffer;
};

template <typename T>
inline std::string SerializableToString(T&& value) {
    std::ostringstream buffer;
    PrintSerializer print(buffer);
    print(value);
    print.end();
    return buffer.str();
};

NEKO_END_NAMESPACE