#pragma once

#include <array>
#include <chrono>
#include <concepts>
#include <functional>
#include <ilias/platform.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/task/group.hpp>
#include <ilias/task/scope.hpp>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "nekoproto/global/log.hpp"
#include "nekoproto/rpc/builtin.hpp"
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/dispatcher.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/registry.hpp"

NEKO_BEGIN_NAMESPACE

template <RpcBackend Backend, typename... ProtocolSets>
class RpcServer : public ProtocolSets... {

private:
    using Dispatcher = detail::RpcDispatcher<Backend>;
    using MethodData = typename Dispatcher::MethodData;

    struct EndpointSlot {
        std::unique_ptr<detail::IMessageEndpoint> endpoint;
        typename Backend::PeerSession session;
        RpcPeerInfo peer;
        std::unique_ptr<ilias::TaskScope> requests = std::make_unique<ilias::TaskScope>();
        ilias::Mutex sendMutex;
    };

    RpcBuiltinMethods mRpc;

public:
    explicit RpcServer(ilias::IoContext& ctx) : RpcServer(ctx, typename Backend::Options{}) {}

    explicit RpcServer(ilias::IoContext& ctx, typename Backend::Options options)
        : ProtocolSets()..., mRpc(), mDispatcher(ctx), mBackendContext(), mScope() {
        _init();
        mBackendContext = Backend::makeServerContext(std::move(options), methodDatas());
        _configureExecution();
    }
    ~RpcServer() {
        try {
            close();
        } catch (...) {
            // Destruction cannot propagate a synchronous cancellation failure.
        }
    }

    auto operator->() noexcept -> RpcServer* { return this; }
    auto operator->() const noexcept -> const RpcServer* { return this; }

    auto close() -> void {
        auto endpoints = _beginClose();
        // Cancel endpoint and request tasks before destroying their streams.
        // Closing a stream concurrently with a just-started read can race in
        // some endpoint implementations; framed reads are cancellation-aware.
        mScope.stop();
        wait().wait();
        for (auto& slot : endpoints) {
            slot->endpoint->close();
        }
    }

    auto flush() -> ilias::IoTask<void> {
        auto endpoints = _snapshotEndpoints();
        for (auto& slot : endpoints) {
            auto guard = co_await slot->sendMutex.lock();
            if (auto ret = co_await slot->endpoint->flush(); !ret) {
                co_return ilias::Err(ret.error());
            }
        }
        co_return {};
    }

    auto shutdown() -> ilias::Task<void> {
        auto endpoints = _beginClose();
        for (auto& slot : endpoints) {
            auto guard = co_await slot->sendMutex.lock();
            if (auto ret = co_await slot->endpoint->flush(); !ret) {
                NEKO_LOG_WARN("rpc", "flush rpc endpoint failed: {}", ret.error().message());
            }
        }
        for (auto& slot : endpoints) {
            auto guard = co_await slot->sendMutex.lock();
            if (auto ret = co_await slot->endpoint->shutdown(); !ret) {
                NEKO_LOG_WARN("rpc", "shutdown rpc endpoint failed: {}", ret.error().message());
            }
        }
        co_await mScope.shutdown();
        for (auto& slot : endpoints) {
            // Closing will cause some internal resources to be released
            // so we need close it after the scope is shutdown.
            auto guard = co_await slot->sendMutex.lock();
            slot->endpoint->close();
        }
    }

    auto wait() -> ilias::Task<void> { co_await mScope.waitAll(); }
    auto cancel(const typename Backend::Id& id) -> void { mDispatcher.cancel(id); }
    auto cancelAll() -> void { mDispatcher.cancelAll(); }
    auto getCurrentIds() const -> std::vector<typename Backend::Id> { return mDispatcher.getCurrentIds(); }
    auto metrics() const noexcept -> RpcMetricsSnapshot { return mDispatcher.metrics(); }

    template <MessageEndpoint EndpointT>
    auto addEndpoint(EndpointT endpoint, RpcPeerInfo peer = {}) -> void {
        auto slot = std::make_shared<EndpointSlot>();
        slot->peer = std::move(peer);
        if constexpr (detail::is_message_endpoint<EndpointT>::value) {
            slot->endpoint = std::make_unique<EndpointT>(std::move(endpoint));
        } else {
            using Wrapper  = detail::MessageEndpointWrapper<EndpointT>;
            slot->endpoint = std::make_unique<Wrapper>(std::move(endpoint));
        }
        _handleEndpoint(slot);
    }

    template <typename StreamT>
        requires detail::RpcStreamBackend<Backend, StreamT>
    auto addEndpoint(StreamT stream, RpcPeerInfo peer = {}) -> void {
        if constexpr (requires(StreamT value, std::vector<MethodData> methods) {
                          {
                              Backend::makeServerEndpoint(std::move(value), std::move(methods), mBackendContext.options)
                          } -> MessageEndpoint;
                      }) {
            addEndpoint(Backend::makeServerEndpoint(std::move(stream), methodDatas(), mBackendContext.options),
                        std::move(peer));
        } else if constexpr (requires(StreamT value, std::vector<MethodData> methods) {
                                 {
                                     Backend::makeServerEndpoint(std::move(value), std::move(methods))
                                 } -> MessageEndpoint;
                             }) {
            addEndpoint(Backend::makeServerEndpoint(std::move(stream), methodDatas()), std::move(peer));
        } else if constexpr (requires(StreamT value) {
                                 {
                                     Backend::makeEndpoint(std::move(value), mBackendContext.options)
                                 } -> MessageEndpoint;
                             }) {
            addEndpoint(Backend::makeEndpoint(std::move(stream), mBackendContext.options), std::move(peer));
        } else {
            addEndpoint(Backend::makeEndpoint(std::move(stream)), std::move(peer));
        }
    }

    // Runtime-name binding API:
    //   server.bindMethod<"lhs", "rhs">("calc.add", func);
    // Use this when the protocol surface is not expressed as a reflected
    // RpcMethod field, or when a method is added after construction.
    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<RetT(Args...)> func) -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mDispatcher.bindRpcMethod(name, std::move(func),
                                  std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...});
        _refreshBackendMethodCatalog();
    }

    // Function-pointer binding API:
    //   server.bindMethod<&free_function, "lhs", "rhs">();
    // The method name follows RpcMethodF<Ptr>::method_name, so it matches the
    // static RpcMethodF declaration form used inside protocol structs.
    template <auto Ptr, ConstexprString... ArgNames>
        requires detail::RpcMethodFuncT<Ptr>
    auto bindMethod() -> void {
        mDispatcher.bindRpcMethod(detail::func_nameof<Ptr>,
                                  traits::FunctionT<std::remove_pointer_t<decltype(Ptr)>>(Ptr),
                                  std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...});
        _refreshBackendMethodCatalog();
    }

    // Coroutine overload for runtime-name binding. It shares the same metadata
    // shape as the synchronous overload; only the stored callable differs.
    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<ilias::IoTask<RetT>(Args...)> func) -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mDispatcher.bindRpcMethod(name, std::move(func),
                                  std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...});
        _refreshBackendMethodCatalog();
    }

    // Context-aware binding keeps RpcRequestContext out of the public wire
    // signature and parameter serializer. Only the server handler sees it.
    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethodWithContext(
        std::string_view name,
        traits::FunctionT<ilias::IoTask<RetT>(const RpcRequestContext&, Args...)> func) -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethodWithContext: The number of public parameters and names do not match.");
        mDispatcher.bindRpcMethodWithContext(
            name, std::move(func), std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...});
        _refreshBackendMethodCatalog();
    }

    auto processMessage(std::span<const std::byte> message) -> ilias::Task<typename Backend::Message> {
        auto session = Backend::makeServerPeerSession(mBackendContext);
        co_return co_await mDispatcher.processMessage(message, mBackendContext, session, nullptr);
    }

    auto callMethod(std::string_view message) -> ilias::Task<typename Backend::Message> {
        auto session = Backend::makeServerPeerSession(mBackendContext);
        co_return co_await mDispatcher.processMessage(
            {reinterpret_cast<const std::byte*>(message.data()), message.size()}, mBackendContext, session, nullptr);
    }

    auto methodDatas() -> std::vector<MethodData> { return mDispatcher.methodDatas(); }
    auto methodDatas(std::string_view name) -> MethodData { return mDispatcher.methodDatas(name); }

public:
    static constexpr int BuiltinMethodsCount = Reflect<RpcBuiltinMethods>::value_count;

private:
    auto _configureExecution() -> void {
        std::size_t maxActive = 4096U;
        std::size_t maxQueued = 4096U;
        std::optional<std::chrono::nanoseconds> requestTimeout;
        if constexpr (requires { mBackendContext.options.max_active_requests_global; }) {
            maxActive = mBackendContext.options.max_active_requests_global;
        }
        if constexpr (requires { mBackendContext.options.max_queued_requests_global; }) {
            maxQueued = mBackendContext.options.max_queued_requests_global;
        }
        if constexpr (requires { mBackendContext.options.request_timeout; }) {
            requestTimeout = mBackendContext.options.request_timeout;
        }
        mDispatcher.configureExecution(maxActive, maxQueued, requestTimeout);
    }

    auto _maxInflightRequests() const noexcept -> std::size_t {
        if constexpr (requires { mBackendContext.options.max_inflight_requests_per_connection; }) {
            return mBackendContext.options.max_inflight_requests_per_connection;
        }
        return 1024U;
    }

    template <typename Protocol>
    void _registerProtocol(Protocol& protocol, std::string_view prefix = {}) {
        detail::for_each_rpc_method(protocol, [this](auto& method) { mDispatcher.registerRpcMethod(method); }, prefix);
    }

    auto _buildMethodInfo(const MethodData& methodData) -> std::string {
        std::string method_info;
        method_info.reserve(256);
        method_info += "name: " + std::string(methodData.name) + "\n";
        method_info += "signature: " + std::string(methodData.signature) + "\n";
        if (!methodData.description.empty()) {
            method_info += "description: " + std::string(methodData.description) + "\n";
        }
        if (!methodData.rpcVersion.empty()) {
            method_info += "version: " + std::string(methodData.rpcVersion) + "\n";
        }
        if (!methodData.argNames.empty()) {
            method_info += "args: ";
            for (auto& arg : methodData.argNames) {
                method_info += arg + ", ";
            }
            if (!methodData.argNames.empty()) {
                method_info.pop_back();
                method_info.pop_back();
            }
            method_info += "\n";
        }
        method_info += "notification: " + std::string(methodData.isNotification ? "true" : "false") + "\n";
        method_info += "bind: " + std::string(methodData.isBind ? "true" : "false") + "\n";
        return method_info;
    }

    auto _handleEndpoint(std::shared_ptr<EndpointSlot> slot) -> void {
        slot->session = Backend::makeServerPeerSession(mBackendContext);
        bool rejected = false;
        {
            std::scoped_lock lock(mEndpointMutex);
            if (mClosing) {
                rejected = true;
            } else {
                mEndpoints.emplace_back(slot);
            }
        }
        if (rejected) {
            slot->endpoint->close();
            return;
        }
        try {
            auto cleanup = [this, slot]() -> ilias::Task<void> {
                _eraseEndpoint(slot);
                mDispatcher.cancelSession(std::addressof(slot->session));
                slot->requests->stop();
                co_await slot->requests->waitAll(); // wait for all requests to finish
                NEKO_LOG_INFO("rpc", "rpc server endpoint loop end");
                co_return;
            };
            mScope.spawn([this, slot, cleanup]() -> ilias::Task<void> {
                NEKO_LOG_INFO("rpc", "rpc server endpoint loop start");
                co_await ilias::finally(_handleClient(slot), cleanup);
                // Don't do anything here; once the coroutine is canceled, the following code won't run. You can only
                // use RAII to clean up.
                co_return;
            });
        } catch (...) {
            _eraseEndpoint(slot);
            slot->endpoint->close();
            throw;
        }
    }

    auto _handleClient(std::shared_ptr<EndpointSlot> slot) -> ilias::Task<void> {
        while (slot->endpoint != nullptr) {
            std::vector<std::byte> buffer;
            if (auto ret = co_await slot->endpoint->recv(buffer); !ret) {
                NEKO_LOG_INFO("rpc", "rpc server endpoint loop stop: recv={}", ret.error().message());
                break;
            }
            if (buffer.empty()) {
                NEKO_LOG_INFO("rpc", "rpc server endpoint loop stop: reason=empty_frame");
                break;
            }

            if constexpr (requires { Backend::validateMessage(mBackendContext, buffer); }) {
                auto validated = Backend::validateMessage(mBackendContext, buffer);
                if (!validated) {
                    NEKO_LOG_WARN("rpc", "rpc server rejected oversized or malformed frame: {}",
                                  validated.error().message());
                    slot->endpoint->close();
                    break;
                }
            }

            if constexpr (requires { Backend::cancelId(buffer); }) {
                if (auto cancel_id = Backend::cancelId(buffer); cancel_id.has_value()) {
                    mDispatcher.cancel(std::addressof(slot->session), *cancel_id);
                    continue;
                }
            }

            // Control frames update backend session state and must not enter
            // the dispatcher as user-call requests.
            auto control =
                co_await Backend::handleServerControl(mBackendContext, slot->session, *slot->endpoint, buffer);
            if (!control) {
                NEKO_LOG_ERROR("rpc", "handle rpc control frame failed: {}", control.error().message());
                break;
            }
            if (control.value()) {
                NEKO_LOG_INFO("rpc", "rpc server handled control frame");
                NEKO_LOG_TRACE("rpc", "rpc server control frame bytes={}", buffer.size());
                continue;
            }

            if (slot->requests->size() >= _maxInflightRequests()) {
                NEKO_LOG_WARN("rpc", "rpc server connection exceeded in-flight request limit");
                (void)co_await mDispatcher.processMessage(buffer, mBackendContext, slot->session,
                                                          slot->endpoint.get(), &slot->sendMutex, true,
                                                          std::addressof(slot->peer));
                continue;
            }
            slot->requests->spawn([this, slot, message = std::move(buffer)]() mutable -> ilias::Task<void> {
                (void)co_await mDispatcher.processMessage(message, mBackendContext, slot->session, slot->endpoint.get(),
                                                          &slot->sendMutex, false, std::addressof(slot->peer));
            });
        }
        // Don't do anything clean up here; once the coroutine is canceled, the following code won't run. You can only
        // use RAII to clean up.
        co_return;
    }

    auto _init() -> void {
        (_registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
        _registerProtocol(mRpc, "rpc");
        mRpc.getMethodInfo = [this](std::string method_name) -> ilias::IoTask<std::string> {
            if (auto ret = mDispatcher.methodDatas(method_name); !ret.name.empty()) {
                co_return _buildMethodInfo(ret);
            }
            co_return std::string("Method not found!");
        };
        mRpc.getMethodInfoList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> method_des_list;
            for (const auto& item : methodDatas()) {
                method_des_list.emplace_back(_buildMethodInfo(item));
            }
            co_return method_des_list;
        };
        mRpc.getMethodList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> method_list;
            for (auto& item : methodDatas()) {
                method_list.emplace_back(item.name);
            }
            co_return method_list;
        };
        mRpc.getBindedMethodList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> method_list;
            for (auto& item : methodDatas()) {
                if (item.isBind) {
                    method_list.emplace_back(item.name);
                }
            }
            co_return method_list;
        };
        mDispatcher.bindRpcMethodWithContext(
            mRpc.getExecutionPolicy,
            traits::FunctionT<ilias::IoTask<std::map<std::string, std::string>>(
                const RpcRequestContext&)>([this](const RpcRequestContext& requestContext)
                                               -> ilias::IoTask<std::map<std::string, std::string>> {
                co_return _publicExecutionPolicy(requestContext);
            }));
        mDispatcher.bindRpcMethodWithContext(
            mRpc.setConnectionTimeout,
            traits::FunctionT<ilias::IoTask<std::uint64_t>(const RpcRequestContext&, std::uint64_t)>(
                [this](const RpcRequestContext& requestContext, std::uint64_t timeoutNanoseconds)
                    -> ilias::IoTask<std::uint64_t> {
                    auto result = mDispatcher.setConnectionTimeout(requestContext, timeoutNanoseconds);
                    if (!result) {
                        co_return ilias::Err(result.error());
                    }
                    co_return result.value();
                }));
        mDispatcher.bindRpcMethodWithContext(
            mRpc.getConnectionStatus,
            traits::FunctionT<ilias::IoTask<std::map<std::string, std::uint64_t>>(
                const RpcRequestContext&)>([this](const RpcRequestContext& requestContext)
                                               -> ilias::IoTask<std::map<std::string, std::uint64_t>> {
                co_return mDispatcher.connectionStatus(requestContext);
            }));
        mDispatcher.bindRpcMethodWithContext(
            mRpc.getConnectionTasks,
            traits::FunctionT<ilias::IoTask<std::map<std::string, std::string>>(
                const RpcRequestContext&)>([this](const RpcRequestContext& requestContext)
                                               -> ilias::IoTask<std::map<std::string, std::string>> {
                co_return mDispatcher.connectionTasks(requestContext);
            }));
    }

    auto _publicExecutionPolicy(const RpcRequestContext& requestContext) const
        -> std::map<std::string, std::string> {
        std::map<std::string, std::string> policy;
        policy["privacy_scope"] = "per_client_contract_only";
        policy["deadline.effective_rule"] = "minimum_connection_timeout_and_server_limit";
        policy["deadline.covers"] = "queue_and_handler";
        policy["deadline.propagation"] = "connection_timeout_builtin_and_client_cancel";
        policy["status.connection"] = "active_queued_in_flight";
        policy["status.tasks"] = "current_only_no_history";
        policy["status.global"] = "not_disclosed";
        policy["limits.connection_timeout_min_inclusive_ns"] = "1";
        if (auto connectionTimeout = mDispatcher.connectionTimeout(requestContext);
            connectionTimeout.has_value()) {
            policy["limits.connection_timeout_ns"] = std::to_string(connectionTimeout->count());
        } else {
            policy["limits.connection_timeout_ns"] = "none";
        }

        const auto& options = mBackendContext.options;
        if constexpr (requires { options.request_timeout; }) {
            policy["limits.server_request_timeout_ns"] =
                options.request_timeout.has_value() ? std::to_string(options.request_timeout->count()) : "none";
            if (!options.request_timeout.has_value()) {
                policy["limits.connection_timeout_max_exclusive_ns"] = "unbounded";
            } else if (options.request_timeout->count() <= 0) {
                policy["limits.connection_timeout_max_exclusive_ns"] = "unavailable";
            } else {
                policy["limits.connection_timeout_max_exclusive_ns"] =
                    std::to_string(options.request_timeout->count());
            }
        }
        if constexpr (requires { options.max_inflight_requests_per_connection; }) {
            policy["limits.max_inflight_requests_per_connection"] =
                std::to_string(options.max_inflight_requests_per_connection);
        }
        if constexpr (requires { options.frame_limits.max_frame_bytes; }) {
            policy["limits.max_frame_bytes"] = std::to_string(options.frame_limits.max_frame_bytes);
            policy["limits.max_method_bytes"] = std::to_string(options.frame_limits.max_method_bytes);
            policy["limits.max_payload_bytes"] = std::to_string(options.frame_limits.max_payload_bytes);
            policy["limits.max_extension_bytes"] = std::to_string(options.frame_limits.max_extension_bytes);
        }
        if constexpr (requires { options.max_decompressed_payload_bytes; }) {
            policy["limits.max_decompressed_payload_bytes"] =
                std::to_string(options.max_decompressed_payload_bytes);
        }
        return policy;
    }

    auto _methodMetadata() -> std::vector<detail::RpcMethodMetadata> {
        std::vector<detail::RpcMethodMetadata> methods;
        for (const auto& item : methodDatas()) {
            methods.push_back({
                .name           = std::string(item.name),
                .signature      = std::string(item.signature),
                .description    = std::string(item.description),
                .rpcVersion     = std::string(item.rpcVersion),
                .argNames       = item.argNames,
                .isNotification = item.isNotification,
                .isBind         = item.isBind,
            });
        }
        return methods;
    }

    auto _refreshBackendMethodCatalog() -> void { Backend::refreshMethodCatalog(mBackendContext, _methodMetadata()); }

    auto _snapshotEndpoints() const -> std::vector<std::shared_ptr<EndpointSlot>> {
        std::scoped_lock lock(mEndpointMutex);
        return {mEndpoints.begin(), mEndpoints.end()};
    }

    auto _beginClose() -> std::list<std::shared_ptr<EndpointSlot>> {
        std::list<std::shared_ptr<EndpointSlot>> endpoints;
        std::scoped_lock lock(mEndpointMutex);
        mClosing = true;
        endpoints.splice(endpoints.end(), mEndpoints);
        return endpoints;
    }

    auto _eraseEndpoint(const std::shared_ptr<EndpointSlot>& slot) -> void {
        std::scoped_lock lock(mEndpointMutex);
        mEndpoints.remove(slot);
    }

    Dispatcher mDispatcher;
    typename Backend::ServerContext mBackendContext;
    std::list<std::shared_ptr<EndpointSlot>> mEndpoints;
    mutable std::mutex mEndpointMutex;
    bool mClosing = false;
    ilias::TaskGroup<void> mScope;
};

NEKO_END_NAMESPACE
