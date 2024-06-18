#include <gtest/gtest.h>

#include <iostream>

#include "../core/dumpable_object.hpp"
#include "../core/proto_json_serializer.hpp"
#include "../core/serializer_base.hpp"

using namespace NEKO_NAMESPACE;

class TestT : public DumpableObject<TestT, JsonSerializer> {
public:
    int a = 1;
    std::string b = " hesd";

    NEKO_SERIALIZER(a, b)
};

class TestA : public DumpableObject<TestA, JsonSerializer> {
public:
    int a = 1;
    double b = 1.1;
    std::string c = " hesd";
    TestT d;

    NEKO_SERIALIZER(a, b, c, d)
};

TEST(DumpObject, Test) {
    TestA t1;
    t1.a = 234324;
    t1.b = 1.234;
    t1.c = "asdasd";
    t1.d.a = 566;
    t1.d.b = "uuuuuos";

    auto t = t1.dumpToString();
    t1.dumpToFile("test.json");
    IDumpableObject* d = IDumpableObject::create("TestA");
    d->loadFromFile("test.json");
    ASSERT_NE(d, nullptr);

    TestA* a = dynamic_cast<TestA*>(d);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->a, 234324);
    EXPECT_EQ(a->b, 1.234);
    EXPECT_STREQ(a->c.c_str(), "asdasd");
    EXPECT_EQ(a->d.a, 566);
    EXPECT_STREQ(a->d.b.c_str(), "uuuuuos");

    delete d;
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}