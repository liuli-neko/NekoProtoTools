#include <gtest/gtest.h>

// #define protected public // for test private member
// #define private   public // for test private member

#include "../core/binary_serializer.hpp"
#include "../core/dump_to_string.hpp"
#include "../core/json_serializer.hpp"
#include "../core/json_serializer_binary.hpp"
#include "../core/json_serializer_container.hpp"
#include "../core/json_serializer_enum.hpp"
#include "../core/json_serializer_struct.hpp"
#include "../core/proto_base.hpp"
#include "../core/serializer_base.hpp"

NEKO_USE_NAMESPACE

struct TestStruct {
    int a                        = 1;
    double b                     = 12.12;
    int c                        = 3;
    double d                     = 3.14;
    std::list<int> e             = {1, 2, 3, 4, 5};
    std::map<std::string, int> f = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::array<int, 5> g         = {1, 2, 3, 4, 5};
    NEKO_SERIALIZER(a, b, c, d, e, f, g)
    NEKO_DECLARE_PROTOCOL(TestStruct, JsonSerializer)
};

TEST(PrivateInvoidParams, RefTest) {
    TestStruct ts;
    auto p = ts.makeProto();
    p.setField("a", 4);
    p.setField("b", 5.0);
    p.setField("c", 6);
    ReflectionSerializer refs;
    refs.start();
    bool ret       = ts.deserialize(refs);
    auto refObject = refs.getObject();
    const int a    = 1;
    refObject->bindField("test_a", &a);
    EXPECT_FALSE(refObject->setField("test_a", 4));
    EXPECT_EQ(refObject->getField<const int>("test_a", 123), 1);
    EXPECT_EQ(refObject->getField("test_a", 123), 1);
    EXPECT_THROW(refObject->bindField<double>("test_b", nullptr), std::invalid_argument);
    int* b = nullptr;
    refObject->bindField("test_a", &b);
    int c = 4, d = 2;
    EXPECT_TRUE(refObject->setField<int*>("test_a", &c));
    EXPECT_EQ(*refObject->getField<int*>("test_a", &d), 4);

    TestStruct ts2{};
    TestStruct::ProtoType::Deserialize(p.toData(), ts2);
    EXPECT_EQ(ts2.a, 4);
    EXPECT_EQ(ts2.b, 5.0);
    EXPECT_EQ(ts2.c, 6);
    EXPECT_STREQ(std::string(p.protoName().data(), p.protoName().size()).c_str(), "TestStruct");
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::debug);
    return RUN_ALL_TESTS();
}