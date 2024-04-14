#include <iostream>
#include "../include/cc_proto_base.hpp"
#include "../include/cc_proto_json_serializer.hpp"
#include "../include/cc_proto_json_serializer_enum.hpp"
#include "../include/cc_proto_json_serializer_struct.hpp"
#include "../include/cc_proto_json_serializer_contrain.hpp"
#include "../include/cc_proto_json_serializer_binary.hpp"

using namespace rapidjson;

CS_PROTO_USE_NAMESPACE

CS_PROTO_SET_TYPE_START(1)

enum TEnum {
    TEnum_A = 1,
    TEnum_B = 2,
    TEnum_C = 3
};

struct StructA {
    int a;
    std::string b;
    bool c;
    double d;
    std::list<int> e;
    std::map<std::string, int> f;
    std::array<int, 5> g;
    TEnum h;
};

#if (defined(_MSC_VER) && _MSVC_LANG <= 201402L) || ( defined(__GNUC__) && cplusplus < 201703L)
CS_PROTO_BEGIN_NAMESPACE
template <>
struct JsonConvert<StructA, void> {
  static void toJsonValue(JsonWriter &writer, const StructA &value) {
    writer.StartArray();
    JsonConvert<int>::toJsonValue(writer, value.a);
    JsonConvert<std::string>::toJsonValue(writer, value.b);
    JsonConvert<bool>::toJsonValue(writer, value.c);
    JsonConvert<double>::toJsonValue(writer, value.d);
    JsonConvert<std::list<int>>::toJsonValue(writer, value.e);
    JsonConvert<std::map<std::string, int>>::toJsonValue(writer, value.f);
    JsonConvert<std::array<int, 5>>::toJsonValue(writer, value.g);
    JsonConvert<TEnum>::toJsonValue(writer, value.h);
    writer.EndArray();
  }
  static bool fromJsonValue(StructA *result, const Value &value) {
    if (result == nullptr || !value.IsArray()) {
        return false;
    }
    JsonConvert<int>::fromJsonValue(&result->a, value[0]);
    JsonConvert<std::string>::fromJsonValue(&result->b, value[1]);
    JsonConvert<bool>::fromJsonValue(&result->c, value[2]);
    JsonConvert<double>::fromJsonValue(&result->d, value[3]);
    JsonConvert<std::list<int>>::fromJsonValue(&result->e, value[4]);
    JsonConvert<std::map<std::string, int>>::fromJsonValue(&result->f, value[5]);
    JsonConvert<std::array<int, 5>>::fromJsonValue(&result->g, value[6]);
    JsonConvert<TEnum>::fromJsonValue(&result->h, value[7]);
    return true;
  }
};

CS_PROTO_END_NAMESPACE
#endif

struct TestP : public ProtoBase<TestP> {
    int a = 1;
    std::string b = "hello";
    bool c = true;
    double d = 3.14;
    std::list<int> e = {1, 2, 3, 4, 5};
    std::map<std::string, int> f = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::array<int, 5> g = {1, 2, 3, 4, 5};
    TEnum h = TEnum_A;
    StructA i = {1, "hello", true, 3.14, {1, 2, 3, 4, 5}, {{"a", 1}, {"b", 2}, {"c", 3}}, {1, 2, 3, 4, 5}, TEnum_A};
    std::tuple<int, std::string> j = {1, "hello"};

    CS_DECLARE_PROTO_FIELD(a)
    CS_DECLARE_PROTO_FIELD(b)
    CS_DECLARE_PROTO_FIELD(c)
    CS_DECLARE_PROTO_FIELD(d)
    CS_DECLARE_PROTO_FIELD(e)
    CS_DECLARE_PROTO_FIELD(f)
    CS_DECLARE_PROTO_FIELD(g)
    CS_DECLARE_PROTO_FIELD(h)
    CS_DECLARE_PROTO_FIELD(i)
    CS_DECLARE_PROTO_FIELD(j)
};
CS_DECLARE_PROTO(TestP, "TestP");

struct MyProto : public ProtoBase<MyProto> {
    int my_int = 42;
    std::string my_string = "Hello, world!";
    TestP testp = TestP();
    std::vector<int> my_ints = {1, 2, 3, 4, 5};
    CS_DECLARE_PROTO_FIELD(my_int)
    CS_DECLARE_PROTO_FIELD(my_string) 
    CS_DECLARE_PROTO_FIELD(testp)
    CS_DECLARE_PROTO_FIELD(my_ints)
};
CS_DECLARE_PROTO(MyProto, "MyProto");

struct MyProto1 : public ProtoBase<MyProto1> {
    int intd = 42;
    int my_int = 1;
    std::string stringd = "Hello, world!";
    std::string my_string = "Hello";
    std::vector<TestP> testps = {TestP(), TestP()};
    CS_DECLARE_PROTO_FIELD(intd)
    CS_DECLARE_PROTO_FIELD(stringd)
    CS_DECLARE_PROTO_FIELD(my_int)
    CS_DECLARE_PROTO_FIELD(my_string)
    CS_DECLARE_PROTO_FIELD(testps)
};
CS_DECLARE_PROTO(MyProto1, "MyProto1");

int main(int argc, char **argv) {
    ProtoFactory proto(1, 0, 0);
    std::shared_ptr<MyProto> my_proto(proto.create<MyProto>());
    my_proto->my_int = 13423;
    my_proto->my_string = "Hello, proto!";
    TestP p;
    p.a = 523523;
    p.b = "Test for p";
    p.j = std::make_tuple(114514, "test tuple");
    my_proto->testp = p;
    my_proto->my_ints = {1, 2, 3, 4, 5};

    std::cout << "my_int: " << my_proto->my_int << std::endl;
    std::cout << "my_string: " << my_proto->my_string << std::endl;
    std::cout << "testp.a: " << my_proto->testp.a << std::endl;

    std::vector<char> data = my_proto->serialize();
    std::cout << std::string(data.data(), data.size()) << std::endl;

    std::shared_ptr<IProto> proto_copy(proto.create(proto.proto_type<MyProto1>()));
    std::shared_ptr<IProto> proto_c(proto.create(my_proto->type())); 
    proto_c->deserialize(data);
    proto_copy->deserialize(data);
    std::cout << "proto_copy type: " << proto_copy->type() << std::endl;

    std::cout << "my_int: " << dynamic_cast<MyProto1*>(proto_copy.get())->my_int << std::endl;
    std::cout << "my_string: " << dynamic_cast<MyProto1*>(proto_copy.get())->my_string << std::endl;
    std::cout << "proto_c: " << dynamic_cast<MyProto*>(proto_c.get())->testp.a << std::endl;
    std::cout << "proto_c: " << dynamic_cast<MyProto*>(proto_c.get())->testp.b << std::endl;

    auto d = proto_copy->serialize();
    std::cout << std::string(d.data(), d.size()) << std::endl;

    auto d1 = proto_c->serialize();
    std::cout << std::string(d1.data(), d1.size()) << std::endl;

    auto bteststr = "this is a test to base64";
    std::vector<char> datadd(bteststr, bteststr + 27);
    auto base64s = Base64Covert::Encode(datadd);
    std::cout << std::string(base64s.data(), base64s.size()) << std::endl;
    auto rec = Base64Covert::Decode(base64s);
    std::cout << std::string(rec.data(), rec.size()) << std::endl;

    return 0;
}