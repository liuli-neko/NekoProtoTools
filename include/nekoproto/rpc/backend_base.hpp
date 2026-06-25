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
#include <vector>

#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"

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

    struct Header {
        std::uint16_t magic         = Magic;
        std::uint8_t version        = Version;
        NekoRpcKind kind            = NekoRpcKind::Request;
        std::uint8_t flags          = 0;
        std::uint8_t codec          = 0;
        std::uint16_t extensionSize = 0;
        Id id                       = 0;
        std::uint32_t methodSize    = 0;
        std::uint32_t payloadSize   = 0;
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
        Header header;
        std::span<const std::byte> method;
        std::span<const std::byte> extensions;
        std::span<const std::byte> payload;
    };

    static constexpr std::uint16_t Magic    = 0x4E52U;
    static constexpr std::uint8_t Version   = 1U;
    static constexpr std::size_t HeaderSize = 24U;

    static auto decodeIncoming(std::span<const std::byte> message, std::uint8_t codec) -> DecodeResult;
    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view;
    static auto id(const DecodedRequest& request) noexcept -> const Id&;
    static auto expectsResponse(const DecodedRequest& request) noexcept -> bool;
    static auto encodeResponses(const ResponseValues& responses, bool batch) -> Message;

    static auto encodeFrame(NekoRpcKind kind, Id id, std::string_view method, const Message& payload,
                            std::uint8_t flags, std::uint8_t codec) -> Message;
    static auto encodeFrame(NekoRpcKind kind, Id id, std::string_view method, std::span<const std::byte> extensions,
                            std::span<const std::byte> payload, std::uint8_t flags, std::uint8_t codec) -> Message;
    static auto encodeHello(std::span<const std::byte> extensions, std::uint8_t codec) -> Message;
    static auto decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code>;

    static auto knownKind(NekoRpcKind kind) -> bool;
    static auto headerBodySize(std::span<const std::byte> header, std::uint8_t codec)
        -> ilias::Result<std::size_t, std::error_code>;
    static auto parseFrame(std::span<const std::byte> data, std::uint8_t codec, FrameParts& parts) -> bool;

private:
    static auto _byteAt(std::span<const std::byte> data, std::size_t offset) -> std::uint8_t;
    static auto _readU16(std::span<const std::byte> data, std::size_t offset) -> std::uint16_t;
    static auto _readU32(std::span<const std::byte> data, std::size_t offset) -> std::uint32_t;
    static auto _readU64(std::span<const std::byte> data, std::size_t offset) -> std::uint64_t;
    static auto _extensionsSupported(std::span<const std::byte> extensions) -> bool;

    static auto _appendU8(Message& out, std::uint8_t value) -> void;
    static auto _appendU16(Message& out, std::uint16_t value) -> void;
    static auto _appendU32(Message& out, std::uint32_t value) -> void;
    static auto _appendU64(Message& out, std::uint64_t value) -> void;
};

class NEKO_PROTO_API NekoRpcExtensionCodec {
public:
    using Message = NekoRpcFrameCodec::Message;

    static auto asBytes(const Message& message) -> std::span<const std::byte>;
    static auto copyBytes(std::span<const std::byte> bytes) -> Message;

    static auto appendTlv(Message& out, std::uint16_t type, std::span<const std::byte> value) -> bool;
    static auto makeTlv(std::uint16_t type, std::span<const std::byte> value) -> Message;
    static auto findTlv(std::span<const std::byte> extensions, std::uint16_t expected, std::span<const std::byte>& out)
        -> bool;
    static auto withoutTlv(std::span<const std::byte> extensions, std::uint16_t skippedType)
        -> ilias::Result<Message, std::error_code>;

    static auto featureModeTlv(std::uint16_t type, NekoRpcFeatureMode mode) -> Message;
    static auto parseFeatureMode(std::span<const std::byte> extensions, std::uint16_t type) -> NekoRpcFeatureMode;
    static auto u64Tlv(std::uint16_t type, std::uint64_t value) -> Message;
    static auto readU64Tlv(std::span<const std::byte> extensions, std::uint16_t type, std::uint64_t& value) -> bool;

    static auto appendU8(Message& out, std::uint8_t value) -> void;
    static auto appendU16(Message& out, std::uint16_t value) -> void;
    static auto appendU32(Message& out, std::uint32_t value) -> void;
    static auto appendU64(Message& out, std::uint64_t value) -> void;
    static auto byteAt(std::span<const std::byte> data, std::size_t offset) -> std::uint8_t;
    static auto readU16(std::span<const std::byte> data, std::size_t offset) -> std::uint16_t;
    static auto readU32(std::span<const std::byte> data, std::size_t offset) -> std::uint32_t;
    static auto readU64(std::span<const std::byte> data, std::size_t offset) -> std::uint64_t;
};

class NEKO_PROTO_API NekoRpcMethodIdExtension {
public:
    using Message = NekoRpcFrameCodec::Message;

    static constexpr std::uint16_t ModeTlv                     = 1U;
    static constexpr std::uint16_t TableVersionTlv             = 2U;
    static constexpr std::uint16_t TableTlv                    = 3U;
    static constexpr std::uint16_t TableDeltaTlv               = 4U;
    static constexpr std::uint16_t SignatureHashTlv            = 5U;
    static constexpr std::uint16_t MinimumCompatibleVersionTlv = 6U;
    static constexpr std::uint64_t InitialTableVersion         = 1U;

    static auto modeTlv(NekoRpcFeatureMode mode) -> Message;
    static auto parseMode(std::span<const std::byte> extensions) -> NekoRpcFeatureMode;
    static auto tableVersionTlv(std::uint64_t version) -> Message;
    static auto readTableVersion(std::span<const std::byte> extensions, std::uint64_t& version) -> bool;
    static auto minimumCompatibleVersionTlv(std::uint64_t version) -> Message;
    static auto readMinimumCompatibleVersion(std::span<const std::byte> extensions, std::uint64_t& version) -> bool;
    static auto signatureHashTlv(std::uint64_t hash) -> Message;
    static auto readSignatureHash(std::span<const std::byte> extensions, std::uint64_t& hash) -> bool;
    static auto methodTableTlv(const std::vector<NekoRpcMethodEntry>& entries) -> Message;
    static auto methodTableTlv(const std::vector<std::string>& names) -> Message;
    static auto methodTableDeltaTlv(const std::vector<NekoRpcMethodEntry>& entries) -> Message;
    static auto parseMethodTable(std::span<const std::byte> extensions, std::vector<NekoRpcMethodEntry>& entries)
        -> bool;
    static auto parseMethodTableDelta(std::span<const std::byte> extensions, std::vector<NekoRpcMethodEntry>& entries)
        -> bool;
    static auto parseMethodTable(std::span<const std::byte> extensions, std::vector<std::string>& names,
                                 std::map<std::string, std::uint64_t>& ids) -> bool;
};

class NEKO_PROTO_API NekoRpcMethodIdTable {
public:
    static auto signatureHash(std::string_view name, std::string_view description = {}) -> std::uint64_t;
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

class NEKO_PROTO_API NekoRpcCompressionExtension {
public:
    using Message = NekoRpcFrameCodec::Message;

    static constexpr std::uint16_t ModeTlv           = 16U;
    static constexpr std::uint16_t AlgorithmTlv      = 17U;
    static constexpr std::uint16_t MinPayloadSizeTlv = 18U;

    static auto modeTlv(NekoRpcFeatureMode mode) -> Message;
    static auto parseMode(std::span<const std::byte> extensions) -> NekoRpcFeatureMode;
    static auto algorithmTlv(NekoRpcCompressionAlgorithm algorithm) -> Message;
    static auto parseAlgorithm(std::span<const std::byte> extensions) -> NekoRpcCompressionAlgorithm;
    static auto minPayloadSizeTlv(std::uint32_t size) -> Message;
    static auto readMinPayloadSize(std::span<const std::byte> extensions, std::uint32_t& size) -> bool;
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
