#pragma once

#include <string>
#include <utility>

#include "nekoproto/global/global.hpp"
#include "nekoproto/transport/endpoint.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {

struct RpcMethodMetadata {
    std::string name;
    std::string description;
    bool isBind = false;
};

template <typename Backend, typename StreamT>
concept RpcStreamBackend = CommunicationStream<StreamT> && requires(StreamT stream) {
    { Backend::makeEndpoint(std::move(stream)) } -> MessageEndpoint;
};

} // namespace detail

NEKO_END_NAMESPACE
