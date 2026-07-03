#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "nekoproto/global/global.hpp"
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
namespace rpc {

template <typename Tuple>
struct NekoRpcDecayTuple;

template <typename... Args>
struct NekoRpcDecayTuple<std::tuple<Args...>> {
    using type = std::tuple<std::decay_t<Args>...>;
};

template <typename Tuple>
using NekoRpcDecayTupleType = typename NekoRpcDecayTuple<Tuple>::type;

struct NekoRpcError {
    std::int32_t code = static_cast<std::int32_t>(RpcError::InternalError);
    std::string message;
    std::vector<std::byte> data;
};

template <typename SerializerT, typename BufferT, typename = void>
struct NekoRpcByteOutputSerializerAvailable : std::false_type {};

template <typename SerializerT, typename BufferT>
struct NekoRpcByteOutputSerializerAvailable<SerializerT, BufferT,
                                            std::void_t<typename SerializerT::ByteOutputSerializer>>
    : std::bool_constant<std::is_constructible_v<typename SerializerT::ByteOutputSerializer, BufferT&>> {};

} // namespace rpc
NEKO_END_NAMESPACE
