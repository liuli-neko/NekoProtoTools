#pragma once

#include <string>
#include <utility>

#include "nekoproto/global/global.hpp"
#include "nekoproto/transport/endpoint.hpp"

namespace nekoproto {

namespace detail {

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

template <typename Backend, typename StreamT>
concept RpcStreamBackend = CommunicationStream<StreamT> && requires(StreamT stream) {
    { Backend::makeEndpoint(std::move(stream)) } -> MessageEndpoint;
};

} // namespace detail

} // namespace nekoproto
