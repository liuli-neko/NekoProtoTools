/**
 * @file to_string.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "private/helpers.hpp"

#include <sstream>
#include <string>
#if NEKO_CPP_PLUS >= 17
#include <string_view>
#endif

NEKO_BEGIN_NAMESPACE

class PrintSerializer : public detail::OutputSerializer<PrintSerializer> {
public:
    using BufferType = std::string;

public:
    PrintSerializer(BufferType& out) NEKO_NOEXCEPT : OutputSerializer<PrintSerializer>(this), mBuffer(out) {}
    PrintSerializer(const PrintSerializer& other) NEKO_NOEXCEPT : OutputSerializer<PrintSerializer>(this),
                                                                  mBuffer(other.mBuffer) {}
    PrintSerializer(PrintSerializer&& other) NEKO_NOEXCEPT : OutputSerializer<PrintSerializer>(this),
                                                             mBuffer(other.mBuffer) {}
    template <typename T>
    bool saveValue(const SizeTag<T>& size) NEKO_NOEXCEPT {
        mBuffer += "[size : " + std::to_string(size.size) + "]";
        return true;
    }
    template <typename T>
        requires std::is_signed_v<T> && (sizeof(T) <= sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool saveValue(const T value) NEKO_NOEXCEPT {
        mBuffer += std::to_string(static_cast<int64_t>(value)) + ", ";
        return true;
    }
    template <typename T>
        requires std::is_unsigned_v<T> && (sizeof(T) <= sizeof(int64_t)) && (!std::is_enum_v<T>)
    bool saveValue(const T value) NEKO_NOEXCEPT {
        mBuffer += std::to_string(static_cast<uint64_t>(value)) + ", ";
        return true;
    }
    bool saveValue(const float value) NEKO_NOEXCEPT {
        std::ostringstream buffer;
        buffer << value << ", ";
        mBuffer += buffer.str();
        return true;
    }
    bool saveValue(const double value) NEKO_NOEXCEPT {
        std::ostringstream buffer;
        buffer << value << ", ";
        mBuffer += buffer.str();
        return true;
    }
    bool saveValue(const bool value) NEKO_NOEXCEPT {
        mBuffer += std::string(value ? "true" : "false") + ", ";
        return true;
    }
    bool saveValue(const std::string& value) NEKO_NOEXCEPT {
        mBuffer += "\"" + value + "\", ";
        return true;
    }
    bool saveValue(const char* value) NEKO_NOEXCEPT {
        mBuffer += "\"" + std::string(value) + "\", ";
        return true;
    }
    bool saveValue(std::nullptr_t) NEKO_NOEXCEPT {
        mBuffer += "\"nullptr\", ";
        return true;
    }
    bool saveValue(const std::string_view value) NEKO_NOEXCEPT {
        mBuffer += "\"" + std::string(value) + "\", ";
        return true;
    }
    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        mBuffer += std::string{value.name, value.nameLen} + " = ";
        if constexpr (traits::optional_like_type<T>::value) {
            if (value.value.has_value()) {
                return (*this)(value.value.value());
            }
            mBuffer += "null, ";
            return true;

        } else {
            return (*this)(value.value);
        }
    }
    bool startArray(const std::size_t /*unused*/) NEKO_NOEXCEPT {
        mBuffer += "[";
        return true;
    }
    bool endArray() NEKO_NOEXCEPT {
        if (mBuffer.size() > 0 && mBuffer.back() == ' ' && *(mBuffer.rbegin() + 1) != '=') {
            mBuffer.pop_back();
            mBuffer.pop_back();
        }
        mBuffer += "], ";
        return true;
    }
    bool startObject(const std::size_t /*unused*/) NEKO_NOEXCEPT {
        mBuffer += "{";
        return true;
    }
    bool endObject() NEKO_NOEXCEPT {
        if (mBuffer.size() > 0 && mBuffer.back() == ' ' && *(mBuffer.rbegin() + 1) != '=') {
            mBuffer.pop_back();
            mBuffer.pop_back();
        }
        mBuffer += "}, ";
        return true;
    }
    bool end() NEKO_NOEXCEPT {
        if (mBuffer.size() > 0 && mBuffer.back() == ' ') {
            mBuffer.pop_back();
            mBuffer.pop_back();
        }
        return true;
    }

private:
    PrintSerializer& operator=(const PrintSerializer&) = delete;
    PrintSerializer& operator=(PrintSerializer&&)      = delete;

private:
    BufferType& mBuffer;
};

template <typename T>
inline std::string serializable_to_string(T&& value) NEKO_NOEXCEPT {
    std::string buffer;
    PrintSerializer print(buffer);
    print(value);
    print.end();
    return buffer;
};

NEKO_END_NAMESPACE