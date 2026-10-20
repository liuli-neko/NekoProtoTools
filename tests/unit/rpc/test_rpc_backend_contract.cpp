#include <gtest/gtest.h>
#include <ilias/io/duplex.hpp>

#include "nekoproto/jsonrpc/jsonrpc.hpp"
#include "nekoproto/rpc/rpc.hpp"

using namespace nekoproto;

namespace {

using IntMethod = RpcMethodSpec<int(int, int)>;
using VoidMethod = RpcMethodSpec<void()>;

// A protocol-independent contract fixture. No context.options, integer wire
// IDs, stream adapter, cancellation, multiplexing or recovery are required.
struct OpaqueBackend {
    struct Id {
        unsigned value = 0;
        auto operator<(const Id& other) const -> bool { return value < other.value; }
    };
    using Message = std::vector<char>;
    using ResponseValues = std::vector<Message>;
    struct Options {};
    struct ClientContext {};
    struct ServerContext {};
    struct PeerSession {};
    struct DecodedRequest { Id id; };
    struct DecodeResult {
        bool ok = true;
        bool batch = false;
        std::vector<DecodedRequest> requests;
        ResponseValues responses;
    };
    struct EncodedRequest { Message message; Id id; };

    static auto makeClientContext(Options) -> ClientContext;
    static auto makeServerContext(Options, std::vector<RpcMethodMetadata>) -> ServerContext;
    static auto makeClientPeerSession(const ClientContext&) -> PeerSession;
    static auto makeServerPeerSession(const ServerContext&) -> PeerSession;
    static void refreshMethodCatalog(ServerContext&, std::vector<RpcMethodMetadata>);
    static auto ensureClientReady(ClientContext&, PeerSession&, detail::IMessageEndpoint&) -> ilias::IoTask<void>;
    static auto handleServerControl(ServerContext&, PeerSession&, detail::IMessageEndpoint&,
                                    std::span<const std::byte>) -> ilias::IoTask<bool>;
    static auto decodeIncoming(ServerContext&, PeerSession&, std::span<const std::byte>) -> DecodeResult;
    static auto methodName(const DecodedRequest&) -> std::string_view;
    static auto id(const DecodedRequest&) -> const Id&;
    static auto expectsResponse(const DecodedRequest&) -> bool;
    static void appendError(ResponseValues&, const DecodedRequest&, std::error_code);
    static auto encodeResponses(ServerContext&, PeerSession&, const ResponseValues&, bool) -> Message;
    static auto clientNotInitError() -> std::error_code;
    static auto notificationOk() -> std::error_code;

    template <typename Method, typename... Args>
    static auto encodeRequest(ClientContext&, PeerSession&, Method&, bool, Args&&...)
        -> ilias::Result<EncodedRequest, std::error_code>;
    template <typename Method>
    static auto decodeResponse(ClientContext&, PeerSession&, std::span<const std::byte>, const Id&)
        -> ilias::Result<typename Method::RawReturnType, std::error_code>;
    template <typename Method>
    static auto decodeParams(const DecodedRequest&, const Method&)
        -> ilias::Result<std::tuple<int, int>, std::error_code>;
    template <typename Method>
        requires std::same_as<typename Method::RawReturnType, int>
    static auto invoke(Method&, std::tuple<int, int>) -> ilias::IoTask<typename Method::RawReturnType>;
    template <typename Method>
    static void appendMethodReturn(ResponseValues&, const DecodedRequest&,
                                   ilias::Result<typename Method::RawReturnType, std::error_code>);
};

struct MissingClientFactory : OpaqueBackend {
    static auto makeClientContext(Options) -> ClientContext = delete;
};
struct MissingServerFactory : OpaqueBackend {
    static auto makeServerContext(Options, std::vector<RpcMethodMetadata>) -> ServerContext = delete;
};
struct MissingClientSession : OpaqueBackend {
    static auto makeClientPeerSession(const ClientContext&) -> PeerSession = delete;
};
struct MissingServerSession : OpaqueBackend {
    static auto makeServerPeerSession(const ServerContext&) -> PeerSession = delete;
};
struct MissingCatalogRefresh : OpaqueBackend {
    static void refreshMethodCatalog(ServerContext&, std::vector<RpcMethodMetadata>) = delete;
};
struct InvalidReadiness : OpaqueBackend {
    static auto ensureClientReady(ClientContext&, PeerSession&, detail::IMessageEndpoint&) -> bool;
};
struct InvalidServerControl : OpaqueBackend {
    static auto handleServerControl(ServerContext&, PeerSession&, detail::IMessageEndpoint&,
                                    std::span<const std::byte>) -> bool;
};
struct InvalidDecodedRequests : OpaqueBackend {
    struct DecodeResult {
        bool ok, batch;
        std::vector<int> requests;
        ResponseValues responses;
    };
    static auto decodeIncoming(ServerContext&, PeerSession&, std::span<const std::byte>) -> DecodeResult;
};
struct InvalidEncodedMessage : OpaqueBackend {
    struct EncodedRequest { std::vector<int> message; Id id; };
};
struct InvalidValidation : OpaqueBackend {
    static auto validateMessage(ClientContext&, std::span<const std::byte>) -> bool;
};
struct InvalidCancellation : OpaqueBackend {
    static auto cancelId(std::span<const std::byte>) -> Id;
};
struct InvalidClientControl : OpaqueBackend {
    static auto handleClientControl(ClientContext&, PeerSession&, detail::IMessageEndpoint&,
                                    std::span<const std::byte>) -> bool;
};
struct AsyncClientControl : OpaqueBackend {
    static auto handleClientControl(ClientContext&, PeerSession&, detail::IMessageEndpoint&,
                                    std::span<const std::byte>) -> ilias::IoTask<bool>;
};
struct MissingMultiplexControl : OpaqueBackend {
    static auto responseId(std::span<const std::byte>) -> std::optional<Id>;
};
struct MissingTypedDecode : OpaqueBackend {
    template <typename Method>
    static auto decodeResponse(ClientContext&, PeerSession&, std::span<const std::byte>, const Id&)
        -> ilias::Result<typename Method::RawReturnType, std::error_code> = delete;
};
struct MissingTypedInvoke : OpaqueBackend {
    template <typename Method>
    static auto invoke(Method&, std::tuple<int, int>) -> ilias::IoTask<typename Method::RawReturnType> = delete;
};
struct MissingTypedParams : OpaqueBackend {
    template <typename Method>
    static auto decodeParams(const DecodedRequest&, const Method&)
        -> ilias::Result<std::tuple<int, int>, std::error_code> = delete;
};
struct InvalidTypedEncode : OpaqueBackend {
    template <typename Method, typename... Args>
    static auto encodeRequest(ClientContext&, PeerSession&, Method&, bool, Args&&...) -> Message;
};

using StreamEndpoint = detail::LengthPrefixedStreamMessageEndpoint<ilias::DuplexStream>;
struct ClientFactoryOnly : OpaqueBackend {
    static auto makeClientEndpoint(ilias::DuplexStream) -> StreamEndpoint;
};
struct ServerFactoryOnly : OpaqueBackend {
    static auto makeServerEndpoint(ilias::DuplexStream, std::vector<RpcMethodMetadata>) -> StreamEndpoint;
};

} // namespace

TEST(RpcBackendContract, AcceptsBuiltinBackendsAndActualMethodSignatures) {
    EXPECT_TRUE(RpcBackend<BinaryRpcBackend>);
    EXPECT_TRUE(RpcBackend<JsonRpcBackend>);
    EXPECT_TRUE((RpcBackendFor<BinaryRpcBackend, IntMethod>));
    EXPECT_TRUE((RpcBackendFor<BinaryRpcBackend, VoidMethod>));
    EXPECT_TRUE((RpcBackendFor<JsonRpcBackend, IntMethod>));
    EXPECT_TRUE((RpcBackendFor<JsonRpcBackend, VoidMethod>));
}

TEST(RpcBackendContract, AllowsOpaqueStateAndOptionalProtocolCapabilities) {
    EXPECT_TRUE(RpcBackend<OpaqueBackend>);
    EXPECT_TRUE((RpcBackendFor<OpaqueBackend, IntMethod>));
    EXPECT_FALSE((RpcBackendFor<OpaqueBackend, VoidMethod>));
    EXPECT_FALSE(RpcBackend<int>);
}

TEST(RpcBackendContract, RejectsMissingContextAndSessionFactories) {
    EXPECT_FALSE(RpcBackend<MissingClientFactory>);
    EXPECT_FALSE(RpcBackend<MissingServerFactory>);
    EXPECT_FALSE(RpcBackend<MissingClientSession>);
    EXPECT_FALSE(RpcBackend<MissingServerSession>);
    EXPECT_FALSE(RpcBackend<MissingCatalogRefresh>);
}

TEST(RpcBackendContract, RejectsMalformedLifecycleAndMessageOperations) {
    EXPECT_FALSE(RpcBackend<InvalidReadiness>);
    EXPECT_FALSE(RpcBackend<InvalidServerControl>);
    EXPECT_FALSE(RpcBackend<InvalidDecodedRequests>);
    EXPECT_FALSE(RpcBackend<InvalidEncodedMessage>);
}

TEST(RpcBackendContract, ChecksOptionalHooksWhenProvided) {
    EXPECT_FALSE(RpcBackend<InvalidValidation>);
    EXPECT_FALSE(RpcBackend<InvalidCancellation>);
    EXPECT_FALSE(RpcBackend<InvalidClientControl>);
    EXPECT_TRUE(RpcBackend<AsyncClientControl>);
    EXPECT_FALSE(RpcBackend<MissingMultiplexControl>);
}

TEST(RpcBackendContract, RejectsMissingTypedOperationsForTheRegisteredMethod) {
    EXPECT_TRUE(RpcBackend<MissingTypedDecode>);
    EXPECT_TRUE(RpcBackend<MissingTypedInvoke>);
    EXPECT_FALSE((RpcBackendFor<MissingTypedDecode, IntMethod>));
    EXPECT_FALSE((RpcBackendFor<MissingTypedInvoke, IntMethod>));
    EXPECT_FALSE((RpcBackendFor<MissingTypedParams, IntMethod>));
    EXPECT_FALSE((RpcBackendFor<InvalidTypedEncode, IntMethod>));
}

TEST(RpcBackendContract, StreamAdaptersAreOptionalAndSelectedPerRole) {
    EXPECT_TRUE((detail::RpcClientStreamBackend<ClientFactoryOnly, ilias::DuplexStream>));
    EXPECT_FALSE((detail::RpcServerStreamBackend<ClientFactoryOnly, ilias::DuplexStream>));
    EXPECT_TRUE((detail::RpcServerStreamBackend<ServerFactoryOnly, ilias::DuplexStream>));
    EXPECT_FALSE((detail::RpcClientStreamBackend<ServerFactoryOnly, ilias::DuplexStream>));
    EXPECT_FALSE((detail::RpcClientStreamBackend<OpaqueBackend, ilias::DuplexStream>));
    EXPECT_FALSE((detail::RpcServerStreamBackend<OpaqueBackend, ilias::DuplexStream>));
    EXPECT_TRUE(RpcBackend<OpaqueBackend>);
}
