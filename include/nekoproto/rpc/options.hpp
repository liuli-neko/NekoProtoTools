#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {
struct RpcRequestContextAccess;
}

// Identity and attributes are supplied by the service provider after it has
// accepted/authenticated an endpoint. The RPC layer never derives an identity
// from an address and never sends these server-side attributes to the peer.
struct RpcPeerInfo {
    std::string id;
    std::map<std::string, std::string> attributes;
};

// Public server-side view of the invocation currently entering a handler.
// It deliberately contains no server pointer, backend session pointer, global
// load, or data belonging to another connection.
class RpcRequestContext {
public:
    using Clock = std::chrono::steady_clock;

    RpcRequestContext(const RpcRequestContext&) = delete;
    auto operator=(const RpcRequestContext&) -> RpcRequestContext& = delete;
    RpcRequestContext(RpcRequestContext&&) noexcept = default;
    auto operator=(RpcRequestContext&&) noexcept -> RpcRequestContext& = default;

    auto method() const noexcept -> std::string_view { return mMethod; }
    auto requestId() const noexcept -> std::string_view { return mRequestId; }
    auto deadline() const noexcept -> const std::optional<Clock::time_point>& { return mDeadline; }
    auto cancellationToken() const noexcept -> std::stop_token { return mCancellationToken; }
    auto peer() const noexcept -> const RpcPeerInfo& {
        static const RpcPeerInfo kAnonymousPeer;
        return mPeer != nullptr ? *mPeer : kAnonymousPeer;
    }
    auto stopRequested() const noexcept -> bool { return mCancellationToken.stop_requested(); }

    auto remaining() const noexcept -> std::optional<std::chrono::nanoseconds> {
        if (!mDeadline.has_value()) {
            return std::nullopt;
        }
        return std::chrono::duration_cast<std::chrono::nanoseconds>(*mDeadline - Clock::now());
    }

private:
    RpcRequestContext() = default;

    std::string_view mMethod;
    std::string mRequestId;
    std::optional<Clock::time_point> mDeadline;
    std::stop_token mCancellationToken;
    const RpcPeerInfo* mPeer = nullptr;
    const void* mSession = nullptr;

    friend struct detail::RpcRequestContextAccess;
};

// Internal construction/access shim keeps transport/session identity out of
// the public context API while allowing the dispatcher to scope introspection.
namespace detail {
struct RpcRequestContextAccess {
    static auto make(std::string_view method, std::string requestId,
                     std::optional<RpcRequestContext::Clock::time_point> deadline, const RpcPeerInfo* peer,
                     const void* session) -> RpcRequestContext {
        RpcRequestContext context;
        context.mMethod = method;
        context.mRequestId = std::move(requestId);
        context.mDeadline = deadline;
        context.mPeer = peer;
        context.mSession = session;
        return context;
    }

    static auto session(const RpcRequestContext& context) noexcept -> const void* { return context.mSession; }
    static auto setCancellationToken(RpcRequestContext& context, std::stop_token token) noexcept -> void {
        context.mCancellationToken = std::move(token);
    }
};
} // namespace detail

// Per-call controls are local to the client. A relative timeout starts when
// the call API is entered; when both values are present, the earlier limit
// wins. Expiration never permits a late response to satisfy another call.
struct RpcCallOptions {
    using Clock = std::chrono::steady_clock;

    std::optional<Clock::time_point> deadline;
    std::optional<std::chrono::nanoseconds> timeout;
    std::stop_token cancellation_token;
};

// A point-in-time, lock-free snapshot. active/queued are gauges; the other
// fields are monotonic counters for the lifetime of the client or server.
struct RpcMetricsSnapshot {
    std::size_t active = 0;
    std::size_t queued = 0;
    std::uint64_t completed = 0;
    std::uint64_t timed_out = 0;
    std::uint64_t canceled = 0;
    std::uint64_t rejected = 0;
};

NEKO_END_NAMESPACE
