#include "nekoproto/rpc/backend_base.hpp"
#include "nekoproto/rpc/error.hpp"

#include "nekoproto/serialization/reflection.hpp"

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

auto readHeader(std::span<const std::byte> data) -> NekoRpcFrameCodec::Header {
    std::size_t offset = 0;
    NekoRpcFrameCodec::Header header;
    Reflect<NekoRpcFrameCodec::Header>::forEach(header, [&](auto& field) {
        using value_type = std::remove_reference_t<decltype(field)>;
        if constexpr (std::is_enum_v<value_type>) {
            using wire_type = std::underlying_type_t<value_type>;
            field           = static_cast<value_type>(NekoRpcExtensionCodec::readInteger<wire_type>(data, offset));
            offset += sizeof(wire_type);
        } else if constexpr (std::is_integral_v<value_type>) {
            field = NekoRpcExtensionCodec::readInteger<value_type>(data, offset);
            offset += sizeof(value_type);
        } else {
            static_assert(always_false_v<value_type>, "Unsupported type");
        }
    });
    return header;
}

auto appendHeader(NekoRpcFrameCodec::Message& out, const NekoRpcFrameCodec::Header& header) -> void {
    Reflect<NekoRpcFrameCodec::Header>::forEach(header, [&](auto& field) {
        using value_type = std::remove_reference_t<decltype(field)>;
        if constexpr (std::is_enum_v<value_type>) {
            NekoRpcExtensionCodec::appendInteger(out, static_cast<std::underlying_type_t<value_type>>(field));
        } else if constexpr (std::is_integral_v<value_type>) {
            NekoRpcExtensionCodec::appendInteger(out, field);
        } else {
            static_assert(always_false_v<value_type>, "Unsupported type");
        }
    });
}

auto frameBodySize(const NekoRpcFrameCodec::Header& header) -> ilias::Result<std::size_t, std::error_code> {
    const auto methodSize    = static_cast<std::size_t>(header.methodSize);
    const auto extensionSize = static_cast<std::size_t>(header.extensionSize);
    const auto payloadSize   = static_cast<std::size_t>(header.payloadSize);
    const auto maxSize       = std::numeric_limits<std::size_t>::max();
    if (methodSize > maxSize - extensionSize || methodSize + extensionSize > maxSize - payloadSize ||
        methodSize + extensionSize + payloadSize > maxSize - NekoRpcFrameCodec::headerSize()) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    return methodSize + extensionSize + payloadSize;
}

auto validHeader(const NekoRpcFrameCodec::Header& header) -> bool {
    return header.magic == NekoRpcFrameCodec::Magic && header.version == NekoRpcFrameCodec::Version &&
           NekoRpcFrameCodec::knownKind(header.kind);
}

auto extensionsSupported(const NekoRpcFrameCodec::ExtensionMap& extensions) -> bool {
    for (const auto& extension : extensions) {
        const auto wireType = static_cast<std::underlying_type_t<NekoRpcExtensionType>>(extension.first);
        if ((wireType & 0x8000U) != 0U) {
            NEKO_LOG_ERROR("rpcbackend", "Extension {} is not supported", wireType);
            return false;
        }
    }
    return true;
}

auto parseFrameParts(std::span<const std::byte> data, NekoRpcFrameCodec::FrameParts& parts) -> bool {
    if (data.size() < NekoRpcFrameCodec::headerSize()) {
        return false;
    }

    parts.header = readHeader(data);
    if (!validHeader(parts.header)) {
        return false;
    }

    const auto body_size = frameBodySize(parts.header);
    if (!body_size || NekoRpcFrameCodec::headerSize() + body_size.value() != data.size()) {
        return false;
    }

    const auto method_size     = static_cast<std::size_t>(parts.header.methodSize);
    const auto extension_size  = static_cast<std::size_t>(parts.header.extensionSize);
    const auto payload_size    = static_cast<std::size_t>(parts.header.payloadSize);
    const auto method_begin    = NekoRpcFrameCodec::headerSize();
    const auto extension_begin = method_begin + method_size;
    const auto payload_begin   = extension_begin + extension_size;

    parts.method = data.subspan(method_begin, method_size);
    parts.extensions.clear();
    const auto extensionsLoaded =
        NekoRpcExtensionCodec::loadTlvs(data.subspan(extension_begin, extension_size), parts.extensions);
    parts.payload = data.subspan(payload_begin, payload_size);
    return extensionsLoaded && extensionsSupported(parts.extensions);
}

auto encodeMethodEntriesValue(const std::vector<NekoRpcMethodEntry>& entries) -> NekoRpcFrameCodec::Message {
    NekoRpcFrameCodec::Message value;
    std::uint32_t count = 0;
    for (const auto& entry : entries) {
        if (entry.name.size() <= std::numeric_limits<std::uint16_t>::max()) {
            ++count;
        }
    }

    NekoRpcExtensionCodec::appendInteger(value, MethodTableFormatVersion);
    NekoRpcExtensionCodec::appendInteger(value, count);
    for (const auto& entry : entries) {
        if (entry.name.size() > std::numeric_limits<std::uint16_t>::max()) {
            continue;
        }
        NekoRpcExtensionCodec::appendInteger(value, entry.id);
        NekoRpcExtensionCodec::appendInteger(value, static_cast<std::uint8_t>(entry.state));
        NekoRpcExtensionCodec::appendInteger(value, entry.signatureHash);
        NekoRpcExtensionCodec::appendInteger(value, static_cast<std::uint16_t>(entry.name.size()));
        value.insert(value.end(), entry.name.begin(), entry.name.end());
    }
    return value;
}

auto parseMethodEntriesValue(std::span<const std::byte> value, std::vector<NekoRpcMethodEntry>& entries) -> bool {
    if (value.size() < 4U) {
        return false;
    }

    entries.clear();
    if (value.size() >= 5U &&
        NekoRpcExtensionCodec::readInteger<std::uint8_t>(value, 0) == MethodTableFormatVersion) {
        const auto count   = NekoRpcExtensionCodec::readInteger<std::uint32_t>(value, 1);
        std::size_t offset = 5U;
        for (std::uint32_t ix = 0; ix < count; ++ix) {
            if (value.size() - offset < 19U) {
                return false;
            }
            NekoRpcMethodEntry entry;
            entry.id = NekoRpcExtensionCodec::readInteger<std::uint64_t>(value, offset);
            offset += 8U;
            entry.state = NekoRpcExtensionCodec::readInteger<std::uint8_t>(value, offset) ==
                                  static_cast<std::uint8_t>(NekoRpcMethodState::Removed)
                              ? NekoRpcMethodState::Removed
                              : NekoRpcMethodState::Active;
            offset += 1U;
            entry.signatureHash = NekoRpcExtensionCodec::readInteger<std::uint64_t>(value, offset);
            offset += 8U;
            const auto nameSize = NekoRpcExtensionCodec::readInteger<std::uint16_t>(value, offset);
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

    const auto count   = NekoRpcExtensionCodec::readInteger<std::uint32_t>(value, 0);
    std::size_t offset = 4U;
    for (std::uint32_t ix = 0; ix < count; ++ix) {
        if (value.size() - offset < 10U) {
            return false;
        }
        NekoRpcMethodEntry entry;
        entry.id = NekoRpcExtensionCodec::readInteger<std::uint64_t>(value, offset);
        offset += 8U;
        const auto nameSize = NekoRpcExtensionCodec::readInteger<std::uint16_t>(value, offset);
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

auto parseMethodEntries(const NekoRpcFrameCodec::ExtensionMap& extensions, NekoRpcExtensionType tlvType,
                        std::vector<NekoRpcMethodEntry>& entries) -> bool {
    const auto item = extensions.find(tlvType);
    if (item == extensions.end()) {
        return false;
    }
    return parseMethodEntriesValue(item->second, entries);
}

} // namespace

auto NekoRpcFrameCodec::headerSize() noexcept -> std::size_t {
    static const auto size = _headerSize();
    return size;
}

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

auto NekoRpcFrameCodec::encodeFrame(FrameParts frame) -> Message {
    Message extensions;
    if (!NekoRpcExtensionCodec::appendTlvs(extensions, frame.extensions)) {
        return {};
    }
    if (frame.method.size() > std::numeric_limits<std::uint32_t>::max() ||
        extensions.size() > std::numeric_limits<std::uint16_t>::max() ||
        frame.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {};
    }

    frame.header.magic         = Magic;
    frame.header.version       = Version;
    frame.header.methodSize    = static_cast<std::uint32_t>(frame.method.size());
    frame.header.extensionSize = static_cast<std::uint16_t>(extensions.size());
    frame.header.payloadSize   = static_cast<std::uint32_t>(frame.payload.size());
    if (!extensions.empty()) {
        frame.header.flags = static_cast<std::uint8_t>(frame.header.flags | NekoRpcFlag::HasExtensions);
    } else {
        frame.header.flags = static_cast<std::uint8_t>(frame.header.flags & ~NekoRpcFlag::HasExtensions);
    }

    Message out;
    out.reserve(headerSize() + frame.method.size() + extensions.size() + frame.payload.size());
    appendHeader(out, frame.header);
    appendBytes(out, frame.method);
    appendBytes(out, NekoRpcExtensionCodec::asBytes(extensions));
    appendBytes(out, frame.payload);
    return out;
}

auto NekoRpcFrameCodec::encodeHello(ExtensionMap extensions, std::uint8_t codec) -> Message {
    Header header;
    header.kind  = NekoRpcKind::Hello;
    header.codec = codec;
    return encodeFrame({
        .header     = header,
        .extensions = extensions,
    });
}

auto NekoRpcFrameCodec::decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code> {
    FrameParts parts;
    if (!parseFrameParts(data, parts)) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    DecodedRequest request;
    request.header = parts.header;
    request.method.assign(reinterpret_cast<const char*>(parts.method.data()), parts.method.size());
    request.payload.resize(parts.payload.size());
    if (!parts.payload.empty()) {
        std::memcpy(request.payload.data(), parts.payload.data(), parts.payload.size());
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
    if (header.size() != headerSize()) {
        return ilias::Err(RpcError::InvalidRequest);
    }

    const auto parsed = readHeader(header);
    if (!validHeader(parsed) || parsed.codec != codec) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    return frameBodySize(parsed);
}

auto NekoRpcFrameCodec::parseFrame(std::span<const std::byte> data, std::uint8_t codec, FrameParts& parts) -> bool {
    return parseFrameParts(data, parts) && parts.header.codec == codec;
}

auto NekoRpcExtensionCodec::asBytes(const Message& message) -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(message.data()), message.size()};
}

auto NekoRpcExtensionCodec::copyBytes(std::span<const std::byte> bytes) -> Message {
    Message out;
    appendBytes(out, bytes);
    return out;
}

auto NekoRpcExtensionCodec::loadTlvs(std::span<const std::byte> data, ExtensionMap& extensions) -> bool {
    extensions.clear();
    std::size_t offset = 0;
    while (offset < data.size()) {
        if (data.size() - offset < 4U) {
            return false;
        }
        const auto wireType = readInteger<std::underlying_type_t<ExtensionType>>(data, offset);
        const auto size     = readInteger<std::uint16_t>(data, offset + sizeof(wireType));
        offset += sizeof(wireType) + sizeof(size);
        if (data.size() - offset < size) {
            return false;
        }
        const auto type = static_cast<ExtensionType>(wireType);
        if (extensions.contains(type)) {
            NEKO_LOG_ERROR("rpcbackend", "Extension {} has duplicate", wireType);
            return false;
        }
        extensions.emplace(type, data.subspan(offset, size));
        offset += size;
    }
    return true;
}

auto NekoRpcExtensionCodec::appendTlvs(Message& out, const ExtensionMap& extensions) -> bool {
    for (const auto& [type, value] : extensions) {
        if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }
        appendInteger(out, static_cast<std::underlying_type_t<ExtensionType>>(type));
        appendInteger(out, static_cast<std::uint16_t>(value.size()));
        appendBytes(out, value);
    }
    return true;
}

auto NekoRpcMethodIdExtension::methodTableValue(const std::vector<NekoRpcMethodEntry>& entries) -> Message {
    return encodeMethodEntriesValue(entries);
}

auto NekoRpcMethodIdExtension::methodTableDeltaValue(const std::vector<NekoRpcMethodEntry>& entries) -> Message {
    return encodeMethodEntriesValue(entries);
}

auto NekoRpcMethodIdExtension::parseMethodTable(const ExtensionMap& extensions,
                                                std::vector<NekoRpcMethodEntry>& entries) -> bool {
    return parseMethodEntries(extensions, NekoRpcExtensionType::MethodTable, entries);
}

auto NekoRpcMethodIdExtension::parseMethodTableDelta(const ExtensionMap& extensions,
                                                     std::vector<NekoRpcMethodEntry>& entries) -> bool {
    return parseMethodEntries(extensions, NekoRpcExtensionType::MethodTableDelta, entries);
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
        const auto control = NekoRpcExtensionCodec::readInteger<std::uint8_t>(payload, offset);
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
