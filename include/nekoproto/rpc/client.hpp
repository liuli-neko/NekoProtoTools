#pragma once

#include <ilias/platform.hpp>
#include <ilias/sync/mutex.hpp>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "nekoproto/rpc/builtin.hpp"
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/registry.hpp"

NEKO_BEGIN_NAMESPACE

template <RpcBackend Backend, typename... ProtocolSets>
class RpcClient : public ProtocolSets... {

public:
    RpcBuiltinMethods rpc;
    static constexpr int BuiltinMethodsCount = Reflect<RpcBuiltinMethods>::value_count;

    explicit RpcClient(ilias::IoContext& /*unused*/) : ProtocolSets()..., rpc() {
        (_registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
        _registerProtocol(rpc, "rpc");
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

    auto flush() noexcept -> ilias::IoTask<void> {
        if (mEndpoint != nullptr) {
            co_return co_await mEndpoint->flush();
        }
        co_return {};
    }

    auto shutdown() noexcept -> ilias::IoTask<void> {
        if (mEndpoint != nullptr) {
            if (auto ret = co_await mEndpoint->flush(); !ret) {
                co_return ilias::Err(ret.error());
            }
            if (auto ret = co_await mEndpoint->shutdown(); !ret) {
                co_return ilias::Err(ret.error());
            }
            mEndpoint->close();
            mEndpoint.reset();
        }
        co_return {};
    }

    auto isConnected() const noexcept -> bool { return mEndpoint != nullptr; }

    template <RpcMessageEndpoint EndpointT>
    auto setEndpoint(EndpointT endpoint) noexcept -> void {
        if constexpr (detail::is_rpc_endpoint<EndpointT>::value) {
            mEndpoint = std::make_unique<EndpointT>(std::move(endpoint));
        } else {
            mEndpoint = std::make_unique<detail::RpcMessageEndpointWrapper<EndpointT>>(std::move(endpoint));
        }
    }

    template <typename StreamT>
        requires detail::RpcStreamBackend<Backend, StreamT>
    auto setEndpoint(StreamT stream) noexcept -> void {
        if constexpr (requires(StreamT value) {
                          { Backend::makeClientEndpoint(std::move(value)) } -> RpcMessageEndpoint;
                      }) {
            setEndpoint(Backend::makeClientEndpoint(std::move(stream)));
        } else {
            setEndpoint(Backend::makeEndpoint(std::move(stream)));
        }
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto callRemote(std::string_view name, Args... args) noexcept -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(name, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemote(Args... args) noexcept
        -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(detail::RpcMethodF<Ptr, ArgNames...>::MethodName, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) noexcept -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(name, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemote(Args... args) noexcept
        -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr), ArgNames...>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(detail::RpcMethodF<Ptr, ArgNames...>::MethodName, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

private:
    template <typename Protocol>
    void _registerProtocol(Protocol& protocol, std::string_view prefix = {}) {
        detail::forEachRpcMethod(protocol, [this](auto& method) { _registerRpcMethod(method); }, prefix);
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
        auto encoded =
            Backend::template encodeRequest<T>(metadata, metadata.isNotification(), mId, std::forward<Args>(args)...);
        if (!encoded) {
            co_return ilias::Err(encoded.error());
        }
        auto& request = encoded.value();
        auto sendRet  = co_await mEndpoint->send(
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
    std::unique_ptr<detail::IRpcMessageEndpoint> mEndpoint = nullptr;
    std::uint64_t mId                                      = 0;
    ilias::Mutex mMutex;
};

NEKO_END_NAMESPACE
