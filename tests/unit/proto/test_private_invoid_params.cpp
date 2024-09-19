#include <gtest/gtest.h>

// #define protected public // for test private member
// #define private   public // for test private member

#include "proto/binary_serializer.hpp"
#include "proto/json_serializer.hpp"
#include "proto/proto_base.hpp"
#include "proto/serializer_base.hpp"
#include "proto/to_string.hpp"
#include "proto/types/array.hpp"
#include "proto/types/binary_data.hpp"
#include "proto/types/enum.hpp"
#include "proto/types/list.hpp"
#include "proto/types/map.hpp"
#include "proto/types/set.hpp"
#include "proto/types/struct_unwrap.hpp"
#include "proto/types/tuple.hpp"
#include "proto/types/variant.hpp"
#include "proto/types/vector.hpp"

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
    detail::ReflectionSerializer refs;
    bool ret       = ts.serialize(refs);
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
    return RUN_ALL_TESTS();
}