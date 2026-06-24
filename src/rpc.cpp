#include "nekoproto/rpc/backend_base.hpp"
#include "nekoproto/rpc/error.hpp"

#include <cstring>
#include <limits>

#include <ilias/io/error.hpp>

NEKO_BEGIN_NAMESPACE
namespace detail {

auto NekoRpcFrameCodec::decodeIncoming(std::span<const std::byte> message, std::uint8_t codec) -> DecodeResult {
    DecodeResult result;
    auto frame = decodeFrame(message);
    if (!frame || frame.value().header.codec != codec) {
        return result;
    }

    auto& request = frame.value();
    if (request.header.kind == NekoRpcKind::Cancel || request.header.kind == NekoRpcKind::Hello) {
        result.ok = true;
        return result;
    }
    if (request.header.kind != NekoRpcKind::Request && request.header.kind != NekoRpcKind::Notify) {
        return result;
    }
    if ((request.header.flags & NekoRpcFlag::Error) != 0U || (request.header.flags & NekoRpcFlag::MethodId) != 0U ||
        request.method.empty()) {
        return result;
    }

    result.ok = true;
    result.requests.emplace_back(std::move(request));
    return result;
}

auto NekoRpcFrameCodec::methodName(const DecodedRequest& request) noexcept -> std::string_view {
    return request.method;
}

auto NekoRpcFrameCodec::id(const DecodedRequest& request) noexcept -> const Id& {
    return request.header.id;
}

auto NekoRpcFrameCodec::expectsResponse(const DecodedRequest& request) noexcept -> bool {
    return request.header.kind == NekoRpcKind::Request;
}

auto NekoRpcFrameCodec::encodeResponses(const ResponseValues& responses, bool /*batch*/) -> Message {
    if (responses.empty()) {
        return {};
    }
    return responses.front();
}

auto NekoRpcFrameCodec::encodeFrame(NekoRpcKind kind, Id id, std::string_view method, const Message& payload,
                                    std::uint8_t flags, std::uint8_t codec) -> Message {
    Message out;
    const auto methodSize = static_cast<std::uint32_t>(method.size());
    const auto payloadSize = static_cast<std::uint32_t>(payload.size());
    out.reserve(HeaderSize + method.size() + payload.size());
    _appendU16(out, Magic);
    _appendU8(out, Version);
    _appendU8(out, static_cast<std::uint8_t>(kind));
    _appendU8(out, flags);
    _appendU8(out, codec);
    _appendU16(out, 0);
    _appendU64(out, id);
    _appendU32(out, methodSize);
    _appendU32(out, payloadSize);
    out.insert(out.end(), method.begin(), method.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

auto NekoRpcFrameCodec::decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code> {
    if (data.size() < HeaderSize) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    Header header;
    header.magic = _readU16(data, 0);
    header.version = _byteAt(data, 2);
    header.kind = static_cast<NekoRpcKind>(_byteAt(data, 3));
    header.flags = _byteAt(data, 4);
    header.codec = _byteAt(data, 5);
    header.extensionSize = _readU16(data, 6);
    header.id = _readU64(data, 8);
    header.methodSize = _readU32(data, 16);
    header.payloadSize = _readU32(data, 20);

    if (header.magic != Magic || header.version != Version) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    if (!knownKind(header.kind)) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    const auto methodSize = static_cast<std::size_t>(header.methodSize);
    const auto extensionSize = static_cast<std::size_t>(header.extensionSize);
    const auto payloadSize = static_cast<std::size_t>(header.payloadSize);
    const auto maxSize = std::numeric_limits<std::size_t>::max();
    if (methodSize > maxSize - extensionSize || methodSize + extensionSize > maxSize - payloadSize ||
        methodSize + extensionSize + payloadSize > maxSize - HeaderSize) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    const auto expectedSize = HeaderSize + methodSize + extensionSize + payloadSize;
    if (expectedSize != data.size()) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    const auto methodBegin = HeaderSize;
    const auto extensionBegin = methodBegin + methodSize;
    const auto payloadBegin = extensionBegin + extensionSize;
    if (methodBegin > data.size() || methodSize > data.size() - methodBegin || extensionBegin > data.size() ||
        extensionSize > data.size() - extensionBegin || payloadBegin > data.size() ||
        payloadSize > data.size() - payloadBegin) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    const auto extensions = std::span<const std::byte>(data.data() + extensionBegin, extensionSize);
    if (!_extensionsSupported(extensions)) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    DecodedRequest request;
    request.header = header;
    request.method.assign(reinterpret_cast<const char*>(data.data() + methodBegin), methodSize);
    request.payload.resize(payloadSize);
    if (payloadSize > 0U) {
        std::memcpy(request.payload.data(), data.data() + payloadBegin, payloadSize);
    }
    return request;
}

auto NekoRpcFrameCodec::knownKind(NekoRpcKind kind) -> bool {
    switch (kind) {
    case NekoRpcKind::Request:
    case NekoRpcKind::Response:
    case NekoRpcKind::Notify:
    case NekoRpcKind::Cancel:
    case NekoRpcKind::Hello:
        return true;
    default:
        return false;
    }
}

auto NekoRpcFrameCodec::headerBodySize(std::span<const std::byte> header, std::uint8_t codec)
    -> ilias::Result<std::size_t, std::error_code> {
    if (header.size() != HeaderSize) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    if (_readU16(header, 0) != Magic || _byteAt(header, 2) != Version ||
        !knownKind(static_cast<NekoRpcKind>(_byteAt(header, 3))) || _byteAt(header, 5) != codec) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    const auto extensionSize = static_cast<std::size_t>(_readU16(header, 6));
    const auto methodSize = static_cast<std::size_t>(_readU32(header, 16));
    const auto payloadSize = static_cast<std::size_t>(_readU32(header, 20));
    const auto maxSize = std::numeric_limits<std::size_t>::max();
    if (methodSize > maxSize - extensionSize || methodSize + extensionSize > maxSize - payloadSize ||
        methodSize + extensionSize + payloadSize > maxSize - HeaderSize) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    return methodSize + extensionSize + payloadSize;
}

auto NekoRpcFrameCodec::_byteAt(std::span<const std::byte> data, std::size_t offset) -> std::uint8_t {
    return static_cast<std::uint8_t>(data[offset]);
}

auto NekoRpcFrameCodec::_readU16(std::span<const std::byte> data, std::size_t offset) -> std::uint16_t {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(_byteAt(data, offset)) << 8U) |
                                      static_cast<std::uint16_t>(_byteAt(data, offset + 1U)));
}

auto NekoRpcFrameCodec::_readU32(std::span<const std::byte> data, std::size_t offset) -> std::uint32_t {
    std::uint32_t value = 0;
    for (std::size_t ix = 0; ix < 4; ++ix) {
        value = (value << 8U) | _byteAt(data, offset + ix);
    }
    return value;
}

auto NekoRpcFrameCodec::_readU64(std::span<const std::byte> data, std::size_t offset) -> std::uint64_t {
    std::uint64_t value = 0;
    for (std::size_t ix = 0; ix < 8; ++ix) {
        value = (value << 8U) | _byteAt(data, offset + ix);
    }
    return value;
}

auto NekoRpcFrameCodec::_extensionsSupported(std::span<const std::byte> extensions) -> bool {
    std::size_t offset = 0;
    while (offset < extensions.size()) {
        if (extensions.size() - offset < 4U) {
            return false;
        }
        const auto type = _readU16(extensions, offset);
        const auto size = _readU16(extensions, offset + 2U);
        offset += 4U;
        if (extensions.size() - offset < size) {
            return false;
        }
        if ((type & 0x8000U) != 0U) {
            return false;
        }
        offset += size;
    }
    return true;
}

auto NekoRpcFrameCodec::_appendU8(Message& out, std::uint8_t value) -> void {
    out.push_back(static_cast<char>(value));
}

auto NekoRpcFrameCodec::_appendU16(Message& out, std::uint16_t value) -> void {
    out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<char>(value & 0xFFU));
}

auto NekoRpcFrameCodec::_appendU32(Message& out, std::uint32_t value) -> void {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU));
    }
}

auto NekoRpcFrameCodec::_appendU64(Message& out, std::uint64_t value) -> void {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU));
    }
}

} // namespace detail

RpcErrorCategory& RpcErrorCategory::instance() {
    static RpcErrorCategory kInstance;
    return kInstance;
}

auto RpcErrorCategory::message(int value) const noexcept -> std::string {
    switch (static_cast<RpcError>(value)) {
    case RpcError::Ok:
        return "Ok";
    case RpcError::MethodNotBind:
        return "Method not Bind";
    case RpcError::ClientNotInit:
        return "Client Not Init";
    case RpcError::ResponseIdNotMatch:
        return "Response Id Not Match";
    case RpcError::InvalidRequest:
        return "Invalid Request";
    case RpcError::MethodNotFound:
        return "Method Not Found";
    case RpcError::InvalidParams:
        return "Invalid Params";
    case RpcError::InternalError:
        return "Internal Error";
    default:
        return "Unknown RPC Error";
    }
}

auto RpcErrorCategory::name() const noexcept -> const char* {
    return "rpc";
}

auto make_error_code(RpcError value) noexcept -> std::error_code {
    return std::error_code(static_cast<int>(value), RpcErrorCategory::instance());
}

NEKO_END_NAMESPACE
