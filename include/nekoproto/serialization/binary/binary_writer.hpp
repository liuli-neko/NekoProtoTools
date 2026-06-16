#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/binary/endian.hpp"
#include "nekoproto/serialization/private/integer.hpp"

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace binary {

class Writer {
public:
    struct OutputArrayType {};
    struct OutputObjectType {};
    struct OutputValueType {};

    explicit Writer(std::vector<char>& buffer) noexcept : mBuffer(buffer) {}

    OutputArrayType arrayAsRoot(std::size_t size) {
        _writeVariable(size);
        return {};
    }

    OutputObjectType objectAsRoot(std::size_t size) {
        _writeVariable(size);
        return {};
    }

    OutputValueType nullAsRoot() {
        _writeString("null");
        return {};
    }

    template <typename T>
    OutputValueType valueAsRoot(const T& value) {
        _writeValue(value);
        return {};
    }

    template <typename T>
    OutputValueType fixedValueAsRoot(const T& value, std::size_t size) {
        writeFixed(value, size);
        return {};
    }

    OutputArrayType addArrayToArray(std::size_t size, OutputArrayType* /*parent*/) {
        _writeVariable(size);
        return {};
    }

    OutputArrayType addArrayToObject(std::string_view name, std::size_t size, OutputObjectType* /*parent*/) {
        _writeString(name);
        _writeVariable(size);
        return {};
    }

    OutputObjectType addObjectToArray(std::size_t size, OutputArrayType* /*parent*/) {
        _writeVariable(size);
        return {};
    }

    OutputObjectType addObjectToObject(std::string_view name, std::size_t size, OutputObjectType* /*parent*/) {
        _writeString(name);
        _writeVariable(size);
        return {};
    }

    void addUnframedObjectToObject(std::string_view name, OutputObjectType* /*parent*/) {
        _writeString(name);
    }

    template <typename T>
    OutputValueType addValueToArray(const T& value, OutputArrayType* /*parent*/) {
        _writeValue(value);
        return {};
    }

    template <typename T>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* /*parent*/) {
        _writeString(name);
        _writeValue(value);
        return {};
    }

    template <typename T>
    OutputValueType addFixedValueToArray(const T& value, std::size_t size,
                                         OutputArrayType* /*parent*/) {
        writeFixed(value, size);
        return {};
    }

    template <typename T>
    OutputValueType addFixedValueToObject(std::string_view name, const T& value,
                                          std::size_t size, OutputObjectType* /*parent*/) {
        _writeString(name);
        writeFixed(value, size);
        return {};
    }

    OutputValueType addNullToArray(OutputArrayType* /*parent*/) {
        _writeString("null");
        return {};
    }

    OutputValueType addNullToObject(std::string_view name, OutputObjectType* /*parent*/) {
        _writeString(name);
        _writeString("null");
        return {};
    }

    std::size_t size() const noexcept { return mBuffer.size(); }

private:
    template <typename T>
    void writeFixed(const T& value, std::size_t size) {
        if (size == sizeof(T)) {
            T encoded = value;
            if constexpr (std::is_integral_v<T> && sizeof(T) > 1) {
                encoded = htobe(value);
            }
            const auto* bytes = reinterpret_cast<const char*>(&encoded);
            mBuffer.insert(mBuffer.end(), bytes, bytes + size);
        } else {
            const auto begin = mBuffer.size();
            _writeValue(value);
            while (mBuffer.size() - begin < size) {
                mBuffer.push_back(0);
            }
        }
    }
    template <typename T>
    void _writeVariable(T value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_signed_v<U>) {
            using Unsigned = std::make_unsigned_t<U>;
            const bool negative = value < 0;
            const auto magnitude =
                negative ? static_cast<Unsigned>(-(value + 1)) + 1U : static_cast<Unsigned>(value);
            mBuffer.push_back(negative ? static_cast<char>(0x80U) : static_cast<char>(0x00U));
            detail::IntegerEncoder::encode(magnitude, mBuffer, 1);
        } else {
            mBuffer.push_back(0);
            detail::IntegerEncoder::encode(value, mBuffer, 1);
        }
    }

    void _writeString(std::string_view value) {
        _writeVariable(value.size());
        mBuffer.insert(mBuffer.end(), value.begin(), value.end());
    }

    template <typename T>
    void _writeValue(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            _writeString(value);
        } else if constexpr (std::is_same_v<U, bool>) {
            mBuffer.push_back(value ? 1 : 0);
        } else if constexpr (std::is_integral_v<U>) {
            _writeVariable(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            const auto* bytes = reinterpret_cast<const char*>(&value);
            mBuffer.insert(mBuffer.end(), bytes, bytes + sizeof(U));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported binary value type");
        }
    }

private:
    std::vector<char>& mBuffer;
};

} // namespace binary
NEKO_END_NAMESPACE
