#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <ilias/platform.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/sync/oneshot.hpp>
#include <ilias/task/scope.hpp>
#include <ilias/task.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "nekoproto/global/log.hpp"
#include "nekoproto/rpc/builtin.hpp"
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/options.hpp"
#include "nekoproto/rpc/registry.hpp"

NEKO_BEGIN_NAMESPACE

template <RpcBackend Backend, typename... ProtocolSets>
class RpcClient : public ProtocolSets... {

public:
    RpcBuiltinMethods rpc;
    static constexpr int BuiltinMethodsCount = Reflect<RpcBuiltinMethods>::value_count;

    explicit RpcClient(ilias::IoContext& ctx) : RpcClient(ctx, typename Backend::Options{}) {}

    explicit RpcClient(ilias::IoContext& /*unused*/, typename Backend::Options options)
        : ProtocolSets()..., rpc(), mBackendContext(Backend::makeClientContext(std::move(options))),
          mPeerSession(Backend::makeClientPeerSession(mBackendContext)) {
        (_registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
        _registerProtocol(rpc, "rpc");
    }
    ~RpcClient() {
        try {
            close();
        } catch (...) {
            // Destruction cannot report a synchronous wait failure. The
            // endpoint has already been detached and closed by close().
        }
    }

    auto operator->() noexcept -> RpcClient* { return this; }
    auto operator->() const noexcept -> const RpcClient* { return this; }

    auto close() -> void {
        std::shared_ptr<detail::IMessageEndpoint> endpoint;
        std::shared_ptr<ilias::TaskScope> receiverScope;
        {
            std::scoped_lock lock(mStateMutex);
            endpoint         = std::exchange(mEndpoint, nullptr);
            receiverScope    = std::exchange(mReceiverScope, nullptr);
            mReceiverStarted = false;
        }
        if (endpoint != nullptr) {
            endpoint->close();
        }
        _failAllPending(Backend::clientNotInitError());
        if (receiverScope != nullptr) {
            receiverScope->stop();
            receiverScope->waitAll().wait();
        }
    }

    auto flush() -> ilias::IoTask<void> {
        std::shared_ptr<detail::IMessageEndpoint> endpoint;
        {
            std::scoped_lock lock(mStateMutex);
            endpoint = mEndpoint;
        }
        if (endpoint != nullptr) {
            auto guard = co_await mSendMutex.lock();
            co_return co_await endpoint->flush();
        }
        co_return {};
    }

    auto shutdown() -> ilias::IoTask<void> {
        std::shared_ptr<detail::IMessageEndpoint> endpoint;
        std::shared_ptr<ilias::TaskScope> receiverScope;
        {
            std::scoped_lock lock(mStateMutex);
            endpoint = mEndpoint;
        }
        if (endpoint != nullptr) {
            auto sendGuard = co_await mSendMutex.lock();
            std::error_code error;
            if (auto ret = co_await endpoint->flush(); !ret) {
                error = ret.error();
            } else if (auto ret = co_await endpoint->shutdown(); !ret) {
                error = ret.error();
            }
            endpoint->close();
            {
                std::scoped_lock lock(mStateMutex);
                if (mEndpoint == endpoint) {
                    mEndpoint.reset();
                    receiverScope    = std::exchange(mReceiverScope, nullptr);
                    mReceiverStarted = false;
                }
            }
            _failAllPending(error ? error : Backend::clientNotInitError());
            if (receiverScope != nullptr) {
                receiverScope->stop();
                co_await receiverScope->waitAll();
            }
            if (error) {
                co_return ilias::Err(error);
            }
        }
        co_return {};
    }

    auto isConnected() const -> bool {
        std::scoped_lock lock(mStateMutex);
        return mEndpoint != nullptr;
    }

    auto metrics() const -> RpcMetricsSnapshot {
        std::size_t pending = 0;
        {
            std::scoped_lock lock(mPendingMutex);
            pending = mPending.size();
        }
        return {.active = pending,
                .queued = 0,
                .completed = mCompleted.load(std::memory_order_relaxed),
                .timed_out = mTimedOut.load(std::memory_order_relaxed),
                .canceled = mCanceled.load(std::memory_order_relaxed),
                .rejected = mRejected.load(std::memory_order_relaxed)};
    }

    auto cancelRemote(const typename Backend::Id& id) -> ilias::IoTask<void>
        requires requires { Backend::encodeCancel(id); }
    {
        std::shared_ptr<detail::IMessageEndpoint> endpoint;
        {
            std::scoped_lock lock(mStateMutex);
            endpoint = mEndpoint;
        }
        if (endpoint == nullptr) {
            co_return ilias::Err(Backend::clientNotInitError());
        }
        const auto message = Backend::encodeCancel(id);
        if (message.empty()) {
            co_return ilias::Err(RpcError::InvalidRequest);
        }
        if constexpr (requires { Backend::validateMessage(mBackendContext, message); }) {
            auto validated = Backend::validateMessage(mBackendContext, message);
            if (!validated) {
                co_return ilias::Err(validated.error());
            }
        }
        auto sendGuard = co_await mSendMutex.lock();
        ILIAS_CO_TRY(auto sent,
                     co_await endpoint->send({reinterpret_cast<const std::byte*>(message.data()), message.size()}));
        if (sent != message.size()) {
            co_return ilias::Err(ilias::IoError::WriteZero);
        }
        _failPending(id, ilias::IoError::Canceled);
        co_return {};
    }

    template <MessageEndpoint EndpointT>
    auto setEndpoint(EndpointT endpoint) -> void {
        std::shared_ptr<detail::IMessageEndpoint> replacement;
        if constexpr (detail::is_message_endpoint<EndpointT>::value) {
            replacement = std::make_shared<EndpointT>(std::move(endpoint));
        } else {
            replacement = std::make_shared<detail::MessageEndpointWrapper<EndpointT>>(std::move(endpoint));
        }
        close();
        std::scoped_lock lock(mStateMutex);
        mEndpoint         = std::move(replacement);
        mReceiverScope    = std::make_shared<ilias::TaskScope>();
        mReceiverStarted  = false;
        mResetPeerSession = true;
    }

    template <typename StreamT>
        requires detail::RpcStreamBackend<Backend, StreamT>
    auto setEndpoint(StreamT stream) -> void {
        if constexpr (requires(StreamT value) {
                          { Backend::makeClientEndpoint(std::move(value), mBackendContext.options) } -> MessageEndpoint;
                      }) {
            setEndpoint(Backend::makeClientEndpoint(std::move(stream), mBackendContext.options));
        } else if constexpr (requires(StreamT value) {
                                 { Backend::makeClientEndpoint(std::move(value)) } -> MessageEndpoint;
                             }) {
            setEndpoint(Backend::makeClientEndpoint(std::move(stream)));
        } else if constexpr (requires(StreamT value) {
                                 {
                                     Backend::makeEndpoint(std::move(value), mBackendContext.options)
                                 } -> MessageEndpoint;
                             }) {
            setEndpoint(Backend::makeEndpoint(std::move(stream), mBackendContext.options));
        } else {
            setEndpoint(Backend::makeEndpoint(std::move(stream)));
        }
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto callRemote(std::string_view name, Args... args) -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto callRemoteWithOptions(std::string_view name, RpcCallOptions options, Args... args) -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

    template <typename Method, typename... Args>
        requires requires { typename std::decay_t<Method>::RawReturnType; }
    auto callRemoteWithOptions(Method& metadata, RpcCallOptions options, Args... args)
        -> ilias::IoTask<typename std::decay_t<Method>::RawReturnType> {
        co_return co_await _callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemote(Args... args) -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemoteWithOptions(RpcCallOptions options, Args... args)
        -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemoteWithOptions(std::string_view name, RpcCallOptions options, Args... args) -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemote(Args... args) -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemoteWithOptions(RpcCallOptions options, Args... args)
        -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

private:
    template <typename Protocol>
    void _registerProtocol(Protocol& protocol, std::string_view prefix = {}) {
        detail::for_each_rpc_method(protocol, [this](auto& method) { this->_registerRpcMethod(method); }, prefix);
    }

    template <typename T>
    void _registerRpcMethod(T& metadata) {
        metadata = (typename std::decay_t<T>::CoroutinesFuncType)[this, &metadata](auto... args)
                       ->ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
            return this->_callRemoteWithOptions<T, decltype(args)...>(metadata, {},
                                                                      std::forward<decltype(args)>(args)...);
        };
    }

    template <typename T, typename... Args>
    auto _callRemoteWithOptions(T& metadata, RpcCallOptions options, Args... args)
        -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
        struct CompletionGuard {
            std::atomic<std::uint64_t>& completed;
            ~CompletionGuard() { completed.fetch_add(1U, std::memory_order_relaxed); }
        } completionGuard{mCompleted};

        const auto started = RpcCallOptions::Clock::now();
        if (options.cancellation_token.stop_requested()) {
            mCanceled.fetch_add(1U, std::memory_order_relaxed);
            co_return ilias::Err(ilias::IoError::Canceled);
        }

        auto remaining = _remainingWait(options, started);
        if (remaining.has_value() && remaining->count() <= 0) {
            mTimedOut.fetch_add(1U, std::memory_order_relaxed);
            co_return ilias::Err(RpcError::DeadlineExceeded);
        }

        auto call = _callRemote(metadata, std::forward<Args>(args)...);
        if (remaining.has_value() && options.cancellation_token.stop_possible()) {
            auto [result, canceled, timedOut] =
                co_await ilias::whenAny(std::move(call), options.cancellation_token, ilias::sleep(*remaining));
            if (result) {
                co_return std::move(*result);
            }
            if (canceled) {
                mCanceled.fetch_add(1U, std::memory_order_relaxed);
                co_return ilias::Err(ilias::IoError::Canceled);
            }
            static_cast<void>(timedOut);
            mTimedOut.fetch_add(1U, std::memory_order_relaxed);
            co_return ilias::Err(RpcError::DeadlineExceeded);
        }

        if (remaining.has_value()) {
            auto result = co_await ilias::timeout(std::move(call), *remaining);
            if (!result) {
                mTimedOut.fetch_add(1U, std::memory_order_relaxed);
                co_return ilias::Err(RpcError::DeadlineExceeded);
            }
            co_return std::move(*result);
        }

        if (options.cancellation_token.stop_possible()) {
            auto [result, canceled] = co_await ilias::whenAny(std::move(call), options.cancellation_token);
            if (result) {
                co_return std::move(*result);
            }
            static_cast<void>(canceled);
            mCanceled.fetch_add(1U, std::memory_order_relaxed);
            co_return ilias::Err(ilias::IoError::Canceled);
        }

        co_return co_await std::move(call);
    }

    static auto _remainingWait(const RpcCallOptions& options, RpcCallOptions::Clock::time_point started)
        -> std::optional<std::chrono::nanoseconds> {
        const auto now = RpcCallOptions::Clock::now();
        std::optional<std::chrono::nanoseconds> remaining;
        if (options.timeout.has_value()) {
            remaining = *options.timeout - std::chrono::duration_cast<std::chrono::nanoseconds>(now - started);
        }
        if (options.deadline.has_value()) {
            const auto deadlineRemaining =
                std::chrono::duration_cast<std::chrono::nanoseconds>(*options.deadline - now);
            if (!remaining.has_value() || deadlineRemaining < *remaining) {
                remaining = deadlineRemaining;
            }
        }
        return remaining;
    }

    template <typename T, typename... Args>
    auto _callRemote(T& metadata, Args... args) -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
        if constexpr (!requires(std::span<const std::byte> message) {
                          { Backend::responseId(message) } -> std::same_as<std::optional<typename Backend::Id>>;
                      }) {
            co_return co_await _callRemoteSerial(metadata, std::forward<Args>(args)...);
        }

        NEKO_LOG_TRACE("rpc", "rpc client call begin: method={} notification={}", metadata.name(),
                       metadata.isNotification());
        std::shared_ptr<detail::IMessageEndpoint> endpoint;
        {
            std::scoped_lock lock(mStateMutex);
            endpoint = mEndpoint;
        }
        if (endpoint == nullptr) {
            co_return ilias::Err(Backend::clientNotInitError());
        }
        // Reject an already-full public pending-call budget before starting
        // protocol negotiation or a background receive loop. The locked check
        // at insertion remains authoritative when callers race for capacity.
        if (!metadata.isNotification()) {
            std::scoped_lock lock(mPendingMutex);
            if (mPending.size() >= _maxPendingCalls()) {
                mRejected.fetch_add(1U, std::memory_order_relaxed);
                co_return ilias::Err(ilias::IoError::WouldBlock);
            }
        }
        ILIAS_CO_TRYV(co_await _ensureReceiver(endpoint));

        std::size_t retry_count = 0;
        while (true) {
            typename Backend::EncodedRequest request;
            auto encoded = Backend::template encodeRequest<T>(mBackendContext, mPeerSession, metadata,
                                                              metadata.isNotification(), args...);
            if (!encoded) {
                co_return ilias::Err(encoded.error());
            }
            request = std::move(encoded.value());

            std::optional<ilias::oneshot::Receiver<PendingResult>> receiver;
            if (!metadata.isNotification()) {
                auto channel = ilias::oneshot::channel<PendingResult>();
                {
                    std::scoped_lock lock(mPendingMutex);
                    if (mPending.size() >= _maxPendingCalls() || mPending.contains(request.id)) {
                        mRejected.fetch_add(1U, std::memory_order_relaxed);
                        co_return ilias::Err(ilias::IoError::WouldBlock);
                    }
                    mPending.emplace(request.id, std::move(channel.sender));
                }
                receiver.emplace(std::move(channel.receiver));
            }
            PendingEraseGuard pendingGuard{this, request.id, !metadata.isNotification()};

            std::size_t send_ret = 0;
            {
                auto sendGuard = co_await mSendMutex.lock();
                ILIAS_CO_TRY(auto written,
                             co_await endpoint->send(
                                 {reinterpret_cast<const std::byte*>(request.message.data()), request.message.size()}));
                send_ret = written;
            }
            if (send_ret != request.message.size()) {
                co_return ilias::Err(ilias::IoError::Other);
            }
            NEKO_LOG_TRACE("rpc", "rpc client request send: method={} bytes={}", metadata.name(),
                           request.message.size());
            if (metadata.isNotification()) {
                NEKO_LOG_TRACE("rpc", "rpc client notification sent: method={}", metadata.name());
                co_return ilias::Err(Backend::notificationOk());
            }

            auto delivered = co_await std::move(*receiver);
            if (!delivered.has_value()) {
                co_return ilias::Err(ilias::IoError::Canceled);
            }
            auto wireResult = std::move(*delivered);
            if (!wireResult) {
                co_return ilias::Err(wireResult.error());
            }
            pendingGuard.active = false;

            ilias::Result<typename std::decay_t<T>::RawReturnType, std::error_code> decoded;
            {
                decoded =
                    Backend::template decodeResponse<T>(mBackendContext, mPeerSession, wireResult.value(), request.id);
            }
            if (decoded) {
                NEKO_LOG_TRACE("rpc", "rpc client call end: method={}", metadata.name());
                if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                    co_return {};
                } else {
                    co_return std::move(decoded.value());
                }
            }

            bool should_retry = false;
            if constexpr (requires {
                              Backend::recoverClientCall(mBackendContext, mPeerSession, *endpoint, decoded.error(),
                                                         retry_count);
                          }) {
                auto protocolGuard = co_await mProtocolMutex.lock();
                ILIAS_CO_TRY(auto recovered,
                             co_await Backend::recoverClientCall(mBackendContext, mPeerSession, *endpoint,
                                                                 decoded.error(), retry_count));
                should_retry = recovered;
            }
            if (!should_retry) {
                co_return ilias::Err(decoded.error());
            }
            ++retry_count;
            NEKO_LOG_INFO("rpc", "rpc client retrying call after backend recovery: method={} retry={}", metadata.name(),
                          retry_count);
        }
    }

    template <typename T, typename... Args>
    auto _callRemoteSerial(T& metadata, Args... args) -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
        auto guard = co_await mFallbackMutex.lock();
        std::shared_ptr<detail::IMessageEndpoint> endpoint;
        {
            std::scoped_lock lock(mStateMutex);
            endpoint = mEndpoint;
        }
        if (endpoint == nullptr) {
            co_return ilias::Err(Backend::clientNotInitError());
        }
        std::size_t retry_count = 0;
        while (true) {
            ILIAS_CO_TRYV(co_await Backend::ensureClientReady(mBackendContext, mPeerSession, *endpoint));
            auto encoded = Backend::template encodeRequest<T>(mBackendContext, mPeerSession, metadata,
                                                              metadata.isNotification(), args...);
            if (!encoded) {
                co_return ilias::Err(encoded.error());
            }
            auto& request       = encoded.value();
            std::size_t sendRet = 0;
            {
                auto sendGuard = co_await mSendMutex.lock();
                ILIAS_CO_TRY(auto written,
                             co_await endpoint->send(
                                 {reinterpret_cast<const std::byte*>(request.message.data()), request.message.size()}));
                sendRet = written;
            }
            if (sendRet != request.message.size()) {
                co_return ilias::Err(ilias::IoError::WriteZero);
            }
            if (metadata.isNotification()) {
                co_return ilias::Err(Backend::notificationOk());
            }
            std::vector<std::byte> buffer;
            ILIAS_CO_TRY(auto received, co_await endpoint->recv(buffer));
            (void)received;
            auto decoded = Backend::template decodeResponse<T>(mBackendContext, mPeerSession, buffer, request.id);
            if (decoded) {
                if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                    co_return {};
                } else {
                    co_return std::move(decoded.value());
                }
            }
            bool shouldRetry = false;
            if constexpr (requires {
                              Backend::recoverClientCall(mBackendContext, mPeerSession, *endpoint, decoded.error(),
                                                         retry_count);
                          }) {
                ILIAS_CO_TRY(auto recovered,
                             co_await Backend::recoverClientCall(mBackendContext, mPeerSession, *endpoint,
                                                                 decoded.error(), retry_count));
                shouldRetry = recovered;
            }
            if (!shouldRetry) {
                co_return ilias::Err(decoded.error());
            }
            ++retry_count;
        }
    }

    auto _ensureReceiver(const std::shared_ptr<detail::IMessageEndpoint>& endpoint) -> ilias::IoTask<void> {
        auto protocolGuard = co_await mProtocolMutex.lock();
        if (mResetPeerSession.exchange(false)) {
            mPeerSession = Backend::makeClientPeerSession(mBackendContext);
        }
        {
            auto sendGuard = co_await mSendMutex.lock();
            ILIAS_CO_TRYV(co_await Backend::ensureClientReady(mBackendContext, mPeerSession, *endpoint));
        }

        std::shared_ptr<ilias::TaskScope> receiverScope;
        {
            std::scoped_lock lock(mStateMutex);
            if (mEndpoint != endpoint || mReceiverScope == nullptr) {
                co_return ilias::Err(Backend::clientNotInitError());
            }
            if (mReceiverStarted) {
                co_return {};
            }
            mReceiverStarted = true;
            receiverScope    = mReceiverScope;
        }
        receiverScope->spawn([this, endpoint]() -> ilias::Task<void> { co_await _receiveLoop(endpoint); });
        co_return {};
    }

    auto _receiveLoop(std::shared_ptr<detail::IMessageEndpoint> endpoint) -> ilias::Task<void> {
        while (true) {
            std::vector<std::byte> buffer;
            auto received = co_await endpoint->recv(buffer);
            if (!received || received.value() == 0U) {
                const auto error = received ? make_error_code(ilias::IoError::UnexpectedEOF) : received.error();
                _disconnect(endpoint, error);
                co_return;
            }
            if constexpr (requires { Backend::validateMessage(mBackendContext, buffer); }) {
                auto validated = Backend::validateMessage(mBackendContext, buffer);
                if (!validated) {
                    endpoint->close();
                    _disconnect(endpoint, validated.error());
                    co_return;
                }
            }
            if (Backend::handleClientControl(mBackendContext, mPeerSession, buffer)) {
                continue;
            }
            auto id = Backend::responseId(buffer);
            if (!id.has_value()) {
                NEKO_LOG_WARN("rpc", "rpc client ignored a frame without a response id");
                continue;
            }
            std::optional<ilias::oneshot::Sender<PendingResult>> sender;
            {
                std::scoped_lock lock(mPendingMutex);
                if (auto item = mPending.find(*id); item != mPending.end()) {
                    sender.emplace(std::move(item->second));
                    mPending.erase(item);
                }
            }
            if (sender.has_value()) {
                (void)sender->send(PendingResult(std::move(buffer)));
            } else {
                NEKO_LOG_WARN("rpc", "rpc client received a response for an unknown request id");
            }
        }
    }

    auto _maxPendingCalls() const noexcept -> std::size_t {
        if constexpr (requires { mBackendContext.options.max_pending_calls; }) {
            return mBackendContext.options.max_pending_calls;
        }
        return 1024U;
    }

    auto _erasePending(const typename Backend::Id& id) -> void {
        std::scoped_lock lock(mPendingMutex);
        mPending.erase(id);
    }

    auto _abandonPending(const typename Backend::Id& id) noexcept -> void {
        _erasePending(id);
        if constexpr (requires { Backend::encodeCancel(id); }) {
            std::shared_ptr<detail::IMessageEndpoint> endpoint;
            std::shared_ptr<ilias::TaskScope> receiverScope;
            {
                std::scoped_lock lock(mStateMutex);
                endpoint = mEndpoint;
                receiverScope = mReceiverScope;
            }
            if (endpoint == nullptr || receiverScope == nullptr) {
                return;
            }
            try {
                receiverScope->spawn([this, endpoint = std::move(endpoint), id]() -> ilias::Task<void> {
                    const auto message = Backend::encodeCancel(id);
                    if (message.empty()) {
                        co_return;
                    }
                    auto sendGuard = co_await mSendMutex.lock();
                    auto sent = co_await endpoint->send(
                        {reinterpret_cast<const std::byte*>(message.data()), message.size()});
                    if (!sent || sent.value() != message.size()) {
                        NEKO_LOG_WARN("rpc", "rpc client failed to send best-effort cancellation");
                    }
                });
            } catch (...) {
                // Abandoning a timed-out call must remain noexcept. Closing the
                // client will still stop all server work associated with it.
            }
        }
    }

    auto _failAllPending(std::error_code error) -> void {
        decltype(mPending) pending;
        {
            std::scoped_lock lock(mPendingMutex);
            pending.swap(mPending);
        }
        for (auto& [id, sender] : pending) {
            static_cast<void>(id);
            (void)sender.send(ilias::Err(error));
        }
    }

    auto _failPending(const typename Backend::Id& id, std::error_code error) -> void {
        std::optional<PendingSender> sender;
        {
            std::scoped_lock lock(mPendingMutex);
            if (auto item = mPending.find(id); item != mPending.end()) {
                sender.emplace(std::move(item->second));
                mPending.erase(item);
            }
        }
        if (sender.has_value()) {
            (void)sender->send(ilias::Err(error));
        }
    }

    auto _disconnect(const std::shared_ptr<detail::IMessageEndpoint>& endpoint, std::error_code error) -> void {
        {
            std::scoped_lock lock(mStateMutex);
            if (mEndpoint == endpoint) {
                mEndpoint.reset();
                mReceiverStarted = false;
            }
        }
        _failAllPending(error);
    }

private:
    using PendingResult = ilias::Result<std::vector<std::byte>, std::error_code>;
    using PendingSender = ilias::oneshot::Sender<PendingResult>;

    struct PendingEraseGuard {
        RpcClient* client;
        typename Backend::Id id;
        bool active;

        ~PendingEraseGuard() {
            if (active) {
                client->_abandonPending(id);
            }
        }
    };

    std::shared_ptr<detail::IMessageEndpoint> mEndpoint;
    std::shared_ptr<ilias::TaskScope> mReceiverScope;
    typename Backend::ClientContext mBackendContext;
    typename Backend::PeerSession mPeerSession;
    std::map<typename Backend::Id, PendingSender> mPending;
    ilias::Mutex mProtocolMutex;
    ilias::Mutex mFallbackMutex;
    ilias::Mutex mSendMutex;
    mutable std::mutex mStateMutex;
    mutable std::mutex mPendingMutex;
    std::atomic<bool> mResetPeerSession{false};
    std::atomic<std::uint64_t> mCompleted{0};
    std::atomic<std::uint64_t> mTimedOut{0};
    std::atomic<std::uint64_t> mCanceled{0};
    std::atomic<std::uint64_t> mRejected{0};
    bool mReceiverStarted = false;
};

NEKO_END_NAMESPACE
