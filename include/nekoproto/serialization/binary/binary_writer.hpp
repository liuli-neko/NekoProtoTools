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

namespace nekoproto {
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

inline constexpr auto fieldId(std::string_view name) noexcept -> std::uint32_t {
    std::uint32_t hash = 2166136261U;
    for (const char ch : name) {
        hash ^= static_cast<std::uint8_t>(ch);
        hash *= 16777619U;
    }
    return hash;
}

inline constexpr auto ulebSize(std::uint64_t value) noexcept -> std::size_t {
    std::size_t size = 1;
    while (value >= 0x80U) {
        value >>= 7U;
        ++size;
    }
    return size;
}

inline constexpr auto useHashedFieldId(std::string_view name) noexcept -> bool {
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
        ContainerScope(Writer& writer, std::size_t expected) noexcept : writer_(&writer), expected_(expected) {}

        ContainerScope(const ContainerScope&)                    = delete;
        auto operator=(const ContainerScope&) -> ContainerScope& = delete;
        ContainerScope(ContainerScope&& other) noexcept { moveFrom(other); }
        auto operator=(ContainerScope&& other) noexcept -> ContainerScope& {
            if (this != &other) {
                finish();
                moveFrom(other);
            }
            return *this;
        }
        ~ContainerScope() { finish(); }

    private:
        friend class Writer;

        void increment() noexcept { ++actual_; }

        auto rememberId(std::uint32_t id) -> bool {
            if constexpr (Kind == ContainerKind::IdObject) {
                return ids_.insert(id).second;
            }
            return true;
        }

        void finish() noexcept {
            if (writer_ != nullptr && actual_ != expected_) {
                writer_->setError(sa::ErrorCode::InvalidLength,
                                   "Binary container emitted member count does not match its declared count");
            }
            writer_ = nullptr;
        }

        void moveFrom(ContainerScope& other) noexcept {
            writer_   = std::exchange(other.writer_, nullptr);
            expected_ = other.expected_;
            actual_   = other.actual_;
            ids_      = std::move(other.ids_);
        }

        Writer*                           writer_   = nullptr;
        std::size_t                       expected_ = 0;
        std::size_t                       actual_   = 0;
        std::unordered_set<std::uint32_t> ids_;
    };

public:
    using OutputArrayType  = ContainerScope<ContainerKind::Array>;
    using OutputObjectType = ContainerScope<ContainerKind::NamedObject>;
    using OutputIdObjectType = ContainerScope<ContainerKind::IdObject>;
    struct OutputValueType {};

    explicit Writer(BufferT& buffer) noexcept : buffer_(buffer) {}

    void beginRawFixedDataAsRoot() noexcept { raw_root_ = true; }

    auto arrayAsRoot(std::size_t size) -> OutputArrayType {
        writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }
    auto objectAsRoot(std::size_t size) -> OutputObjectType {
        writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }
    auto idObjectAsRoot(std::size_t size) -> OutputIdObjectType {
        writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }

    auto nullAsRoot() -> OutputValueType {
        if (raw_root_) {
            setError(sa::ErrorCode::InvalidType, "raw_fixed_data binary values cannot encode null");
        } else {
            ensureDocumentHeader();
            pushByte(ValueTag::Null);
        }
        return {};
    }

    template <typename T>
    auto valueAsRoot(const T& value) -> OutputValueType {
        raw_root_ ? writeRawValue(value) : writeValue(value);
        return {};
    }

    template <typename T>
    auto fixedValueAsRoot(const T& value, std::size_t size) -> OutputValueType {
        raw_root_ ? writeRawFixed(value, size) : writeFixed(value, size);
        return {};
    }

    auto addArrayToArray(std::size_t size, OutputArrayType* parent) -> OutputArrayType {
        increment(parent);
        writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }
    auto addArrayToObject(std::string_view name, std::size_t size, OutputObjectType* parent) -> OutputArrayType {
        beginNamedField(name, parent);
        writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }
    auto addArrayToObject(std::string_view name, std::size_t size, OutputIdObjectType* parent) -> OutputArrayType {
        beginIdField(name, parent);
        writeContainerHeader(ValueTag::Array, size);
        return {*this, size};
    }

    auto addObjectToArray(std::size_t size, OutputArrayType* parent) -> OutputObjectType {
        increment(parent);
        writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }
    auto addObjectToObject(std::string_view name, std::size_t size, OutputObjectType* parent) -> OutputObjectType {
        beginNamedField(name, parent);
        writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }
    auto addObjectToObject(std::string_view name, std::size_t size, OutputIdObjectType* parent) -> OutputObjectType {
        beginIdField(name, parent);
        writeContainerHeader(ValueTag::NamedObject, size);
        return {*this, size};
    }

    auto addIdObjectToArray(std::size_t size, OutputArrayType* parent) -> OutputIdObjectType {
        increment(parent);
        writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }
    auto addIdObjectToObject(std::string_view name, std::size_t size, OutputObjectType* parent) -> OutputIdObjectType {
        beginNamedField(name, parent);
        writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }
    auto addIdObjectToObject(std::string_view name, std::size_t size, OutputIdObjectType* parent) -> OutputIdObjectType {
        beginIdField(name, parent);
        writeContainerHeader(ValueTag::IdObject, size);
        return {*this, size};
    }

    template <typename T>
    auto addValueToArray(const T& value, OutputArrayType* parent) -> OutputValueType {
        increment(parent);
        writeValue(value);
        return {};
    }
    template <typename T>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) -> OutputValueType {
        beginNamedField(name, parent);
        writeValue(value);
        return {};
    }
    template <typename T>
    auto addValueToObject(std::string_view name, const T& value, OutputIdObjectType* parent) -> OutputValueType {
        beginIdField(name, parent);
        writeValue(value);
        return {};
    }

    template <typename T>
    auto addFixedValueToArray(const T& value, std::size_t size, OutputArrayType* parent) -> OutputValueType {
        increment(parent);
        writeFixed(value, size);
        return {};
    }
    template <typename T>
    auto addFixedValueToObject(std::string_view name, const T& value, std::size_t size,
                                          OutputObjectType* parent) -> OutputValueType {
        beginNamedField(name, parent);
        writeFixed(value, size);
        return {};
    }
    template <typename T>
    auto addFixedValueToObject(std::string_view name, const T& value, std::size_t size,
                                          OutputIdObjectType* parent) -> OutputValueType {
        beginIdField(name, parent);
        writeFixed(value, size);
        return {};
    }

    auto addNullToArray(OutputArrayType* parent) -> OutputValueType {
        increment(parent);
        ensureDocumentHeader();
        pushByte(ValueTag::Null);
        return {};
    }
    auto addNullToObject(std::string_view name, OutputObjectType* parent) -> OutputValueType {
        beginNamedField(name, parent);
        pushByte(ValueTag::Null);
        return {};
    }
    auto addNullToObject(std::string_view name, OutputIdObjectType* parent) -> OutputValueType {
        beginIdField(name, parent);
        pushByte(ValueTag::Null);
        return {};
    }

    auto result() const -> sa::Result<void> {
        if (error_) {
            return *error_;
        }
        return sa::success();
    }
    auto size() const noexcept -> std::size_t { return buffer_.size(); }

private:
    template <ContainerKind Kind>
    static void increment(ContainerScope<Kind>* parent) noexcept {
        if (parent != nullptr) {
            parent->increment();
        }
    }

    void writeContainerHeader(ValueTag tag, std::size_t size) {
        ensureDocumentHeader();
        pushByte(tag);
        writeUleb128(static_cast<std::uint64_t>(size));
    }

    void beginNamedField(std::string_view name, OutputObjectType* parent) {
        increment(parent);
        writeUleb128(static_cast<std::uint64_t>(name.size()));
        appendBytes(name.data(), name.size());
    }

    void beginIdField(std::string_view name, OutputIdObjectType* parent) {
        increment(parent);
        if (!useHashedFieldId(name)) {
            writeUleb128(static_cast<std::uint64_t>(name.size()) << 1U);
            appendBytes(name.data(), name.size());
            return;
        }
        const auto id = fieldId(name);
        if (parent != nullptr && !parent->rememberId(id)) {
            setError(sa::ErrorCode::InvalidField, "Reflected binary object contains colliding hashed field ids");
        }
        writeUleb128((static_cast<std::uint64_t>(id) << 1U) | 1U);
    }

    void ensureDocumentHeader() {
        if (document_started_ || raw_root_) {
            return;
        }
        document_started_ = true;
        appendBytes(BinaryMagic, sizeof(BinaryMagic));
    }

    template <typename T>
    void writeValue(const T& value) {
        using U = std::remove_cvref_t<T>;
        ensureDocumentHeader();
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            pushByte(ValueTag::String);
            writeUleb128(static_cast<std::uint64_t>(value.size()));
            appendBytes(value.data(), value.size());
        } else if constexpr (std::is_same_v<U, bool>) {
            pushByte(value ? ValueTag::True : ValueTag::False);
        } else if constexpr (std::is_integral_v<U>) {
            if constexpr (std::is_signed_v<U>) {
                using Unsigned = std::make_unsigned_t<U>;
                const bool negative = value < 0;
                const auto magnitude = negative ? static_cast<Unsigned>(-(value + 1)) + Unsigned{1}
                                                : static_cast<Unsigned>(value);
                const auto zigzag = static_cast<Unsigned>((magnitude << 1U) - (negative ? Unsigned{1} : Unsigned{0}));
                pushByte(ValueTag::SignedInteger);
                writeUleb128(zigzag);
            } else {
                pushByte(ValueTag::UnsignedInteger);
                writeUleb128(value);
            }
        } else if constexpr (std::is_floating_point_v<U>) {
            writeFloating(value);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported binary value type");
        }
    }

    template <typename T>
    void writeFixed(const T& value, std::size_t size) {
        using U = std::remove_cvref_t<T>;
        ensureDocumentHeader();
        if (size != sizeof(U)) {
            setError(sa::ErrorCode::InvalidLength, "Fixed binary value width does not match its C++ type");
            return;
        }
        if constexpr (std::is_same_v<U, bool>) {
            pushByte(value ? ValueTag::True : ValueTag::False);
        } else if constexpr (std::is_integral_v<U>) {
            pushByte(fixedTag<U>());
            writeRawFixed(value, size);
        } else if constexpr (std::is_floating_point_v<U>) {
            writeFloating(value);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported fixed binary value type");
        }
    }

    template <typename T>
    void writeRawValue(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            writeUleb128(static_cast<std::uint64_t>(value.size()));
            appendBytes(value.data(), value.size());
        } else if constexpr (std::is_arithmetic_v<U>) {
            writeRawFixed(value, sizeof(U));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported raw binary value type");
        }
    }

    template <typename T>
    void writeRawFixed(const T& value, std::size_t size) {
        using U = std::remove_cvref_t<T>;
        if (size != sizeof(U)) {
            setError(sa::ErrorCode::InvalidLength, "Raw fixed binary value width does not match its C++ type");
            return;
        }
        if constexpr (std::is_same_v<U, bool>) {
            const std::uint8_t encoded = value ? 1U : 0U;
            appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_integral_v<U> && sizeof(U) > 1) {
            const U encoded = htobe(value);
            appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_same_v<U, float>) {
            const auto encoded = htobe(std::bit_cast<std::uint32_t>(value));
            appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_same_v<U, double>) {
            const auto encoded = htobe(std::bit_cast<std::uint64_t>(value));
            appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_floating_point_v<U>) {
            setError(sa::ErrorCode::InvalidType, "Raw fixed binary supports only IEEE-754 float and double");
        } else {
            appendBytes(&value, sizeof(value));
        }
    }

    template <typename T>
    void writeFloating(const T& value) {
        using U = std::remove_cvref_t<T>;
        static_assert(std::numeric_limits<U>::is_iec559, "Binary floating-point requires IEEE-754");
        if constexpr (std::is_same_v<U, float>) {
            pushByte(ValueTag::Float32);
            const auto encoded = htobe(std::bit_cast<std::uint32_t>(value));
            appendBytes(&encoded, sizeof(encoded));
        } else if constexpr (std::is_same_v<U, double>) {
            pushByte(ValueTag::Float64);
            const auto encoded = htobe(std::bit_cast<std::uint64_t>(value));
            appendBytes(&encoded, sizeof(encoded));
        } else {
            static_assert(std::is_same_v<U, void>, "Binary V2 supports only IEEE-754 float and double");
        }
    }

    template <typename U>
    static consteval auto fixedTag() -> ValueTag {
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
    void writeUleb128(UInt value) {
        static_assert(std::is_unsigned_v<UInt>);
        do {
            auto byte = static_cast<std::uint8_t>(value & static_cast<UInt>(0x7FU));
            value >>= 7U;
            if (value != 0) byte |= 0x80U;
            pushByte(byte);
        } while (value != 0);
    }

    void setError(sa::ErrorCode code, std::string message) noexcept {
        if (!error_) {
            error_ = sa::error(code, std::move(message));
        }
    }

    void pushByte(ValueTag tag) { pushByte(static_cast<std::uint8_t>(tag)); }
    void pushByte(std::uint8_t byte) { buffer_.push_back(static_cast<typename BufferT::value_type>(byte)); }
    void appendBytes(const void* data, std::size_t size) {
        if (size == 0) return;
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        buffer_.reserve(buffer_.size() + size);
        for (std::size_t ix = 0; ix < size; ++ix) pushByte(bytes[ix]);
    }

private:
    BufferT&                 buffer_;
    bool                     document_started_ = false;
    bool                     raw_root_         = false;
    std::optional<sa::Error> error_;
};

} // namespace binary
} // namespace nekoproto
