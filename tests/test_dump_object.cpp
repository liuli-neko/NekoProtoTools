#include <iostream>
#include <gtest/gtest.h>

#include "../proto/cc_dumpable_object.hpp"
#include "../proto/cc_proto_json_serializer.hpp"
#include "../proto/cc_serializer_base.hpp"

using namespace CS_PROTO_NAMESPACE;

class TestT : public CS_PROTO_NAMESPACE::DumpableObject<TestT, CS_PROTO_NAMESPACE::JsonSerializer<>> {

public:
    int a = 1;
    std::string b = " hesd";

    CS_SERIALIZER(a, b)
};

class TestA : public CS_PROTO_NAMESPACE::DumpableObject<TestA, CS_PROTO_NAMESPACE::JsonSerializer<>> {

public:
    int a = 1;
    double b = 1.1;
    std::string c = " hesd";
    TestT d;

    CS_SERIALIZER(a, b, c, d)
};

TEST(DumpObject, Test) {
    TestA t1;
    t1.a = 234324;
    t1.b = 1.234;
    t1.c = "asdasd";
    t1.d.a = 566;
    t1.d.b = "uuuuuos";
    
    auto t = t1.dumpToString();
    IDumpableObject::dumpToFile("test.json", &t1);
    IDumpableObject *d = nullptr;
    IDumpableObject::loadFromFile("test.json", &d);
    ASSERT_NE(d, nullptr);
    
    TestA* a = dynamic_cast<TestA *>(d);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->a, 234324);
    EXPECT_EQ(a->b, 1.234);
    EXPECT_STREQ(a->c.c_str(), "asdasd");
    EXPECT_EQ(a->d.a, 566);
    EXPECT_STREQ(a->d.b.c_str(), "uuuuuos");

    delete d;
}


int main(int argc, char **argv) {
    std::cout << "CS_CPP_PLUS: " << CS_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}