#pragma once

#include <array>
#include <chrono>
#include <ilias/platform.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/sync/oneshot.hpp>
#include <ilias/task.hpp>
#include <ilias/task/scope.hpp>
#include <iostream>
#include <map>
#include <memory>
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

namespace nekoproto {

template <RpcBackend Backend>
class RpcClientBase {
public:
    RpcBuiltinMethods rpc;
    static constexpr int BuiltinMethodsCount = Reflect<RpcBuiltinMethods>::value_count;

private:
    using PendingResult = ilias::Result<std::vector<std::byte>, std::error_code>;
    using PendingSender = ilias::oneshot::Sender<PendingResult>;

    struct PendingEraseGuard {
        RpcClientBase* client;
        typename Backend::Id id;
        bool active;

        ~PendingEraseGuard() {
            if (active) {
                client->abandonPending(id);
            }
        }
    };

protected:
    std::shared_ptr<detail::IMessageEndpoint> mEndpoint;
    std::shared_ptr<ilias::TaskScope> mReceiverScope;
    typename Backend::ClientContext mBackendContext;
    typename Backend::PeerSession mPeerSession;
    std::map<typename Backend::Id, PendingSender> mPending;
    ilias::Mutex mProtocolMutex;
    ilias::Mutex mFallbackMutex;
    ilias::Mutex mSendMutex;
    bool mResetPeerSession = false;
    std::uint64_t mCompleted = 0;
    std::uint64_t mTimedOut  = 0;
    std::uint64_t mCanceled  = 0;
    std::uint64_t mRejected  = 0;
    bool mReceiverStarted    = false;

    static auto remainingWait(const RpcCallOptions& options, RpcCallOptions::Clock::time_point started)
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

    auto maxPendingCalls() const noexcept -> std::size_t {
        if constexpr (requires { mBackendContext.options.max_pending_calls; }) {
            return mBackendContext.options.max_pending_calls;
        }
        return 1024U;
    }

    void erasePending(const typename Backend::Id& id) {
        mPending.erase(id);
    }

    void abandonPending(const typename Backend::Id& id) noexcept {
        erasePending(id);
        if constexpr (requires { Backend::encodeCancel(id); }) {
            auto endpoint      = mEndpoint;
            auto receiverScope = mReceiverScope;
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
                    auto sent =
                        co_await endpoint->send({reinterpret_cast<const std::byte*>(message.data()), message.size()});
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

    void failAllPending(std::error_code error) {
        auto pending = std::move(mPending);
        mPending.clear();
        for (auto& [id, sender] : pending) {
            static_cast<void>(id);
            (void)sender.send(ilias::Err(error));
        }
    }

    void failPending(const typename Backend::Id& id, std::error_code error) {
        if (auto item = mPending.find(id); item != mPending.end()) {
            auto sender = std::move(item->second);
            mPending.erase(item);
            (void)sender.send(ilias::Err(error));
        }
    }

    void disconnect(const std::shared_ptr<detail::IMessageEndpoint>& endpoint, std::error_code error) {
        if (mEndpoint == endpoint) {
            mEndpoint.reset();
            mReceiverStarted = false;
        }
        failAllPending(error);
    }

    auto ensureReady() -> ilias::IoTask<std::shared_ptr<detail::IMessageEndpoint>> {
        auto endpoint = mEndpoint;
        if (endpoint == nullptr) {
            co_return ilias::Err(Backend::clientNotInitError());
        }
        if constexpr (requires(std::span<const std::byte> message) {
                          { Backend::responseId(message) } -> std::same_as<std::optional<typename Backend::Id>>;
                      }) {
            ILIAS_CO_TRYV(co_await ensureReceiver(endpoint));
        } else {
            ILIAS_CO_TRYV(co_await Backend::ensureClientReady(mBackendContext, mPeerSession, *endpoint));
        }
        co_return endpoint;
    }

    auto ensureReceiver(const std::shared_ptr<detail::IMessageEndpoint>& endpoint) -> ilias::IoTask<void> {
        auto protocolGuard = co_await mProtocolMutex.lock();
        if (std::exchange(mResetPeerSession, false)) {
            mPeerSession = Backend::makeClientPeerSession(mBackendContext);
        }
        {
            auto sendGuard = co_await mSendMutex.lock();
            ILIAS_CO_TRYV(co_await Backend::ensureClientReady(mBackendContext, mPeerSession, *endpoint));
        }

        if (mEndpoint != endpoint || mReceiverScope == nullptr) {
            co_return ilias::Err(Backend::clientNotInitError());
        }
        if (mReceiverStarted) {
            co_return {};
        }
        mReceiverStarted = true;
        auto receiverScope = mReceiverScope;
        receiverScope->spawn([this, endpoint]() -> ilias::Task<void> { co_await receiveLoop(endpoint); });
        co_return {};
    }

    auto receiveLoop(std::shared_ptr<detail::IMessageEndpoint> endpoint) -> ilias::Task<void> {
        while (true) {
            std::vector<std::byte> buffer;
            auto received = co_await endpoint->recv(buffer);
            if (!received || received.value() == 0U) {
                const auto error = received ? make_error_code(ilias::IoError::UnexpectedEOF) : received.error();
                disconnect(endpoint, error);
                co_return;
            }
            if constexpr (requires { Backend::validateMessage(mBackendContext, buffer); }) {
                auto validated = Backend::validateMessage(mBackendContext, buffer);
                if (!validated) {
                    endpoint->close();
                    disconnect(endpoint, validated.error());
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
            if (auto item = mPending.find(*id); item != mPending.end()) {
                sender.emplace(std::move(item->second));
                mPending.erase(item);
            }
            if (sender.has_value()) {
                (void)sender->send(PendingResult(std::move(buffer)));
            } else {
                NEKO_LOG_WARN("rpc", "rpc client received a response for an unknown request id");
            }
        }
    }

    auto sendAndReceiveSerial(const std::shared_ptr<detail::IMessageEndpoint>& endpoint,
                              typename Backend::EncodedRequest request, bool isNotification,
                              std::string_view /*methodName*/)
        -> ilias::IoTask<std::vector<std::byte>> {
        auto guard = co_await mFallbackMutex.lock();
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
        if (isNotification) {
            co_return std::vector<std::byte>{};
        }
        std::vector<std::byte> buffer;
        ILIAS_CO_TRY(auto received, co_await endpoint->recv(buffer));
        (void)received;
        co_return buffer;
    }

    auto sendAndReceive(const std::shared_ptr<detail::IMessageEndpoint>& endpoint,
                        typename Backend::EncodedRequest request, bool isNotification,
                        std::string_view methodName)
        -> ilias::IoTask<std::vector<std::byte>> {
        if constexpr (!requires(std::span<const std::byte> message) {
                          { Backend::responseId(message) } -> std::same_as<std::optional<typename Backend::Id>>;
                      }) {
            co_return co_await sendAndReceiveSerial(endpoint, std::move(request), isNotification, methodName);
        } else {
            NEKO_LOG_TRACE("rpc", "rpc client call begin: method={} notification={}", methodName, isNotification);
            if (!isNotification) {
                if (mPending.size() >= maxPendingCalls()) {
                    ++mRejected;
                    co_return ilias::Err(ilias::IoError::WouldBlock);
                }
            }

            std::optional<ilias::oneshot::Receiver<PendingResult>> receiver;
            if (!isNotification) {
                auto channel = ilias::oneshot::channel<PendingResult>();
                if (mPending.size() >= maxPendingCalls() || mPending.contains(request.id)) {
                    ++mRejected;
                    co_return ilias::Err(ilias::IoError::WouldBlock);
                }
                mPending.emplace(request.id, std::move(channel.sender));
                receiver.emplace(std::move(channel.receiver));
            }
            PendingEraseGuard pendingGuard{this, request.id, !isNotification};

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
            NEKO_LOG_TRACE("rpc", "rpc client request send: method={} bytes={}", methodName, request.message.size());
            if (isNotification) {
                NEKO_LOG_TRACE("rpc", "rpc client notification sent: method={}", methodName);
                co_return std::vector<std::byte>{};
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
            co_return std::move(wireResult.value());
        }
    }

    auto sendAndReceiveWithOptions(const std::shared_ptr<detail::IMessageEndpoint>& endpoint,
                                   typename Backend::EncodedRequest request, bool isNotification,
                                   std::string_view methodName, const RpcCallOptions& options,
                                   std::optional<std::chrono::nanoseconds> remaining)
        -> ilias::IoTask<std::vector<std::byte>> {
        if (!remaining.has_value() && !options.cancellation_token.stop_possible()) {
            co_return co_await sendAndReceive(endpoint, std::move(request), isNotification, methodName);
        }

        auto call = sendAndReceive(endpoint, std::move(request), isNotification, methodName);
        if (remaining.has_value() && options.cancellation_token.stop_possible()) {
            auto [result, canceled, timedOut] =
                co_await ilias::whenAny(std::move(call), options.cancellation_token, ilias::sleep(*remaining));
            if (result) {
                co_return std::move(*result);
            }
            if (canceled) {
                co_return ilias::Err(ilias::IoError::Canceled);
            }
            static_cast<void>(timedOut);
            co_return ilias::Err(RpcError::DeadlineExceeded);
        }

        if (remaining.has_value()) {
            auto result = co_await ilias::timeout(std::move(call), *remaining);
            if (!result) {
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
            co_return ilias::Err(ilias::IoError::Canceled);
        }

        co_return co_await std::move(call);
    }

    template <typename T>
    void registerRpcMethod(T& metadata) {
        metadata = (typename std::decay_t<T>::CoroutinesFuncType)[this, &metadata](auto... args)
                       -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
            return this->callRemoteWithOptions<T, decltype(args)...>(metadata, {},
                                                                     std::forward<decltype(args)>(args)...);
        };
    }

public:
    explicit RpcClientBase(ilias::IoContext& /*unused*/, typename Backend::Options options = {})
        : rpc(), mBackendContext(Backend::makeClientContext(std::move(options))),
          mPeerSession(Backend::makeClientPeerSession(mBackendContext)) {
        registerProtocol(rpc, "rpc");
    }

    ~RpcClientBase() {
        try {
            close();
        } catch (...) {
            // Destruction cannot report a synchronous wait failure. The
            // endpoint has already been detached and closed by close().
        }
    }

    template <typename Protocol>
    void registerProtocol(Protocol& protocol, std::string_view prefix = {}) {
        detail::forEachRpcMethod(protocol, [this](auto& method) { this->registerRpcMethod(method); }, prefix);
    }

    void close() {
        auto endpoint         = std::exchange(mEndpoint, nullptr);
        auto receiverScope    = std::exchange(mReceiverScope, nullptr);
        mReceiverStarted      = false;
        if (endpoint != nullptr) {
            endpoint->close();
        }
        failAllPending(Backend::clientNotInitError());
        if (receiverScope != nullptr) {
            receiverScope->stop();
            receiverScope->waitAll().wait();
        }
    }

    auto flush() -> ilias::IoTask<void> {
        auto endpoint = mEndpoint;
        if (endpoint != nullptr) {
            auto guard = co_await mSendMutex.lock();
            co_return co_await endpoint->flush();
        }
        co_return {};
    }

    auto shutdown() -> ilias::IoTask<void> {
        auto endpoint = mEndpoint;
        if (endpoint != nullptr) {
            auto sendGuard = co_await mSendMutex.lock();
            std::error_code error;
            if (auto ret = co_await endpoint->flush(); !ret) {
                error = ret.error();
            } else if (auto ret = co_await endpoint->shutdown(); !ret) {
                error = ret.error();
            }
            endpoint->close();
            if (mEndpoint == endpoint) {
                mEndpoint.reset();
                mReceiverScope.reset();
                mReceiverStarted = false;
            }
            failAllPending(error ? error : Backend::clientNotInitError());
            if (error) {
                co_return ilias::Err(error);
            }
        }
        co_return {};
    }

    auto isConnected() const -> bool {
        return mEndpoint != nullptr;
    }

    auto metrics() const -> RpcMetricsSnapshot {
        return {.active    = mPending.size(),
                .queued    = 0,
                .completed = mCompleted,
                .timed_out = mTimedOut,
                .canceled  = mCanceled,
                .rejected  = mRejected};
    }

    auto cancelRemote(const typename Backend::Id& id) -> ilias::IoTask<void>
        requires requires { Backend::encodeCancel(id); }
    {
        auto endpoint = mEndpoint;
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
        failPending(id, ilias::IoError::Canceled);
        co_return {};
    }

    template <MessageEndpoint EndpointT>
    void setEndpoint(EndpointT endpoint) {
        std::shared_ptr<detail::IMessageEndpoint> replacement;
        if constexpr (detail::IsMessageEndpoint<EndpointT>::value) {
            replacement = std::make_shared<EndpointT>(std::move(endpoint));
        } else {
            replacement = std::make_shared<detail::MessageEndpointWrapper<EndpointT>>(std::move(endpoint));
        }
        close();
        mEndpoint         = std::move(replacement);
        mReceiverScope    = std::make_shared<ilias::TaskScope>();
        mReceiverStarted  = false;
        mResetPeerSession = true;
    }

    template <typename StreamT>
        requires detail::RpcStreamBackend<Backend, StreamT>
    void setEndpoint(StreamT stream) {
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
        co_return co_await callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto callRemoteWithOptions(std::string_view name, RpcCallOptions options, Args... args) -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), false);
        co_return co_await callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    auto callRemote(T& metadata, Args... args) -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
        return callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    auto callRemoteWithOptions(T& metadata, RpcCallOptions options, Args... args)
        -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
        using RetType = typename std::decay_t<T>::RawReturnType;

        struct CompletionGuard {
            std::uint64_t& completed;
            ~CompletionGuard() { ++completed; }
        } completionGuard{mCompleted};

        const auto started = RpcCallOptions::Clock::now();
        if (options.cancellation_token.stop_requested()) {
            ++mCanceled;
            co_return ilias::Err(ilias::IoError::Canceled);
        }

        auto remaining = remainingWait(options, started);
        if (remaining.has_value() && remaining->count() <= 0) {
            ++mTimedOut;
            co_return ilias::Err(RpcError::DeadlineExceeded);
        }

        auto endpointRes = co_await ensureReady();
        if (!endpointRes) {
            co_return ilias::Err(endpointRes.error());
        }
        auto endpoint = std::move(endpointRes.value());

        std::size_t retry_count = 0;
        while (true) {
            auto encoded = Backend::template encodeRequest<T>(mBackendContext, mPeerSession, metadata,
                                                              metadata.isNotification(), args...);
            if (!encoded) {
                co_return ilias::Err(encoded.error());
            }
            const auto requestId = encoded->id;
            const bool isNotif   = metadata.isNotification();

            auto wireResult = co_await sendAndReceiveWithOptions(endpoint, std::move(encoded.value()), isNotif,
                                                                 metadata.name(), options, remaining);
            if (!wireResult) {
                if (wireResult.error() == RpcError::DeadlineExceeded) {
                    ++mTimedOut;
                } else if (wireResult.error() == ilias::IoError::Canceled) {
                    ++mCanceled;
                }
                co_return ilias::Err(wireResult.error());
            }
            if (isNotif) {
                co_return ilias::Err(Backend::notificationOk());
            }

            auto decoded = Backend::template decodeResponse<T>(mBackendContext, mPeerSession, *wireResult, requestId);
            if (decoded) {
                NEKO_LOG_TRACE("rpc", "rpc client call end: method={}", metadata.name());
                if constexpr (std::is_void_v<RetType>) {
                    co_return {};
                } else {
                    co_return std::move(decoded.value());
                }
            }

            bool should_retry = false;
            if constexpr (requires(detail::IMessageEndpoint& ep) {
                              Backend::recoverClientCall(mBackendContext, mPeerSession, ep, decoded.error(), retry_count);
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

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemote(Args... args) -> ilias::IoTask<typename traits::FunctionTraits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), false);
        co_return co_await callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemoteWithOptions(RpcCallOptions options, Args... args)
        -> ilias::IoTask<typename traits::FunctionTraits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), false);
        co_return co_await callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), true);
        co_return co_await callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemoteWithOptions(std::string_view name, RpcCallOptions options, Args... args) -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), true);
        co_return co_await callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemote(Args... args) -> ilias::IoTask<typename traits::FunctionTraits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), true);
        co_return co_await callRemoteWithOptions(metadata, {}, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemoteWithOptions(RpcCallOptions options, Args... args)
        -> ilias::IoTask<typename traits::FunctionTraits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), true);
        co_return co_await callRemoteWithOptions(metadata, std::move(options), std::forward<Args>(args)...);
    }
};

template <RpcBackend Backend, typename... ProtocolSets>
class RpcClient : public RpcClientBase<Backend>, public ProtocolSets... {
public:
    using RpcClientBase<Backend>::BuiltinMethodsCount;
    using RpcClientBase<Backend>::rpc;

    explicit RpcClient(ilias::IoContext& ctx) : RpcClient(ctx, typename Backend::Options{}) {}

    explicit RpcClient(ilias::IoContext& ctx, typename Backend::Options options)
        : RpcClientBase<Backend>(ctx, std::move(options)), ProtocolSets()... {
        if constexpr (sizeof...(ProtocolSets) > 0) {
            (this->registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
        }
    }

    auto operator->() noexcept -> RpcClient* { return this; }
    auto operator->() const noexcept -> const RpcClient* { return this; }
};

} // namespace nekoproto
