#pragma once

#include <ilias/sync/mutex.hpp>
#include <ilias/platform.hpp>
#include <memory>
#include <span>
#include <vector>

#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/registry.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Backend, typename... ProtocolSets>
class RpcClient : public ProtocolSets... {
    static_assert(sizeof...(ProtocolSets) > 0, "RpcClient requires at least one API/protocol struct");

public:
    explicit RpcClient(ilias::IoContext& /*unused*/) : ProtocolSets()... {
        (_registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
    }
    ~RpcClient() { close(); }

    auto operator->() noexcept -> RpcClient* { return this; }
    auto operator->() const noexcept -> const RpcClient* { return this; }

    auto close() noexcept -> void {
        if (mEndpoint != nullptr) {
            mEndpoint->cancel();
            mEndpoint->close();
        }
    }

    auto isConnected() const noexcept -> bool { return mEndpoint != nullptr; }

    template <RpcEndpoint EndpointT>
    auto setEndpoint(EndpointT endpoint) noexcept -> void {
        mEndpoint = std::make_unique<detail::RpcEndpointWrapper<EndpointT>>(std::move(endpoint));
    }

    auto setEndpoint(std::unique_ptr<detail::IRpcEndpoint> endpoint) noexcept -> void { mEndpoint = std::move(endpoint); }

    template <RpcEndpoint EndpointT>
    auto setTransport(EndpointT endpoint) noexcept -> void {
        setEndpoint(std::move(endpoint));
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto callRemote(std::string_view name, Args... args) noexcept -> ilias::IoTask<RetT> {
        using Metadata = detail::RpcMethodDynamic<RetT(Args...), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(name, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemote(Args... args) noexcept -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata = detail::RpcMethodDynamic<decltype(Ptr), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(detail::RpcMethodF<Ptr, ArgNames...>::MethodName, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) noexcept -> ilias::IoTask<RetT> {
        using Metadata = detail::RpcMethodDynamic<RetT(Args...), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(name, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemote(Args... args) noexcept -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata = detail::RpcMethodDynamic<decltype(Ptr), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(detail::RpcMethodF<Ptr, ArgNames...>::MethodName, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

private:
    template <typename Protocol>
    void _registerProtocol(Protocol& protocol) {
        detail::forEachRpcMethod(protocol, [this](auto& method) { _registerRpcMethod(method); });
    }

    template <typename T>
    void _registerRpcMethod(T& metadata) noexcept {
        metadata = (typename std::decay_t<T>::CoroutinesFuncType)[this, &metadata](auto... args) noexcept
            -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
            return _callRemote<T, decltype(args)...>(metadata, std::forward<decltype(args)>(args)...);
        };
    }

    template <typename T, typename... Args>
    auto _callRemote(T& metadata, Args... args) noexcept -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
        if (mEndpoint == nullptr) {
            co_return ilias::Err(Backend::clientNotInitError());
        }
        auto guard = co_await mMutex.lock();
        auto encoded = Backend::template encodeRequest<T>(metadata, metadata.isNotification(), mId,
                                                          std::forward<Args>(args)...);
        if (!encoded) {
            co_return ilias::Err(encoded.error());
        }
        auto& request = encoded.value();
        auto sendRet = co_await mEndpoint->send(
            {reinterpret_cast<const std::byte*>(request.message.data()), request.message.size()});
        if (!sendRet) {
            co_return ilias::Err(sendRet.error());
        }
        if (!metadata.isNotification()) {
            std::vector<std::byte> buffer;
            if (auto ret = co_await mEndpoint->recv(buffer); ret) {
                auto decoded = Backend::template decodeResponse<T>(buffer, request.id);
                if (!decoded) {
                    co_return ilias::Err(decoded.error());
                }
                if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                    co_return {};
                } else {
                    co_return decoded.value();
                }
            } else {
                co_return ilias::Err(ret.error());
            }
        } else {
            co_return ilias::Err(Backend::notificationOk());
        }
    }

private:
    std::unique_ptr<detail::IRpcEndpoint> mEndpoint = nullptr;
    std::uint64_t mId = 0;
    ilias::Mutex mMutex;
};

NEKO_END_NAMESPACE
