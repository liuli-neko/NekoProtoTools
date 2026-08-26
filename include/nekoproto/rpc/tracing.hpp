#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "nekoproto/global/config.h"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/options.hpp"

namespace nekoproto {

enum class RpcCallStage : std::uint8_t {
    Received = 0, // Frame received and decoded from network
    Queued,       // Admitted into coroutine context, waiting for execution permit
    Executing,    // Coroutine scheduled, handler executing
    Executed,     // Handler completed, response serialized
    Sending,      // Sending response across endpoint
    Completed,    // Execution and transmission fully completed
    Canceled,     // Canceled by client, server, or WebUI
    TimedOut,     // Timed out or deadline exceeded
    Rejected,     // Rejected due to server overload
    Failed,       // Handler exception or protocol error
};

inline auto toString(RpcCallStage stage) noexcept -> std::string_view {
    switch (stage) {
    case RpcCallStage::Received:  return "Received";
    case RpcCallStage::Queued:    return "Queued";
    case RpcCallStage::Executing: return "Executing";
    case RpcCallStage::Executed:  return "Executed";
    case RpcCallStage::Sending:   return "Sending";
    case RpcCallStage::Completed: return "Completed";
    case RpcCallStage::Canceled:  return "Canceled";
    case RpcCallStage::TimedOut:  return "TimedOut";
    case RpcCallStage::Rejected:  return "Rejected";
    case RpcCallStage::Failed:    return "Failed";
    default:                      return "Unknown";
    }
}

inline constexpr bool isTerminalStage(RpcCallStage stage) noexcept {
    return stage == RpcCallStage::Completed || stage == RpcCallStage::Canceled ||
           stage == RpcCallStage::TimedOut || stage == RpcCallStage::Rejected ||
           stage == RpcCallStage::Failed;
}

#if defined(NEKO_PROTO_RPC_TRACE)

using RpcMethodMetadata = detail::RpcMethodMetadata;

struct RpcServerConfigInfo {
    std::string backendName;
    std::size_t maxConcurrent = 0;
    std::size_t maxQueue      = 0;
    std::optional<std::chrono::nanoseconds> defaultTimeout;
    std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();
    std::vector<RpcMethodMetadata> methods;
};

struct RpcCallTraceSpan {
    std::uint64_t traceId = 0;
    std::string requestId;
    std::string methodName;
    RpcCallStage stage = RpcCallStage::Received;
    std::string peerId;
    std::map<std::string, std::string> peerAttributes;
    const void* session = nullptr;

    std::chrono::system_clock::time_point receivedTime{};
    std::chrono::steady_clock::time_point receivedAt{};
    std::chrono::steady_clock::time_point queuedAt{};
    std::chrono::steady_clock::time_point startedAt{};
    std::chrono::steady_clock::time_point executedAt{};
    std::chrono::steady_clock::time_point completedAt{};

    std::optional<std::chrono::nanoseconds> timeout;
    std::optional<std::chrono::steady_clock::time_point> deadline;

    std::size_t requestBytes = 0;
    std::size_t responseBytes = 0;
    std::string errorMessage;
    int errorCode = 0;
    bool isNotification = false;

    auto queueDurationMs() const noexcept -> double {
        if (startedAt == std::chrono::steady_clock::time_point{}) {
            if (queuedAt == std::chrono::steady_clock::time_point{}) return 0.0;
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - queuedAt).count();
        }
        if (queuedAt == std::chrono::steady_clock::time_point{}) return 0.0;
        return std::chrono::duration<double, std::milli>(startedAt - queuedAt).count();
    }

    auto execDurationMs() const noexcept -> double {
        if (startedAt == std::chrono::steady_clock::time_point{}) return 0.0;
        if (executedAt == std::chrono::steady_clock::time_point{}) {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startedAt).count();
        }
        return std::chrono::duration<double, std::milli>(executedAt - startedAt).count();
    }

    auto totalDurationMs() const noexcept -> double {
        if (receivedAt == std::chrono::steady_clock::time_point{}) return 0.0;
        if (completedAt == std::chrono::steady_clock::time_point{}) {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - receivedAt).count();
        }
        return std::chrono::duration<double, std::milli>(completedAt - receivedAt).count();
    }

    auto remainingDeadlineMs() const noexcept -> std::optional<double> {
        if (!deadline.has_value()) return std::nullopt;
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(*deadline - now).count();
    }
};

class RpcTraceRegistry {
public:
    using CancelHook = std::function<bool(const void* session, std::string_view requestId)>;

    static auto instance() -> RpcTraceRegistry&;

    auto registerSpan(std::string_view methodName, std::string requestId, std::size_t requestBytes,
                      bool isNotification, const RpcPeerInfo* peer, const void* session,
                      std::optional<std::chrono::steady_clock::time_point> deadline,
                      std::optional<std::chrono::nanoseconds> timeout) -> std::shared_ptr<RpcCallTraceSpan>;

    void markQueued(const std::shared_ptr<RpcCallTraceSpan>& span);
    void markExecuting(const std::shared_ptr<RpcCallTraceSpan>& span);
    void markExecuted(const std::shared_ptr<RpcCallTraceSpan>& span, std::size_t responseBytes,
                      std::error_code ec = {});
    void markSending(const std::shared_ptr<RpcCallTraceSpan>& span, std::size_t responseBytes = 0);
    void markCompleted(const std::shared_ptr<RpcCallTraceSpan>& span, std::size_t responseBytes = 0);
    void markCanceled(const std::shared_ptr<RpcCallTraceSpan>& span);
    void markTimedOut(const std::shared_ptr<RpcCallTraceSpan>& span);
    void markRejected(const std::shared_ptr<RpcCallTraceSpan>& span);
    void markFailed(const std::shared_ptr<RpcCallTraceSpan>& span, std::error_code ec,
                    std::string_view msg = {});

    using ServerInfoProvider = std::function<RpcServerConfigInfo()>;

    void registerCancelHook(CancelHook hook);
    void unregisterCancelHook();
    auto requestCancel(std::string_view requestId, const void* session = nullptr) -> bool;
    auto requestCancelByTraceId(std::uint64_t traceId) -> bool;

    void registerServerInfoProvider(ServerInfoProvider provider);
    void unregisterServerInfoProvider();
    auto serverInfoJson() -> std::string;

    auto snapshotJson() -> std::string;
    void clear();

    void setMaxHistorySize(std::size_t size) {
        std::scoped_lock lock(mMutex);
        mMaxHistory = size;
    }

private:
    std::mutex mMutex;
    std::uint64_t mNextTraceId = 1;
    std::size_t mMaxHistory = 100;

    std::unordered_map<std::uint64_t, std::shared_ptr<RpcCallTraceSpan>> mActive;
    std::deque<std::shared_ptr<RpcCallTraceSpan>> mRecentCompleted;
    std::deque<std::shared_ptr<RpcCallTraceSpan>> mRecentFailed;

    CancelHook mCancelHook;
    ServerInfoProvider mServerInfoProvider;

    // Cumulative stats
    std::uint64_t mTotalCompleted = 0;
    std::uint64_t mTotalTimedOut = 0;
    std::uint64_t mTotalCanceled = 0;
    std::uint64_t mTotalRejected = 0;
    std::uint64_t mTotalFailed = 0;
};

class RpcTraceContext {
public:
    RpcTraceContext() = default;

    void onReceived(std::string_view methodName, std::string requestId, std::size_t requestBytes,
                    bool isNotification, const RpcPeerInfo* peer, const void* session,
                    std::optional<std::chrono::steady_clock::time_point> deadline,
                    std::optional<std::chrono::nanoseconds> timeout) {
        mSpan = RpcTraceRegistry::instance().registerSpan(
            methodName, std::move(requestId), requestBytes, isNotification, peer, session, deadline, timeout);
    }

    void onQueued() {
        if (mSpan) {
            RpcTraceRegistry::instance().markQueued(mSpan);
        }
    }

    void onExecuting() {
        if (mSpan) {
            RpcTraceRegistry::instance().markExecuting(mSpan);
        }
    }

    void onExecuted(std::size_t responseBytes, std::error_code ec = {}) {
        if (mSpan) {
            RpcTraceRegistry::instance().markExecuted(mSpan, responseBytes, ec);
        }
    }

    void onSending(std::size_t responseBytes = 0) {
        if (mSpan) {
            RpcTraceRegistry::instance().markSending(mSpan, responseBytes);
        }
    }

    void onCompleted(std::size_t responseBytes = 0) {
        if (mSpan) {
            RpcTraceRegistry::instance().markCompleted(mSpan, responseBytes);
            mSpan.reset();
        }
    }

    void onCanceled() {
        if (mSpan) {
            RpcTraceRegistry::instance().markCanceled(mSpan);
            mSpan.reset();
        }
    }

    void onTimedOut() {
        if (mSpan) {
            RpcTraceRegistry::instance().markTimedOut(mSpan);
            mSpan.reset();
        }
    }

    void onRejected() {
        if (mSpan) {
            RpcTraceRegistry::instance().markRejected(mSpan);
            mSpan.reset();
        }
    }

    void onFailed(std::error_code ec, std::string_view msg = {}) {
        if (mSpan) {
            RpcTraceRegistry::instance().markFailed(mSpan, ec, msg);
            mSpan.reset();
        }
    }

    auto span() const noexcept -> const std::shared_ptr<RpcCallTraceSpan>& { return mSpan; }

private:
    std::shared_ptr<RpcCallTraceSpan> mSpan;
};

#else // !defined(NEKO_PROTO_RPC_TRACE) - Zero-overhead no-op stubs

struct RpcCallTraceSpan {};

class RpcTraceRegistry {
public:
    using CancelHook = std::function<bool(const void*, std::string_view)>;
    using ServerInfoProvider = std::function<void()>;
    static auto instance() -> RpcTraceRegistry& {
        static RpcTraceRegistry sInstance;
        return sInstance;
    }
    void registerCancelHook(CancelHook) noexcept {}
    void unregisterCancelHook() noexcept {}
    auto requestCancel(std::string_view, const void* = nullptr) noexcept -> bool { return false; }
    auto requestCancelByTraceId(std::uint64_t) noexcept -> bool { return false; }
    template <typename F>
    void registerServerInfoProvider(F&&) noexcept {}
    void unregisterServerInfoProvider() noexcept {}
    auto serverInfoJson() -> std::string { return "{\"server\":{},\"methods\":[]}"; }
    auto snapshotJson() -> std::string { return "{\"metrics\":{},\"active\":[],\"recent_completed\":[],\"recent_failed\":[]}"; }
    void clear() noexcept {}
};

class RpcTraceContext {
public:
    constexpr void onReceived(std::string_view, std::string_view, std::size_t, bool, const RpcPeerInfo*,
                              const void*, std::optional<std::chrono::steady_clock::time_point>,
                              std::optional<std::chrono::nanoseconds>) noexcept {}
    constexpr void onQueued() noexcept {}
    constexpr void onExecuting() noexcept {}
    constexpr void onExecuted(std::size_t, std::error_code = {}) noexcept {}
    constexpr void onSending(std::size_t = 0) noexcept {}
    constexpr void onCompleted(std::size_t = 0) noexcept {}
    constexpr void onCanceled() noexcept {}
    constexpr void onTimedOut() noexcept {}
    constexpr void onRejected() noexcept {}
    constexpr void onFailed(std::error_code, std::string_view = {}) noexcept {}
};

#endif // defined(NEKO_PROTO_RPC_TRACE)

} // namespace nekoproto

