#include "nekoproto/rpc/backend_base.hpp"
#include "nekoproto/rpc/error.hpp"

#include <cstring>
#include <limits>
#include <utility>

#include <ilias/io/error.hpp>

NEKO_BEGIN_NAMESPACE
namespace detail {

namespace {

auto appendBytes(NekoRpcFrameCodec::Message& out, std::span<const std::byte> bytes) -> void {
    if (bytes.empty()) {
        return;
    }
    const auto* begin = reinterpret_cast<const char*>(bytes.data());
    out.insert(out.end(), begin, begin + bytes.size());
}

constexpr std::uint8_t MethodTableFormatVersion = 2U;

auto encodeMethodEntries(const std::vector<NekoRpcMethodEntry>& entries, std::uint16_t tlvType)
    -> NekoRpcFrameCodec::Message {
    NekoRpcFrameCodec::Message value;
    std::uint32_t count = 0;
    for (const auto& entry : entries) {
        if (entry.name.size() <= std::numeric_limits<std::uint16_t>::max()) {
            ++count;
        }
    }

    NekoRpcExtensionCodec::appendU8(value, MethodTableFormatVersion);
    NekoRpcExtensionCodec::appendU32(value, count);
    for (const auto& entry : entries) {
        if (entry.name.size() > std::numeric_limits<std::uint16_t>::max()) {
            continue;
        }
        NekoRpcExtensionCodec::appendU64(value, entry.id);
        NekoRpcExtensionCodec::appendU8(value, static_cast<std::uint8_t>(entry.state));
        NekoRpcExtensionCodec::appendU64(value, entry.signatureHash);
        NekoRpcExtensionCodec::appendU16(value, static_cast<std::uint16_t>(entry.name.size()));
        value.insert(value.end(), entry.name.begin(), entry.name.end());
    }
    return NekoRpcExtensionCodec::makeTlv(tlvType, NekoRpcExtensionCodec::asBytes(value));
}

auto parseMethodEntries(std::span<const std::byte> extensions, std::uint16_t tlvType,
                        std::vector<NekoRpcMethodEntry>& entries) -> bool {
    std::span<const std::byte> value;
    if (!NekoRpcExtensionCodec::findTlv(extensions, tlvType, value) || value.size() < 4U) {
        return false;
    }

    entries.clear();
    if (value.size() >= 5U && NekoRpcExtensionCodec::byteAt(value, 0) == MethodTableFormatVersion) {
        const auto count   = NekoRpcExtensionCodec::readU32(value, 1);
        std::size_t offset = 5U;
        for (std::uint32_t ix = 0; ix < count; ++ix) {
            if (value.size() - offset < 19U) {
                return false;
            }
            NekoRpcMethodEntry entry;
            entry.id = NekoRpcExtensionCodec::readU64(value, offset);
            offset += 8U;
            entry.state =
                NekoRpcExtensionCodec::byteAt(value, offset) == static_cast<std::uint8_t>(NekoRpcMethodState::Removed)
                    ? NekoRpcMethodState::Removed
                    : NekoRpcMethodState::Active;
            offset += 1U;
            entry.signatureHash = NekoRpcExtensionCodec::readU64(value, offset);
            offset += 8U;
            const auto nameSize = NekoRpcExtensionCodec::readU16(value, offset);
            offset += 2U;
            if (value.size() - offset < nameSize) {
                return false;
            }
            entry.name.assign(reinterpret_cast<const char*>(value.data() + offset), nameSize);
            offset += nameSize;
            entries.emplace_back(std::move(entry));
        }
        return offset == value.size();
    }

    const auto count   = NekoRpcExtensionCodec::readU32(value, 0);
    std::size_t offset = 4U;
    for (std::uint32_t ix = 0; ix < count; ++ix) {
        if (value.size() - offset < 10U) {
            return false;
        }
        NekoRpcMethodEntry entry;
        entry.id = NekoRpcExtensionCodec::readU64(value, offset);
        offset += 8U;
        const auto nameSize = NekoRpcExtensionCodec::readU16(value, offset);
        offset += 2U;
        if (value.size() - offset < nameSize) {
            return false;
        }
        entry.name.assign(reinterpret_cast<const char*>(value.data() + offset), nameSize);
        entry.signatureHash = NekoRpcMethodIdTable::signatureHash(entry.name);
        offset += nameSize;
        entries.emplace_back(std::move(entry));
    }
    return offset == value.size();
}

} // namespace

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
        (request.header.flags & NekoRpcFlag::Compressed) != 0U || request.method.empty()) {
        return result;
    }

    result.ok = true;
    result.requests.emplace_back(std::move(request));
    return result;
}

auto NekoRpcFrameCodec::methodName(const DecodedRequest& request) noexcept -> std::string_view {
    return request.method;
}

auto NekoRpcFrameCodec::id(const DecodedRequest& request) noexcept -> const Id& { return request.header.id; }

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
    return encodeFrame(kind, id, method, std::span<const std::byte>{}, NekoRpcExtensionCodec::asBytes(payload), flags,
                       codec);
}

auto NekoRpcFrameCodec::encodeFrame(NekoRpcKind kind, Id id, std::string_view method,
                                    std::span<const std::byte> extensions, std::span<const std::byte> payload,
                                    std::uint8_t flags, std::uint8_t codec) -> Message {
    if (method.size() > std::numeric_limits<std::uint32_t>::max() ||
        extensions.size() > std::numeric_limits<std::uint16_t>::max() ||
        payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {};
    }

    Message out;
    const auto methodSize  = static_cast<std::uint32_t>(method.size());
    const auto extSize     = static_cast<std::uint16_t>(extensions.size());
    const auto payloadSize = static_cast<std::uint32_t>(payload.size());
    if (!extensions.empty()) {
        flags = static_cast<std::uint8_t>(flags | NekoRpcFlag::HasExtensions);
    }

    out.reserve(HeaderSize + method.size() + extensions.size() + payload.size());
    _appendU16(out, Magic);
    _appendU8(out, Version);
    _appendU8(out, static_cast<std::uint8_t>(kind));
    _appendU8(out, flags);
    _appendU8(out, codec);
    _appendU16(out, extSize);
    _appendU64(out, id);
    _appendU32(out, methodSize);
    _appendU32(out, payloadSize);
    out.insert(out.end(), method.begin(), method.end());
    appendBytes(out, extensions);
    appendBytes(out, payload);
    return out;
}

auto NekoRpcFrameCodec::encodeHello(std::span<const std::byte> extensions, std::uint8_t codec) -> Message {
    return encodeFrame(NekoRpcKind::Hello, 0, {}, extensions, std::span<const std::byte>{}, 0, codec);
}

auto NekoRpcFrameCodec::decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code> {
    if (data.size() < HeaderSize) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    Header header;
    header.magic         = _readU16(data, 0);
    header.version       = _byteAt(data, 2);
    header.kind          = static_cast<NekoRpcKind>(_byteAt(data, 3));
    header.flags         = _byteAt(data, 4);
    header.codec         = _byteAt(data, 5);
    header.extensionSize = _readU16(data, 6);
    header.id            = _readU64(data, 8);
    header.methodSize    = _readU32(data, 16);
    header.payloadSize   = _readU32(data, 20);

    if (header.magic != Magic || header.version != Version) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    if (!knownKind(header.kind)) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    const auto methodSize    = static_cast<std::size_t>(header.methodSize);
    const auto extensionSize = static_cast<std::size_t>(header.extensionSize);
    const auto payloadSize   = static_cast<std::size_t>(header.payloadSize);
    const auto maxSize       = std::numeric_limits<std::size_t>::max();
    if (methodSize > maxSize - extensionSize || methodSize + extensionSize > maxSize - payloadSize ||
        methodSize + extensionSize + payloadSize > maxSize - HeaderSize) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    const auto expectedSize = HeaderSize + methodSize + extensionSize + payloadSize;
    if (expectedSize != data.size()) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    const auto methodBegin    = HeaderSize;
    const auto extensionBegin = methodBegin + methodSize;
    const auto payloadBegin   = extensionBegin + extensionSize;
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
    const auto methodSize    = static_cast<std::size_t>(_readU32(header, 16));
    const auto payloadSize   = static_cast<std::size_t>(_readU32(header, 20));
    const auto maxSize       = std::numeric_limits<std::size_t>::max();
    if (methodSize > maxSize - extensionSize || methodSize + extensionSize > maxSize - payloadSize ||
        methodSize + extensionSize + payloadSize > maxSize - HeaderSize) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    return methodSize + extensionSize + payloadSize;
}

auto NekoRpcFrameCodec::parseFrame(std::span<const std::byte> data, std::uint8_t codec, FrameParts& parts) -> bool {
    if (data.size() < HeaderSize) {
        return false;
    }

    parts.header.magic         = _readU16(data, 0);
    parts.header.version       = _byteAt(data, 2);
    parts.header.kind          = static_cast<NekoRpcKind>(_byteAt(data, 3));
    parts.header.flags         = _byteAt(data, 4);
    parts.header.codec         = _byteAt(data, 5);
    parts.header.extensionSize = _readU16(data, 6);
    parts.header.id            = _readU64(data, 8);
    parts.header.methodSize    = _readU32(data, 16);
    parts.header.payloadSize   = _readU32(data, 20);
    if (parts.header.magic != Magic || parts.header.version != Version || parts.header.codec != codec ||
        !knownKind(parts.header.kind)) {
        return false;
    }

    const auto methodSize    = static_cast<std::size_t>(parts.header.methodSize);
    const auto extensionSize = static_cast<std::size_t>(parts.header.extensionSize);
    const auto payloadSize   = static_cast<std::size_t>(parts.header.payloadSize);
    const auto maxSize       = std::numeric_limits<std::size_t>::max();
    if (methodSize > maxSize - extensionSize || methodSize + extensionSize > maxSize - payloadSize ||
        methodSize + extensionSize + payloadSize > maxSize - HeaderSize) {
        return false;
    }
    const auto expectedSize = HeaderSize + methodSize + extensionSize + payloadSize;
    if (expectedSize != data.size()) {
        return false;
    }
    if (methodSize > data.size() - HeaderSize) {
        return false;
    }
    const auto methodBegin    = HeaderSize;
    const auto extensionBegin = methodBegin + methodSize;
    if (extensionBegin > data.size() || extensionSize > data.size() - extensionBegin) {
        return false;
    }
    const auto payloadBegin = extensionBegin + extensionSize;
    if (payloadBegin > data.size() || payloadSize > data.size() - payloadBegin ||
        payloadBegin + payloadSize != data.size()) {
        return false;
    }

    parts.method     = data.subspan(methodBegin, methodSize);
    parts.extensions = data.subspan(extensionBegin, extensionSize);
    parts.payload    = data.subspan(payloadBegin, payloadSize);
    return _extensionsSupported(parts.extensions);
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

auto NekoRpcFrameCodec::_appendU8(Message& out, std::uint8_t value) -> void { out.push_back(static_cast<char>(value)); }

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

auto NekoRpcExtensionCodec::asBytes(const Message& message) -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(message.data()), message.size()};
}

auto NekoRpcExtensionCodec::copyBytes(std::span<const std::byte> bytes) -> Message {
    Message out;
    appendBytes(out, bytes);
    return out;
}

auto NekoRpcExtensionCodec::appendTlv(Message& out, std::uint16_t type, std::span<const std::byte> value) -> bool {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }
    appendU16(out, type);
    appendU16(out, static_cast<std::uint16_t>(value.size()));
    appendBytes(out, value);
    return true;
}

auto NekoRpcExtensionCodec::makeTlv(std::uint16_t type, std::span<const std::byte> value) -> Message {
    Message out;
    appendTlv(out, type, value);
    return out;
}

auto NekoRpcExtensionCodec::findTlv(std::span<const std::byte> extensions, std::uint16_t expected,
                                    std::span<const std::byte>& out) -> bool {
    std::size_t offset = 0;
    while (offset < extensions.size()) {
        if (extensions.size() - offset < 4U) {
            return false;
        }
        const auto type = readU16(extensions, offset);
        const auto size = readU16(extensions, offset + 2U);
        offset += 4U;
        if (extensions.size() - offset < size) {
            return false;
        }
        if (type == expected) {
            out = extensions.subspan(offset, size);
            return true;
        }
        offset += size;
    }
    return false;
}

auto NekoRpcExtensionCodec::withoutTlv(std::span<const std::byte> extensions, std::uint16_t skippedType)
    -> ilias::Result<Message, std::error_code> {
    Message out;
    std::size_t offset = 0;
    while (offset < extensions.size()) {
        if (extensions.size() - offset < 4U) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        const auto type      = readU16(extensions, offset);
        const auto size      = readU16(extensions, offset + 2U);
        const auto itemBegin = offset;
        offset += 4U;
        if (extensions.size() - offset < size) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        if (type != skippedType) {
            appendBytes(out, extensions.subspan(itemBegin, 4U + static_cast<std::size_t>(size)));
        }
        offset += size;
    }
    return out;
}

auto NekoRpcExtensionCodec::featureModeTlv(std::uint16_t type, NekoRpcFeatureMode mode) -> Message {
    Message value;
    appendU8(value, static_cast<std::uint8_t>(mode));
    return makeTlv(type, asBytes(value));
}

auto NekoRpcExtensionCodec::parseFeatureMode(std::span<const std::byte> extensions, std::uint16_t type)
    -> NekoRpcFeatureMode {
    std::span<const std::byte> value;
    if (!findTlv(extensions, type, value) || value.empty()) {
        return NekoRpcFeatureMode::Disable;
    }
    switch (byteAt(value, 0)) {
    case static_cast<std::uint8_t>(NekoRpcFeatureMode::Auto):
        return NekoRpcFeatureMode::Auto;
    case static_cast<std::uint8_t>(NekoRpcFeatureMode::Require):
        return NekoRpcFeatureMode::Require;
    default:
        return NekoRpcFeatureMode::Disable;
    }
}

auto NekoRpcExtensionCodec::u64Tlv(std::uint16_t type, std::uint64_t value) -> Message {
    Message encoded;
    appendU64(encoded, value);
    return makeTlv(type, asBytes(encoded));
}

auto NekoRpcExtensionCodec::readU64Tlv(std::span<const std::byte> extensions, std::uint16_t type, std::uint64_t& value)
    -> bool {
    std::span<const std::byte> encoded;
    if (!findTlv(extensions, type, encoded) || encoded.size() != sizeof(std::uint64_t)) {
        return false;
    }
    value = readU64(encoded, 0);
    return true;
}

auto NekoRpcExtensionCodec::appendU8(Message& out, std::uint8_t value) -> void {
    out.push_back(static_cast<char>(value));
}

auto NekoRpcExtensionCodec::appendU16(Message& out, std::uint16_t value) -> void {
    out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<char>(value & 0xFFU));
}

auto NekoRpcExtensionCodec::appendU32(Message& out, std::uint32_t value) -> void {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU));
    }
}

auto NekoRpcExtensionCodec::appendU64(Message& out, std::uint64_t value) -> void {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU));
    }
}

auto NekoRpcExtensionCodec::byteAt(std::span<const std::byte> data, std::size_t offset) -> std::uint8_t {
    return static_cast<std::uint8_t>(data[offset]);
}

auto NekoRpcExtensionCodec::readU16(std::span<const std::byte> data, std::size_t offset) -> std::uint16_t {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byteAt(data, offset)) << 8U) |
                                      static_cast<std::uint16_t>(byteAt(data, offset + 1U)));
}

auto NekoRpcExtensionCodec::readU32(std::span<const std::byte> data, std::size_t offset) -> std::uint32_t {
    std::uint32_t value = 0;
    for (std::size_t ix = 0; ix < 4; ++ix) {
        value = (value << 8U) | byteAt(data, offset + ix);
    }
    return value;
}

auto NekoRpcExtensionCodec::readU64(std::span<const std::byte> data, std::size_t offset) -> std::uint64_t {
    std::uint64_t value = 0;
    for (std::size_t ix = 0; ix < 8; ++ix) {
        value = (value << 8U) | byteAt(data, offset + ix);
    }
    return value;
}

auto NekoRpcMethodIdExtension::modeTlv(NekoRpcFeatureMode mode) -> Message {
    return NekoRpcExtensionCodec::featureModeTlv(ModeTlv, mode);
}

auto NekoRpcMethodIdExtension::parseMode(std::span<const std::byte> extensions) -> NekoRpcFeatureMode {
    return NekoRpcExtensionCodec::parseFeatureMode(extensions, ModeTlv);
}

auto NekoRpcMethodIdExtension::tableVersionTlv(std::uint64_t version) -> Message {
    return NekoRpcExtensionCodec::u64Tlv(TableVersionTlv, version);
}

auto NekoRpcMethodIdExtension::readTableVersion(std::span<const std::byte> extensions, std::uint64_t& version) -> bool {
    return NekoRpcExtensionCodec::readU64Tlv(extensions, TableVersionTlv, version);
}

auto NekoRpcMethodIdExtension::minimumCompatibleVersionTlv(std::uint64_t version) -> Message {
    return NekoRpcExtensionCodec::u64Tlv(MinimumCompatibleVersionTlv, version);
}

auto NekoRpcMethodIdExtension::readMinimumCompatibleVersion(std::span<const std::byte> extensions,
                                                            std::uint64_t& version) -> bool {
    return NekoRpcExtensionCodec::readU64Tlv(extensions, MinimumCompatibleVersionTlv, version);
}

auto NekoRpcMethodIdExtension::signatureHashTlv(std::uint64_t hash) -> Message {
    return NekoRpcExtensionCodec::u64Tlv(SignatureHashTlv, hash);
}

auto NekoRpcMethodIdExtension::readSignatureHash(std::span<const std::byte> extensions, std::uint64_t& hash) -> bool {
    return NekoRpcExtensionCodec::readU64Tlv(extensions, SignatureHashTlv, hash);
}

auto NekoRpcMethodIdExtension::methodTableTlv(const std::vector<NekoRpcMethodEntry>& entries) -> Message {
    return encodeMethodEntries(entries, TableTlv);
}

auto NekoRpcMethodIdExtension::methodTableTlv(const std::vector<std::string>& names) -> Message {
    return methodTableTlv(NekoRpcMethodIdTable::entriesFromNames(names));
}

auto NekoRpcMethodIdExtension::methodTableDeltaTlv(const std::vector<NekoRpcMethodEntry>& entries) -> Message {
    return encodeMethodEntries(entries, TableDeltaTlv);
}

auto NekoRpcMethodIdExtension::parseMethodTable(std::span<const std::byte> extensions,
                                                std::vector<NekoRpcMethodEntry>& entries) -> bool {
    return parseMethodEntries(extensions, TableTlv, entries);
}

auto NekoRpcMethodIdExtension::parseMethodTableDelta(std::span<const std::byte> extensions,
                                                     std::vector<NekoRpcMethodEntry>& entries) -> bool {
    return parseMethodEntries(extensions, TableDeltaTlv, entries);
}

auto NekoRpcMethodIdExtension::parseMethodTable(std::span<const std::byte> extensions, std::vector<std::string>& names,
                                                std::map<std::string, std::uint64_t>& ids) -> bool {
    std::vector<NekoRpcMethodEntry> entries;
    if (!parseMethodTable(extensions, entries)) {
        return false;
    }
    names.clear();
    ids.clear();
    for (const auto& entry : entries) {
        if (entry.id >= names.size()) {
            names.resize(static_cast<std::size_t>(entry.id) + 1U);
        }
        if (entry.state == NekoRpcMethodState::Active) {
            names[static_cast<std::size_t>(entry.id)] = entry.name;
            ids.emplace(entry.name, entry.id);
        }
    }
    return true;
}

auto NekoRpcMethodIdTable::signatureHash(std::string_view name, std::string_view signature) -> std::uint64_t {
    constexpr std::uint64_t kOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime  = 1099511628211ULL;
    std::uint64_t hash              = kOffset;
    auto update                     = [&hash](std::string_view bytes) {
        for (char ch : bytes) {
            hash ^= static_cast<std::uint8_t>(ch);
            hash *= kPrime;
        }
    };
    update(name);
    hash ^= 0U;
    hash *= kPrime;
    update(signature);
    return hash == 0U ? 1U : hash;
}

auto NekoRpcMethodIdTable::entriesFromNames(const std::vector<std::string>& names) -> std::vector<NekoRpcMethodEntry> {
    std::vector<NekoRpcMethodEntry> entries;
    entries.reserve(names.size());
    for (std::uint64_t id = 0; id < names.size(); ++id) {
        const auto& name = names[static_cast<std::size_t>(id)];
        entries.push_back({
            .id            = id,
            .name          = name,
            .signatureHash = signatureHash(name),
            .state         = NekoRpcMethodState::Active,
        });
    }
    return entries;
}

auto NekoRpcMethodIdTable::reset() -> void {
    mVersion                  = 0;
    mMinimumCompatibleVersion = 0;
    mNextId                   = 0;
    mEntries.clear();
    mNameToId.clear();
}

auto NekoRpcMethodIdTable::reset(std::vector<NekoRpcMethodEntry> entries, std::uint64_t version) -> void {
    mEntries.clear();
    mNameToId.clear();
    mNextId                   = 0;
    mVersion                  = entries.empty() ? 0U : version;
    mMinimumCompatibleVersion = mVersion == 0U ? 0U : mVersion;
    for (auto& entry : entries) {
        if (entry.signatureHash == 0U && !entry.name.empty()) {
            entry.signatureHash = signatureHash(entry.name);
        }
        _installEntry(std::move(entry));
    }
    _rebuildIndex();
}

auto NekoRpcMethodIdTable::resetFromNames(const std::vector<std::string>& names, std::uint64_t version) -> void {
    reset(entriesFromNames(names), version);
}

auto NekoRpcMethodIdTable::applyRemoteTable(std::vector<NekoRpcMethodEntry> entries, std::uint64_t version) -> bool {
    reset(std::move(entries), version);
    return mVersion == version || (version == 0U && mEntries.empty());
}

auto NekoRpcMethodIdTable::applyRemoteDelta(const std::vector<NekoRpcMethodEntry>& entries, std::uint64_t version)
    -> bool {
    if (version == 0U || (mVersion != 0U && version < mVersion)) {
        return false;
    }
    for (auto entry : entries) {
        if (entry.signatureHash == 0U && !entry.name.empty()) {
            entry.signatureHash = signatureHash(entry.name);
        }
        if (!_installEntry(std::move(entry))) {
            return false;
        }
    }
    mVersion = version;
    if (mMinimumCompatibleVersion == 0U) {
        mMinimumCompatibleVersion = version;
    }
    _rebuildIndex();
    return true;
}

auto NekoRpcMethodIdTable::upsert(std::string name, std::uint64_t hash) -> const NekoRpcMethodEntry* {
    if (name.empty()) {
        return nullptr;
    }
    if (hash == 0U) {
        hash = signatureHash(name);
    }
    if (auto item = mNameToId.find(name); item != mNameToId.end()) {
        auto& current = mEntries[static_cast<std::size_t>(item->second)];
        if (current.signatureHash == hash && current.state == NekoRpcMethodState::Active) {
            return &current;
        }
        current.state = NekoRpcMethodState::Removed;
        mNameToId.erase(item);
    }

    NekoRpcMethodEntry entry{
        .id            = mNextId++,
        .name          = std::move(name),
        .signatureHash = hash,
        .state         = NekoRpcMethodState::Active,
    };
    const auto id = entry.id;
    if (!_installEntry(std::move(entry))) {
        return nullptr;
    }
    _bumpVersion();
    _rebuildIndex();
    return findById(id);
}

auto NekoRpcMethodIdTable::remove(std::string_view name) -> bool {
    auto item = mNameToId.find(name);
    if (item == mNameToId.end()) {
        return false;
    }
    mEntries[static_cast<std::size_t>(item->second)].state = NekoRpcMethodState::Removed;
    mNameToId.erase(item);
    _bumpVersion();
    return true;
}

auto NekoRpcMethodIdTable::resolve(std::uint64_t id, std::uint64_t clientVersion, std::uint64_t hash,
                                   bool requireSignatureHash) const -> NekoRpcMethodIdResolution {
    NekoRpcMethodIdResolution result;
    result.currentVersion = mVersion;
    if (clientVersion < mMinimumCompatibleVersion || clientVersion > mVersion || clientVersion == 0U) {
        result.status = NekoRpcMethodIdResolveStatus::TableOutdated;
        return result;
    }
    const auto* entry = findById(id);
    if (entry == nullptr || entry->name.empty()) {
        result.status = NekoRpcMethodIdResolveStatus::MethodNotFound;
        return result;
    }
    result.entry = entry;
    if (entry->state == NekoRpcMethodState::Removed) {
        result.status = NekoRpcMethodIdResolveStatus::MethodRemoved;
        return result;
    }
    if (requireSignatureHash && hash != entry->signatureHash) {
        result.status = NekoRpcMethodIdResolveStatus::SignatureMismatch;
        return result;
    }
    result.status = NekoRpcMethodIdResolveStatus::Ok;
    return result;
}

auto NekoRpcMethodIdTable::setMinimumCompatibleVersion(std::uint64_t version) noexcept -> void {
    if (version > mVersion) {
        mMinimumCompatibleVersion = mVersion;
    } else {
        mMinimumCompatibleVersion = version;
    }
}

auto NekoRpcMethodIdTable::findByName(std::string_view name) const -> const NekoRpcMethodEntry* {
    auto item = mNameToId.find(name);
    if (item == mNameToId.end()) {
        return nullptr;
    }
    return findById(item->second);
}

auto NekoRpcMethodIdTable::findById(std::uint64_t id) const -> const NekoRpcMethodEntry* {
    if (id >= mEntries.size()) {
        return nullptr;
    }
    return &mEntries[static_cast<std::size_t>(id)];
}

auto NekoRpcMethodIdTable::_bumpVersion() -> void {
    if (mVersion == 0U) {
        mVersion                  = NekoRpcMethodIdExtension::InitialTableVersion;
        mMinimumCompatibleVersion = mVersion;
    } else if (mVersion != std::numeric_limits<std::uint64_t>::max()) {
        ++mVersion;
    }
}

auto NekoRpcMethodIdTable::_rebuildIndex() -> void {
    mNameToId.clear();
    for (const auto& entry : mEntries) {
        if (entry.state == NekoRpcMethodState::Active && !entry.name.empty()) {
            mNameToId[entry.name] = entry.id;
        }
        if (entry.id >= mNextId) {
            mNextId = entry.id + 1U;
        }
    }
}

auto NekoRpcMethodIdTable::_installEntry(NekoRpcMethodEntry entry) -> bool {
    if (entry.id > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return false;
    }
    if (entry.id >= mEntries.size()) {
        mEntries.resize(static_cast<std::size_t>(entry.id) + 1U);
    }
    if (entry.id >= mNextId) {
        mNextId = entry.id + 1U;
    }
    mEntries[static_cast<std::size_t>(entry.id)] = std::move(entry);
    return true;
}

auto NekoRpcCompressionExtension::modeTlv(NekoRpcFeatureMode mode) -> Message {
    return NekoRpcExtensionCodec::featureModeTlv(ModeTlv, mode);
}

auto NekoRpcCompressionExtension::parseMode(std::span<const std::byte> extensions) -> NekoRpcFeatureMode {
    return NekoRpcExtensionCodec::parseFeatureMode(extensions, ModeTlv);
}

auto NekoRpcCompressionExtension::algorithmTlv(NekoRpcCompressionAlgorithm algorithm) -> Message {
    Message value;
    NekoRpcExtensionCodec::appendU8(value, static_cast<std::uint8_t>(algorithm));
    return NekoRpcExtensionCodec::makeTlv(AlgorithmTlv, NekoRpcExtensionCodec::asBytes(value));
}

auto NekoRpcCompressionExtension::parseAlgorithm(std::span<const std::byte> extensions) -> NekoRpcCompressionAlgorithm {
    std::span<const std::byte> value;
    if (!NekoRpcExtensionCodec::findTlv(extensions, AlgorithmTlv, value) || value.empty()) {
        return NekoRpcCompressionAlgorithm::None;
    }
    if (NekoRpcExtensionCodec::byteAt(value, 0) == static_cast<std::uint8_t>(NekoRpcCompressionAlgorithm::RunLength)) {
        return NekoRpcCompressionAlgorithm::RunLength;
    }
    return NekoRpcCompressionAlgorithm::None;
}

auto NekoRpcCompressionExtension::minPayloadSizeTlv(std::uint32_t size) -> Message {
    Message value;
    NekoRpcExtensionCodec::appendU32(value, size);
    return NekoRpcExtensionCodec::makeTlv(MinPayloadSizeTlv, NekoRpcExtensionCodec::asBytes(value));
}

auto NekoRpcCompressionExtension::readMinPayloadSize(std::span<const std::byte> extensions, std::uint32_t& size)
    -> bool {
    std::span<const std::byte> value;
    if (!NekoRpcExtensionCodec::findTlv(extensions, MinPayloadSizeTlv, value) ||
        value.size() != sizeof(std::uint32_t)) {
        return false;
    }
    size = NekoRpcExtensionCodec::readU32(value, 0);
    return true;
}

auto NekoRpcCompressionCodec::compress(std::span<const std::byte> payload, NekoRpcCompressionAlgorithm algorithm)
    -> ilias::Result<Message, std::error_code> {
    switch (algorithm) {
    case NekoRpcCompressionAlgorithm::None:
        return NekoRpcExtensionCodec::copyBytes(payload);
    case NekoRpcCompressionAlgorithm::RunLength:
        return _compressRunLength(payload);
    default:
        return ilias::Err(RpcError::InvalidRequest);
    }
}

auto NekoRpcCompressionCodec::decompress(std::span<const std::byte> payload, NekoRpcCompressionAlgorithm algorithm)
    -> ilias::Result<Message, std::error_code> {
    switch (algorithm) {
    case NekoRpcCompressionAlgorithm::None:
        return NekoRpcExtensionCodec::copyBytes(payload);
    case NekoRpcCompressionAlgorithm::RunLength:
        return _decompressRunLength(payload);
    default:
        return ilias::Err(RpcError::InvalidRequest);
    }
}

auto NekoRpcCompressionCodec::_compressRunLength(std::span<const std::byte> payload) -> Message {
    Message out;
    out.reserve(payload.size());

    std::size_t offset = 0;
    while (offset < payload.size()) {
        std::size_t run = 1;
        while (offset + run < payload.size() && run < 130U && payload[offset + run] == payload[offset]) {
            ++run;
        }
        if (run >= 3U) {
            out.push_back(static_cast<char>(0x80U | static_cast<std::uint8_t>(run - 3U)));
            out.push_back(static_cast<char>(payload[offset]));
            offset += run;
            continue;
        }

        const auto literalBegin = offset;
        offset += run;
        while (offset < payload.size()) {
            run = 1;
            while (offset + run < payload.size() && run < 130U && payload[offset + run] == payload[offset]) {
                ++run;
            }
            if (run >= 3U || offset - literalBegin >= 128U) {
                break;
            }
            offset += run;
        }
        const auto literalSize = offset - literalBegin;
        out.push_back(static_cast<char>(literalSize - 1U));
        appendBytes(out, payload.subspan(literalBegin, literalSize));
    }
    return out;
}

auto NekoRpcCompressionCodec::_decompressRunLength(std::span<const std::byte> payload)
    -> ilias::Result<Message, std::error_code> {
    Message out;
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto control = NekoRpcExtensionCodec::byteAt(payload, offset);
        ++offset;
        if ((control & 0x80U) != 0U) {
            if (offset >= payload.size()) {
                return ilias::Err(RpcError::InvalidRequest);
            }
            const auto count = static_cast<std::size_t>(control & 0x7FU) + 3U;
            const auto value = static_cast<char>(payload[offset]);
            ++offset;
            out.insert(out.end(), count, value);
            continue;
        }

        const auto count = static_cast<std::size_t>(control) + 1U;
        if (payload.size() - offset < count) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        appendBytes(out, payload.subspan(offset, count));
        offset += count;
    }
    return out;
}

} // namespace detail

NEKO_END_NAMESPACE
