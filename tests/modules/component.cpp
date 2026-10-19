#include <coroutine>
#include <string>
#include <tuple>
#include <type_traits>
#include <system_error>
#include <vector>
#if defined(NEKO_TEST_SERIALIZER)
import nekoproto.serializer;
#endif
#if defined(NEKO_TEST_ARGPARSER)
import nekoproto.argparser;
#endif
#if defined(NEKO_TEST_PROTOCOL)
import nekoproto.proto;
#endif
#if defined(NEKO_TEST_TRANSPORT)
import nekoproto.transport;
#endif
#if defined(NEKO_TEST_RPC)
import nekoproto.rpc;
#endif
#if defined(NEKO_TEST_JSONRPC)
import nekoproto.jsonrpc;
#endif
#if defined(NEKO_TEST_COMMUNICATION)
import nekoproto.communication;
#endif
#include "component_shared.hpp"

#if defined(NEKO_TEST_ARGPARSER)
struct ModuleOptions { int count = 0; };
template <> struct nekoproto::Meta<ModuleOptions> {
    static constexpr auto value = nekoproto::Object("count", &ModuleOptions::count);
};
#endif

template <typename Serializer>
bool roundtrip() {
    ModuleRecord source{7, "module"};
    std::vector<char> data;
    typename Serializer::OutputSerializer output(data);
    if (!output(source) || !output.end()) return false;
    typename Serializer::InputSerializer input(data.data(), data.size());
    ModuleRecord decoded;
    return input(decoded) && decoded == source;
}

int main() {
#if defined(NEKO_TEST_RPC) || defined(NEKO_TEST_JSONRPC)
    ilias::PlatformContext io_context;
    io_context.install();
#endif
    const ModuleRecord source{9, "header"};
    auto shared = module_header_roundtrip(source);
    if (!shared || *shared != source || !roundtrip<nekoproto::BinarySerializer>()) return 1;
#if defined(NEKO_TEST_SERIALIZER)
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON) || defined(NEKO_PROTO_ENABLE_YYJSON) || defined(NEKO_PROTO_ENABLE_SIMDJSON)
    if (!roundtrip<nekoproto::JsonSerializer>()) return 2;
#endif
#if defined(NEKO_PROTO_ENABLE_LIBFYAML) || defined(NEKO_PROTO_ENABLE_YAMLCPP)
    if (!roundtrip<nekoproto::YamlSerializer>()) return 3;
#endif
#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)
    if (!roundtrip<nekoproto::TomlSerializer>()) return 4;
#endif
#if defined(NEKO_PROTO_ENABLE_PUGIXML)
    std::vector<char> xml;
    nekoproto::PugiXmlOutputSerializer output(xml, "module");
    if (!output(source) || !output.end()) return 5;
    nekoproto::PugiXmlInputSerializer input(xml.data(), xml.size());
    ModuleRecord decoded;
    if (!input(decoded) || decoded != source) return 6;
#endif
#endif
#if defined(NEKO_TEST_ARGPARSER)
    const char* argv[] = {"module", "--count", "12"};
    auto options = nekoproto::argparser::parser<ModuleOptions>(3, argv);
    if (!options || options->count != 12) return 7;
#endif
#if defined(NEKO_TEST_PROTOCOL)
    nekoproto::ProtoFactory factory(1, 2, 3);
    if (factory.version() != 0x010203) return 8;
    auto packet = ModuleMessage::makeProto(ModuleMessage{17});
    if (packet.toData().empty() || packet.getField<int>("id", 0) != 17) return 16;
    auto reflected = nekoproto::ReflectionSerializer::reflection(source);
    if (reflected.getObject()->getField<int>("id", 0) != 9) return 9;
#endif
#if defined(NEKO_TEST_TRANSPORT)
    static_assert(std::is_abstract_v<nekoproto::detail::IMessageEndpoint>);
    static_assert(!nekoproto::MessageEndpoint<int>);
#endif
#if defined(NEKO_TEST_RPC)
    std::error_code error = nekoproto::RpcError::MethodNotFound;
    if (error.message() != "Method Not Found") return 10;
    using Backend = nekoproto::NekoRpcBackend<nekoproto::BinarySerializer>;
    auto context = Backend::makeClientContext();
    if (context.next_id != 0) return 11;
    nekoproto::RpcServer<Backend> server(io_context);
    nekoproto::RpcClient<Backend> client(io_context);
#endif
#if defined(NEKO_TEST_JSONRPC)
    nekoproto::JsonRpcServer<> json_server(io_context);
    nekoproto::JsonRpcClient<> json_client(io_context);
    nekoproto::detail::JsonRpcRequestMethod request;
    request.method = "module.call";
    std::vector<char> json;
    nekoproto::JsonSerializer::OutputSerializer out(json);
    if (!out(request) || !out.end()) return 12;
    nekoproto::detail::JsonRpcRequestMethod decoded_request;
    nekoproto::JsonSerializer::InputSerializer in(json.data(), json.size());
    if (!in(decoded_request) || decoded_request.method != request.method) return 13;
#endif
#if defined(NEKO_TEST_COMMUNICATION)
    nekoproto::MessageHeader header(7, 42, nekoproto::MessageType::Complete), decoded_header;
    std::vector<char> bytes;
    nekoproto::BinarySerializer::OutputSerializer out_header(bytes);
    if (!out_header(header) || !out_header.end()) return 14;
    nekoproto::BinarySerializer::InputSerializer in_header(bytes.data(), bytes.size());
    if (!in_header(decoded_header) || decoded_header.length != 7 || decoded_header.data != 42) return 15;
#endif
    return 0;
}
