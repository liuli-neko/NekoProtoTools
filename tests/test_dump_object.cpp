#include <iostream>
#include <gtest/gtest.h>

#include "../proto/cc_dumpable_object.hpp"
#include "../proto/cc_proto_json_serializer.hpp"
#include "../proto/cc_serializer_base.hpp"

using namespace CS_PROTO_NAMESPACE;

class T1 : public CS_PROTO_NAMESPACE::DumpableObject<T1, CS_PROTO_NAMESPACE::JsonSerializer> {

public:
    std::string objectName() const override {
        return "T1";
    }

    int a = 1;
    std::string b = " hesd";

    CS_SERIALIZER(a, b)
};

TEST(DumpObject, Test) {
    T1 t1;
    auto t = t1.dumpToString();
    std::cout << t << std::endl;
    // auto d = IDumpableObject::create("T1");
    // ASSERT_NE(d, nullptr);
    // d->loadFromString(t);
}


int main(int argc, char **argv) {
    std::cout << "CS_CPP_PLUS: " << CS_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}