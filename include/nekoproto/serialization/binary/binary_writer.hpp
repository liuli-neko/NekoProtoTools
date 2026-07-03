#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/binary/endian.hpp"
#include "nekoproto/serialization/private/integer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace binary {

template <typename BufferT = std::vector<char>>
class Writer {
public:
    struct OutputArrayType {};
    struct OutputObjectType {};
    struct OutputValueType {};

    explicit Writer(BufferT& buffer) noexcept : mBuffer(buffer) {}

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
            _appendBytes(&encoded, size);
        } else {
            const auto begin = mBuffer.size();
            _writeValue(value);
            while (mBuffer.size() - begin < size) {
                _pushByte(0);
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
            _pushByte(negative ? 0x80U : 0x00U);
            detail::IntegerEncoder::encode(magnitude, mBuffer, 1);
        } else {
            _pushByte(0);
            detail::IntegerEncoder::encode(value, mBuffer, 1);
        }
    }

    void _writeString(std::string_view value) {
        _writeVariable(value.size());
        _appendBytes(value.data(), value.size());
    }

    template <typename T>
    void _writeValue(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            _writeString(value);
        } else if constexpr (std::is_same_v<U, bool>) {
            _pushByte(value ? 1U : 0U);
        } else if constexpr (std::is_integral_v<U>) {
            _writeVariable(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            _appendBytes(&value, sizeof(U));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported binary value type");
        }
    }

    void _pushByte(std::uint8_t byte) {
        mBuffer.push_back(static_cast<typename BufferT::value_type>(byte));
    }

    void _appendBytes(const void* data, std::size_t size) {
        if constexpr (std::is_same_v<typename BufferT::value_type, char>) {
            const auto* bytes = static_cast<const char*>(data);
            mBuffer.insert(mBuffer.end(), bytes, bytes + size);
        } else if constexpr (std::is_same_v<typename BufferT::value_type, std::byte>) {
            const auto* bytes = static_cast<const std::byte*>(data);
            mBuffer.insert(mBuffer.end(), bytes, bytes + size);
        } else {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            mBuffer.reserve(mBuffer.size() + size);
            for (std::size_t ix = 0; ix < size; ++ix) {
                _pushByte(bytes[ix]);
            }
        }
    }

private:
    BufferT& mBuffer;
};

} // namespace binary
NEKO_END_NAMESPACE
