#pragma once

#include <ilias/task/scope.hpp>
#include <ilias/platform.hpp>
#include <list>
#include <memory>
#include <string_view>
#include <vector>

#include "nekoproto/rpc/dispatcher.hpp"
#include "nekoproto/rpc/registry.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Backend, typename... ProtocolSets>
class RpcServer : public ProtocolSets... {
    static_assert(sizeof...(ProtocolSets) > 0, "RpcServer requires at least one API/protocol struct");

private:
    using Dispatcher = detail::RpcDispatcher<Backend>;
    using MethodData = typename Dispatcher::MethodData;

    struct BuiltinRpc {
        RpcMethod<std::vector<std::string>(), "rpc.get_method_list"> getMethodList;
        RpcMethod<std::vector<std::string>(), "rpc.get_method_info_list"> getMethodInfoList;
        RpcMethod<std::string(std::string), "rpc.get_method_info", "method_name"> getMethodInfo;
        RpcMethod<std::vector<std::string>(), "rpc.get_bind_method_list"> getBindedMethodList;
    };

public:
    explicit RpcServer(ilias::IoContext& ctx) : ProtocolSets()..., mDispatcher(ctx), mScope() { _init(); }
    ~RpcServer() {
        close();
        wait().wait();
    }

    auto operator->() noexcept -> RpcServer* { return this; }
    auto operator->() const noexcept -> const RpcServer* { return this; }

    auto close() noexcept -> void {
        for (auto& endpoint : mEndpoints) {
            endpoint->cancel();
        }
        mScope.stop();
        for (auto& endpoint : mEndpoints) {
            endpoint->close();
        }
    }

    auto wait() -> ilias::Task<void> { co_await mScope.waitAll(); }
    auto cancel(typename Backend::Id id) noexcept -> void { mDispatcher.cancel(std::move(id)); }
    auto cancelAll() noexcept -> void { mDispatcher.cancelAll(); }
    auto getCurrentIds() const noexcept -> const std::vector<typename Backend::Id>& {
        return mDispatcher.getCurrentIds();
    }

    template <RpcEndpoint EndpointT>
    auto addEndpoint(EndpointT endpoint) noexcept -> void {
        auto item = mEndpoints.emplace(mEndpoints.end(),
                                       std::make_unique<detail::RpcEndpointWrapper<EndpointT>>(std::move(endpoint)));
        mScope.spawn([this, item]() -> ilias::Task<void> {
            co_await mDispatcher.receiveLoop((*item).get());
            mEndpoints.erase(item);
        });
    }

    auto addEndpoint(std::unique_ptr<detail::IRpcEndpoint> endpoint) noexcept -> void {
        auto item = mEndpoints.emplace(mEndpoints.end(), std::move(endpoint));
        mScope.spawn([this, item]() -> ilias::Task<void> {
            co_await mDispatcher.receiveLoop((*item).get());
            mEndpoints.erase(item);
        });
    }

    template <RpcEndpoint EndpointT>
    auto addTransport(EndpointT endpoint) noexcept -> void {
        addEndpoint(std::move(endpoint));
    }

    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<RetT(Args...)> func) noexcept -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mDispatcher.bindRpcMethod(name, std::move(func), traits::ArgNamesHelper<ArgNames...>{});
    }

    template <auto Ptr, ConstexprString... ArgNames>
        requires detail::RpcMethodFuncT<Ptr>
    auto bindMethod() noexcept -> void {
        mDispatcher.bindRpcMethod(detail::RpcMethodF<Ptr, ArgNames...>::MethodName,
                                  traits::FunctionT<std::remove_pointer_t<decltype(Ptr)>>(Ptr),
                                  traits::ArgNamesHelper<ArgNames...>{});
    }

    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<ilias::IoTask<RetT>(Args...)> func) noexcept -> void {
        mDispatcher.bindRpcMethod(name, std::move(func), traits::ArgNamesHelper<ArgNames...>{});
    }

    auto processMessage(std::span<const std::byte> message) noexcept -> ilias::Task<typename Backend::Message> {
        co_return co_await mDispatcher.processMessage(message, nullptr);
    }

    auto callMethod(std::string_view json) noexcept -> ilias::Task<typename Backend::Message> {
        co_return co_await mDispatcher.processMessage(
            {reinterpret_cast<const std::byte*>(json.data()), json.size()}, nullptr);
    }

    auto methodDatas() noexcept -> std::vector<MethodData> { return mDispatcher.methodDatas(); }
    auto methodDatas(std::string_view name) noexcept -> MethodData { return mDispatcher.methodDatas(name); }

private:
    template <typename Protocol>
    void _registerProtocol(Protocol& protocol) {
        detail::forEachRpcMethod(protocol, [this](auto& method) { mDispatcher.registerRpcMethod(method); });
    }

    auto _init() noexcept -> void {
        (_registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
        _registerProtocol(mRpc);
        mRpc.getMethodInfo = [this](std::string methodName) -> ilias::IoTask<std::string> {
            if (auto ret = mDispatcher.methodDatas(methodName); !ret.name.empty()) {
                co_return std::string(ret.description);
            }
            co_return std::string("Method not found!");
        };
        mRpc.getMethodInfoList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> methodDesList;
            for (auto& item : methodDatas()) {
                methodDesList.emplace_back(item.description);
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

private:
    BuiltinRpc mRpc;
    Dispatcher mDispatcher;
    std::list<std::unique_ptr<detail::IRpcEndpoint>> mEndpoints;
    ilias::TaskGroup<void> mScope;
};

NEKO_END_NAMESPACE
