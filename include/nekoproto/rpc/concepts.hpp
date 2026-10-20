#pragma once
#include "nekoproto/global/macros.hpp"

#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "nekoproto/reflect.hpp"
#include "nekoproto/rpc/endpoint.hpp"
#include "nekoproto/rpc/options.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto {

namespace detail {

template <typename Message>
concept RpcBackendMessage = std::default_initializable<Message> && std::movable<Message> &&
    std::ranges::contiguous_range<Message> && std::ranges::sized_range<Message> &&
    (sizeof(std::ranges::range_value_t<Message>) == sizeof(std::byte)) &&
    requires(Message& message) {
        message.data();
        { message.size() } -> std::convertible_to<std::size_t>;
        { message.empty() } -> std::convertible_to<bool>;
    };

template <typename Backend>
concept RpcMultiplexBackend = requires(std::span<const std::byte> message) {
    { Backend::responseId(message) } -> std::same_as<std::optional<typename Backend::Id>>;
};

} // namespace detail

// Common, backend-neutral operations used by the client/server. Optional wire
// features (multiplexing, cancellation, validation, recovery and stream framing)
// are checked only where the corresponding capability is used.
// Readiness/control hooks may be no-ops: the core has no shared wire envelope,
// handshake, version scheme, or mandatory protocol extension.
template <typename Backend>
concept RpcBackend = requires {
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
} && detail::RpcBackendMessage<typename Backend::Message> &&
    std::semiregular<typename Backend::Id> &&
    std::default_initializable<typename Backend::Options> &&
    std::copy_constructible<typename Backend::Options> &&
    std::is_default_constructible_v<typename Backend::ServerContext> &&
    std::is_default_constructible_v<typename Backend::PeerSession> &&
    std::default_initializable<typename Backend::ResponseValues> &&
    requires(std::span<const std::byte> message, const typename Backend::DecodedRequest& request,
             typename Backend::ResponseValues& responses, typename Backend::DecodeResult& decoded,
             typename Backend::EncodedRequest& encoded, typename Backend::Options options,
             typename Backend::ClientContext& cctxt, typename Backend::ServerContext& sctxt,
             typename Backend::PeerSession& psess, const typename Backend::Id& id,
             std::vector<RpcMethodMetadata> methods, detail::IMessageEndpoint& endpoint,
             bool batch, std::error_code error, std::size_t retryCount) {
        { id < id } -> std::convertible_to<bool>;
        { Backend::makeClientContext(std::move(options)) } -> std::same_as<typename Backend::ClientContext>;
        { Backend::makeServerContext(std::move(options), std::move(methods)) }
            -> std::same_as<typename Backend::ServerContext>;
        { sctxt = Backend::makeServerContext(std::move(options), std::move(methods)) }
            -> std::same_as<typename Backend::ServerContext&>;
        { Backend::makeClientPeerSession(cctxt) } -> std::same_as<typename Backend::PeerSession>;
        { Backend::makeServerPeerSession(sctxt) } -> std::same_as<typename Backend::PeerSession>;
        { psess = Backend::makeClientPeerSession(cctxt) } -> std::same_as<typename Backend::PeerSession&>;
        { Backend::refreshMethodCatalog(sctxt, std::move(methods)) } -> std::same_as<void>;
        { Backend::ensureClientReady(cctxt, psess, endpoint) } -> std::same_as<ilias::IoTask<void>>;
        { Backend::handleServerControl(sctxt, psess, endpoint, message) } -> std::same_as<ilias::IoTask<bool>>;
        { Backend::decodeIncoming(sctxt, psess, message) } -> std::same_as<typename Backend::DecodeResult>;
        { decoded.ok } -> std::convertible_to<bool>;
        { decoded.batch } -> std::convertible_to<bool>;
        requires std::ranges::random_access_range<decltype(decoded.requests)>;
        requires std::same_as<std::ranges::range_value_t<decltype(decoded.requests)>, typename Backend::DecodedRequest>;
        { decoded.requests.size() } -> std::convertible_to<std::size_t>;
        { decoded.requests.empty() } -> std::convertible_to<bool>;
        { decoded.requests[std::size_t{}] } -> std::convertible_to<const typename Backend::DecodedRequest&>;
        { decoded.responses } -> std::same_as<typename Backend::ResponseValues&>;
        { responses.size() } -> std::convertible_to<std::size_t>;
        { responses.empty() } -> std::convertible_to<bool>;
        responses.insert(responses.end(), responses.begin(), responses.end());
        responses.insert(responses.end(), std::make_move_iterator(responses.begin()),
                         std::make_move_iterator(responses.end()));
        { encoded.id } -> std::same_as<typename Backend::Id&>;
        { encoded.message } -> std::same_as<typename Backend::Message&>;
        { Backend::methodName(request) } -> std::convertible_to<std::string_view>;
        { Backend::id(request) } -> std::same_as<const typename Backend::Id&>;
        { Backend::expectsResponse(request) } -> std::same_as<bool>;
        { Backend::appendError(responses, request, error) } -> std::same_as<void>;
        { Backend::encodeResponses(sctxt, psess, responses, batch) } -> std::same_as<typename Backend::Message>;
        { Backend::clientNotInitError() } -> std::same_as<std::error_code>;
        { Backend::notificationOk() } -> std::same_as<std::error_code>;
        requires (!requires { Backend::validateMessage(cctxt, message); } || requires {
            { Backend::validateMessage(cctxt, message) } -> std::same_as<ilias::Result<void, std::error_code>>;
        });
        requires (!requires { Backend::validateMessage(sctxt, message); } || requires {
            { Backend::validateMessage(sctxt, message) } -> std::same_as<ilias::Result<void, std::error_code>>;
        });
        requires (!requires { Backend::handleClientControl(cctxt, psess, endpoint, message); } || requires {
            { Backend::handleClientControl(cctxt, psess, endpoint, message) } -> std::same_as<ilias::IoTask<bool>>;
        });
        requires (!requires { Backend::encodeCancel(id); } || requires {
            { Backend::encodeCancel(id) } -> std::same_as<typename Backend::Message>;
        });
        requires (!requires { Backend::cancelId(message); } || requires {
            { Backend::cancelId(message) } -> std::same_as<std::optional<typename Backend::Id>>;
        });
        requires (!requires { Backend::recoverClientCall(cctxt, psess, endpoint, error, retryCount); } || requires {
            { Backend::recoverClientCall(cctxt, psess, endpoint, error, retryCount) }
                -> std::same_as<ilias::IoTask<bool>>;
        });
    } && (!detail::RpcMultiplexBackend<Backend> ||
          requires(typename Backend::ClientContext& context, typename Backend::PeerSession& session,
                   std::span<const std::byte> message) {
              { Backend::handleClientControl(context, session, message) } -> std::same_as<bool>;
          });

namespace detail {

template <typename Backend, typename Method>
using RpcBackendParamsResult = decltype(Backend::template decodeParams<Method>(
    std::declval<const typename Backend::DecodedRequest&>(), std::declval<const Method&>()));

template <typename Backend, typename Method>
using RpcBackendParams = typename RpcBackendParamsResult<Backend, Method>::value_type;

template <typename Backend, typename Method, typename Params>
struct RpcBackendMethodCheck : std::false_type {};

template <typename Backend, typename Method, typename... Args>
struct RpcBackendMethodCheck<Backend, Method, std::tuple<Args...>> {
    static constexpr bool value = requires(
        typename Backend::ClientContext& context, typename Backend::PeerSession& session,
        Method& method, const typename Backend::DecodedRequest& request,
        typename Backend::ResponseValues& responses, std::span<const std::byte> message,
        const typename Backend::Id& id,
        ilias::Result<typename Method::RawReturnType, std::error_code> result, std::decay_t<Args>&... args) {
        { Backend::template encodeRequest<Method>(context, session, method, false, args...) }
            -> std::same_as<ilias::Result<typename Backend::EncodedRequest, std::error_code>>;
        { Backend::template decodeResponse<Method>(context, session, message, id) }
            -> std::same_as<ilias::Result<typename Method::RawReturnType, std::error_code>>;
        typename RpcBackendParams<Backend, Method>;
        { Backend::template decodeParams<Method>(request, method) }
            -> std::same_as<ilias::Result<RpcBackendParams<Backend, Method>, std::error_code>>;
        { Backend::template invoke<Method>(method, std::declval<RpcBackendParams<Backend, Method>>()) }
            -> std::same_as<ilias::IoTask<typename Method::RawReturnType>>;
        { Backend::template appendMethodReturn<Method>(responses, request, std::move(result)) } -> std::same_as<void>;
    };
};

} // namespace detail

// Check typed operations for the actual method, without assuming that a backend
// supports an arbitrary built-in probe signature or a particular serializer.
template <typename Backend, typename Method>
concept RpcBackendFor = RpcBackend<Backend> && requires {
    typename Method::RawReturnType;
    typename Method::RawParamsType;
} && detail::RpcBackendMethodCheck<Backend, Method, typename Method::RawParamsType>::value;

template <typename Backend, typename T>
concept BackendSerializable = std::is_void_v<T> || requires { requires Backend::template serializable<T>(); };

} // namespace nekoproto
NEKO_END_EXPORT
