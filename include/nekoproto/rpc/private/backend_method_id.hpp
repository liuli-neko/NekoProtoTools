#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/rpc/private/backend_base.hpp"
#include "nekoproto/rpc/private/method_id.hpp"

NEKO_BEGIN_NAMESPACE
namespace rpc {

inline auto NekoRpcMethodIdResolveStatusName(NekoRpcMethodIdResolveStatus status) noexcept -> std::string_view {
    switch (status) {
    case NekoRpcMethodIdResolveStatus::Ok:
        return "ok";
    case NekoRpcMethodIdResolveStatus::TableOutdated:
        return "table_outdated";
    case NekoRpcMethodIdResolveStatus::MethodNotFound:
        return "method_not_found";
    case NekoRpcMethodIdResolveStatus::MethodRemoved:
        return "method_removed";
    case NekoRpcMethodIdResolveStatus::SignatureMismatch:
        return "signature_mismatch";
    }
    return "unknown";
}

template <typename Methods>
auto NekoRpcMethodEntries(Methods&& methods) -> std::vector<NekoRpcMethodEntry> {
    std::vector<NekoRpcMethodEntry> entries;
    std::uint64_t id = 0;
    for (const auto& method : methods) {
        std::string_view signature;
        if constexpr (requires { method.signature; }) {
            signature = method.signature;
        }
        std::string name{method.name};
        entries.push_back({
            .id             = id++,
            .name           = name,
            .signature_hash = NekoRpcMethodIdTable::signatureHash(name, signature),
            .state          = NekoRpcMethodState::Active,
        });
    }
    return entries;
}

inline auto NekoRpcMethodEntriesFromMetadata(const std::vector<::NEKO_NAMESPACE::detail::RpcMethodMetadata>& methods)
    -> std::vector<NekoRpcMethodEntry> {
    return NekoRpcMethodEntries(methods);
}

inline auto NekoRpcSyncMethodTable(NekoRpcMethodIdTable& table,
                                   const std::vector<NekoRpcMethodEntry>& active_entries,
                                   std::vector<NekoRpcMethodEntry>& delta) -> void {
    delta.clear();
    for (const auto& entry : active_entries) {
        const auto* current = table.findByName(entry.name);
        if (current != nullptr && current->state == NekoRpcMethodState::Active &&
            current->signature_hash == entry.signature_hash) {
            continue;
        }
        if (const auto* updated = table.upsert(entry.name, entry.signature_hash); updated != nullptr) {
            delta.push_back(*updated);
        }
    }

    std::vector<std::string> removed_names;
    for (const auto& current : table.entries()) {
        if (current.state != NekoRpcMethodState::Active || current.name.empty()) {
            continue;
        }
        const bool still_active = std::any_of(active_entries.begin(), active_entries.end(),
                                              [&](const auto& entry) { return entry.name == current.name; });
        if (!still_active) {
            removed_names.push_back(current.name);
        }
    }

    for (const auto& name : removed_names) {
        const auto* current = table.findByName(name);
        if (current == nullptr) {
            continue;
        }
        const auto id = current->id;
        if (table.remove(name)) {
            if (const auto* tombstone = table.findById(id); tombstone != nullptr) {
                delta.push_back(*tombstone);
            }
        }
    }
}

template <typename Backend>
auto NekoRpcIncomingMethodName(typename Backend::ServerContext& context,
                              typename Backend::PeerSession& session,
                              const typename Backend::Codec::FrameParts& parts)
    -> ilias::Result<std::string, std::error_code> {
    using Flag = typename Backend::Flag;

    if ((parts.header.flags & Flag::MethodId) == 0U) {
        return std::string{reinterpret_cast<const char*>(parts.method.data()), parts.method.size()};
    }

    if (!session.method_id_enabled || parts.method.size() != sizeof(std::uint64_t)) {
        NEKO_LOG_WARN("rpc", "rpc backend method-id rejected: reason=not_negotiated method_bytes={}",
                      parts.method.size());
        return ilias::Err(RpcError::MethodIdNotNegotiated);
    }

    std::uint64_t version = 0;
    if (!NekoRpcExtensionCodec::readIntegerTlv(parts.extensions, NekoRpcExtensionType::MethodTableVersion, version)) {
        NEKO_LOG_WARN("rpc", "rpc backend method-id rejected: reason=missing_table_version");
        return ilias::Err(RpcError::MethodTableOutdated);
    }

    std::uint64_t signature_hash = 0;
    const bool has_signature =
        NekoRpcExtensionCodec::readIntegerTlv(parts.extensions, NekoRpcExtensionType::MethodSignatureHash,
                                              signature_hash);
    const auto method_id = NekoRpcExtensionCodec::readInteger<std::uint64_t>(parts.method);
    auto resolved        = context.method_table.resolve(method_id, version, signature_hash, true);
    NEKO_LOG_TRACE("rpc", "rpc backend method-id lookup: id={} version={} signature_present={} status={}",
                   method_id, version, has_signature, NekoRpcMethodIdResolveStatusName(resolved.status));

    if (!has_signature && resolved.status == NekoRpcMethodIdResolveStatus::SignatureMismatch) {
        NEKO_LOG_WARN("rpc", "rpc backend method-id rejected: id={} version={} reason=missing_signature", method_id,
                      version);
        return ilias::Err(RpcError::MethodSignatureMismatch);
    }

    // Method-id failures are ordinary request errors. The server reports them
    // and leaves cache refresh or name fallback decisions to the client.
    switch (resolved.status) {
    case NekoRpcMethodIdResolveStatus::Ok:
        NEKO_LOG_TRACE("rpc", "rpc backend method-id resolved: id={} version={} method={}", method_id, version,
                       resolved.entry->name);
        break;
    case NekoRpcMethodIdResolveStatus::TableOutdated:
        return ilias::Err(RpcError::MethodTableOutdated);
    case NekoRpcMethodIdResolveStatus::MethodNotFound:
        return ilias::Err(RpcError::MethodIdNotFound);
    case NekoRpcMethodIdResolveStatus::MethodRemoved:
        return ilias::Err(RpcError::MethodIdRemoved);
    case NekoRpcMethodIdResolveStatus::SignatureMismatch:
        return ilias::Err(RpcError::MethodSignatureMismatch);
    }

    return resolved.entry->name;
}

} // namespace rpc
NEKO_END_NAMESPACE
