#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "nekoproto/jsonrpc/backend.hpp"
#include "nekoproto/jsonrpc/protocol.hpp"
#include "nekoproto/rpc/method.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/reflection.hpp"

NEKO_USE_NAMESPACE

namespace {

struct ProtocolObjectParam {
    int count = 0;
    std::string label;

    NEKO_SERIALIZER(count, label)
};

using AddTraits = detail::RpcMethodTraits<int(int, int)>;
using OptionalTraits = detail::RpcMethodTraits<void(std::optional<int>)>;
using ObjectTraits = detail::RpcMethodTraits<std::string(ProtocolObjectParam)>;
using TupleTraits = detail::RpcMethodTraits<std::string(std::tuple<int, double, bool>)>;
using PingTraits = detail::RpcMethodTraits<void()>;
using AddMethod = RpcMethodSpec<int(int, int), rpc_args<"lhs", "rhs">>;

static_assert(detail::JsonRpcMethodTraits<AddTraits>::ParamsSize == 2);
static_assert(std::is_same_v<detail::JsonRpcMethodTraits<AddTraits>::ParamsTupleType, std::tuple<int, int>>);
static_assert(detail::JsonRpcMethodTraits<ObjectTraits>::IsAutomaticExpansionAble);
static_assert(std::is_same_v<detail::JsonRpcMethodTraits<ObjectTraits>::ParamsTupleType, ProtocolObjectParam>);
static_assert(detail::JsonRpcMethodTraits<TupleTraits>::IsTopTuple);
static_assert(detail::JsonRpcMethodTraits<OptionalTraits>::IsNullAble);
static_assert(std::is_same_v<detail::JsonRpcMethodTraits<PingTraits>::ParamsTupleType, std::tuple<>>);

template <typename T>
std::string writeJson(const T& value) {
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer out(buffer);
    EXPECT_TRUE(out(value));
    EXPECT_TRUE(out.end());
    return {buffer.begin(), buffer.end()};
}

template <typename T>
bool readJson(std::string_view json, T& value) {
    JsonSerializer::InputSerializer in(json.data(), json.size());
    const bool parsed = in(value);
    return parsed && static_cast<bool>(in);
}

} // namespace

TEST(JsonRpcProtocol, NamedParamsRequestRoundTripsAsObject) {
    using Request = detail::JsonRpcRequest2<AddTraits>;
    const std::vector<std::string> argNames{"lhs", "rhs"};
    const detail::JsonRpcMethodContext context{.argNames = argNames};

    Request request;
    request.method   = "math.add";
    request.params   = std::make_tuple(20, 22);
    request.id       = uint64_t{7};
    detail::JsonRpcRequestWithContext<Request> requestWithContext{request, context};

    EXPECT_EQ(writeJson(requestWithContext),
              R"({"jsonrpc":"2.0","method":"math.add","id":7,"params":{"lhs":20,"rhs":22}})");

    Request decoded;
    detail::JsonRpcRequestWithContext<Request> decodedWithContext{decoded, context};
    ASSERT_TRUE(
        readJson(R"({"jsonrpc":"2.0","method":"math.add","id":7,"params":{"rhs":22,"lhs":20}})", decodedWithContext));
    EXPECT_EQ(decoded.method, "math.add");
    EXPECT_EQ(std::get<0>(decoded.params), 20);
    EXPECT_EQ(std::get<1>(decoded.params), 22);
    ASSERT_TRUE(std::holds_alternative<uint64_t>(decoded.id));
    EXPECT_EQ(std::get<uint64_t>(decoded.id), 7U);
}

TEST(JsonRpcProtocol, PositionalParamsRequestUsesArrayForUnnamedParameters) {
    using Request = detail::JsonRpcRequest2<AddTraits>;

    Request request;
    request.method = "math.add";
    request.params = std::make_tuple(3, 4);
    request.id     = std::string{"req-1"};

    EXPECT_EQ(writeJson(request), R"({"jsonrpc":"2.0","method":"math.add","id":"req-1","params":[3,4]})");

    Request decoded;
    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","method":"math.add","id":"req-1","params":[3,4]})", decoded));
    EXPECT_EQ(decoded.method, "math.add");
    EXPECT_EQ(decoded.params, std::make_tuple(3, 4));
    ASSERT_TRUE(std::holds_alternative<std::string>(decoded.id));
    EXPECT_EQ(std::get<std::string>(decoded.id), "req-1");
}

TEST(JsonRpcProtocol, ReflectedSingleParameterExpandsToParamsObject) {
    using Request = detail::JsonRpcRequest2<ObjectTraits>;

    Request request;
    request.method       = "object.echo";
    request.params.count = 3;
    request.params.label = "three";
    request.id           = std::string{"object-id"};

    EXPECT_EQ(writeJson(request),
              R"({"jsonrpc":"2.0","method":"object.echo","id":"object-id","params":{"count":3,"label":"three"}})");

    Request decoded;
    ASSERT_TRUE(readJson(
        R"({"jsonrpc":"2.0","method":"object.echo","id":"object-id","params":{"count":3,"label":"three"}})",
        decoded));
    EXPECT_EQ(decoded.params.count, 3);
    EXPECT_EQ(decoded.params.label, "three");
}

TEST(JsonRpcProtocol, SingleTupleParameterKeepsTopLevelParamsArray) {
    using Request = detail::JsonRpcRequest2<TupleTraits>;

    Request request;
    request.method = "tuple.echo";
    request.params = std::make_tuple(1, 2.5, true);

    EXPECT_EQ(writeJson(request), R"({"jsonrpc":"2.0","method":"tuple.echo","params":[1,2.5,true]})");

    Request decoded;
    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","method":"tuple.echo","id":null,"params":[1,2.5,true]})", decoded));
    EXPECT_EQ(decoded.params, std::make_tuple(1, 2.5, true));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(decoded.id));
}

TEST(JsonRpcProtocol, EmptyParamsRequestWritesEmptyArray) {
    using Request = detail::JsonRpcRequest2<PingTraits>;

    Request request;
    request.method = "ping";

    EXPECT_EQ(writeJson(request), R"({"jsonrpc":"2.0","method":"ping","params":[]})");

    Request decoded;
    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","method":"ping","id":null,"params":[]})", decoded));
    EXPECT_EQ(decoded.method, "ping");
}

TEST(JsonRpcProtocol, OmittedNullableParamsAreAccepted) {
    using Request = detail::JsonRpcRequest2<OptionalTraits>;
    const std::vector<std::string> argNames{"value"};
    const detail::JsonRpcMethodContext context{.argNames = argNames};

    Request request;
    detail::JsonRpcRequestWithContext<Request> requestWithContext{request, context};
    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","method":"maybe","id":1})", requestWithContext));
    EXPECT_EQ(request.method, "maybe");
    EXPECT_FALSE(std::get<0>(request.params).has_value());
}

TEST(JsonRpcProtocol, OmittedRequiredParamsAreRejected) {
    using Request = detail::JsonRpcRequest2<AddTraits>;
    const std::vector<std::string> argNames{"lhs", "rhs"};
    const detail::JsonRpcMethodContext context{.argNames = argNames};

    Request request;
    detail::JsonRpcRequestWithContext<Request> requestWithContext{request, context};
    EXPECT_FALSE(readJson(R"({"jsonrpc":"2.0","method":"math.add","id":1})", requestWithContext));
}

TEST(JsonRpcProtocol, NonObjectRequestIsRejected) {
    using Request = detail::JsonRpcRequest2<AddTraits>;

    Request request;
    EXPECT_FALSE(readJson(R"([1,2,3])", request));
}

TEST(JsonRpcProtocol, HelperObjectCanFallbackToTupleArrayWithoutNames) {
    std::tuple<int, std::string> params{7, "seven"};
    detail::JsonRpcSerializerHelperObject<decltype(params)> helper{params};

    EXPECT_EQ(writeJson(helper), R"([7,"seven"])");

    std::tuple<int, std::string> decodedParams;
    detail::JsonRpcSerializerHelperObject<decltype(decodedParams)> decoded{decodedParams};
    ASSERT_TRUE(readJson(R"([8,"eight"])", decoded));
    EXPECT_EQ(decodedParams, std::make_tuple(8, std::string{"eight"}));
}

TEST(JsonRpcProtocol, MethodProbeIgnoresParamsAndKeepsRequestId) {
    detail::JsonRpcRequestMethod method;
    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","method":"math.add","id":"probe","params":[1,2]})", method));

    EXPECT_EQ(method.jsonrpc, std::optional<std::string>{"2.0"});
    EXPECT_EQ(method.method, "math.add");
    ASSERT_TRUE(std::holds_alternative<std::string>(method.id));
    EXPECT_EQ(std::get<std::string>(method.id), "probe");
    EXPECT_EQ(writeJson(method), R"({"jsonrpc":"2.0","method":"math.add","id":"probe"})");
}

TEST(JsonRpcProtocol, SuccessResponseSkipsErrorField) {
    detail::JsonRpcResponse<AddTraits> response;
    response.result = 42;
    response.id     = uint64_t{9};

    EXPECT_EQ(writeJson(response), R"({"jsonrpc":"2.0","result":42,"id":9})");

    detail::JsonRpcResponse<AddTraits> decoded;
    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","result":42,"id":9})", decoded));
    ASSERT_TRUE(decoded.result.has_value());
    EXPECT_EQ(decoded.result.value(), 42);
    EXPECT_FALSE(decoded.error.has_value());
}

TEST(JsonRpcProtocol, ErrorResponseSkipsMissingResultAndKeepsErrorObject) {
    detail::JsonRpcResponse<AddTraits> response;

    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","error":{"code":-32601,"message":"missing"},"id":"req-9"})", response));
    EXPECT_FALSE(response.result.has_value());
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, -32601);
    EXPECT_EQ(response.error->message, "missing");
    ASSERT_TRUE(std::holds_alternative<std::string>(response.id));
    EXPECT_EQ(std::get<std::string>(response.id), "req-9");
}

TEST(JsonRpcProtocol, VoidResponseDoesNotExposeResultField) {
    detail::JsonRpcResponse<void> response;
    response.error = detail::JsonRpcErrorResponse{.code = -32603, .message = "internal"};

    EXPECT_EQ(writeJson(response), R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"internal"}})");

    detail::JsonRpcResponse<void> decoded;
    ASSERT_TRUE(readJson(R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"internal"},"id":null})", decoded));
    ASSERT_TRUE(decoded.error.has_value());
    EXPECT_EQ(decoded.error->code, -32603);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(decoded.id));
}

TEST(JsonRpcProtocol, DecodeIncomingReturnsErrorsAndContinuesMixedBatch) {
    JsonRpcBackend::ServerContext context;
    JsonRpcBackend::PeerSession session;

    const auto parseError = JsonRpcBackend::decodeIncoming(
        context, session, {reinterpret_cast<const std::byte*>("{"), 1U});
    ASSERT_TRUE(parseError.ok);
    ASSERT_EQ(parseError.responses.size(), 1U);
    detail::JsonRpcResponse<detail::RpcMethodTraits<void(void)>> errorResponse;
    ASSERT_TRUE(readJson(writeJson(parseError.responses.front()), errorResponse));
    ASSERT_TRUE(errorResponse.error.has_value());
    EXPECT_EQ(errorResponse.error->code, static_cast<int>(JsonRpcError::ParseError));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(errorResponse.id));

    constexpr std::string_view emptyBatch = "[]";
    const auto empty = JsonRpcBackend::decodeIncoming(
        context, session,
        {reinterpret_cast<const std::byte*>(emptyBatch.data()), emptyBatch.size()});
    ASSERT_TRUE(empty.ok);
    EXPECT_FALSE(empty.batch);
    ASSERT_EQ(empty.responses.size(), 1U);
    errorResponse = {};
    ASSERT_TRUE(readJson(writeJson(empty.responses.front()), errorResponse));
    ASSERT_TRUE(errorResponse.error.has_value());
    EXPECT_EQ(errorResponse.error->code, static_cast<int>(JsonRpcError::InvalidRequest));

    constexpr std::string_view mixed =
        R"([{"jsonrpc":"2.0","method":"a","id":1},1,{"jsonrpc":"2.0","method":"b","id":2}])";
    const auto decoded = JsonRpcBackend::decodeIncoming(
        context, session, {reinterpret_cast<const std::byte*>(mixed.data()), mixed.size()});
    ASSERT_TRUE(decoded.ok);
    EXPECT_TRUE(decoded.batch);
    EXPECT_EQ(decoded.requests.size(), 2U);
    EXPECT_EQ(decoded.responses.size(), 1U);
}

TEST(JsonRpcProtocol, DecodeIncomingRejectsWrongVersionAndHonorsMessageLimit) {
    JsonRpcBackend::ServerContext context;
    JsonRpcBackend::PeerSession session;
    constexpr std::string_view wrongVersion = R"({"jsonrpc":"1.0","method":"a","id":1})";
    auto decoded = JsonRpcBackend::decodeIncoming(
        context, session, {reinterpret_cast<const std::byte*>(wrongVersion.data()), wrongVersion.size()});
    EXPECT_TRUE(decoded.ok);
    EXPECT_TRUE(decoded.requests.empty());
    EXPECT_EQ(decoded.responses.size(), 1U);

    context.options.max_message_bytes = 1U;
    constexpr std::string_view request = "{}";
    decoded = JsonRpcBackend::decodeIncoming(
        context, session, {reinterpret_cast<const std::byte*>(request.data()), request.size()});
    ASSERT_EQ(decoded.responses.size(), 1U);
    detail::JsonRpcResponse<detail::RpcMethodTraits<void(void)>> response;
    ASSERT_TRUE(readJson(writeJson(decoded.responses.front()), response));
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, static_cast<int>(JsonRpcError::MessageToolLarge));
}

TEST(JsonRpcProtocol, DecodeResponseRequiresValidExclusiveEnvelope) {
    JsonRpcBackend::ClientContext context;
    JsonRpcBackend::PeerSession session;
    const JsonRpcBackend::Id id = std::uint64_t{7};
    const auto decode = [&](std::string_view json) {
        return JsonRpcBackend::decodeResponse<AddMethod>(
            context, session, {reinterpret_cast<const std::byte*>(json.data()), json.size()}, id);
    };

    auto valid = decode(R"({"jsonrpc":"2.0","result":42,"id":7})");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(valid.value(), 42);

    auto missingBoth = decode(R"({"jsonrpc":"2.0","id":7})");
    ASSERT_FALSE(missingBoth.has_value());
    EXPECT_EQ(missingBoth.error(), make_error_code(JsonRpcError::InvalidRequest));

    auto both = decode(
        R"({"jsonrpc":"2.0","result":42,"error":{"code":-32603,"message":"bad"},"id":7})");
    ASSERT_FALSE(both.has_value());
    EXPECT_EQ(both.error(), make_error_code(JsonRpcError::InvalidRequest));

    auto wrongVersion = decode(R"({"jsonrpc":"1.0","result":42,"id":7})");
    ASSERT_FALSE(wrongVersion.has_value());
    EXPECT_EQ(wrongVersion.error(), make_error_code(JsonRpcError::InvalidRequest));

    context.options.max_message_bytes = 1U;
    auto oversized = decode(R"({"jsonrpc":"2.0","result":42,"id":7})");
    ASSERT_FALSE(oversized.has_value());
    EXPECT_EQ(oversized.error(), make_error_code(JsonRpcError::MessageToolLarge));
}

#include "../common/common_main.cpp.in" // IWYU pragma: export
