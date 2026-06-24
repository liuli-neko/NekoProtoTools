#pragma once

#include <concepts>
#include <ilias/io.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/task.hpp>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename T>
concept RpcMessageEndpoint = requires(T endpoint, std::vector<std::byte>& out, std::span<const std::byte> in) {
    { endpoint.recv(out) } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.send(in) } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.close() } -> std::same_as<void>;
    { endpoint.cancel() } -> std::same_as<void>;
};

namespace detail {

class IRpcMessageEndpoint {
public:
    IRpcMessageEndpoint()          = default;
    virtual ~IRpcMessageEndpoint() = default;

    virtual auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<void>    = 0;
    virtual auto send(std::span<const std::byte> buffer) -> ilias::IoTask<void> = 0;
    virtual auto close() -> void                                                = 0;
    virtual auto cancel() -> void                                               = 0;
};

template <RpcMessageEndpoint T>
class RpcMessageEndpointWrapper : public IRpcMessageEndpoint {
public:
    explicit RpcMessageEndpointWrapper(T&& endpoint) : mEndpoint(std::move(endpoint)) {}
    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<void> override { return mEndpoint.recv(buffer); }
    auto send(std::span<const std::byte> buffer) -> ilias::IoTask<void> override { return mEndpoint.send(buffer); }
    auto close() -> void override { return mEndpoint.close(); }
    auto cancel() -> void override { return mEndpoint.cancel(); }

private:
    T mEndpoint;
};

template <typename Backend, typename StreamT>
concept RpcStreamBackend = ilias::Stream<StreamT> && requires(StreamT stream) {
    { Backend::makeEndpoint(std::move(stream)) } -> RpcMessageEndpoint;
};

} // namespace detail

NEKO_END_NAMESPACE
