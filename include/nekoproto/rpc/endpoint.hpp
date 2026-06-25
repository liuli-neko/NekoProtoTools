#pragma once

#include <concepts>
#include <cstddef>
#include <ilias/io.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/task.hpp>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {

template <typename T>
concept RpcEndpointRecvSize = requires(T endpoint, std::vector<std::byte>& out) {
    { endpoint.recv(out) } -> std::same_as<ilias::IoTask<std::size_t>>;
};

template <typename T>
concept RpcEndpointRecvVoid = requires(T endpoint, std::vector<std::byte>& out) {
    { endpoint.recv(out) } -> std::same_as<ilias::IoTask<void>>;
};

template <typename T>
concept RpcEndpointSendSize = requires(T endpoint, std::span<const std::byte> in) {
    { endpoint.send(in) } -> std::same_as<ilias::IoTask<std::size_t>>;
};

template <typename T>
concept RpcEndpointSendVoid = requires(T endpoint, std::span<const std::byte> in) {
    { endpoint.send(in) } -> std::same_as<ilias::IoTask<void>>;
};

} // namespace detail

template <typename T>
concept RpcMessageEndpoint = requires(T endpoint, std::vector<std::byte>& out, std::span<const std::byte> in) {
    { endpoint.close() } -> std::same_as<void>;
    { endpoint.cancel() } -> std::same_as<void>;
    { endpoint.shutdown() } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.flush() } -> std::same_as<ilias::IoTask<void>>;
} && (detail::RpcEndpointRecvSize<T> || detail::RpcEndpointRecvVoid<T>) &&
                             (detail::RpcEndpointSendSize<T> || detail::RpcEndpointSendVoid<T>);

namespace detail {

struct RpcMethodMetadata {
    std::string name;
    std::string description;
    bool isBind = false;
};

class IRpcMessageEndpoint {
public:
    IRpcMessageEndpoint()          = default;
    virtual ~IRpcMessageEndpoint() = default;

    virtual auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<size_t>    = 0;
    virtual auto send(std::span<const std::byte> buffer) -> ilias::IoTask<size_t> = 0;
    virtual auto close() -> void                                                  = 0;
    virtual auto cancel() -> void                                                 = 0;
    virtual auto shutdown() -> ilias::IoTask<void>                                = 0;
    virtual auto flush() -> ilias::IoTask<void>                                   = 0;
    virtual auto refreshRpcMethods(std::vector<RpcMethodMetadata> /*methods*/) -> ilias::IoTask<void> { co_return {}; }
};

template <typename T, typename = void>
class RpcMessageEndpointWrapper;

template <RpcMessageEndpoint T>
class RpcMessageEndpointWrapper<T, void> : public IRpcMessageEndpoint {
public:
    explicit RpcMessageEndpointWrapper(T&& endpoint) : mEndpoint(std::move(endpoint)) {}
    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> override {
        if constexpr (RpcEndpointRecvSize<T>) {
            auto ret = co_await mEndpoint.recv(buffer);
            if (!ret) {
                co_return ilias::Err(ret.error());
            }
            co_return ret.value();
        } else {
            auto ret = co_await mEndpoint.recv(buffer);
            if (!ret) {
                co_return ilias::Err(ret.error());
            }
            co_return buffer.size();
        }
    }
    auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> override {
        if constexpr (RpcEndpointSendSize<T>) {
            auto ret = co_await mEndpoint.send(buffer);
            if (!ret) {
                co_return ilias::Err(ret.error());
            }
            co_return ret.value();
        } else {
            auto ret = co_await mEndpoint.send(buffer);
            if (!ret) {
                co_return ilias::Err(ret.error());
            }
            co_return buffer.size();
        }
    }
    auto close() -> void override { return mEndpoint.close(); }
    auto cancel() -> void override { return mEndpoint.cancel(); }
    auto shutdown() -> ilias::IoTask<void> override { return mEndpoint.shutdown(); }
    auto flush() -> ilias::IoTask<void> override { return mEndpoint.flush(); }
    auto refreshRpcMethods(std::vector<RpcMethodMetadata> methods) -> ilias::IoTask<void> override {
        if constexpr (requires(T& endpoint, std::vector<RpcMethodMetadata> items) {
                          { endpoint.refreshRpcMethods(std::move(items)) } -> std::same_as<ilias::IoTask<void>>;
                      }) {
            co_return co_await mEndpoint.refreshRpcMethods(std::move(methods));
        } else {
            co_return {};
        }
    }

private:
    T mEndpoint;
};

template <typename Backend, typename StreamT>
concept RpcStreamBackend = ilias::Stream<StreamT> && requires(StreamT stream) {
    { Backend::makeEndpoint(std::move(stream)) } -> RpcMessageEndpoint;
};

template <typename T, class enable = void>
struct is_rpc_endpoint : std::false_type {};

template <RpcMessageEndpoint T>
struct is_rpc_endpoint<T, std::enable_if_t<std::is_base_of_v<IRpcMessageEndpoint, T>>> : std::true_type {};

} // namespace detail

NEKO_END_NAMESPACE
