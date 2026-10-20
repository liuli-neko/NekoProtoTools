#pragma once
#include "nekoproto/global/macros.hpp"

#include <string>
#include <utility>

#include "nekoproto/reflect.hpp"
#include "nekoproto/transport/endpoint.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto {

struct RpcMethodMetadata {
    std::string name;
    std::string signature;
    std::string description;
    std::string rpcVersion;
    std::vector<std::string> argNames;
    bool isNotification = false;
    bool isBind = false;
    bool usesContext = false;
};

namespace detail {

using RpcMethodMetadata = ::nekoproto::RpcMethodMetadata;

template <typename Backend, typename StreamT>
concept RpcClientStreamBackend = CommunicationStream<StreamT> &&
    (requires(StreamT stream, typename Backend::ClientContext& context) {
        { Backend::makeClientEndpoint(std::move(stream), context.options) } -> MessageEndpoint;
    } || requires(StreamT stream) {
        { Backend::makeClientEndpoint(std::move(stream)) } -> MessageEndpoint;
    } || requires(StreamT stream, typename Backend::ClientContext& context) {
        { Backend::makeEndpoint(std::move(stream), context.options) } -> MessageEndpoint;
    } || requires(StreamT stream) {
        { Backend::makeEndpoint(std::move(stream)) } -> MessageEndpoint;
    });

template <typename Backend, typename StreamT>
concept RpcServerStreamBackend = CommunicationStream<StreamT> &&
    (requires(StreamT stream, std::vector<RpcMethodMetadata> methods, typename Backend::ServerContext& context) {
        { Backend::makeServerEndpoint(std::move(stream), std::move(methods), context.options) } -> MessageEndpoint;
    } || requires(StreamT stream, std::vector<RpcMethodMetadata> methods) {
        { Backend::makeServerEndpoint(std::move(stream), std::move(methods)) } -> MessageEndpoint;
    } || requires(StreamT stream, typename Backend::ServerContext& context) {
        { Backend::makeEndpoint(std::move(stream), context.options) } -> MessageEndpoint;
    } || requires(StreamT stream) {
        { Backend::makeEndpoint(std::move(stream)) } -> MessageEndpoint;
    });

} // namespace detail

} // namespace nekoproto
NEKO_END_EXPORT
