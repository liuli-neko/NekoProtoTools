#pragma once
#include "nekoproto/global/macros.hpp"

#include <string>
#include <utility>

#include "nekoproto/reflect.hpp"
#include "nekoproto/transport/endpoint.hpp"

NEKO_BEGIN_EXPORT
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
NEKO_END_EXPORT
