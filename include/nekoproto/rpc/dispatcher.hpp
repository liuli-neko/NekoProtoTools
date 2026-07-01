#pragma once

#include <ilias/task/group.hpp>
#include <ilias/io/system_error.hpp>
#include <ilias/platform.hpp>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "nekoproto/global/log.hpp"
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/method.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

template <RpcBackend Backend>
class RpcMethodWrapperBase {
public:
    RpcMethodWrapperBase()                  = default;
    RpcMethodWrapperBase(RpcMethodWrapperBase&&) = default;
    virtual ~RpcMethodWrapperBase()         = default;

    virtual auto call(const typename Backend::DecodedRequest& request, typename Backend::ResponseValues& responses)
        noexcept -> ilias::Task<void> = 0;
    virtual auto name() noexcept -> std::string_view         = 0;
    virtual auto signature() noexcept -> std::string_view    = 0;
    virtual auto description() noexcept -> std::string_view  = 0;
    virtual auto rpcVersion() noexcept -> std::string_view   = 0;
    virtual auto argNames() noexcept -> std::vector<std::string> = 0;
    virtual auto isNotification() noexcept -> bool           = 0;
    virtual auto isBind() noexcept -> bool                   = 0;
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
    RpcMethodWrapperImpl(T methodData, RpcDispatcher<Backend>* self) : mMethodData(std::move(methodData)), mSelf(self) {}

    auto call(const typename Backend::DecodedRequest& request, typename Backend::ResponseValues& responses) noexcept
        -> ilias::Task<void> override;
    auto name() noexcept -> std::string_view override { return mMethodData->name(); }
    auto signature() noexcept -> std::string_view override { return mMethodData->signature; }
    auto description() noexcept -> std::string_view override { return mMethodData->description(); }
    auto rpcVersion() noexcept -> std::string_view override { return mMethodData->rpcVersion(); }
    auto argNames() noexcept -> std::vector<std::string> override { return mMethodData->metadataArgNames(); }
    auto isNotification() noexcept -> bool override { return mMethodData->isNotification(); }
    auto isBind() noexcept -> bool override { return (bool)(*mMethodData); }

private:
    T mMethodData;
    RpcDispatcher<Backend>* mSelf = nullptr;
};

template <RpcBackend Backend>
class RpcDispatcher {
public:
    using Id = typename Backend::Id;
    using Message = typename Backend::Message;

    struct MethodData {
        std::string_view name;
        std::string_view signature;
        std::string_view description;
        std::string_view rpcVersion;
        std::vector<std::string> argNames;
        bool isNotification = false;
        bool isBind = false;
    };

    explicit RpcDispatcher([[maybe_unused]] ilias::IoContext& ctx) : mTaskScope() {}
    ~RpcDispatcher() {
        cancelAll();
        mTaskScope.waitAll().wait();
    }

    template <typename T>
    auto registerRpcMethod(T& metadata) noexcept -> void {
        const auto name = std::string(metadata.name());
        if (auto item = mHandlers.find(name); item != mHandlers.end()) {
            NEKO_LOG_ERROR("rpc", "Method {} exist!!!", metadata.name());
            return;
        }
        mHandlers[name] = std::make_unique<RpcMethodWrapperImpl<Backend, T*>>(&metadata, this);
    }

    template <typename RetT, typename... Args, ConstexprString... ArgNames>
    auto bindRpcMethod(std::string_view name, traits::FunctionT<RetT(Args...)> func,
                       traits::ArgNamesHelper<ArgNames...> /*unused*/ = {}) noexcept -> void {
        return _registerRpcMethod<RpcMethodDynamic<RetT(Args...), ArgNames...>>(name, std::move(func));
    }

    template <typename RetT, typename... Args, ConstexprString... ArgNames>
    auto bindRpcMethod(std::string_view name, traits::FunctionT<ilias::IoTask<RetT>(Args...)> func,
                       traits::ArgNamesHelper<ArgNames...> /*unused*/ = {}) noexcept -> void {
        _registerRpcMethod<RpcMethodDynamic<RetT(Args...), ArgNames...>>(name, std::move(func));
    }

    auto methodDatas() noexcept -> std::vector<MethodData> {
        std::vector<MethodData> metas;
        for (auto& [name, handler] : mHandlers) {
            metas.push_back({.name             = handler->name(),
                             .signature        = handler->signature(),
                             .description      = handler->description(),
                             .rpcVersion       = handler->rpcVersion(),
                             .argNames         = handler->argNames(),
                             .isNotification   = handler->isNotification(),
                             .isBind           = handler->isBind()});
        }
        return metas;
    }

    auto methodDatas(std::string_view name) noexcept -> MethodData {
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

    auto processMessage(std::span<const std::byte> data, IMessageEndpoint* endpoint = nullptr) noexcept
        -> ilias::Task<Message> {
        Message buffer;
        auto decoded = Backend::decodeIncoming(data);
        if (!decoded.ok) {
            NEKO_LOG_ERROR("rpc", "parse rpc request failed");
            co_return buffer;
        }

        typename Backend::ResponseValues responses;
        for (const auto& request : decoded.requests) {
            mCurrentIds.emplace_back(Backend::id(request));
            const auto methodName = std::string(Backend::methodName(request));
            if (auto it = mHandlers.find(methodName); it != mHandlers.end()) {
                NEKO_LOG_INFO("rpc", "spawn taskhandle for method {}", methodName);
                mTaskScope.spawn(it->second->call(request, responses));
            } else {
                NEKO_LOG_WARN("rpc", "method {} not found!", methodName);
                Backend::appendError(responses, request, RpcError::MethodNotFound);
            }
        }

        co_await mTaskScope.waitAll();
        NEKO_LOG_INFO("rpc", "Finished processing request, batch size {}", decoded.requests.size());
        mCurrentIds.clear();
        mCancelHandles.clear();

        buffer = Backend::encodeResponses(responses, decoded.batch);
        if (endpoint != nullptr && !buffer.empty()) {
            auto ret = co_await endpoint->send(
                {reinterpret_cast<const std::byte*>(buffer.data()), static_cast<std::size_t>(buffer.size())});
            if (!ret) {
                NEKO_LOG_ERROR("rpc", "send rpc response failed: {}", ret.error().message());
            }
        }
        co_return buffer;
    }

    auto receiveLoop(IMessageEndpoint* endpoint) noexcept -> ilias::Task<void> {
        while (endpoint != nullptr) {
            std::vector<std::byte> buffer;
            if (auto ret = co_await endpoint->recv(buffer); ret && !buffer.empty()) {
                co_await (processMessage(buffer, endpoint));
            } else {
                break;
            }
        }
    }

    void cancel(Id id) noexcept {
        if (auto it = mCancelHandles.find(id); it != mCancelHandles.end()) {
            it->second.stop();
        } else {
            NEKO_LOG_WARN("rpc", "id not found");
        }
    }

    void cancelAll() {
        for (auto& [id, handle] : mCancelHandles) {
            handle.stop();
        }
        mCancelHandles.clear();
    }

    auto getCurrentIds() const noexcept -> const std::vector<Id>& { return mCurrentIds; }

private:
    template <typename T, typename Callable>
    auto _registerRpcMethod(std::string_view name, traits::FunctionT<Callable> func) noexcept -> void {
        auto method = std::make_unique<T>(name, std::move(func));
        const auto key = std::string(method->name());
        mHandlers[key] = std::make_unique<RpcMethodWrapperImpl<Backend, std::unique_ptr<T>>>(std::move(method), this);
    }

    template <typename T>
    auto _handle(const typename Backend::DecodedRequest& request, typename Backend::ResponseValues& responses,
                 T& metadata) noexcept -> ilias::Task<void> {
        auto decodedParams = Backend::template decodeParams<T>(request);
        if (!decodedParams) {
            Backend::appendError(responses, request, decodedParams.error());
            co_return;
        }

        auto handle = mTaskScope.spawn([&, this, request, params = std::move(decodedParams.value())]() mutable
                                           -> ilias::Task<void> {
            ilias::Result<typename T::RawReturnType, std::error_code> result =
                ilias::Err(ilias::SystemError::Canceled);
            auto onfinally = [&]() -> ilias::Task<void> {
                Backend::template appendMethodReturn<T>(responses, request, std::move(result));
                co_return;
            };
            co_await ([&result, params = std::move(params)](T& metadata) mutable -> ilias::Task<void> {
                result = co_await Backend::template invoke<T>(metadata, std::move(params));
                co_return;
            }(metadata) | ilias::finally(onfinally));
            co_return;
        });
        if (Backend::expectsResponse(request)) {
            mCancelHandles[Backend::id(request)] = std::move(handle);
        }
    }

private:
    std::vector<Id> mCurrentIds;
    std::map<Id, ilias::StopHandle> mCancelHandles;
    ilias::TaskGroup<void> mTaskScope;
    std::map<std::string, std::unique_ptr<RpcMethodWrapperBase<Backend>>> mHandlers;

    template <RpcBackend B, typename T>
    friend class RpcMethodWrapperImpl;
};

template <RpcBackend Backend, typename T>
auto RpcMethodWrapperImpl<Backend, T>::call(const typename Backend::DecodedRequest& request,
                                            typename Backend::ResponseValues& responses) noexcept -> ilias::Task<void> {
    return mSelf->_handle(request, responses, *mMethodData);
}

} // namespace detail

NEKO_END_NAMESPACE
