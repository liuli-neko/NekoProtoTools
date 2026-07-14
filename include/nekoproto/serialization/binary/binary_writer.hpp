#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/binary/endian.hpp"
#include "nekoproto/serialization/error.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace binary {

/**
 * Binary V2 wire format (all numbers below are bytes unless stated otherwise).
 *
 * An ordinary document is:
 *
 *   4E 50 02 | root-value
 *   'N''P' V2
 *
 * Every framed value starts with one ValueTag byte.  To decode a value, read
 * that byte first, then consume exactly the payload described below:
 *
 *   00                    null
 *   01 / 02               false / true; there is no following payload
 *   03 uleb128             signed integer; ULEB128 contains zigzag(value)
 *   04 uleb128             unsigned integer
 *   05 be-u32              IEEE-754 float bits, most-significant byte first
 *   06 be-u64              IEEE-754 double bits, most-significant byte first
 *   07 uleb128(n) bytes[n] UTF-8/opaque string bytes (the codec does not
 *                         validate UTF-8)
 *   08 uleb128(n) value*n  array
 *   09 uleb128(n) field*n  name-preserving object
 *   0A uleb128(n) field*n  reflected/schema-known object with compact keys
 *   0B..12 fixed-bytes     signed/unsigned 8/16/32/64-bit integer; widths
 *                         greater than one byte are most-significant first
 *
 * ULEB128 stores seven low bits per byte.  Bit 7 means another byte follows.
 * Encodings must be minimal.  Zigzag maps 0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3,
 * so small signed magnitudes remain compact.
 *
 * A NamedObject field is:
 *
 *   uleb128(name-byte-count) | name-bytes | value
 *
 * An IdObject field is:
 *
 *   uleb128(key) | [name-bytes when key is even] | value
 *
 * For an even key, key >> 1 is the name byte count.  For an odd key, key >> 1
 * is the 32-bit FNV-1a hash of a schema-known field name and no name bytes are
 * stored.  Writer chooses the hash only when its encoded key is smaller than
 * the literal name.  The low bit keeps literal names and hashes in separate
 * wire namespaces.
 *
 * Containers carry an element/member count, and every child is recursively
 * self-delimiting.  Therefore Reader can locate and skip an unknown field
 * without a byte-length on every scalar.  A new, unknown ValueTag still
 * requires a new protocol version because its payload length is not known.
 *
 * std::variant has no dedicated ValueTag.  The generic parser layer encodes a
 * binary variant as Array [UnsignedInteger alternative-index, value], for
 * example alternative 1 containing "A":
 *
 *   08 02 04 01 07 01 41
 *
 * Metadata tags are not additional bytes by themselves.  BinaryTag
 * fixed_length selects tags 0B..12 for non-bool integral values; ParserTag
 * flat and rename/ignore tags change the object fields presented to this
 * writer.
 * BinaryTag raw_fixed_data is the deliberate exception to the framed grammar:
 * a root reflected record is emitted as concatenated fixed-width big-endian
 * fields with no magic, ValueTag, field key, count, or length.  Its schema must
 * therefore be known out of band and match exactly on both sides.
 */
enum class ValueTag : std::uint8_t {
    Null = 0,
    False = 1,
    True = 2,
    SignedInteger = 3,
    UnsignedInteger = 4,
    Float32 = 5,
    Float64 = 6,
    String = 7,
    Array = 8,
    NamedObject = 9,
    IdObject = 10,
    FixedSigned8 = 11,
    FixedSigned16 = 12,
    FixedSigned32 = 13,
    FixedSigned64 = 14,
    FixedUnsigned8 = 15,
    FixedUnsigned16 = 16,
    FixedUnsigned32 = 17,
    FixedUnsigned64 = 18,
};

inline constexpr std::byte BinaryMagic[] = {std::byte{0x4E}, std::byte{0x50}, std::byte{0x02}};

inline constexpr std::uint32_t fieldId(std::string_view name) noexcept {
    std::uint32_t hash = 2166136261U;
    for (const char ch : name) {
        hash ^= static_cast<std::uint8_t>(ch);
        hash *= 16777619U;
    }
    return hash;
}

inline constexpr std::size_t ulebSize(std::uint64_t value) noexcept {
    std::size_t size = 1;
    while (value >= 0x80U) {
        value >>= 7U;
        ++size;
    }
    return size;
}

inline constexpr bool useHashedFieldId(std::string_view name) noexcept {
    const auto namedKey = static_cast<std::uint64_t>(name.size()) << 1U;
    const auto hashedKey = (static_cast<std::uint64_t>(fieldId(name)) << 1U) | 1U;
    return ulebSize(hashedKey) < ulebSize(namedKey) + name.size();
}

template <typename BufferT = std::vector<char>>
class Writer {
private:
    enum class ContainerKind { Array, NamedObject, IdObject };

    template <ContainerKind Kind>
    class ContainerScope {
    public:
        ContainerScope() = default;
        ContainerScope(Writer& writer, std::size_t expected) noexcept : mWriter(&writer), mExpected(expected) {}

        ContainerScope(const ContainerScope&)            = delete;
        ContainerScope& operator=(const ContainerScope&) = delete;
        ContainerScope(ContainerScope&& other) noexcept { _moveFrom(other); }
        ContainerScope& operator=(ContainerScope&& other) noexcept {
            if (this != &other) {
                _finish();
                _moveFrom(other);
            }
            return *this;
        }
        ~ContainerScope() { _finish(); }

    private:
        friend class Writer;

        void _increment() noexcept { ++mActual; }

        bool _rememberId(std::uint32_t id) {
            if constexpr (Kind == ContainerKind::IdObject) {
                return mIds.insert(id).second;
            }
            return true;
        }

        void _finish() noexcept {
            if (mWriter != nullptr && mActual != mExpected) {
                mWriter->_setError(sa::ErrorCode::InvalidLength,
                                   "Binary container emitted member count does not match its declared count");
            }
            mWriter = nullptr;
        }

        void _moveFrom(ContainerScope& other) noexcept {
            mWriter   = std::exchange(other.mWriter, nullptr);
            mExpected = other.mExpected;
            mActual   = other.mActual;
            mIds      = std::move(other.mIds);
        }

        Writer* mWriter = nullptr;
        std::size_t mExpected = 0;
        std::size_t mActual   = 0;
        std::unordered_set<std::uint32_t> mIds;
    };

public:
    using OutputArrayType  = ContainerScope<ContainerKind::Array>;
    using OutputObjectType = ContainerScope<ContainerKind::NamedObject>;
    using OutputIdObjectType = ContainerScope<ContainerKind::IdObject>;
    struct OutputValueType {};

    explicit Writer(BufferT& buffer) noexcept : mBuffer(buffer) {}

    void beginRawFixedDataAsRoot() noexcept { mRawRoot = true; }

    OutputArrayType arrayAsRoot(std::size_t size) {
        _writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }
    OutputObjectType objectAsRoot(std::size_t size) {
        _writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }
    OutputIdObjectType idObjectAsRoot(std::size_t size) {
        _writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }

    OutputValueType nullAsRoot() {
        if (mRawRoot) {
            _setError(sa::ErrorCode::InvalidType, "raw_fixed_data binary values cannot encode null");
        } else {
            _ensureDocumentHeader();
            _pushByte(ValueTag::Null);
        }
        return {};
    }

    template <typename T>
    OutputValueType valueAsRoot(const T& value) {
        mRawRoot ? _writeRawValue(value) : _writeValue(value);
        return {};
    }

    template <typename T>
    OutputValueType fixedValueAsRoot(const T& value, std::size_t size) {
        mRawRoot ? _writeRawFixed(value, size) : _writeFixed(value, size);
        return {};
    }

    OutputArrayType addArrayToArray(std::size_t size, OutputArrayType* parent) {
        _increment(parent);
        _writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }
    OutputArrayType addArrayToObject(std::string_view name, std::size_t size, OutputObjectType* parent) {
        _beginNamedField(name, parent);
        _writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }
    OutputArrayType addArrayToObject(std::string_view name, std::size_t size, OutputIdObjectType* parent) {
        _beginIdField(name, parent);
        _writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }

    OutputObjectType addObjectToArray(std::size_t size, OutputArrayType* parent) {
        _increment(parent);
        _writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }
    OutputObjectType addObjectToObject(std::string_view name, std::size_t size, OutputObjectType* parent) {
        _beginNamedField(name, parent);
        _writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }
    OutputObjectType addObjectToObject(std::string_view name, std::size_t size, OutputIdObjectType* parent) {
        _beginIdField(name, parent);
        _writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }

    OutputIdObjectType addIdObjectToArray(std::size_t size, OutputArrayType* parent) {
        _increment(parent);
        _writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }
    OutputIdObjectType addIdObjectToObject(std::string_view name, std::size_t size, OutputObjectType* parent) {
        _beginNamedField(name, parent);
        _writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }
    OutputIdObjectType addIdObjectToObject(std::string_view name, std::size_t size, OutputIdObjectType* parent) {
        _beginIdField(name, parent);
        _writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }

    template <typename T>
    OutputValueType addValueToArray(const T& value, OutputArrayType* parent) {
        _increment(parent);
        _writeValue(value);
        return {};
    }
    template <typename T>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) {
        _beginNamedField(name, parent);
        _writeValue(value);
        return {};
    }
    template <typename T>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputIdObjectType* parent) {
        _beginIdField(name, parent);
        _writeValue(value);
        return {};
    }

    template <typename T>
    OutputValueType addFixedValueToArray(const T& value, std::size_t size, OutputArrayType* parent) {
        _increment(parent);
        _writeFixed(value, size);
        return {};
    }
    template <typename T>
    OutputValueType addFixedValueToObject(std::string_view name, const T& value, std::size_t size,
                                          OutputObjectType* parent) {
        _beginNamedField(name, parent);
        _writeFixed(value, size);
        return {};
    }
    template <typename T>
    OutputValueType addFixedValueToObject(std::string_view name, const T& value, std::size_t size,
                                          OutputIdObjectType* parent) {
        _beginIdField(name, parent);
        _writeFixed(value, size);
        return {};
    }

    OutputValueType addNullToArray(OutputArrayType* parent) {
        _increment(parent);
        _ensureDocumentHeader();
        _pushByte(ValueTag::Null);
        return {};
    }
    OutputValueType addNullToObject(std::string_view name, OutputObjectType* parent) {
        _beginNamedField(name, parent);
        _pushByte(ValueTag::Null);
        return {};
    }
    OutputValueType addNullToObject(std::string_view name, OutputIdObjectType* parent) {
        _beginIdField(name, parent);
        _pushByte(ValueTag::Null);
        return {};
    }

    sa::Result<void> result() const {
        if (mError) {
            return *mError;
        }
        return sa::success();
    }
    std::size_t size() const noexcept { return mBuffer.size(); }

private:
    template <ContainerKind Kind>
    static void _increment(ContainerScope<Kind>* parent) noexcept {
        if (parent != nullptr) {
            parent->_increment();
        }
    }

    void _writeContainerHeader(ValueTag tag, std::size_t size) {
        _ensureDocumentHeader();
        _pushByte(tag);
        _writeUleb128(static_cast<std::uint64_t>(size));
    }

    void _beginNamedField(std::string_view name, OutputObjectType* parent) {
        _increment(parent);
        _writeUleb128(static_cast<std::uint64_t>(name.size()));
        _appendBytes(name.data(), name.size());
    }

    void _beginIdField(std::string_view name, OutputIdObjectType* parent) {
        _increment(parent);
        if (!useHashedFieldId(name)) {
            _writeUleb128(static_cast<std::uint64_t>(name.size()) << 1U);
            _appendBytes(name.data(), name.size());
            return;
        }
        const auto id = fieldId(name);
        if (parent != nullptr && !parent->_rememberId(id)) {
            _setError(sa::ErrorCode::InvalidField, "Reflected binary object contains colliding hashed field ids");
        }
        _writeUleb128((static_cast<std::uint64_t>(id) << 1U) | 1U);
    }

    void _ensureDocumentHeader() {
        if (mDocumentStarted || mRawRoot) {
            return;
        }
        mDocumentStarted = true;
        _appendBytes(BinaryMagic, sizeof(BinaryMagic));
    }

    template <typename T>
    void _writeValue(const T& value) {
        using U = std::remove_cvref_t<T>;
        _ensureDocumentHeader();
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            _pushByte(ValueTag::String);
            _writeUleb128(static_cast<std::uint64_t>(value.size()));
            _appendBytes(value.data(), value.size());
        } else if constexpr (std::is_same_v<U, bool>) {
            _pushByte(value ? ValueTag::True : ValueTag::False);
        } else if constexpr (std::is_integral_v<U>) {
            if constexpr (std::is_signed_v<U>) {
                using Unsigned = std::make_unsigned_t<U>;
                const bool negative = value < 0;
                const auto magnitude = negative ? static_cast<Unsigned>(-(value + 1)) + Unsigned{1}
                                                : static_cast<Unsigned>(value);
                const auto zigzag = static_cast<Unsigned>((magnitude << 1U) - (negative ? Unsigned{1} : Unsigned{0}));
                _pushByte(ValueTag::SignedInteger);
                _writeUleb128(zigzag);
            } else {
                _pushByte(ValueTag::UnsignedInteger);
                _writeUleb128(value);
            }
        } else if constexpr (std::is_floating_point_v<U>) {
            _writeFloating(value);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported binary value type");
        }
    }

    template <typename T>
    void _writeFixed(const T& value, std::size_t size) {
        using U = std::remove_cvref_t<T>;
        _ensureDocumentHeader();
        if (size != sizeof(U)) {
            _setError(sa::ErrorCode::InvalidLength, "Fixed binary value width does not match its C++ type");
            return;
        }
        if constexpr (std::is_same_v<U, bool>) {
            _pushByte(value ? ValueTag::True : ValueTag::False);
        } else if constexpr (std::is_integral_v<U>) {
            _pushByte(_fixedTag<U>());
            _writeRawFixed(value, size);
        } else if constexpr (std::is_floating_point_v<U>) {
            _writeFloating(value);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported fixed binary value type");
        }
    }

    template <typename T>
    void _writeRawValue(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            _writeUleb128(static_cast<std::uint64_t>(value.size()));
            _appendBytes(value.data(), value.size());
        } else if constexpr (std::is_arithmetic_v<U>) {
            _writeRawFixed(value, sizeof(U));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported raw binary value type");
        }
    }

    template <typename T>
    void _writeRawFixed(const T& value, std::size_t size) {
        using U = std::remove_cvref_t<T>;
        if (size != sizeof(U)) {
            _setError(sa::ErrorCode::InvalidLength, "Raw fixed binary value width does not match its C++ type");
            return;
        }
        if constexpr (std::is_same_v<U, bool>) {
            const std::uint8_t encoded = value ? 1U : 0U;
            _appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_integral_v<U> && sizeof(U) > 1) {
            const U encoded = htobe(value);
            _appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_same_v<U, float>) {
            const auto encoded = htobe(std::bit_cast<std::uint32_t>(value));
            _appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_same_v<U, double>) {
            const auto encoded = htobe(std::bit_cast<std::uint64_t>(value));
            _appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_floating_point_v<U>) {
            _setError(sa::ErrorCode::InvalidType, "Raw fixed binary supports only IEEE-754 float and double");
        } else {
            _appendBytes(&value, sizeof(value));
        }
    }

    template <typename T>
    void _writeFloating(const T& value) {
        using U = std::remove_cvref_t<T>;
        static_assert(std::numeric_limits<U>::is_iec559, "Binary floating-point requires IEEE-754");
        if constexpr (std::is_same_v<U, float>) {
            _pushByte(ValueTag::Float32);
            const auto encoded = htobe(std::bit_cast<std::uint32_t>(value));
            _appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_same_v<U, double>) {
            _pushByte(ValueTag::Float64);
            const auto encoded = htobe(std::bit_cast<std::uint64_t>(value));
            _appendBytes(&encoded, sizeof(encoded));
        } else {
            static_assert(std::is_same_v<U, void>, "Binary V2 supports only IEEE-754 float and double");
        }
    }

    template <typename U>
    static consteval ValueTag _fixedTag() {
        if constexpr (std::is_signed_v<U>) {
            if constexpr (sizeof(U) == 1) return ValueTag::FixedSigned8;
            else if constexpr (sizeof(U) == 2) return ValueTag::FixedSigned16;
            else if constexpr (sizeof(U) == 4) return ValueTag::FixedSigned32;
            else if constexpr (sizeof(U) == 8) return ValueTag::FixedSigned64;
            else static_assert(std::is_same_v<U, void>, "Unsupported fixed-width integer type");
        } else {
            if constexpr (sizeof(U) == 1) return ValueTag::FixedUnsigned8;
            else if constexpr (sizeof(U) == 2) return ValueTag::FixedUnsigned16;
            else if constexpr (sizeof(U) == 4) return ValueTag::FixedUnsigned32;
            else if constexpr (sizeof(U) == 8) return ValueTag::FixedUnsigned64;
            else static_assert(std::is_same_v<U, void>, "Unsupported fixed-width integer type");
        }
    }

    template <typename UInt>
    void _writeUleb128(UInt value) {
        static_assert(std::is_unsigned_v<UInt>);
        do {
            auto byte = static_cast<std::uint8_t>(value & static_cast<UInt>(0x7FU));
            value >>= 7U;
            if (value != 0) byte |= 0x80U;
            _pushByte(byte);
        } while (value != 0);
    }

    void _setError(sa::ErrorCode code, std::string message) noexcept {
        if (!mError) {
            mError = sa::error(code, std::move(message));
        }
    }

    void _pushByte(ValueTag tag) { _pushByte(static_cast<std::uint8_t>(tag)); }
    void _pushByte(std::uint8_t byte) { mBuffer.push_back(static_cast<typename BufferT::value_type>(byte)); }
    void _appendBytes(const void* data, std::size_t size) {
        if (size == 0) return;
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        mBuffer.reserve(mBuffer.size() + size);
        for (std::size_t ix = 0; ix < size; ++ix) _pushByte(bytes[ix]);
    }

private:
    BufferT& mBuffer;
    bool mDocumentStarted = false;
    bool mRawRoot = false;
    std::optional<sa::Error> mError;
};

} // namespace binary
NEKO_END_NAMESPACE
