#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/rpc/private/global.hpp"

#include <map>
#include <cstddef>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace rpc {

enum class NekoRpcMethodState : std::uint8_t {
    Active  = 0,
    Removed = 1,
};

struct NekoRpcMethodEntry {
    std::uint64_t id = 0;
    std::string name;
    std::uint64_t signature_hash = 0;
    NekoRpcMethodState state     = NekoRpcMethodState::Active;
};

enum class NekoRpcMethodIdResolveStatus : std::uint8_t {
    Ok = 0,
    TableOutdated,
    MethodNotFound,
    MethodRemoved,
    SignatureMismatch,
};

struct NekoRpcMethodIdResolution {
    NekoRpcMethodIdResolveStatus status = NekoRpcMethodIdResolveStatus::MethodNotFound;
    const NekoRpcMethodEntry* entry     = nullptr;
    std::uint64_t current_version       = 0;
};

class NEKO_PROTO_API NekoRpcMethodIdTable {
public:
    static constexpr std::size_t DefaultMaxEntries = 64U * 1024U;

    explicit NekoRpcMethodIdTable(std::size_t max_entries = DefaultMaxEntries) noexcept
        : mMaxEntries(max_entries) {}

    static auto signatureHash(std::string_view name, std::string_view signature = {}) -> std::uint64_t;
    static auto entriesFromNames(const std::vector<std::string>& names) -> std::vector<NekoRpcMethodEntry>;

    auto reset() -> void;
    auto reset(std::vector<NekoRpcMethodEntry> entries, std::uint64_t version) -> void;
    auto resetFromNames(const std::vector<std::string>& names, std::uint64_t version) -> void;
    auto applyRemoteTable(std::vector<NekoRpcMethodEntry> entries, std::uint64_t version) -> bool;
    auto applyRemoteDelta(const std::vector<NekoRpcMethodEntry>& entries, std::uint64_t version) -> bool;

    auto upsert(std::string name, std::uint64_t signature_hash = 0) -> const NekoRpcMethodEntry*;
    auto remove(std::string_view name) -> bool;
    auto resolve(std::uint64_t id, std::uint64_t client_version, std::uint64_t signature_hash,
                 bool require_signature_hash) const -> NekoRpcMethodIdResolution;

    auto version() const noexcept -> std::uint64_t { return mVersion; }
    auto minimumCompatibleVersion() const noexcept -> std::uint64_t { return mMinimumCompatibleVersion; }
    auto setMinimumCompatibleVersion(std::uint64_t version) noexcept -> void;
    auto empty() const noexcept -> bool { return mEntries.empty(); }
    auto maxEntries() const noexcept -> std::size_t { return mMaxEntries; }
    auto entries() const noexcept -> const std::vector<NekoRpcMethodEntry>& { return mEntries; }
    auto findByName(std::string_view name) const -> const NekoRpcMethodEntry*;
    auto findById(std::uint64_t id) const -> const NekoRpcMethodEntry*;

private:
    auto _resetValidated(std::vector<NekoRpcMethodEntry> entries, std::uint64_t version) -> void;
    auto _bumpVersion() -> void;
    auto _rebuildIndex() -> void;
    auto _installEntry(NekoRpcMethodEntry entry) -> bool;
    auto _validTable(const std::vector<NekoRpcMethodEntry>& entries, std::uint64_t version,
                     bool require_contiguous_ids) const -> bool;

    std::uint64_t mVersion                  = 0;
    std::uint64_t mMinimumCompatibleVersion = 0;
    std::uint64_t mNextId                   = 0;
    std::vector<NekoRpcMethodEntry> mEntries;
    std::map<std::string, std::uint64_t, std::less<>> mNameToId;
    std::size_t mMaxEntries = DefaultMaxEntries;
};

} // namespace rpc
NEKO_END_NAMESPACE
