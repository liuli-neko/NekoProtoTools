#pragma once

#include <ilias/platform.hpp>
#include <ilias/sync/mutex.hpp>
#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "nekoproto/global/log.hpp"
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

    explicit RpcClient(ilias::IoContext& ctx) : RpcClient(ctx, typename Backend::Options{}) {}

    explicit RpcClient(ilias::IoContext& /*unused*/, typename Backend::Options options)
        : ProtocolSets()...,
          rpc(),
          mBackendContext(Backend::makeClientContext(std::move(options))),
          mPeerSession(Backend::makeClientPeerSession(mBackendContext)) {
        (_registerProtocol(static_cast<ProtocolSets&>(*this)), ...);
        _registerProtocol(rpc, "rpc");
    }
    ~RpcClient() { close(); }

    auto operator->() noexcept -> RpcClient* { return this; }
    auto operator->() const noexcept -> const RpcClient* { return this; }

    auto close() noexcept -> void {
        if (mEndpoint != nullptr) {
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

    template <MessageEndpoint EndpointT>
    auto setEndpoint(EndpointT endpoint) noexcept -> void {
        if constexpr (detail::is_message_endpoint<EndpointT>::value) {
            mEndpoint = std::make_unique<EndpointT>(std::move(endpoint));
        } else {
            mEndpoint = std::make_unique<detail::MessageEndpointWrapper<EndpointT>>(std::move(endpoint));
        }
        mPeerSession = Backend::makeClientPeerSession(mBackendContext);
    }

    template <typename StreamT>
        requires detail::RpcStreamBackend<Backend, StreamT>
    auto setEndpoint(StreamT stream) noexcept -> void {
        if constexpr (requires(StreamT value) {
                          { Backend::makeClientEndpoint(std::move(value)) } -> MessageEndpoint;
                      }) {
            setEndpoint(Backend::makeClientEndpoint(std::move(stream)));
        } else {
            setEndpoint(Backend::makeEndpoint(std::move(stream)));
        }
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto callRemote(std::string_view name, Args... args) noexcept -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemote(Args... args) noexcept
        -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) noexcept -> ilias::IoTask<RetT> {
        using Metadata           = detail::RpcMethodDynamic<RetT(Args...)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, name,
                          (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemote(Args... args) noexcept
        -> ilias::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using Metadata           = detail::RpcMethodDynamic<decltype(Ptr)>;
        using CoroutinesFuncType = typename Metadata::CoroutinesFuncType;
        Metadata metadata(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                          detail::func_nameof<Ptr>, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

private:
    template <typename Protocol>
    void _registerProtocol(Protocol& protocol, std::string_view prefix = {}) {
        detail::for_each_rpc_method(protocol, [this](auto& method) { this->_registerRpcMethod(method); }, prefix);
    }

    template <typename T>
    void _registerRpcMethod(T& metadata) noexcept {
        metadata = (typename std::decay_t<T>::CoroutinesFuncType)[this, &metadata](auto... args) noexcept
            -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
            return this->_callRemote<T, decltype(args)...>(metadata, std::forward<decltype(args)>(args)...);
        };
    }

    template <typename T, typename... Args>
    auto _callRemote(T& metadata, Args... args) noexcept -> ilias::IoTask<typename std::decay_t<T>::RawReturnType> {
        if (mEndpoint == nullptr) {
            co_return ilias::Err(Backend::clientNotInitError());
        }
        NEKO_LOG_TRACE("rpc", "rpc client call begin: method={} notification={}", metadata.name(),
                       metadata.isNotification());
        auto guard = co_await mMutex.lock();
        std::size_t retry_count = 0;
        while (true) {
            ILIAS_CO_TRYV(co_await Backend::ensureClientReady(mBackendContext, mPeerSession, *mEndpoint));
            auto encoded = Backend::template encodeRequest<T>(mBackendContext, mPeerSession, metadata,
                                                              metadata.isNotification(), args...);
            if (!encoded) {
                co_return ilias::Err(encoded.error());
            }
            auto& request = encoded.value();
            ILIAS_CO_TRY(auto send_ret,
                          co_await mEndpoint->send({reinterpret_cast<const std::byte*>(request.message.data()),
                                                    request.message.size()}));
            if (send_ret != request.message.size()) {
                co_return ilias::Err(ilias::IoError::Other);
            }
            NEKO_LOG_TRACE("rpc", "rpc client request send: method={} bytes={}", metadata.name(),
                           request.message.size());
            if (metadata.isNotification()) {
                NEKO_LOG_TRACE("rpc", "rpc client notification sent: method={}", metadata.name());
                co_return ilias::Err(Backend::notificationOk());
            }

            while (true) {
                std::vector<std::byte> buffer;
                ILIAS_CO_TRY(auto received, co_await mEndpoint->recv(buffer));
                (void)received;
                if (Backend::handleClientControl(mBackendContext, mPeerSession, buffer)) {
                    NEKO_LOG_INFO("rpc", "rpc client consumed control frame while waiting");
                    continue;
                }

                // Response decoding is the backend boundary: client code only
                // supplies the expected id and receives the typed result.
                auto decoded = Backend::template decodeResponse<T>(mBackendContext, mPeerSession, buffer,
                                                                   request.id);
                if (decoded) {
                    NEKO_LOG_TRACE("rpc", "rpc client call end: method={}", metadata.name());
                    if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                        co_return {};
                    } else {
                        co_return decoded.value();
                    }
                }

                // The generic client does not know which extension failed. It
                // only asks the backend whether its session state was recovered
                // well enough to replay this call once.
                bool should_retry = false;
                if constexpr (requires {
                                  Backend::recoverClientCall(mBackendContext, mPeerSession, *mEndpoint,
                                                             decoded.error(), retry_count);
                              }) {
                    ILIAS_CO_TRY(auto recovered,
                                  co_await Backend::recoverClientCall(mBackendContext, mPeerSession, *mEndpoint,
                                                                      decoded.error(), retry_count));
                    should_retry = recovered;
                }

                if (!should_retry) {
                    co_return ilias::Err(decoded.error());
                }
                ++retry_count;
                NEKO_LOG_INFO("rpc", "rpc client retrying call after backend recovery: method={} retry={}",
                              metadata.name(), retry_count);
                break;
            }
        }
    }

private:
    std::unique_ptr<detail::IMessageEndpoint> mEndpoint = nullptr;
    typename Backend::ClientContext mBackendContext;
    typename Backend::PeerSession mPeerSession;
    ilias::Mutex mMutex;
};

NEKO_END_NAMESPACE
