#pragma once

#include <cstddef>
#include <cstdint>
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
    Request = 1,
    Response = 2,
    Notify = 3,
    Cancel = 4,
    Hello = 5,
};

enum NekoRpcFlag : std::uint8_t {
    Error = 1U << 0U,
    MethodId = 1U << 1U,
    HasExtensions = 1U << 2U,
};

class NEKO_PROTO_API NekoRpcFrameCodec {
public:
    using Id = std::uint64_t;
    using Message = std::vector<char>;
    using ResponseValues = std::vector<Message>;

    struct Header {
        std::uint16_t magic = Magic;
        std::uint8_t version = Version;
        NekoRpcKind kind = NekoRpcKind::Request;
        std::uint8_t flags = 0;
        std::uint8_t codec = 0;
        std::uint16_t extensionSize = 0;
        Id id = 0;
        std::uint32_t methodSize = 0;
        std::uint32_t payloadSize = 0;
    };

    struct DecodedRequest {
        Header header;
        std::string method;
        Message payload;
    };

    struct DecodeResult {
        bool ok = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
    };

    static constexpr std::uint16_t Magic = 0x4E52U;
    static constexpr std::uint8_t Version = 1U;
    static constexpr std::size_t HeaderSize = 24U;

    static auto decodeIncoming(std::span<const std::byte> message, std::uint8_t codec) -> DecodeResult;
    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view;
    static auto id(const DecodedRequest& request) noexcept -> const Id&;
    static auto expectsResponse(const DecodedRequest& request) noexcept -> bool;
    static auto encodeResponses(const ResponseValues& responses, bool batch) -> Message;

    static auto encodeFrame(NekoRpcKind kind, Id id, std::string_view method, const Message& payload,
                            std::uint8_t flags, std::uint8_t codec) -> Message;
    static auto decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code>;

    static auto knownKind(NekoRpcKind kind) -> bool;
    static auto headerBodySize(std::span<const std::byte> header, std::uint8_t codec)
        -> ilias::Result<std::size_t, std::error_code>;

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

} // namespace detail
NEKO_END_NAMESPACE
