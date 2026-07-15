#pragma once

#include "nekoproto/global/global.hpp"

#include <cstdint>

NEKO_BEGIN_NAMESPACE
namespace rpc {
enum class NekoRpcExtensionType : std::uint16_t {
    // Client Hello advertises MethodId support. Server Hello includes this only when MethodId is enabled for the
    // connection; later request/notify frames may then set NekoRpcFlag::MethodId and put an 8-byte id in `method`.
    MethodId = 1U,
    // Server Hello/table refresh publishes the current method table version.
    // MethodId request frames echo this value so the server can reject stale ids.
    MethodTableVersion = 2U,
    // Server Hello sends a complete method-id table when a client first negotiates MethodId.
    MethodTable = 3U,
    // Server Hello/table refresh sends incremental method-id updates after negotiation.
    MethodTableDelta = 4U,
    // MethodId request frames carry the resolved method signature hash.
    // The server uses it to detect stale or incompatible client-side method metadata.
    MethodSignatureHash = 5U,
    // Server Hello/table refresh tells clients which older table versions are still accepted.
    MethodMinimumCompatibleVersion = 6U,
    // Client Hello advertises compression support. Server Hello includes this only when compression is enabled for the
    // connection; later non-Hello frames may then set NekoRpcFlag::Compressed.
    Compression = 16U,
    // Hello frames name the compression algorithm supported by the client or selected by the server.
    CompressionAlgorithm = 17U,
    // Hello frames carry the smallest payload size worth compressing for this connection.
    CompressionMinPayloadSize = 18U,
};
}
NEKO_END_NAMESPACE
