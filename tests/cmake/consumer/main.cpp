#include <nekoproto/argparser/argparser.hpp>
#include <nekoproto/reflect.hpp>
#include <nekoproto/serialization/binary_serializer.hpp>
#ifdef NEKO_CONSUMER_HAS_PROTOCOL
#include <nekoproto/proto/proto_base.hpp>
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
    return 0;
}
