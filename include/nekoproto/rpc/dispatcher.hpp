#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <compare>
#include <ilias/io/system_error.hpp>
#include <ilias/platform.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/sync/semaphore.hpp>
#include <ilias/task/group.hpp>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "nekoproto/global/log.hpp"
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/method.hpp"
#include "nekoproto/rpc/options.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

class RpcExecutionLimiter {
public:
    class Admission {
    public:
        Admission() = default;
        Admission(const Admission&) = delete;
        auto operator=(const Admission&) -> Admission& = delete;

        Admission(Admission&& other) noexcept
            : mOwner(std::exchange(other.mOwner, nullptr)), mQueued(other.mQueued), mStarted(other.mStarted) {}

        auto operator=(Admission&& other) noexcept -> Admission& {
            if (this != std::addressof(other)) {
                _release();
                mOwner = std::exchange(other.mOwner, nullptr);
                mQueued = other.mQueued;
                mStarted = other.mStarted;
            }
            return *this;
        }

        ~Admission() { _release(); }

        auto start() -> void {
            if (mOwner != nullptr && !mStarted) {
                mOwner->_start(*this);
            }
        }

    private:
        Admission(RpcExecutionLimiter& owner, bool queued) : mOwner(std::addressof(owner)), mQueued(queued) {}

        auto _release() -> void {
            if (mOwner != nullptr) {
                mOwner->_release(*this);
                mOwner = nullptr;
            }
        }

        RpcExecutionLimiter* mOwner = nullptr;
        bool mQueued = false;
        bool mStarted = false;

        friend class RpcExecutionLimiter;
    };

    RpcExecutionLimiter() { configure(4096U, 4096U); }

    auto configure(std::size_t maxActive, std::size_t maxQueued) -> void {
        const auto semaphoreMax = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
        mMaxActive = std::min(maxActive, semaphoreMax);
        mMaxQueued = maxQueued;
        mPermits = std::make_unique<ilias::Semaphore>(static_cast<std::ptrdiff_t>(mMaxActive));
    }

    auto tryAdmit() -> std::optional<Admission> {
        std::scoped_lock lock(mAdmissionMutex);
        if (mMaxActive == 0U) {
            mRejected.fetch_add(1U, std::memory_order_relaxed);
            return std::nullopt;
        }

        const bool queued = mOutstanding >= mMaxActive;
        if (queued && mOutstanding - mMaxActive >= mMaxQueued) {
            mRejected.fetch_add(1U, std::memory_order_relaxed);
            return std::nullopt;
        }

        ++mOutstanding;
        if (queued) {
            mQueued.fetch_add(1U, std::memory_order_relaxed);
        }
        return Admission(*this, queued);
    }

    auto reject() -> void { mRejected.fetch_add(1U, std::memory_order_relaxed); }
    auto acquire() { return mPermits->acquire(); }

    auto markTimedOut() -> void { mTimedOut.fetch_add(1U, std::memory_order_relaxed); }
    auto markCanceled(std::size_t count = 1U) -> void {
        mCanceled.fetch_add(static_cast<std::uint64_t>(count), std::memory_order_relaxed);
    }

    auto metrics() const noexcept -> RpcMetricsSnapshot {
        return {.active = mActive.load(std::memory_order_relaxed),
                .queued = mQueued.load(std::memory_order_relaxed),
                .completed = mCompleted.load(std::memory_order_relaxed),
                .timed_out = mTimedOut.load(std::memory_order_relaxed),
                .canceled = mCanceled.load(std::memory_order_relaxed),
                .rejected = mRejected.load(std::memory_order_relaxed)};
    }

private:
    auto _start(Admission& admission) -> void {
        if (admission.mQueued) {
            mQueued.fetch_sub(1U, std::memory_order_relaxed);
        }
        mActive.fetch_add(1U, std::memory_order_relaxed);
        admission.mStarted = true;
    }

    auto _release(const Admission& admission) -> void {
        if (admission.mStarted) {
            mActive.fetch_sub(1U, std::memory_order_relaxed);
            mCompleted.fetch_add(1U, std::memory_order_relaxed);
        } else if (admission.mQueued) {
            mQueued.fetch_sub(1U, std::memory_order_relaxed);
        }
        std::scoped_lock lock(mAdmissionMutex);
        --mOutstanding;
    }

    std::unique_ptr<ilias::Semaphore> mPermits;
    mutable std::mutex mAdmissionMutex;
    std::size_t mMaxActive = 0;
    std::size_t mMaxQueued = 0;
    std::size_t mOutstanding = 0;
    std::atomic<std::size_t> mActive{0};
    std::atomic<std::size_t> mQueued{0};
    std::atomic<std::uint64_t> mCompleted{0};
    std::atomic<std::uint64_t> mTimedOut{0};
    std::atomic<std::uint64_t> mCanceled{0};
    std::atomic<std::uint64_t> mRejected{0};
};

template <RpcBackend Backend>
class RpcMethodWrapperBase {
public:
    RpcMethodWrapperBase()                       = default;
    RpcMethodWrapperBase(RpcMethodWrapperBase&&) = default;
    virtual ~RpcMethodWrapperBase()              = default;

    virtual auto call(const typename Backend::DecodedRequest& request,
                      typename Backend::ResponseValues& responses, const RpcRequestContext* context)
        -> ilias::Task<void> = 0;
    virtual auto usesContext() const noexcept -> bool = 0;
    virtual auto name() noexcept -> std::string_view                                             = 0;
    virtual auto signature() noexcept -> std::string_view                                        = 0;
    virtual auto description() noexcept -> std::string_view                                      = 0;
    virtual auto rpcVersion() noexcept -> std::string_view                                       = 0;
    virtual auto argNames() -> std::vector<std::string>                                          = 0;
    virtual auto isNotification() noexcept -> bool                                               = 0;
    virtual auto isBind() noexcept -> bool                                                       = 0;
};

template <typename T>
struct RpcMethodTypeHelper {
    using MethodType = T;
};

template <typename T>
struct RpcMethodTypeHelper<T*> {
    using MethodType = T;
};

template <typename T>
struct RpcMethodTypeHelper<std::unique_ptr<T>> {
    using MethodType = T;
};

template <RpcBackend Backend>
class RpcDispatcher;

template <RpcBackend Backend, typename T>
class RpcMethodWrapperImpl : public RpcMethodWrapperBase<Backend> {
public:
    using MethodType = typename RpcMethodTypeHelper<T>::MethodType;
    RpcMethodWrapperImpl(T methodData, RpcDispatcher<Backend>* self)
        : mMethodData(std::move(methodData)), mSelf(self) {}

    auto call(const typename Backend::DecodedRequest& request, typename Backend::ResponseValues& responses,
              const RpcRequestContext* context)
        -> ilias::Task<void> override;
    auto usesContext() const noexcept -> bool override { return false; }
    auto name() noexcept -> std::string_view override { return mMethodData->name(); }
    auto signature() noexcept -> std::string_view override { return mMethodData->signature; }
    auto description() noexcept -> std::string_view override { return mMethodData->description(); }
    auto rpcVersion() noexcept -> std::string_view override { return mMethodData->rpcVersion(); }
    auto argNames() -> std::vector<std::string> override { return mMethodData->rpcArgNames(); }
    auto isNotification() noexcept -> bool override { return mMethodData->isNotification(); }
    auto isBind() noexcept -> bool override { return (bool)(*mMethodData); }

private:
    T mMethodData;
    RpcDispatcher<Backend>* mSelf = nullptr;
};

template <RpcBackend Backend, typename T, typename ContextFunction>
class RpcContextMethodWrapperImpl : public RpcMethodWrapperBase<Backend> {
public:
    RpcContextMethodWrapperImpl(T methodData, ContextFunction function, RpcDispatcher<Backend>* self)
        : mMethodData(std::move(methodData)), mFunction(std::move(function)), mSelf(self) {}

    auto call(const typename Backend::DecodedRequest& request, typename Backend::ResponseValues& responses,
              const RpcRequestContext* context) -> ilias::Task<void> override;
    auto usesContext() const noexcept -> bool override { return true; }
    auto name() noexcept -> std::string_view override { return mMethodData->name(); }
    auto signature() noexcept -> std::string_view override { return mMethodData->signature; }
    auto description() noexcept -> std::string_view override { return mMethodData->description(); }
    auto rpcVersion() noexcept -> std::string_view override { return mMethodData->rpcVersion(); }
    auto argNames() -> std::vector<std::string> override { return mMethodData->rpcArgNames(); }
    auto isNotification() noexcept -> bool override { return mMethodData->isNotification(); }
    auto isBind() noexcept -> bool override { return static_cast<bool>(mFunction); }

private:
    T mMethodData;
    ContextFunction mFunction;
    RpcDispatcher<Backend>* mSelf = nullptr;
};

template <RpcBackend Backend>
class RpcDispatcher {
public:
    using Id      = typename Backend::Id;
    using Message = typename Backend::Message;

private:
    struct ActiveKey {
        const void* session = nullptr;
        Id id{};
        auto operator<=>(const ActiveKey&) const = default;
    };

    enum class RequestState : std::uint8_t { Queued, Running };

public:

    struct MethodData {
        std::string_view name;
        std::string_view signature;
        std::string_view description;
        std::string_view rpcVersion;
        std::vector<std::string> argNames;
        bool isNotification = false;
        bool isBind         = false;
    };

    explicit RpcDispatcher([[maybe_unused]] ilias::IoContext& ctx) {}
    ~RpcDispatcher() {
        try {
            cancelAll();
        } catch (...) {
            // Destruction cannot surface a mutex/runtime shutdown failure.
        }
    }

    auto configureExecution(std::size_t maxActive, std::size_t maxQueued,
                            std::optional<std::chrono::nanoseconds> requestTimeout) -> void {
        mExecution.configure(maxActive, maxQueued);
        mRequestTimeout = requestTimeout;
    }

    auto metrics() const noexcept -> RpcMetricsSnapshot { return mExecution.metrics(); }

    template <typename T>
    auto registerRpcMethod(T& metadata) -> void {
        const auto name = std::string(metadata.name());
        if (auto item = mHandlers.find(name); item != mHandlers.end()) {
            NEKO_LOG_ERROR("rpc", "Method {} exist!!!", metadata.name());
            return;
        }
        mHandlers[name] = std::make_unique<RpcMethodWrapperImpl<Backend, T*>>(&metadata, this);
    }

    template <typename RetT, typename... Args, std::size_t N>
    auto bindRpcMethod(std::string_view name, traits::FunctionT<RetT(Args...)> func,
                       const std::array<std::string_view, N>& arg_names) -> void {
        return _registerRpcMethod<RpcMethodDynamic<RetT(Args...)>>(arg_names, name, std::move(func));
    }

    template <typename RetT, typename... Args, std::size_t N>
    auto bindRpcMethod(std::string_view name, traits::FunctionT<ilias::IoTask<RetT>(Args...)> func,
                       const std::array<std::string_view, N>& arg_names) -> void {
        _registerRpcMethod<RpcMethodDynamic<RetT(Args...)>>(arg_names, name, std::move(func));
    }

    template <typename Method, typename RetT, typename... Args>
    auto bindRpcMethodWithContext(
        Method& metadata,
        traits::FunctionT<ilias::IoTask<RetT>(const RpcRequestContext&, Args...)> func) -> void {
        static_assert(std::is_same_v<typename std::decay_t<Method>::RawReturnType, RetT>);
        static_assert(std::is_same_v<typename std::decay_t<Method>::RawParamsType, std::tuple<Args...>>);
        using Function = traits::FunctionT<ilias::IoTask<RetT>(const RpcRequestContext&, Args...)>;
        const auto name = std::string(metadata.name());
        mHandlers[name] = std::make_unique<RpcContextMethodWrapperImpl<Backend, Method*, Function>>(
            std::addressof(metadata), std::move(func), this);
    }

    template <typename RetT, typename... Args, std::size_t N>
    auto bindRpcMethodWithContext(
        std::string_view name,
        traits::FunctionT<ilias::IoTask<RetT>(const RpcRequestContext&, Args...)> func,
        const std::array<std::string_view, N>& argNames) -> void {
        static_assert(N == 0 || N == sizeof...(Args),
                      "bindRpcMethodWithContext: parameter names must match public RPC parameters");
        using Metadata = RpcMethodDynamic<RetT(Args...)>;
        using Function = traits::FunctionT<ilias::IoTask<RetT>(const RpcRequestContext&, Args...)>;
        auto metadata = std::make_unique<Metadata>(argNames, name, false);
        const auto key = std::string(metadata->name());
        mHandlers[key] =
            std::make_unique<RpcContextMethodWrapperImpl<Backend, std::unique_ptr<Metadata>, Function>>(
                std::move(metadata), std::move(func), this);
    }

    auto connectionStatus(const RpcRequestContext& context) const -> std::map<std::string, std::uint64_t> {
        std::map<std::string, std::uint64_t> status{{"active", 0U}, {"queued", 0U}, {"in_flight", 0U}};
        const auto* session = RpcRequestContextAccess::session(context);
        std::scoped_lock lock(mActiveMutex);
        for (const auto& [key, state] : mRequestStates) {
            if (key.session != session || _stringifyId(key.id) == context.requestId()) {
                continue;
            }
            if (state == RequestState::Running) {
                ++status["active"];
            } else {
                ++status["queued"];
            }
            ++status["in_flight"];
        }
        return status;
    }

    auto connectionTasks(const RpcRequestContext& context) const -> std::map<std::string, std::string> {
        std::map<std::string, std::string> tasks;
        const auto* session = RpcRequestContextAccess::session(context);
        std::scoped_lock lock(mActiveMutex);
        for (const auto& [key, state] : mRequestStates) {
            const auto requestId = _stringifyId(key.id);
            if (key.session == session && requestId != context.requestId()) {
                tasks.emplace(requestId, state == RequestState::Running ? "running" : "queued");
            }
        }
        return tasks;
    }

    auto connectionTimeout(const RpcRequestContext& context) const -> std::optional<std::chrono::nanoseconds> {
        const auto* session = RpcRequestContextAccess::session(context);
        std::scoped_lock lock(mActiveMutex);
        if (auto item = mConnectionTimeouts.find(session); item != mConnectionTimeouts.end()) {
            return item->second;
        }
        return std::nullopt;
    }

    auto setConnectionTimeout(const RpcRequestContext& context, std::uint64_t timeoutNanoseconds)
        -> ilias::Result<std::uint64_t, std::error_code> {
        const auto* session = RpcRequestContextAccess::session(context);
        std::scoped_lock lock(mActiveMutex);
        if (timeoutNanoseconds == 0U) {
            mConnectionTimeouts.erase(session);
            return 0U;
        }
        if (timeoutNanoseconds > static_cast<std::uint64_t>(std::chrono::nanoseconds::max().count())) {
            return ilias::Err(RpcError::InvalidParams);
        }

        const auto requested = std::chrono::nanoseconds(static_cast<std::int64_t>(timeoutNanoseconds));
        if (mRequestTimeout.has_value() && (mRequestTimeout->count() <= 0 || requested >= *mRequestTimeout)) {
            return ilias::Err(RpcError::InvalidParams);
        }
        mConnectionTimeouts.insert_or_assign(session, requested);
        return timeoutNanoseconds;
    }

    auto methodDatas() -> std::vector<MethodData> {
        std::vector<MethodData> metas;
        for (auto& [name, handler] : mHandlers) {
            metas.push_back({.name           = handler->name(),
                             .signature      = handler->signature(),
                             .description    = handler->description(),
                             .rpcVersion     = handler->rpcVersion(),
                             .argNames       = handler->argNames(),
                             .isNotification = handler->isNotification(),
                             .isBind         = handler->isBind()});
        }
        return metas;
    }

    auto methodDatas(std::string_view name) -> MethodData {
        if (auto item = mHandlers.find(std::string(name)); item != mHandlers.end()) {
            return {.name           = item->second->name(),
                    .signature      = item->second->signature(),
                    .description    = item->second->description(),
                    .rpcVersion     = item->second->rpcVersion(),
                    .argNames       = item->second->argNames(),
                    .isNotification = item->second->isNotification(),
                    .isBind         = item->second->isBind()};
        }
        return {};
    }

    auto processMessage(std::span<const std::byte> data, typename Backend::ServerContext& context,
                        typename Backend::PeerSession& session, IMessageEndpoint* endpoint = nullptr,
                        ilias::Mutex* endpointSendMutex = nullptr, bool rejectOnly = false,
                        const RpcPeerInfo* peer = nullptr)
        -> ilias::Task<Message> {
        Message buffer;
        auto decoded = Backend::decodeIncoming(context, session, data);
        if (!decoded.ok) {
            NEKO_LOG_ERROR("rpc", "parse rpc request failed");
            co_return buffer;
        }
        NEKO_LOG_TRACE("rpc", "rpc dispatcher message decoded: requests={} prebuilt_responses={} batch={}",
                       decoded.requests.size(), decoded.responses.size(), decoded.batch);

        typename Backend::ResponseValues responses;
        // The backend may already have protocol-level error responses, for
        // example method-id resolution failures. Dispatcher only adds
        // application method results after this point.
        responses.insert(responses.end(), decoded.responses.begin(), decoded.responses.end());
        std::vector<typename Backend::ResponseValues> responseSlots(decoded.requests.size());
        std::vector<ActiveKey> activeKeys;
        activeKeys.reserve(decoded.requests.size());
        ilias::TaskGroup<void> requestTasks;
        for (std::size_t index = 0; index < decoded.requests.size(); ++index) {
            const auto& request = decoded.requests[index];
            const auto method_name = std::string(Backend::methodName(request));
            const auto handler = mHandlers.find(method_name);
            if (handler == mHandlers.end()) {
                NEKO_LOG_WARN("rpc", "method {} not found!", method_name);
                Backend::appendError(responseSlots[index], request, RpcError::MethodNotFound);
                continue;
            }

            NEKO_LOG_TRACE("rpc", "rpc dispatcher method dispatch: method={}", method_name);
            if (rejectOnly) {
                mExecution.reject();
                Backend::appendError(responseSlots[index], request, RpcError::Overloaded);
            } else {
                auto admission = mExecution.tryAdmit();
                if (!admission.has_value()) {
                    Backend::appendError(responseSlots[index], request, RpcError::Overloaded);
                    continue;
                }
                std::optional<ActiveKey> activeKey;
                if (Backend::expectsResponse(request)) {
                    activeKey = ActiveKey{.session = std::addressof(session), .id = Backend::id(request)};
                    {
                        std::scoped_lock lock(mActiveMutex);
                        mRequestStates.insert_or_assign(*activeKey, RequestState::Queued);
                    }
                }
                const auto effectiveTimeout = _effectiveRequestTimeout(std::addressof(session));
                const auto deadline = _deadlineFromNow(effectiveTimeout);
                auto handle = requestTasks.spawn(_executeRequest(*handler->second, request, responseSlots[index],
                                                                 std::move(*admission), activeKey,
                                                                 std::addressof(session), peer, effectiveTimeout,
                                                                 deadline));
                if (activeKey.has_value()) {
                    {
                        std::scoped_lock lock(mActiveMutex);
                        mCancelHandles.insert_or_assign(*activeKey, std::move(handle));
                    }
                    activeKeys.push_back(std::move(*activeKey));
                }
            }
        }

        co_await requestTasks.waitAll();
        NEKO_LOG_TRACE("rpc", "Finished processing request, batch size {}", decoded.requests.size());
        for (std::size_t index = 0; index < decoded.requests.size(); ++index) {
            const auto& request = decoded.requests[index];
            if (Backend::expectsResponse(request) && responseSlots[index].empty()) {
                // A stopped handler task cannot resume far enough to append its
                // own return value. Complete the protocol request explicitly so
                // the peer does not wait forever after server-side cancellation.
                Backend::appendError(responseSlots[index], request, ilias::IoError::Canceled);
            }
        }
        {
            std::scoped_lock lock(mActiveMutex);
            for (const auto& key : activeKeys) {
                mCancelHandles.erase(key);
                mRequestStates.erase(key);
            }
        }
        for (auto& slot : responseSlots) {
            responses.insert(responses.end(), std::make_move_iterator(slot.begin()), std::make_move_iterator(slot.end()));
        }

        buffer = Backend::encodeResponses(context, session, responses, decoded.batch);
        if (!buffer.empty()) {
            if constexpr (requires { Backend::validateMessage(context, buffer); }) {
                auto validated = Backend::validateMessage(context, buffer);
                if (!validated) {
                    NEKO_LOG_ERROR("rpc", "rpc response violates configured message limits: {}",
                                   validated.error().message());
                    buffer.clear();
                }
            }
        }
        NEKO_LOG_TRACE("rpc", "rpc dispatcher response encoded: responses={} bytes={}", responses.size(),
                       buffer.size());

        if (endpoint != nullptr && !responses.empty() && buffer.empty()) {
            endpoint->close();
            co_return buffer;
        }

        if (endpoint != nullptr && !buffer.empty()) {
            const auto sendResponse = [endpoint, &buffer](const auto& ret) {
                if (!ret || ret.value() != buffer.size()) {
                    NEKO_LOG_ERROR("rpc", "send rpc response failed: {}",
                                   ret ? "short write" : ret.error().message());
                    endpoint->close();
                }
            };
            if (endpointSendMutex != nullptr) {
                auto guard = co_await endpointSendMutex->lock();
                sendResponse(co_await endpoint->send(
                    {reinterpret_cast<const std::byte*>(buffer.data()), static_cast<std::size_t>(buffer.size())}));
            } else {
                sendResponse(co_await endpoint->send(
                    {reinterpret_cast<const std::byte*>(buffer.data()), static_cast<std::size_t>(buffer.size())}));
            }
        }
        co_return buffer;
    }

    void cancel(const void* session, const Id& id) {
        std::scoped_lock lock(mActiveMutex);
        if (auto it = mCancelHandles.find(ActiveKey{.session = session, .id = id}); it != mCancelHandles.end()) {
            it->second.stop();
            mExecution.markCanceled();
        }
    }

    void cancel(const Id& id) {
        std::scoped_lock lock(mActiveMutex);
        for (auto& [key, handle] : mCancelHandles) {
            if (key.id == id) {
                handle.stop();
                mExecution.markCanceled();
            }
        }
    }

    void cancelAll() {
        std::scoped_lock lock(mActiveMutex);
        mExecution.markCanceled(mCancelHandles.size());
        for (auto& [id, handle] : mCancelHandles) {
            handle.stop();
        }
        mCancelHandles.clear();
        mRequestStates.clear();
        mConnectionTimeouts.clear();
    }

    void cancelSession(const void* session) {
        std::scoped_lock lock(mActiveMutex);
        std::size_t canceled = 0U;
        for (auto it = mCancelHandles.begin(); it != mCancelHandles.end();) {
            if (it->first.session == session) {
                it->second.stop();
                it = mCancelHandles.erase(it);
                ++canceled;
            } else {
                ++it;
            }
        }
        mExecution.markCanceled(canceled);
        std::erase_if(mRequestStates, [session](const auto& item) { return item.first.session == session; });
        mConnectionTimeouts.erase(session);
    }

    auto getCurrentIds() const -> std::vector<Id> {
        std::scoped_lock lock(mActiveMutex);
        std::vector<Id> ids;
        ids.reserve(mCancelHandles.size());
        for (const auto& [key, handle] : mCancelHandles) {
            static_cast<void>(handle);
            ids.push_back(key.id);
        }
        return ids;
    }

private:
    auto _executeRequest(RpcMethodWrapperBase<Backend>& handler, const typename Backend::DecodedRequest& request,
                         typename Backend::ResponseValues& responses, RpcExecutionLimiter::Admission admission,
                         std::optional<ActiveKey> activeKey, const void* session, const RpcPeerInfo* peer,
                         std::optional<std::chrono::nanoseconds> effectiveTimeout,
                         std::optional<RpcRequestContext::Clock::time_point> deadline)
        -> ilias::Task<void> {
        if (effectiveTimeout.has_value()) {
            if (effectiveTimeout->count() <= 0) {
                mExecution.markTimedOut();
                Backend::appendError(responses, request, RpcError::DeadlineExceeded);
                co_return;
            }
            const auto remaining = *deadline - RpcRequestContext::Clock::now();
            if (remaining <= RpcRequestContext::Clock::duration::zero()) {
                mExecution.markTimedOut();
                Backend::appendError(responses, request, RpcError::DeadlineExceeded);
                co_return;
            }
            auto completed = co_await ilias::timeout(
                _runAdmittedRequest(handler, request, responses, std::move(admission), std::move(activeKey),
                                    deadline, session, peer),
                remaining);
            if (!completed) {
                mExecution.markTimedOut();
                Backend::appendError(responses, request, RpcError::DeadlineExceeded);
            }
            co_return;
        }

        co_await _runAdmittedRequest(handler, request, responses, std::move(admission), std::move(activeKey),
                                    deadline, session, peer);
    }

    auto _runAdmittedRequest(RpcMethodWrapperBase<Backend>& handler,
                             const typename Backend::DecodedRequest& request,
                             typename Backend::ResponseValues& responses, RpcExecutionLimiter::Admission admission,
                             std::optional<ActiveKey> activeKey,
                             std::optional<RpcRequestContext::Clock::time_point> deadline, const void* session,
                             const RpcPeerInfo* peer)
        -> ilias::Task<void> {
        auto permit = co_await mExecution.acquire();
        admission.start();
        if (activeKey.has_value()) {
            std::scoped_lock lock(mActiveMutex);
            if (auto state = mRequestStates.find(*activeKey); state != mRequestStates.end()) {
                state->second = RequestState::Running;
            }
        }
        std::optional<RpcRequestContext> context;
        if (handler.usesContext()) {
            context.emplace(RpcRequestContextAccess::make(Backend::methodName(request),
                                                          _stringifyId(Backend::id(request)), deadline, peer, session));
            RpcRequestContextAccess::setCancellationToken(*context, co_await ilias::this_coro::stopToken());
        }
        co_await handler.call(request, responses, context.has_value() ? std::addressof(*context) : nullptr);
    }

    auto _effectiveRequestTimeout(const void* session) const -> std::optional<std::chrono::nanoseconds> {
        std::optional<std::chrono::nanoseconds> connectionTimeout;
        {
            std::scoped_lock lock(mActiveMutex);
            if (auto item = mConnectionTimeouts.find(session); item != mConnectionTimeouts.end()) {
                connectionTimeout = item->second;
            }
        }
        if (!connectionTimeout.has_value()) {
            return mRequestTimeout;
        }
        if (!mRequestTimeout.has_value()) {
            return connectionTimeout;
        }
        return std::min(*connectionTimeout, *mRequestTimeout);
    }

    static auto _deadlineFromNow(const std::optional<std::chrono::nanoseconds>& timeout)
        -> std::optional<RpcRequestContext::Clock::time_point> {
        if (!timeout.has_value() || timeout->count() <= 0) {
            return std::nullopt;
        }
        const auto now = RpcRequestContext::Clock::now();
        const auto delay = std::chrono::duration_cast<RpcRequestContext::Clock::duration>(*timeout);
        if (delay >= RpcRequestContext::Clock::time_point::max() - now) {
            return RpcRequestContext::Clock::time_point::max();
        }
        return now + delay;
    }

    template <typename T, size_t N, typename Callable>
    auto _registerRpcMethod(const std::array<std::string_view, N>& array, std::string_view name,
                            traits::FunctionT<Callable> func) -> void {
        auto method    = std::make_unique<T>(array, name, std::move(func));
        const auto key = std::string(method->name());
        mHandlers[key] = std::make_unique<RpcMethodWrapperImpl<Backend, std::unique_ptr<T>>>(std::move(method), this);
    }

    template <typename T>
    auto _handle(const typename Backend::DecodedRequest& request, typename Backend::ResponseValues& responses,
                 T& metadata) -> ilias::Task<void> {
        auto decoded_params = Backend::template decodeParams<T>(request, metadata);

        if (!decoded_params) {
            NEKO_LOG_WARN("rpc", "rpc dispatcher parameter decode failed: method={} error={}", metadata.name(),
                          decoded_params.error().message());
            Backend::appendError(responses, request, decoded_params.error());
            co_return;
        }

        // From here onward we are in user-code territory. Backend decoding has
        // already converted wire bytes into typed parameters.
        NEKO_LOG_TRACE("rpc", "rpc dispatcher invoke begin: method={}", metadata.name());
        ilias::Result<typename T::RawReturnType, std::error_code> result = ilias::Err(ilias::SystemError::Canceled);
        try {
            result = co_await Backend::template invoke<T>(metadata, std::move(decoded_params.value()));
        } catch (...) {
            NEKO_LOG_ERROR("rpc", "rpc handler threw an exception: method={}", metadata.name());
            result = ilias::Err(RpcError::InternalError);
        }
        Backend::template appendMethodReturn<T>(responses, request, std::move(result));
        NEKO_LOG_TRACE("rpc", "rpc dispatcher invoke end: method={}", metadata.name());
    }

    template <typename T, typename Function>
    auto _handleWithContext(const typename Backend::DecodedRequest& request,
                            typename Backend::ResponseValues& responses, T& metadata, Function& function,
                            const RpcRequestContext& context) -> ilias::Task<void> {
        auto decodedParams = Backend::template decodeParams<T>(request, metadata);
        if (!decodedParams) {
            Backend::appendError(responses, request, decodedParams.error());
            co_return;
        }

        NEKO_LOG_TRACE("rpc", "rpc dispatcher context invoke begin: method={}", metadata.name());
        ilias::Result<typename T::RawReturnType, std::error_code> result =
            ilias::Err(ilias::SystemError::Canceled);
        try {
            result = co_await std::apply(
                [&](auto&&... args) {
                    return function(context, std::forward<decltype(args)>(args)...);
                },
                std::move(decodedParams.value()));
        } catch (...) {
            NEKO_LOG_ERROR("rpc", "rpc context handler threw an exception: method={}", metadata.name());
            result = ilias::Err(RpcError::InternalError);
        }
        Backend::template appendMethodReturn<T>(responses, request, std::move(result));
        NEKO_LOG_TRACE("rpc", "rpc dispatcher context invoke end: method={}", metadata.name());
    }

    template <typename Value>
    static auto _stringifyId(const Value& value) -> std::string {
        using Type = std::remove_cvref_t<Value>;
        if constexpr (std::is_same_v<Type, std::monostate>) {
            return {};
        } else if constexpr (std::is_integral_v<Type>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<Type, std::string>) {
            return value;
        } else if constexpr (requires { std::variant_size<Type>::value; }) {
            return std::visit([](const auto& active) { return _stringifyId(active); }, value);
        } else {
            return {};
        }
    }

private:
    std::map<ActiveKey, ilias::StopHandle> mCancelHandles;
    std::map<ActiveKey, RequestState> mRequestStates;
    std::map<const void*, std::chrono::nanoseconds> mConnectionTimeouts;
    mutable std::mutex mActiveMutex;
    std::map<std::string, std::unique_ptr<RpcMethodWrapperBase<Backend>>> mHandlers;
    RpcExecutionLimiter mExecution;
    std::optional<std::chrono::nanoseconds> mRequestTimeout;

    template <RpcBackend B, typename T>
    friend class RpcMethodWrapperImpl;
    template <RpcBackend B, typename T, typename Function>
    friend class RpcContextMethodWrapperImpl;
};

template <RpcBackend Backend, typename T>
auto RpcMethodWrapperImpl<Backend, T>::call(const typename Backend::DecodedRequest& request,
                                            typename Backend::ResponseValues& responses,
                                            const RpcRequestContext* /*context*/) -> ilias::Task<void> {
    return mSelf->_handle(request, responses, *mMethodData);
}

template <RpcBackend Backend, typename T, typename ContextFunction>
auto RpcContextMethodWrapperImpl<Backend, T, ContextFunction>::call(
    const typename Backend::DecodedRequest& request, typename Backend::ResponseValues& responses,
    const RpcRequestContext* context) -> ilias::Task<void> {
    if (context == nullptr) {
        co_return;
    }
    co_await mSelf->_handleWithContext(request, responses, *mMethodData, mFunction, *context);
}

} // namespace detail

NEKO_END_NAMESPACE
