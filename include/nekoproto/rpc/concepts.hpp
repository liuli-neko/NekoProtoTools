#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Backend>
concept RpcBackend = requires(std::span<const std::byte> message, const typename Backend::DecodedRequest& request,
                              typename Backend::ResponseValues& responses, typename Backend::ClientContext cctxt,
                              typename Backend::ServerContext sctxt, typename Backend::PeerSession psess, bool batch,
                              std::error_code error) {
    typename Backend::Id;
    typename Backend::Message;
    typename Backend::ResponseValues;
    typename Backend::DecodedRequest;
    typename Backend::DecodeResult;
    typename Backend::EncodedRequest;
    typename Backend::Options;
    typename Backend::ClientContext;
    typename Backend::ServerContext;
    typename Backend::PeerSession;

    { Backend::decodeIncoming(sctxt, psess, message) } -> std::same_as<typename Backend::DecodeResult>;
    { Backend::methodName(request) } -> std::convertible_to<std::string_view>;
    { Backend::id(request) } -> std::same_as<const typename Backend::Id&>;
    { Backend::expectsResponse(request) } -> std::same_as<bool>;
    { Backend::appendError(responses, request, error) } -> std::same_as<void>;
    { Backend::encodeResponses(sctxt, psess, responses, batch) } -> std::same_as<typename Backend::Message>;
    { Backend::clientNotInitError() } -> std::same_as<std::error_code>;
    { Backend::notificationOk() } -> std::same_as<std::error_code>;
};

template <typename Backend, typename T>
concept BackendSerializable = std::is_void_v<T> || requires { requires Backend::template serializable<T>(); };

NEKO_END_NAMESPACE
