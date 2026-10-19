#include <nekoproto/global/config.h>
#include <nekoproto/argparser/argparser.hpp>
#include <nekoproto/reflect.hpp>
#include <nekoproto/serialization/binary_serializer.hpp>
#ifdef NEKO_PROTO_ENABLE_RAPIDJSON
#include <nekoproto/serialization/json/rapid_json_serializer.hpp>
#endif
#ifdef NEKO_PROTO_ENABLE_YYJSON
#include <nekoproto/serialization/json/yyjson_serializer.hpp>
#endif
#ifdef NEKO_PROTO_ENABLE_SIMDJSON
#include <nekoproto/serialization/json/simd_json_serializer.hpp>
#endif
#ifdef NEKO_CONSUMER_HAS_PUGIXML
#include <nekoproto/serialization/xml_serializer.hpp>
#endif
#if defined(NEKO_PROTO_ENABLE_YAMLCPP) || defined(NEKO_PROTO_ENABLE_LIBFYAML)
#include <nekoproto/serialization/yaml_serializer.hpp>
#endif
#ifdef NEKO_PROTO_ENABLE_TOMLPLUSPLUS
#include <nekoproto/serialization/toml_serializer.hpp>
#endif
#ifdef NEKO_CONSUMER_HAS_PROTOCOL
#include <nekoproto/proto/proto_base.hpp>
#endif
#ifdef NEKO_CONSUMER_HAS_COMMUNICATION
#include <nekoproto/communication/communication_base.hpp>
#endif
#ifdef NEKO_CONSUMER_HAS_JSONRPC
#include <nekoproto/rpc/backend.hpp>
#ifdef NEKO_PROTO_RPC_TRACE
#include <nekoproto/rpc/tracing.hpp>
#endif
#endif

#if defined(NEKO_CONSUMER_HAS_COMMUNICATION) || defined(NEKO_CONSUMER_HAS_JSONRPC)
#include <ilias/platform.hpp>
#include <chrono>
#endif

#include <string>
#include <vector>

struct Message {
    int id = 3;
    std::string name = "consumer";
#ifdef NEKO_CONSUMER_HAS_PROTOCOL
    NEKO_DECLARE_PROTOCOL(Message, nekoproto::BinarySerializer)
#endif
};

template <>
struct nekoproto::Meta<Message> {
    static constexpr auto value = nekoproto::Object("id", &Message::id, "name", &Message::name);
};

int main() {
    const Message source;
    std::vector<char> bytes;
    nekoproto::BinarySerializer::OutputSerializer output(bytes);
    if (!output(source)) return 1;
    Message copy;
    nekoproto::BinarySerializer::InputSerializer input(bytes.data(), bytes.size());
    if (!input(copy)) return 2;
    if (copy.id != source.id || copy.name != source.name) return 3;
    const char* args[] = {"consumer", "--id", "7", "--name", "Neko"};
    auto parsed = nekoproto::argparser::parser<Message>(5, args);
    if (!parsed || parsed->id != 7 || parsed->name != "Neko") return 5;
    const char* help_args[] = {"consumer", "--help"};
    auto help = nekoproto::argparser::parser<Message>(2, help_args);
    if (help || help.error() != nekoproto::argparser::makeErrorCode(
                                   nekoproto::argparser::ArgParserError::HelpRequested)) return 6;
    if (nekoproto::argparser::formatHelp<Message>().find("--id") == std::string::npos) return 7;
#ifdef NEKO_CONSUMER_HAS_PROTOCOL
    auto wrapped = Message::makeProto(source);
    if (wrapped == nullptr) return 4;
    nekoproto::ProtoFactory factory(1, 0, 0);
    if (factory.version() != 0x010000U) return 4;
    auto created = factory.create("Message");
    if (created == nullptr || !created.fromData(bytes.data(), bytes.size())) return 8;
    const auto* protocol_copy = created.cast<Message>();
    if (protocol_copy == nullptr || protocol_copy->id != source.id || protocol_copy->name != source.name) return 9;
#endif
#ifdef NEKO_CONSUMER_HAS_COMMUNICATION
    if (std::string{nekoproto::ErrorCategory::instance().name()} != "NekoCommunicationError") return 24;
#endif
#if defined(NEKO_CONSUMER_HAS_COMMUNICATION) || defined(NEKO_CONSUMER_HAS_JSONRPC)
    ilias::PlatformContext context;
    context.install();
    ilias::sleep(std::chrono::milliseconds(1)).wait();
#endif
#ifdef NEKO_CONSUMER_HAS_JSONRPC
    if (nekoproto::rpc::NekoRpcFrameCodec::headerSize() == 0) return 25;
#ifdef NEKO_PROTO_RPC_TRACE
    if (nekoproto::RpcTraceRegistry::instance().snapshotJson().empty()) return 26;
#endif
#endif
#ifdef NEKO_CONSUMER_HAS_PUGIXML
    std::vector<char> xml;
    nekoproto::PugiXmlOutputSerializer xml_output(xml, "message");
    if (!xml_output(source) || !xml_output.end()) return 10;
    Message xml_copy;
    nekoproto::PugiXmlInputSerializer xml_input(xml.data(), xml.size());
    if (!xml_input(xml_copy) || xml_copy.id != source.id || xml_copy.name != source.name) return 11;
#endif
#ifdef NEKO_PROTO_ENABLE_RAPIDJSON
    std::vector<char> json;
    nekoproto::RapidJsonSerializer::OutputSerializer json_output(json);
    if (!json_output(source)) return 12;
    Message json_copy;
    nekoproto::RapidJsonSerializer::InputSerializer json_input(json.data(), json.size());
    if (!json_input(json_copy) || json_copy.id != source.id || json_copy.name != source.name) return 13;
#endif
#ifdef NEKO_PROTO_ENABLE_YYJSON
    std::vector<char> yyjson_data;
    nekoproto::YyJsonSerializer::OutputSerializer yyjson_output(yyjson_data);
    if (!yyjson_output(source) || !yyjson_output.end()) return 14;
    Message yyjson_copy;
    nekoproto::YyJsonSerializer::InputSerializer yyjson_input(yyjson_data.data(), yyjson_data.size());
    if (!yyjson_input(yyjson_copy) || yyjson_copy.id != source.id || yyjson_copy.name != source.name) return 15;
#endif
#ifdef NEKO_PROTO_ENABLE_SIMDJSON
    std::vector<char> simdjson_data;
    nekoproto::SimdJsonSerializer::OutputSerializer simdjson_output(simdjson_data);
    if (!simdjson_output(source) || !simdjson_output.end()) return 16;
    Message simdjson_copy;
    nekoproto::SimdJsonSerializer::InputSerializer simdjson_input(simdjson_data.data(), simdjson_data.size());
    if (!simdjson_input(simdjson_copy) || simdjson_copy.id != source.id || simdjson_copy.name != source.name) return 17;
#endif
#ifdef NEKO_PROTO_ENABLE_YAMLCPP
    std::vector<char> yaml_data;
    nekoproto::YamlCppSerializer::OutputSerializer yaml_output(yaml_data);
    if (!yaml_output(source) || !yaml_output.end()) return 18;
    Message yaml_copy;
    nekoproto::YamlCppSerializer::InputSerializer yaml_input(yaml_data.data(), yaml_data.size());
    if (!yaml_input(yaml_copy) || yaml_copy.id != source.id || yaml_copy.name != source.name) return 19;
#endif
#ifdef NEKO_PROTO_ENABLE_LIBFYAML
    std::vector<char> fyaml_data;
    nekoproto::LibfyamlSerializer::OutputSerializer fyaml_output(fyaml_data);
    if (!fyaml_output(source) || !fyaml_output.end()) return 20;
    Message fyaml_copy;
    nekoproto::LibfyamlSerializer::InputSerializer fyaml_input(fyaml_data.data(), fyaml_data.size());
    if (!fyaml_input(fyaml_copy) || fyaml_copy.id != source.id || fyaml_copy.name != source.name) return 21;
#endif
#ifdef NEKO_PROTO_ENABLE_TOMLPLUSPLUS
    std::vector<char> toml_data;
    nekoproto::TomlSerializer::OutputSerializer toml_output(toml_data);
    if (!toml_output(source) || !toml_output.end()) return 22;
    Message toml_copy;
    nekoproto::TomlSerializer::InputSerializer toml_input(toml_data.data(), toml_data.size());
    if (!toml_input(toml_copy) || toml_copy.id != source.id || toml_copy.name != source.name) return 23;
#endif
    return 0;
}
