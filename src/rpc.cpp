#include "nekoproto/rpc/private/backend_base.hpp"
#include "nekoproto/rpc/error.hpp"

#include "nekoproto/serialization/reflection.hpp"

#include <cstring>
#include <limits>
#include <utility>

#include <ilias/io/error.hpp>

NEKO_BEGIN_NAMESPACE
namespace rpc {

namespace {

auto append_bytes(NekoRpcFrameCodec::MessageType& out, std::span<const std::byte> bytes) -> void {
    if (bytes.empty()) {
        return;
    }
    out.insert(out.end(), bytes.begin(), bytes.end());
}

constexpr std::uint8_t MethodTableFormatVersion = 2U;

auto read_header(std::span<const std::byte> data) -> NekoRpcFrameCodec::Header {
    std::size_t offset = 0;
    NekoRpcFrameCodec::Header header;
    Reflect<NekoRpcFrameCodec::Header>::forEach(header, [&](auto& field) {
        using ValueType = std::remove_reference_t<decltype(field)>;
        if constexpr (std::is_enum_v<ValueType>) {
            using WireType = std::underlying_type_t<ValueType>;
            field          = static_cast<ValueType>(NekoRpcExtensionCodec::readInteger<WireType>(data, offset));
            offset += sizeof(WireType);
        } else if constexpr (std::is_integral_v<ValueType>) {
            field = NekoRpcExtensionCodec::readInteger<ValueType>(data, offset);
            offset += sizeof(ValueType);
        } else {
            static_assert(always_false_v<ValueType>, "Unsupported type");
        }
    });
    return header;
}

auto append_header(NekoRpcFrameCodec::MessageType& out, const NekoRpcFrameCodec::Header& header) -> void {
    Reflect<NekoRpcFrameCodec::Header>::forEach(header, [&](auto& field) {
        using ValueType = std::remove_reference_t<decltype(field)>;
        if constexpr (std::is_enum_v<ValueType>) {
            NekoRpcExtensionCodec::appendInteger(out, static_cast<std::underlying_type_t<ValueType>>(field));
        } else if constexpr (std::is_integral_v<ValueType>) {
            NekoRpcExtensionCodec::appendInteger(out, field);
        } else {
            static_assert(always_false_v<ValueType>, "Unsupported type");
        }
    });
}

auto frame_body_size(const NekoRpcFrameCodec::Header& header) -> ilias::Result<std::size_t, std::error_code> {
    const auto method_size    = static_cast<std::size_t>(header.method_size);
    const auto extension_size = static_cast<std::size_t>(header.extension_size);
    const auto payload_size   = static_cast<std::size_t>(header.payload_size);
    const auto max_size       = std::numeric_limits<std::size_t>::max();
    if (method_size > max_size - extension_size || method_size + extension_size > max_size - payload_size ||
        method_size + extension_size + payload_size > max_size - NekoRpcFrameCodec::headerSize()) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    return method_size + extension_size + payload_size;
}

auto valid_header(const NekoRpcFrameCodec::Header& header) -> bool {
    return header.magic == NekoRpcFrameCodec::Magic && header.version == NekoRpcFrameCodec::Version &&
           NekoRpcFrameCodec::knownKind(header.kind);
}

auto extensions_supported(const NekoRpcFrameCodec::ExtensionMapType& extensions) -> bool {
    for (const auto& extension : extensions) {
        const auto wire_type = static_cast<std::underlying_type_t<NekoRpcExtensionType>>(extension.first);
        if ((wire_type & 0x8000U) != 0U) {
            NEKO_LOG_ERROR("rpcbackend", "Extension {} is not supported", wire_type);
            return false;
        }
    }
    return true;
}

auto parse_frame_parts(std::span<const std::byte> data, NekoRpcFrameCodec::FrameParts& parts) -> bool {
    if (data.size() < NekoRpcFrameCodec::headerSize()) {
        return false;
    }

    parts.header = read_header(data);
    if (!valid_header(parts.header)) {
        return false;
    }

    const auto body_size = frame_body_size(parts.header);
    if (!body_size || NekoRpcFrameCodec::headerSize() + body_size.value() != data.size()) {
        return false;
    }

    const auto method_size     = static_cast<std::size_t>(parts.header.method_size);
    const auto extension_size  = static_cast<std::size_t>(parts.header.extension_size);
    const auto payload_size    = static_cast<std::size_t>(parts.header.payload_size);
    const auto method_begin    = NekoRpcFrameCodec::headerSize();
    const auto extension_begin = method_begin + method_size;
    const auto payload_begin   = extension_begin + extension_size;

    parts.method = data.subspan(method_begin, method_size);
    parts.extensions.clear();
    const auto extensions_loaded =
        NekoRpcExtensionCodec::loadTlvs(data.subspan(extension_begin, extension_size), parts.extensions);
    parts.payload = data.subspan(payload_begin, payload_size);
    return extensions_loaded && extensions_supported(parts.extensions);
}

auto encode_method_entries_value(const std::vector<NekoRpcMethodEntry>& entries) -> NekoRpcFrameCodec::MessageType {
    NekoRpcFrameCodec::MessageType value;
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
        NekoRpcExtensionCodec::appendInteger(value, entry.signature_hash);
        NekoRpcExtensionCodec::appendInteger(value, static_cast<std::uint16_t>(entry.name.size()));
        value.insert(value.end(), reinterpret_cast<const std::byte*>(entry.name.data()),
                     reinterpret_cast<const std::byte*>(entry.name.data()) + entry.name.size());
    }
    return value;
}

auto parse_method_entries_value(std::span<const std::byte> value, std::vector<NekoRpcMethodEntry>& entries) -> bool {
    if (value.size() < 4U) {
        return false;
    }

    entries.clear();
    if (value.size() >= 5U && NekoRpcExtensionCodec::readInteger<std::uint8_t>(value, 0) == MethodTableFormatVersion) {
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
            entry.signature_hash = NekoRpcExtensionCodec::readInteger<std::uint64_t>(value, offset);
            offset += 8U;
            const auto name_size = NekoRpcExtensionCodec::readInteger<std::uint16_t>(value, offset);
            offset += 2U;
            if (value.size() - offset < name_size) {
                return false;
            }
            entry.name.assign(reinterpret_cast<const char*>(value.data() + offset), name_size);
            offset += name_size;
            entries.emplace_back(std::move(entry));
        }
        return offset == value.size();
    }
    return false;
}

auto parse_method_entries(const NekoRpcFrameCodec::ExtensionMapType& extensions, NekoRpcExtensionType tlv_type,
                          std::vector<NekoRpcMethodEntry>& entries) -> bool {
    const auto item = extensions.find(tlv_type);
    if (item == extensions.end()) {
        return false;
    }
    return parse_method_entries_value(item->second, entries);
}

} // namespace

auto NekoRpcFrameCodec::headerSize() noexcept -> std::size_t {
    static const auto size = header_size();
    return size;
}

auto NekoRpcFrameCodec::methodName(const DecodedRequest& request) noexcept -> std::string_view {
    return request.method;
}

auto NekoRpcFrameCodec::id(const DecodedRequest& request) noexcept -> const IdType& { return request.header.id; }

auto NekoRpcFrameCodec::expectsResponse(const DecodedRequest& request) noexcept -> bool {
    return request.header.kind == NekoRpcKind::Request;
}

auto NekoRpcFrameCodec::encodeResponses(const ResponseValuesType& responses, bool /*batch*/) -> MessageType {
    if (responses.empty()) {
        return {};
    }
    return responses.front();
}

auto NekoRpcFrameCodec::encodeFrame(FrameParts frame) -> MessageType {
    MessageType extensions;
    if (!NekoRpcExtensionCodec::appendTlvs(extensions, frame.extensions)) {
        return {};
    }
    if (frame.method.size() > std::numeric_limits<std::uint32_t>::max() ||
        extensions.size() > std::numeric_limits<std::uint16_t>::max() ||
        frame.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {};
    }

    frame.header.magic          = Magic;
    frame.header.version        = Version;
    frame.header.method_size    = static_cast<std::uint32_t>(frame.method.size());
    frame.header.extension_size = static_cast<std::uint16_t>(extensions.size());
    frame.header.payload_size   = static_cast<std::uint32_t>(frame.payload.size());
    if (!extensions.empty()) {
        frame.header.flags = static_cast<std::uint8_t>(frame.header.flags | NekoRpcFlag::HasExtensions);
    } else {
        frame.header.flags = static_cast<std::uint8_t>(frame.header.flags & ~NekoRpcFlag::HasExtensions);
    }

    MessageType out;
    out.reserve(headerSize() + frame.method.size() + extensions.size() + frame.payload.size());
    append_header(out, frame.header);
    append_bytes(out, frame.method);
    append_bytes(out, NekoRpcExtensionCodec::asBytes(extensions));
    append_bytes(out, frame.payload);
    return out;
}

auto NekoRpcFrameCodec::encodeHello(ExtensionMapType extensions, std::uint8_t codec) -> MessageType {
    Header header;
    header.kind  = NekoRpcKind::Hello;
    header.codec = codec;
    return encodeFrame({
        .header     = header,
        .method     = {},
        .extensions = extensions,
        .payload    = {},
    });
}

auto NekoRpcFrameCodec::decodeFrame(std::span<const std::byte> data) -> ilias::Result<DecodedRequest, std::error_code> {
    FrameParts parts;
    if (!parse_frame_parts(data, parts)) {
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

    const auto parsed = read_header(header);
    if (!valid_header(parsed) || parsed.codec != codec) {
        return ilias::Err(RpcError::InvalidRequest);
    }
    return frame_body_size(parsed);
}

auto NekoRpcFrameCodec::parseFrame(std::span<const std::byte> data, std::uint8_t codec, FrameParts& parts) -> bool {
    return parse_frame_parts(data, parts) && parts.header.codec == codec;
}

auto NekoRpcExtensionCodec::asBytes(const MessageType& message) -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(message.data()), message.size()};
}

auto NekoRpcExtensionCodec::copyBytes(std::span<const std::byte> bytes) -> MessageType {
    MessageType out;
    append_bytes(out, bytes);
    return out;
}

auto NekoRpcExtensionCodec::loadTlvs(std::span<const std::byte> data, ExtensionMapType& extensions) -> bool {
    extensions.clear();
    std::size_t offset = 0;
    while (offset < data.size()) {
        if (data.size() - offset < 4U) {
            return false;
        }
        const auto wire_type = readInteger<std::underlying_type_t<ExtensionType>>(data, offset);
        const auto size      = readInteger<std::uint16_t>(data, offset + sizeof(wire_type));
        offset += sizeof(wire_type) + sizeof(size);
        if (data.size() - offset < size) {
            return false;
        }
        const auto type = static_cast<ExtensionType>(wire_type);
        if (extensions.contains(type)) {
            NEKO_LOG_ERROR("rpcbackend", "Extension {} has duplicate", wire_type);
            return false;
        }
        extensions.emplace(type, data.subspan(offset, size));
        offset += size;
    }
    return true;
}

auto NekoRpcExtensionCodec::appendTlvs(MessageType& out, const ExtensionMapType& extensions) -> bool {
    for (const auto& [type, value] : extensions) {
        if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }
        appendInteger(out, static_cast<std::underlying_type_t<ExtensionType>>(type));
        appendInteger(out, static_cast<std::uint16_t>(value.size()));
        append_bytes(out, value);
    }
    return true;
}

auto NekoRpcMethodIdExtension::methodTableValue(const std::vector<NekoRpcMethodEntry>& entries) -> MessageType {
    return encode_method_entries_value(entries);
}

auto NekoRpcMethodIdExtension::methodTableDeltaValue(const std::vector<NekoRpcMethodEntry>& entries) -> MessageType {
    return encode_method_entries_value(entries);
}

auto NekoRpcMethodIdExtension::parseMethodTable(const ExtensionMapType& extensions,
                                                std::vector<NekoRpcMethodEntry>& entries) -> bool {
    return parse_method_entries(extensions, NekoRpcExtensionType::MethodTable, entries);
}

auto NekoRpcMethodIdExtension::parseMethodTableDelta(const ExtensionMapType& extensions,
                                                     std::vector<NekoRpcMethodEntry>& entries) -> bool {
    return parse_method_entries(extensions, NekoRpcExtensionType::MethodTableDelta, entries);
}

auto NekoRpcMethodIdTable::signatureHash(std::string_view name, std::string_view signature) -> std::uint64_t {
    constexpr std::uint64_t k_offset = 14695981039346656037ULL;
    constexpr std::uint64_t k_prime  = 1099511628211ULL;
    std::uint64_t hash               = k_offset;
    auto update                      = [&hash](std::string_view bytes) {
        for (char ch : bytes) {
            hash ^= static_cast<std::uint8_t>(ch);
            hash *= k_prime;
        }
    };
    update(name);
    hash ^= 0U;
    hash *= k_prime;
    update(signature);
    return hash == 0U ? 1U : hash;
}

auto NekoRpcMethodIdTable::entriesFromNames(const std::vector<std::string>& names) -> std::vector<NekoRpcMethodEntry> {
    std::vector<NekoRpcMethodEntry> entries;
    entries.reserve(names.size());
    for (std::uint64_t id = 0; id < names.size(); ++id) {
        const auto& name = names[static_cast<std::size_t>(id)];
        entries.push_back({
            .id             = id,
            .name           = name,
            .signature_hash = signatureHash(name),
            .state          = NekoRpcMethodState::Active,
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
        if (entry.signature_hash == 0U && !entry.name.empty()) {
            entry.signature_hash = signatureHash(entry.name);
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
        if (entry.signature_hash == 0U && !entry.name.empty()) {
            entry.signature_hash = signatureHash(entry.name);
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
        if (current.signature_hash == hash && current.state == NekoRpcMethodState::Active) {
            return &current;
        }
        current.state = NekoRpcMethodState::Removed;
        mNameToId.erase(item);
    }

    NekoRpcMethodEntry entry{
        .id             = mNextId++,
        .name           = std::move(name),
        .signature_hash = hash,
        .state          = NekoRpcMethodState::Active,
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

auto NekoRpcMethodIdTable::resolve(std::uint64_t id, std::uint64_t client_version, std::uint64_t hash,
                                   bool require_signature_hash) const -> NekoRpcMethodIdResolution {
    NekoRpcMethodIdResolution result;
    result.current_version = mVersion;
    if (client_version < mMinimumCompatibleVersion || client_version > mVersion || client_version == 0U) {
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
    if (require_signature_hash && hash != entry->signature_hash) {
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
    -> ilias::Result<MessageType, std::error_code> {
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
    -> ilias::Result<MessageType, std::error_code> {
    switch (algorithm) {
    case NekoRpcCompressionAlgorithm::None:
        return NekoRpcExtensionCodec::copyBytes(payload);
    case NekoRpcCompressionAlgorithm::RunLength:
        return _decompressRunLength(payload);
    default:
        return ilias::Err(RpcError::InvalidRequest);
    }
}

auto NekoRpcCompressionCodec::_compressRunLength(std::span<const std::byte> payload) -> MessageType {
    MessageType out;
    out.reserve(payload.size());

    std::size_t offset = 0;
    while (offset < payload.size()) {
        std::size_t run = 1;
        while (offset + run < payload.size() && run < 130U && payload[offset + run] == payload[offset]) {
            ++run;
        }
        if (run >= 3U) {
            out.push_back(static_cast<std::byte>(0x80U | static_cast<std::uint8_t>(run - 3U)));
            out.push_back(payload[offset]);
            offset += run;
            continue;
        }

        const auto literal_begin = offset;
        offset += run;
        while (offset < payload.size()) {
            run = 1;
            while (offset + run < payload.size() && run < 130U && payload[offset + run] == payload[offset]) {
                ++run;
            }
            if (run >= 3U || offset - literal_begin >= 128U) {
                break;
            }
            offset += run;
        }
        const auto literal_size = offset - literal_begin;
        out.push_back(static_cast<std::byte>(literal_size - 1U));
        append_bytes(out, payload.subspan(literal_begin, literal_size));
    }
    return out;
}

auto NekoRpcCompressionCodec::_decompressRunLength(std::span<const std::byte> payload)
    -> ilias::Result<MessageType, std::error_code> {
    MessageType out;
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto control = NekoRpcExtensionCodec::readInteger<std::uint8_t>(payload, offset);
        ++offset;
        if ((control & 0x80U) != 0U) {
            if (offset >= payload.size()) {
                return ilias::Err(RpcError::InvalidRequest);
            }
            const auto count = static_cast<std::size_t>(control & 0x7FU) + 3U;
            const auto value = payload[offset];
            ++offset;
            out.insert(out.end(), count, value);
            continue;
        }

        const auto count = static_cast<std::size_t>(control) + 1U;
        if (payload.size() - offset < count) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        append_bytes(out, payload.subspan(offset, count));
        offset += count;
    }
    return out;
}

} // namespace rpc

NEKO_END_NAMESPACE
