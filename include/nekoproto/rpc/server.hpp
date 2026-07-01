#pragma once

#include <ilias/platform.hpp>
#include <ilias/task/scope.hpp>
#include <array>
#include <concepts>
#include <functional>
#include <list>
#include <memory>
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
    using EndpointRefresh = std::function<ilias::IoTask<void>(std::vector<detail::RpcMethodMetadata>)>;

    struct EndpointSlot {
        std::unique_ptr<detail::IMessageEndpoint> endpoint;
        EndpointRefresh refreshRpcMethods;
    };

    RpcBuiltinMethods mRpc;

public:
    explicit RpcServer(ilias::IoContext& ctx) : ProtocolSets()..., mRpc(), mDispatcher(ctx), mScope() { _init(); }
    ~RpcServer() {
        close();
    }

    auto operator->() noexcept -> RpcServer* { return this; }
    auto operator->() const noexcept -> const RpcServer* { return this; }

    auto close() noexcept -> void {
        mScope.stop();
        wait().wait();
        for (auto& endpoint : mEndpoints) {
            endpoint.endpoint->close();
        }
    }

    auto flush() noexcept -> ilias::IoTask<void> {
        for (auto& endpoint : mEndpoints) {
            if (auto ret = co_await endpoint.endpoint->flush(); !ret) {
                co_return ilias::Err(ret.error());
            }
        }
        co_return {};
    }

    auto shutdown() noexcept -> ilias::Task<void> {
        for (auto& endpoint : mEndpoints) {
            if (auto ret = co_await endpoint.endpoint->flush(); !ret) {
                NEKO_LOG_WARN("rpc", "flush rpc endpoint failed: {}", ret.error().message());
            }
        }
        for (auto& endpoint : mEndpoints) {
            if (auto ret = co_await endpoint.endpoint->shutdown(); !ret) {
                NEKO_LOG_WARN("rpc", "shutdown rpc endpoint failed: {}", ret.error().message());
            }
        }
        co_await mScope.shutdown();
        for (auto& endpoint : mEndpoints) {
            endpoint.endpoint->close();
        }
    }

    auto wait() -> ilias::Task<void> { co_await mScope.waitAll(); }
    auto cancel(typename Backend::Id id) noexcept -> void { mDispatcher.cancel(std::move(id)); }
    auto cancelAll() noexcept -> void { mDispatcher.cancelAll(); }
    auto getCurrentIds() const noexcept -> const std::vector<typename Backend::Id>& {
        return mDispatcher.getCurrentIds();
    }

    template <MessageEndpoint EndpointT>
    auto addEndpoint(EndpointT endpoint) noexcept -> void {
        EndpointSlot slot;
        if constexpr (detail::is_message_endpoint<EndpointT>::value) {
            auto owned             = std::make_unique<EndpointT>(std::move(endpoint));
            slot.refreshRpcMethods = _makeRefreshCallback(owned.get());
            slot.endpoint          = std::move(owned);
        } else {
            using Wrapper          = detail::MessageEndpointWrapper<EndpointT>;
            auto owned             = std::make_unique<Wrapper>(std::move(endpoint));
            slot.refreshRpcMethods = _makeWrappedRefreshCallback(owned.get());
            slot.endpoint          = std::move(owned);
        }
        auto item = mEndpoints.emplace(mEndpoints.end(), std::move(slot));
        mScope.spawn([this, item]() -> ilias::Task<void> {
            struct EraseGuard {
                using type     = std::list<EndpointSlot>;
                using iterator = typename type::iterator;
                EraseGuard(type& endpoints, iterator it) : mEndpoints(endpoints), mIt(it) { mIt = it; }
                ~EraseGuard() { mEndpoints.erase(mIt); }
                type& mEndpoints;
                iterator mIt;
            };
            EraseGuard guard(mEndpoints, item);
            co_await mDispatcher.receiveLoop(item->endpoint.get());
        });
    }

    template <typename StreamT>
        requires detail::RpcStreamBackend<Backend, StreamT>
    auto addEndpoint(StreamT stream) noexcept -> void {
        if constexpr (requires(StreamT value, std::vector<MethodData> methods) {
                          { Backend::makeServerEndpoint(std::move(value), std::move(methods)) } -> MessageEndpoint;
                      }) {
            addEndpoint(Backend::makeServerEndpoint(std::move(stream), methodDatas()));
        } else {
            addEndpoint(Backend::makeEndpoint(std::move(stream)));
        }
    }

    // Runtime-name binding API:
    //   server.bindMethod<"lhs", "rhs">("calc.add", func);
    // Use this when the protocol surface is not expressed as a reflected
    // RpcMethod field, or when a method is added after construction.
    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<RetT(Args...)> func) noexcept -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mDispatcher.bindRpcMethod(name, std::move(func),
                                  std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...});
        _refreshEndpointMethodTables();
    }

    // Function-pointer binding API:
    //   server.bindMethod<&free_function, "lhs", "rhs">();
    // The method name follows RpcMethodF<Ptr>::method_name, so it matches the
    // static RpcMethodF declaration form used inside protocol structs.
    template <auto Ptr, ConstexprString... ArgNames>
        requires detail::RpcMethodFuncT<Ptr>
    auto bindMethod() noexcept -> void {
        mDispatcher.bindRpcMethod(detail::func_nameof<Ptr>,
                                  traits::FunctionT<std::remove_pointer_t<decltype(Ptr)>>(Ptr),
                                  std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...});
        _refreshEndpointMethodTables();
    }

    // Coroutine overload for runtime-name binding. It shares the same metadata
    // shape as the synchronous overload; only the stored callable differs.
    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<ilias::IoTask<RetT>(Args...)> func) noexcept -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mDispatcher.bindRpcMethod(name, std::move(func),
                                  std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...});
        _refreshEndpointMethodTables();
    }

    auto processMessage(std::span<const std::byte> message) noexcept -> ilias::Task<typename Backend::Message> {
        co_return co_await mDispatcher.processMessage(message, nullptr);
    }

    auto callMethod(std::string_view message) noexcept -> ilias::Task<typename Backend::Message> {
        co_return co_await mDispatcher.processMessage(
            {reinterpret_cast<const std::byte*>(message.data()), message.size()}, nullptr);
    }

    auto methodDatas() noexcept -> std::vector<MethodData> { return mDispatcher.methodDatas(); }
    auto methodDatas(std::string_view name) noexcept -> MethodData { return mDispatcher.methodDatas(name); }

public:
    static constexpr int BuiltinMethodsCount = Reflect<RpcBuiltinMethods>::value_count;

private:
    template <typename Protocol>
    void _registerProtocol(Protocol& protocol, std::string_view prefix = {}) {
        detail::for_each_rpc_method(protocol, [this](auto& method) { mDispatcher.registerRpcMethod(method); }, prefix);
    }

    auto _init() noexcept -> void {
        (_registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
        _registerProtocol(mRpc, "rpc");
        mRpc.getMethodInfo = [this](std::string methodName) -> ilias::IoTask<std::string> {
            if (auto ret = mDispatcher.methodDatas(methodName); !ret.name.empty()) {
                co_return std::string(ret.signature);
            }
            co_return std::string("Method not found!");
        };
        mRpc.getMethodInfoList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> methodDesList;
            for (auto& item : methodDatas()) {
                methodDesList.emplace_back(item.signature);
            }
            co_return methodDesList;
        };
        mRpc.getMethodList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> methodList;
            for (auto& item : methodDatas()) {
                methodList.emplace_back(item.name);
            }
            co_return methodList;
        };
        mRpc.getBindedMethodList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> methodList;
            for (auto& item : methodDatas()) {
                if (item.isBind) {
                    methodList.emplace_back(item.name);
                }
            }
            co_return methodList;
        };
    }

    auto _methodMetadata() noexcept -> std::vector<detail::RpcMethodMetadata> {
        std::vector<detail::RpcMethodMetadata> methods;
        for (const auto& item : methodDatas()) {
            methods.push_back({
                .name              = std::string(item.name),
                .signature         = std::string(item.signature),
                .description       = std::string(item.description),
                .rpcVersion        = std::string(item.rpcVersion),
                .argNames          = item.argNames,
                .isNotification    = item.isNotification,
                .isBind            = item.isBind,
            });
        }
        return methods;
    }

    auto _refreshEndpointMethodTables() noexcept -> void {
        auto methods = _methodMetadata();
        for (auto& endpoint : mEndpoints) {
            auto ret = endpoint.refreshRpcMethods(methods).wait();
            if (!ret) {
                NEKO_LOG_WARN("rpc", "refresh rpc method table failed: {}", ret.error().message());
            }
        }
    }

private:
    template <typename EndpointT>
    static auto _makeRefreshCallback(EndpointT* endpoint) -> EndpointRefresh {
        return [endpoint](std::vector<detail::RpcMethodMetadata> methods) -> ilias::IoTask<void> {
            if constexpr (requires(EndpointT& value, std::vector<detail::RpcMethodMetadata> items) {
                              { value.refreshRpcMethods(std::move(items)) } -> std::same_as<ilias::IoTask<void>>;
                          }) {
                co_return co_await endpoint->refreshRpcMethods(std::move(methods));
            } else {
                co_return {};
            }
        };
    }

    template <typename EndpointT>
    static auto _makeWrappedRefreshCallback(detail::MessageEndpointWrapper<EndpointT>* endpoint) -> EndpointRefresh {
        return [endpoint](std::vector<detail::RpcMethodMetadata> methods) -> ilias::IoTask<void> {
            if constexpr (requires(EndpointT& value, std::vector<detail::RpcMethodMetadata> items) {
                              { value.refreshRpcMethods(std::move(items)) } -> std::same_as<ilias::IoTask<void>>;
                          }) {
                co_return co_await endpoint->wrappedEndpoint().refreshRpcMethods(std::move(methods));
            } else {
                co_return {};
            }
        };
    }

    Dispatcher mDispatcher;
    std::list<EndpointSlot> mEndpoints;
    ilias::TaskGroup<void> mScope;
};

NEKO_END_NAMESPACE
