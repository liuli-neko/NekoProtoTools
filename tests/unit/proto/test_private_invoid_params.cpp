#include <gtest/gtest.h>

// #define protected public // for test private member
// #define private   public // for test private member

#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/serialization/binary_serializer.hpp" // IWYU pragma: export
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/to_string.hpp"           // IWYU pragma: export
#include "nekoproto/serialization/types/array.hpp"         // IWYU pragma: export
#include "nekoproto/serialization/types/binary_data.hpp"   // IWYU pragma: export
#include "nekoproto/serialization/types/enum.hpp"          // IWYU pragma: export
#include "nekoproto/serialization/types/list.hpp"          // IWYU pragma: export
#include "nekoproto/serialization/types/map.hpp"           // IWYU pragma: export
#include "nekoproto/serialization/types/set.hpp"           // IWYU pragma: export
#include "nekoproto/serialization/types/struct_unwrap.hpp" // IWYU pragma: export
#include "nekoproto/serialization/types/tuple.hpp"         // IWYU pragma: export
#include "nekoproto/serialization/types/variant.hpp"       // IWYU pragma: export
#include "nekoproto/serialization/types/vector.hpp"        // IWYU pragma: export

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
    auto proto = ts.makeProto();
    proto.setField("a", 4);
    proto.setField("b", 5.0);
    proto.setField("c", 6);
    ReflectionSerializer refs = ReflectionSerializer::reflection(ts);
    EXPECT_TRUE(refs.getObject() != nullptr);
    auto* refObject = refs.getObject();
    const int num   = 1;
    refObject->bindField("test_a", &num);
    EXPECT_FALSE(refObject->setField("test_a", 4));
    EXPECT_EQ(refObject->getField<const int>("test_a", 123), 1);
    EXPECT_EQ(refObject->getField("test_a", 123), 1);
    EXPECT_TRUE(refObject->bindField<double>("test_b", nullptr) == nullptr);
    int* numptr = nullptr;
    refObject->bindField("test_a", &numptr);
    int num2 = 4;
    int num3 = 2;
    EXPECT_TRUE(refObject->setField<int*>("test_a", &num2));
    EXPECT_EQ(*refObject->getField<int*>("test_a", &num3), 4);

    TestStruct ts2{};
    auto pdata = proto.toData();
    pdata.push_back('\0');
    NEKO_LOG_DEBUG("unit test", "{}", pdata.data());
    TestStruct::ProtoType::Deserialize(pdata.data(), pdata.size(), ts2);
    EXPECT_EQ(ts2.a, 4);
    EXPECT_EQ(ts2.b, 5.0);
    EXPECT_EQ(ts2.c, 6);
    EXPECT_STREQ(std::string(proto.protoName().data(), proto.protoName().size()).c_str(), "TestStruct");
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);

    TestStruct ts;
    auto proto = ts.makeProto();
    proto.setField("a", 4);
    proto.setField("b", 5.0);
    proto.setField("c", 6);
    ReflectionSerializer refs = ReflectionSerializer::reflection(ts);
    EXPECT_TRUE(refs.getObject() != nullptr);
    auto* refObject = refs.getObject();
    const int num   = 1;
    refObject->bindField("test_a", &num);
    EXPECT_FALSE(refObject->setField("test_a", 4));
    EXPECT_EQ(refObject->getField<const int>("test_a", 123), 1);
    EXPECT_EQ(refObject->getField("test_a", 123), 1);
    EXPECT_TRUE(refObject->bindField<double>("test_b", nullptr) == nullptr);
    int* numptr = nullptr;
    refObject->bindField("test_a", &numptr);
    int num2 = 4;
    int num3 = 2;
    EXPECT_TRUE(refObject->setField<int*>("test_a", &num2));
    EXPECT_EQ(*refObject->getField<int*>("test_a", &num3), 4);

    TestStruct ts2{};
    auto pdata = proto.toData();
    pdata.push_back('\0');
    NEKO_LOG_DEBUG("unit test", "{}", pdata.data());
    TestStruct::ProtoType::Deserialize(pdata.data(), pdata.size(), ts2);
    EXPECT_EQ(ts2.a, 4);
    EXPECT_EQ(ts2.b, 5.0);
    EXPECT_EQ(ts2.c, 6);
    EXPECT_STREQ(std::string(proto.protoName().data(), proto.protoName().size()).c_str(), "TestStruct");

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}