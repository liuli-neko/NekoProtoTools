#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/rpc/private/global.hpp"
#include "nekoproto/rpc/private/method_id.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
namespace rpc {

enum class NekoRpcKind : std::uint8_t {
    Request  = 1,
    Response = 2,
    Notify   = 3,
    Cancel   = 4,
    Hello    = 5,
};

enum NekoRpcFlag : std::uint8_t {
    Error         = 1U << 0U,
    MethodId      = 1U << 1U,
    HasExtensions = 1U << 2U,
    Compressed    = 1U << 3U,
};

enum class NekoRpcFeatureMode : std::uint8_t {
    Disable = 0,
    Auto    = 1,
    Require = 2,
};

enum class NekoRpcCompressionAlgorithm : std::uint8_t {
    None      = 0,
    RunLength = 1,
};

struct NekoRpcCompressionStats {
    std::atomic<std::uint64_t> compression_attempts{0};
    std::atomic<std::uint64_t> compressed_frames{0};
    std::atomic<std::uint64_t> decompressed_frames{0};
    std::atomic<std::uint64_t> compression_input_bytes{0};
    std::atomic<std::uint64_t> compression_output_bytes{0};
    std::atomic<std::uint64_t> decompression_input_bytes{0};
    std::atomic<std::uint64_t> decompression_output_bytes{0};
    std::atomic<std::uint64_t> skipped_small_payloads{0};
    std::atomic<std::uint64_t> skipped_ineffective_frames{0};
    std::atomic<std::uint64_t> compression_errors{0};
    std::atomic<std::uint64_t> decompression_errors{0};
};

struct NekoRpcFrameLimits {
    std::size_t max_frame_bytes = 16U * 1024U * 1024U;
    std::size_t max_method_bytes = 64U * 1024U;
    std::size_t max_payload_bytes = 16U * 1024U * 1024U;
    std::size_t max_extension_bytes = 64U * 1024U;
};

class NEKO_PROTO_API NekoRpcFrameCodec {
public:
    using IdType             = std::uint64_t;
    using MessageType        = std::vector<std::byte>;
    using ResponseValuesType = std::vector<MessageType>;
    using ExtensionType      = NekoRpcExtensionType;
    using ExtensionValueType = std::span<const std::byte>;
    using ExtensionMapType   = std::map<ExtensionType, ExtensionValueType>;

    static constexpr std::uint16_t Magic  = 0x4E52U;
    static constexpr std::uint8_t Version = 1U;
    static constexpr std::size_t MaxExtensionBytes = std::numeric_limits<std::uint16_t>::max();

    struct Header {
        // Protocol sentinel, encoded on the wire as "NR".
        std::uint16_t magic = Magic;
        // Wire format version.
        std::uint8_t version = Version;
        // Request, response, notification, cancel, or hello frame.
        NekoRpcKind kind = NekoRpcKind::Request;
        // NekoRpcFlag bitset describing optional frame features.
        std::uint8_t flags = 0;
        // Serializer/backend codec id expected by the endpoint.
        std::uint8_t codec = 0;
        // Bytes of TLV extensions between method and payload.
        std::uint16_t extension_size = 0;
        // Correlation id for request/response pairs.
        IdType id = 0;
        // Bytes of method name, or method-id bytes when MethodId is set.
        std::uint32_t method_size = 0;
        // Bytes of serialized payload following method and extensions.
        std::uint32_t payload_size = 0;
    };

    struct DecodedRequest {
        Header header;
        std::string method;
        MessageType payload;
    };

    struct DecodeResult {
        bool ok    = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
        ResponseValuesType responses;
    };

    struct FrameParts {
        // Parsed or to-be-encoded fixed header.
        Header header;
        // Method name bytes, or method-id bytes when MethodId is set.
        std::span<const std::byte> method;
        // TLV extensions keyed by protocol extension; values point into the frame or caller-owned storage.
        ExtensionMapType extensions;
        // Serialized RPC payload bytes.
        std::span<const std::byte> payload;
    };

private:
    template <typename T>
    static consteval auto wire_field_size() -> std::size_t {
        using ValueType = std::remove_cvref_t<T>;
        if constexpr (std::is_enum_v<ValueType>) {
            return sizeof(std::underlying_type_t<ValueType>);
        } else if constexpr (std::is_integral_v<ValueType>) {
            return sizeof(ValueType);
        } else {
            static_assert(always_false_v<ValueType>, "NekoRpcFrameCodec::Header contains a non-fixed-width wire field");
        }
    }

    template <typename Tuple, std::size_t... Is>
    static consteval auto wire_tuple_size(std::index_sequence<Is...> /**/) -> std::size_t {
        return (wire_field_size<std::tuple_element_t<Is, Tuple>>() + ... + 0U);
    }

    static consteval auto header_size() -> std::size_t {
        using FieldsType = typename Reflect<Header>::value_types;
        return wire_tuple_size<FieldsType>(std::make_index_sequence<std::tuple_size_v<FieldsType>>{});
    }

public:
    static auto headerSize() noexcept -> std::size_t;

    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view;
    static auto id(const DecodedRequest& request) noexcept -> const IdType&;
    static auto expectsResponse(const DecodedRequest& request) noexcept -> bool;
    static auto encodeResponses(const ResponseValuesType& responses, bool batch) -> MessageType;

    static auto encodeFrame(FrameParts frame) -> MessageType;
    static auto encodeHello(ExtensionMapType extensions, std::uint8_t codec) -> MessageType;
    static auto decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code>;

    static auto knownKind(NekoRpcKind kind) -> bool;
    static auto headerBodySize(std::span<const std::byte> header, std::uint8_t codec)
        -> ilias::Result<std::size_t, std::error_code>;
    static auto headerBodySize(std::span<const std::byte> header, std::uint8_t codec,
                               const NekoRpcFrameLimits& limits)
        -> ilias::Result<std::size_t, std::error_code>;
    static auto parseFrame(std::span<const std::byte> data, std::uint8_t codec, FrameParts& parts) -> bool;
};

class NEKO_PROTO_API NekoRpcExtensionCodec {
public:
    using MessageType      = NekoRpcFrameCodec::MessageType;
    using ExtensionType    = NekoRpcFrameCodec::ExtensionType;
    using ExtensionMapType = NekoRpcFrameCodec::ExtensionMapType;

    static auto asBytes(const MessageType& message) -> std::span<const std::byte>;
    static auto copyBytes(std::span<const std::byte> bytes) -> MessageType;

    static auto loadTlvs(std::span<const std::byte> data, ExtensionMapType& extensions) -> bool;
    static auto appendTlvs(MessageType& out, const ExtensionMapType& extensions) -> bool;

    template <typename Int>
    static auto appendInteger(MessageType& out, Int value) -> void {
        using ValueType = std::remove_cvref_t<Int>;
        static_assert(std::is_integral_v<ValueType> && !std::is_same_v<ValueType, bool>,
                      "appendInteger requires an integer wire value");
        using WireType = std::make_unsigned_t<ValueType>;

        auto wire = static_cast<WireType>(value);
        for (std::size_t ix = sizeof(WireType); ix > 0U; --ix) {
            const auto shift = static_cast<unsigned>((ix - 1U) * 8U);
            out.push_back(static_cast<std::byte>((wire >> shift) & static_cast<WireType>(0xFFU)));
        }
    }

    template <typename Int>
    static auto readInteger(std::span<const std::byte> data, std::size_t offset = 0) -> Int {
        using ValueType = std::remove_cvref_t<Int>;
        static_assert(std::is_integral_v<ValueType> && !std::is_same_v<ValueType, bool>,
                      "readInteger requires an integer wire value");
        using WireType = std::make_unsigned_t<ValueType>;

        WireType wire = 0;
        for (std::size_t ix = 0; ix < sizeof(WireType); ++ix) {
            wire = static_cast<WireType>((wire << 8U) |
                                         static_cast<WireType>(static_cast<std::uint8_t>(data[offset + ix])));
        }
        return static_cast<ValueType>(wire);
    }

    template <typename UInt>
    static auto integerValue(UInt value) -> MessageType {
        using ValueType = std::remove_cvref_t<UInt>;
        static_assert(std::is_integral_v<ValueType> && !std::is_same_v<ValueType, bool>,
                      "integerValue requires an integer wire value");

        MessageType out;
        appendInteger(out, value);
        return out;
    }

    template <typename Enum>
    static auto enumValue(Enum value) -> MessageType {
        static_assert(std::is_enum_v<Enum>, "enumValue requires an enum wire value");
        return integerValue(static_cast<std::underlying_type_t<Enum>>(value));
    }

    template <typename UInt>
    static auto readIntegerValue(std::span<const std::byte> data, UInt& value) -> bool {
        using ValueType = std::remove_cvref_t<UInt>;
        static_assert(std::is_integral_v<ValueType> && !std::is_same_v<ValueType, bool>,
                      "readIntegerValue requires an integer wire value");
        if (data.size() != sizeof(ValueType)) {
            return false;
        }
        value = readInteger<ValueType>(data);
        return true;
    }

    template <typename UInt>
    static auto readIntegerTlv(const ExtensionMapType& extensions, ExtensionType type, UInt& value) -> bool {
        const auto item = extensions.find(type);
        return item != extensions.end() && readIntegerValue(item->second, value);
    }

    template <typename Enum>
    static auto readEnumValue(std::span<const std::byte> data, Enum& value) -> bool {
        static_assert(std::is_enum_v<Enum>, "readEnumValue requires an enum wire value");
        std::underlying_type_t<Enum> raw{};
        if (!readIntegerValue(data, raw)) {
            return false;
        }
        value = static_cast<Enum>(raw);
        return true;
    }

    template <typename Enum>
    static auto readEnumTlv(const ExtensionMapType& extensions, ExtensionType type, Enum& value) -> bool {
        const auto item = extensions.find(type);
        return item != extensions.end() && readEnumValue(item->second, value);
    }
};

class NEKO_PROTO_API NekoRpcMethodIdExtension {
public:
    using MessageType      = NekoRpcFrameCodec::MessageType;
    using ExtensionType    = NekoRpcFrameCodec::ExtensionType;
    using ExtensionMapType = NekoRpcFrameCodec::ExtensionMapType;

    static constexpr std::uint64_t InitialTableVersion = 1U;

    static auto methodTableValue(const std::vector<NekoRpcMethodEntry>& entries) -> MessageType;
    static auto methodTableDeltaValue(const std::vector<NekoRpcMethodEntry>& entries) -> MessageType;
    static auto parseMethodTable(const ExtensionMapType& extensions, std::vector<NekoRpcMethodEntry>& entries) -> bool;
    static auto parseMethodTableDelta(const ExtensionMapType& extensions, std::vector<NekoRpcMethodEntry>& entries)
        -> bool;
};

class NEKO_PROTO_API NekoRpcCompressionCodec {
public:
    using MessageType = NekoRpcFrameCodec::MessageType;

    static constexpr auto preferred_algorithm() noexcept -> NekoRpcCompressionAlgorithm {
        return NekoRpcCompressionAlgorithm::RunLength;
    }
    static constexpr bool supports(NekoRpcCompressionAlgorithm algorithm) noexcept {
        return algorithm == NekoRpcCompressionAlgorithm::RunLength;
    }

    static auto compress(std::span<const std::byte> payload, NekoRpcCompressionAlgorithm algorithm)
        -> ilias::Result<MessageType, std::error_code>;
    static auto decompress(std::span<const std::byte> payload, NekoRpcCompressionAlgorithm algorithm,
                           std::size_t max_output_bytes)
        -> ilias::Result<MessageType, std::error_code>;

private:
    static auto _compressRunLength(std::span<const std::byte> payload) -> MessageType;
    static auto _decompressRunLength(std::span<const std::byte> payload, std::size_t max_output_bytes)
        -> ilias::Result<MessageType, std::error_code>;
};

} // namespace rpc
template <>
struct Meta<rpc::NekoRpcKind, void> {
    static constexpr auto value =
        Enumerate("Request", rpc::NekoRpcKind::Request, "Response", rpc::NekoRpcKind::Response, "Notify",
                  rpc::NekoRpcKind::Notify, "Cancel", rpc::NekoRpcKind::Cancel, "Hello", rpc::NekoRpcKind::Hello);
};

template <>
struct Meta<rpc::NekoRpcFlag, void> {
    static constexpr auto value =
        Enumerate("Error", rpc::NekoRpcFlag::Error, "MethodId", rpc::NekoRpcFlag::MethodId, "HasExtensions",
                  rpc::NekoRpcFlag::HasExtensions, "Compressed", rpc::NekoRpcFlag::Compressed);
};

template <>
struct Meta<rpc::NekoRpcCompressionAlgorithm, void> {
    static constexpr auto value = Enumerate("None", rpc::NekoRpcCompressionAlgorithm::None, "RunLength",
                                            rpc::NekoRpcCompressionAlgorithm::RunLength);
};

NEKO_END_NAMESPACE
