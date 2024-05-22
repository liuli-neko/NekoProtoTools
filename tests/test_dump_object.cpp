#include <iostream>
#include <gtest/gtest.h>

#include "../proto/cc_dumpable_object.hpp"
#include "../proto/cc_proto_json_serializer.hpp"
#include "../proto/cc_serializer_base.hpp"

using namespace CS_PROTO_NAMESPACE;

class TestT : public CS_PROTO_NAMESPACE::DumpableObject<TestT, CS_PROTO_NAMESPACE::JsonSerializer> {

public:
    int a = 1;
    std::string b = " hesd";
    IDumpableObject *c = nullptr;

    CS_SERIALIZER(a, b, c)
};

TEST(DumpObject, Test) {
    TestT t1;
    t1.a = 234324;
    t1.b = "asdasd";
    t1.c = new TestT();
    static_cast<TestT*>(t1.c)->a = 123;
    static_cast<TestT*>(t1.c)->b = "asdasd";
    
    auto t = t1.dumpToString();
    IDumpableObject::dumpToFile("test.json", &t1);
    IDumpableObject *d = nullptr;
    IDumpableObject::loadFromFile("test.json", &d);
    ASSERT_NE(d, nullptr);
    
    TestT* a = dynamic_cast<TestT *>(d);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->a, t1.a);
    EXPECT_STREQ(a->b.c_str(), t1.b.c_str());

    delete d;
}


int main(int argc, char **argv) {
    std::cout << "CS_CPP_PLUS: " << CS_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}