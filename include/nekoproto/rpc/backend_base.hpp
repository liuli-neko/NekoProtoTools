#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <tuple>
#include <utility>
#include <vector>

#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

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

enum class NekoRpcMethodState : std::uint8_t {
    Active  = 0,
    Removed = 1,
};

enum class NekoRpcMethodIdResolveStatus : std::uint8_t {
    Ok = 0,
    TableOutdated,
    MethodNotFound,
    MethodRemoved,
    SignatureMismatch,
};

enum class NekoRpcCompressionAlgorithm : std::uint8_t {
    None      = 0,
    RunLength = 1,
};

enum class NekoRpcExtensionType : std::uint16_t {
    // Client Hello advertises MethodId support. Server Hello includes this only when MethodId is enabled for the
    // connection; later request/notify frames may then set NekoRpcFlag::MethodId and put an 8-byte id in `method`.
    MethodId = 1U,
    // Server Hello/table refresh publishes the current method table version.
    // MethodId request frames echo this value so the server can reject stale ids.
    MethodTableVersion = 2U,
    // Server Hello sends a complete method-id table when a client first negotiates MethodId.
    MethodTable = 3U,
    // Server Hello/table refresh sends incremental method-id updates after negotiation.
    MethodTableDelta = 4U,
    // MethodId request frames carry the resolved method signature hash.
    // The server uses it to detect stale or incompatible client-side method metadata.
    MethodSignatureHash = 5U,
    // Server Hello/table refresh tells clients which older table versions are still accepted.
    MethodMinimumCompatibleVersion = 6U,
    // Client Hello advertises compression support. Server Hello includes this only when compression is enabled for the
    // connection; later non-Hello frames may then set NekoRpcFlag::Compressed.
    Compression = 16U,
    // Hello frames name the compression algorithm supported by the client or selected by the server.
    CompressionAlgorithm = 17U,
    // Hello frames carry the smallest payload size worth compressing for this connection.
    CompressionMinPayloadSize = 18U,
};

struct NekoRpcCompressionStats {
    std::atomic<std::uint64_t> compressionAttempts{0};
    std::atomic<std::uint64_t> compressedFrames{0};
    std::atomic<std::uint64_t> decompressedFrames{0};
    std::atomic<std::uint64_t> compressionInputBytes{0};
    std::atomic<std::uint64_t> compressionOutputBytes{0};
    std::atomic<std::uint64_t> decompressionInputBytes{0};
    std::atomic<std::uint64_t> decompressionOutputBytes{0};
    std::atomic<std::uint64_t> skippedSmallPayloads{0};
    std::atomic<std::uint64_t> skippedIneffectiveFrames{0};
    std::atomic<std::uint64_t> compressionErrors{0};
    std::atomic<std::uint64_t> decompressionErrors{0};
};

struct NekoRpcMethodEntry {
    std::uint64_t id = 0;
    std::string name;
    std::uint64_t signatureHash = 0;
    NekoRpcMethodState state    = NekoRpcMethodState::Active;
};

struct NekoRpcMethodIdResolution {
    NekoRpcMethodIdResolveStatus status = NekoRpcMethodIdResolveStatus::MethodNotFound;
    const NekoRpcMethodEntry* entry     = nullptr;
    std::uint64_t currentVersion        = 0;
};

class NEKO_PROTO_API NekoRpcFrameCodec {
public:
    using Id             = std::uint64_t;
    using Message        = std::vector<char>;
    using ResponseValues = std::vector<Message>;
    using ExtensionType  = NekoRpcExtensionType;
    using ExtensionValue = std::span<const std::byte>;
    using ExtensionMap   = std::map<ExtensionType, ExtensionValue>;

    static constexpr std::uint16_t Magic  = 0x4E52U;
    static constexpr std::uint8_t Version = 1U;

    struct Header {
        // Protocol sentinel, encoded on the wire as "NR".
        std::uint16_t magic         = Magic;
        // Wire format version.
        std::uint8_t version        = Version;
        // Request, response, notification, cancel, or hello frame.
        NekoRpcKind kind            = NekoRpcKind::Request;
        // NekoRpcFlag bitset describing optional frame features.
        std::uint8_t flags          = 0;
        // Serializer/backend codec id expected by the endpoint.
        std::uint8_t codec          = 0;
        // Bytes of TLV extensions between method and payload.
        std::uint16_t extensionSize = 0;
        // Correlation id for request/response pairs.
        Id id                       = 0;
        // Bytes of method name, or method-id bytes when MethodId is set.
        std::uint32_t methodSize    = 0;
        // Bytes of serialized payload following method and extensions.
        std::uint32_t payloadSize   = 0;

        NEKO_SERIALIZER(magic, version, kind, flags, codec, extensionSize, id, methodSize, payloadSize)
    };

    struct DecodedRequest {
        Header header;
        std::string method;
        Message payload;
    };

    struct DecodeResult {
        bool ok    = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
    };

    struct FrameParts {
        // Parsed or to-be-encoded fixed header.
        Header header;
        // Method name bytes, or method-id bytes when MethodId is set.
        std::span<const std::byte> method;
        // TLV extensions keyed by protocol extension; values point into the frame or caller-owned storage.
        ExtensionMap extensions;
        // Serialized RPC payload bytes.
        std::span<const std::byte> payload;
    };

private:
    template <typename T>
    static consteval auto _wireFieldSize() -> std::size_t {
        using Value = std::remove_cvref_t<T>;
        if constexpr (std::is_enum_v<Value>) {
            return sizeof(std::underlying_type_t<Value>);
        } else if constexpr (std::is_integral_v<Value>) {
            return sizeof(Value);
        } else {
            static_assert(always_false_v<Value>, "NekoRpcFrameCodec::Header contains a non-fixed-width wire field");
        }
    }

    template <typename Tuple, std::size_t... Is>
    static consteval auto _wireTupleSize(std::index_sequence<Is...>) -> std::size_t {
        return (_wireFieldSize<std::tuple_element_t<Is, Tuple>>() + ... + 0U);
    }

    static consteval auto _headerSize() -> std::size_t {
        using Fields = typename Reflect<Header>::value_types;
        return _wireTupleSize<Fields>(std::make_index_sequence<std::tuple_size_v<Fields>>{});
    }

public:
    static auto headerSize() noexcept -> std::size_t;

    static auto decodeIncoming(std::span<const std::byte> message, std::uint8_t codec) -> DecodeResult;
    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view;
    static auto id(const DecodedRequest& request) noexcept -> const Id&;
    static auto expectsResponse(const DecodedRequest& request) noexcept -> bool;
    static auto encodeResponses(const ResponseValues& responses, bool batch) -> Message;

    static auto encodeFrame(FrameParts frame) -> Message;
    static auto encodeHello(ExtensionMap extensions, std::uint8_t codec) -> Message;
    static auto decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code>;

    static auto knownKind(NekoRpcKind kind) -> bool;
    static auto headerBodySize(std::span<const std::byte> header, std::uint8_t codec)
        -> ilias::Result<std::size_t, std::error_code>;
    static auto parseFrame(std::span<const std::byte> data, std::uint8_t codec, FrameParts& parts) -> bool;
};

class NEKO_PROTO_API NekoRpcExtensionCodec {
public:
    using Message      = NekoRpcFrameCodec::Message;
    using ExtensionType = NekoRpcFrameCodec::ExtensionType;
    using ExtensionMap = NekoRpcFrameCodec::ExtensionMap;

    static auto asBytes(const Message& message) -> std::span<const std::byte>;
    static auto copyBytes(std::span<const std::byte> bytes) -> Message;

    static auto loadTlvs(std::span<const std::byte> data, ExtensionMap& extensions) -> bool;
    static auto appendTlvs(Message& out, const ExtensionMap& extensions) -> bool;

    template <typename Int>
    static auto appendInteger(Message& out, Int value) -> void {
        using Value = std::remove_cvref_t<Int>;
        static_assert(std::is_integral_v<Value> && !std::is_same_v<Value, bool>,
                      "appendInteger requires an integer wire value");
        using Wire = std::make_unsigned_t<Value>;

        auto wire = static_cast<Wire>(value);
        for (std::size_t ix = sizeof(Wire); ix > 0U; --ix) {
            const auto shift = static_cast<unsigned>((ix - 1U) * 8U);
            out.push_back(static_cast<char>((wire >> shift) & static_cast<Wire>(0xFFU)));
        }
    }

    template <typename Int>
    static auto readInteger(std::span<const std::byte> data, std::size_t offset = 0) -> Int {
        using Value = std::remove_cvref_t<Int>;
        static_assert(std::is_integral_v<Value> && !std::is_same_v<Value, bool>,
                      "readInteger requires an integer wire value");
        using Wire = std::make_unsigned_t<Value>;

        Wire wire = 0;
        for (std::size_t ix = 0; ix < sizeof(Wire); ++ix) {
            wire = static_cast<Wire>((wire << 8U) | static_cast<Wire>(static_cast<std::uint8_t>(data[offset + ix])));
        }
        return static_cast<Value>(wire);
    }

    template <typename UInt>
    static auto integerValue(UInt value) -> Message {
        using Value = std::remove_cvref_t<UInt>;
        static_assert(std::is_integral_v<Value> && !std::is_same_v<Value, bool>,
                      "integerValue requires an integer wire value");

        Message out;
        appendInteger(out, value);
        return out;
    }

    template <typename Enum>
    static auto enumValue(Enum value) -> Message {
        static_assert(std::is_enum_v<Enum>, "enumValue requires an enum wire value");
        return integerValue(static_cast<std::underlying_type_t<Enum>>(value));
    }

    template <typename UInt>
    static auto readIntegerValue(std::span<const std::byte> data, UInt& value) -> bool {
        using Value = std::remove_cvref_t<UInt>;
        static_assert(std::is_integral_v<Value> && !std::is_same_v<Value, bool>,
                      "readIntegerValue requires an integer wire value");
        if (data.size() != sizeof(Value)) {
            return false;
        }
        value = readInteger<Value>(data);
        return true;
    }

    template <typename UInt>
    static auto readIntegerTlv(const ExtensionMap& extensions, ExtensionType type, UInt& value) -> bool {
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
    static auto readEnumTlv(const ExtensionMap& extensions, ExtensionType type, Enum& value) -> bool {
        const auto item = extensions.find(type);
        return item != extensions.end() && readEnumValue(item->second, value);
    }

};

class NEKO_PROTO_API NekoRpcMethodIdExtension {
public:
    using Message      = NekoRpcFrameCodec::Message;
    using ExtensionType = NekoRpcFrameCodec::ExtensionType;
    using ExtensionMap = NekoRpcFrameCodec::ExtensionMap;

    static constexpr std::uint64_t InitialTableVersion         = 1U;

    static auto methodTableValue(const std::vector<NekoRpcMethodEntry>& entries) -> Message;
    static auto methodTableDeltaValue(const std::vector<NekoRpcMethodEntry>& entries) -> Message;
    static auto parseMethodTable(const ExtensionMap& extensions, std::vector<NekoRpcMethodEntry>& entries) -> bool;
    static auto parseMethodTableDelta(const ExtensionMap& extensions, std::vector<NekoRpcMethodEntry>& entries) -> bool;
};

class NEKO_PROTO_API NekoRpcMethodIdTable {
public:
    static auto signatureHash(std::string_view name, std::string_view signature = {}) -> std::uint64_t;
    static auto entriesFromNames(const std::vector<std::string>& names) -> std::vector<NekoRpcMethodEntry>;

    auto reset() -> void;
    auto reset(std::vector<NekoRpcMethodEntry> entries,
               std::uint64_t version = NekoRpcMethodIdExtension::InitialTableVersion) -> void;
    auto resetFromNames(const std::vector<std::string>& names,
                        std::uint64_t version = NekoRpcMethodIdExtension::InitialTableVersion) -> void;
    auto applyRemoteTable(std::vector<NekoRpcMethodEntry> entries, std::uint64_t version) -> bool;
    auto applyRemoteDelta(const std::vector<NekoRpcMethodEntry>& entries, std::uint64_t version) -> bool;

    auto upsert(std::string name, std::uint64_t signatureHash = 0) -> const NekoRpcMethodEntry*;
    auto remove(std::string_view name) -> bool;
    auto resolve(std::uint64_t id, std::uint64_t clientVersion, std::uint64_t signatureHash,
                 bool requireSignatureHash) const -> NekoRpcMethodIdResolution;

    auto version() const noexcept -> std::uint64_t { return mVersion; }
    auto minimumCompatibleVersion() const noexcept -> std::uint64_t { return mMinimumCompatibleVersion; }
    auto setMinimumCompatibleVersion(std::uint64_t version) noexcept -> void;
    auto empty() const noexcept -> bool { return mEntries.empty(); }
    auto entries() const noexcept -> const std::vector<NekoRpcMethodEntry>& { return mEntries; }
    auto findByName(std::string_view name) const -> const NekoRpcMethodEntry*;
    auto findById(std::uint64_t id) const -> const NekoRpcMethodEntry*;

private:
    auto _bumpVersion() -> void;
    auto _rebuildIndex() -> void;
    auto _installEntry(NekoRpcMethodEntry entry) -> bool;

    std::uint64_t mVersion                  = 0;
    std::uint64_t mMinimumCompatibleVersion = 0;
    std::uint64_t mNextId                   = 0;
    std::vector<NekoRpcMethodEntry> mEntries;
    std::map<std::string, std::uint64_t, std::less<>> mNameToId;
};

class NEKO_PROTO_API NekoRpcCompressionCodec {
public:
    using Message = NekoRpcFrameCodec::Message;

    static constexpr auto preferredAlgorithm() noexcept -> NekoRpcCompressionAlgorithm {
        return NekoRpcCompressionAlgorithm::RunLength;
    }
    static constexpr bool supports(NekoRpcCompressionAlgorithm algorithm) noexcept {
        return algorithm == NekoRpcCompressionAlgorithm::RunLength;
    }

    static auto compress(std::span<const std::byte> payload, NekoRpcCompressionAlgorithm algorithm)
        -> ilias::Result<Message, std::error_code>;
    static auto decompress(std::span<const std::byte> payload, NekoRpcCompressionAlgorithm algorithm)
        -> ilias::Result<Message, std::error_code>;

private:
    static auto _compressRunLength(std::span<const std::byte> payload) -> Message;
    static auto _decompressRunLength(std::span<const std::byte> payload) -> ilias::Result<Message, std::error_code>;
};

} // namespace detail
NEKO_END_NAMESPACE
