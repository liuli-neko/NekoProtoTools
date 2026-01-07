#include <cstddef>
#include <gtest/gtest.h>
#include <string>

#include "nekoproto/global/reflect.hpp"
#include "nekoproto/jsonrpc/jsonrpc.hpp"
#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"
#include "nekoproto/serialization/reflection.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

NEKO_USE_NAMESPACE
struct TestStruct {
    int a;
    std::string b;
    std::optional<std::vector<int>> c;
    std::optional<std::string> d;

    NEKO_SERIALIZER(a, b, c, d)
};

struct TestStructWithLowerCase {
    int aMember;
    std::string bMember;
    std::optional<std::vector<int>> cMember;
    std::optional<std::string> dMember;
};

struct TestStructWithSub {
    int aMember;
    std::string bMember;
    std::optional<std::vector<int>> cMember;
    std::optional<std::string> dMember;

    NEKO_SERIALIZER(aMember, cMember, dMember)
};

void test_func(int aMember) {
    std::cout << "test_func : " << aMember << std::endl;
    (void)aMember;
};

template <auto Ptr, typename... Ts>
    requires std::is_invocable_v<decltype(Ptr), Ts...>
void call(Ts&&... args) {
    auto func = Ptr;
    std::invoke(func, std::forward<Ts>(args)...);
}

struct TestStructWithFunc {
    int aMember;
    std::string bMember;
    std::optional<std::vector<int>> cMember;
    std::optional<std::string> dMember;

    static void staticTestFunc(int aMember) { (void)aMember; }
    void testFunc(int aMember) { this->aMember = aMember; }

    void testFuncNoArgs() {}
    int constTestFunc(int val) const { return aMember + val; }
};

void test_func_with_struct(TestStructWithFunc& aMember) { (void)aMember; }
void free_func_void() {}
int free_func_one_arg(int ii) { return ii; }
void free_func_multi_args(int /*unused*/, const std::string& /*unused*/) {}
void free_func_auto_expand(TestStruct ss) { (void)ss; }
void free_func_tuple_arg(std::tuple<int, bool> tt) { (void)tt; }

struct MyFunctor {
    bool operator()(int /*unused*/, const std::string& /*unused*/) { return true; }
};

TEST(RefectNames, Test) { // NOLINT
    std::cout << detail::mangled_name<detail::NekoReflector::nekoStaticFunc>() << std::endl;
    std::cout << detail::mangled_name<test_func>() << std::endl;
    ASSERT_EQ(detail::class_nameof<TestStruct>, "TestStruct");
    auto names = Reflect<TestStruct>::names();
    ASSERT_EQ(names.size(), 4);
    EXPECT_EQ(names[0], "a");
    EXPECT_EQ(names[1], "b");
    EXPECT_EQ(names[2], "c");
    EXPECT_EQ(names[3], "d");

    ASSERT_EQ(detail::class_nameof<TestStructWithLowerCase>, "TestStructWithLowerCase");
    names = Reflect<TestStructWithLowerCase>::names();
    ASSERT_EQ(names.size(), 4);
    EXPECT_EQ(names[0], "aMember");
    EXPECT_EQ(names[1], "bMember");
    EXPECT_EQ(names[2], "cMember");
    EXPECT_EQ(names[3], "dMember");

    ASSERT_EQ(detail::class_nameof<TestStructWithSub>, "TestStructWithSub");
    auto names1 = Reflect<TestStructWithSub>::names();
    ASSERT_EQ(names1.size(), 3);
    EXPECT_EQ(names1[0], "aMember");
    EXPECT_EQ(names1[1], "cMember");
    EXPECT_EQ(names1[2], "dMember");
    std::string name = std::string(detail::func_nameof<test_func>);
    EXPECT_STREQ(name.c_str(), "test_func");
    call<test_func>(1);
    name = std::string(detail::func_nameof<test_func_with_struct>);
    EXPECT_STREQ(name.c_str(), "test_func_with_struct");
    TestStructWithFunc aa;
    call<test_func_with_struct>(aa);
    name = std::string(detail::func_nameof<TestStructWithFunc::staticTestFunc>);
    EXPECT_STREQ(name.c_str(), "staticTestFunc");
    call<TestStructWithFunc::staticTestFunc>(1);
    std::cout << detail::mangled_name<&TestStructWithFunc::testFunc>() << std::endl;
}

TEST(RpcMethodTraitsTest, FunctionType) {
    // Case 1: 无参，void 返回
    {
        using Traits = detail::RpcMethodTraits<void()>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<std::nullptr_t>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<>>);
        static_assert(Traits::NumParams == 0);
        static_assert(Traits::ParamsSize == 0);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 2: 单参数不可序列化，void 返回
    {
        using Traits = detail::RpcMethodTraits<void(std::string)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<std::nullptr_t>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<std::string>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<std::string>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 1);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 3: 单参数可序列化，void 返回
    {
        using Traits = detail::RpcMethodTraits<void(TestStruct)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<std::nullptr_t>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, TestStruct>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == Reflect<TestStruct>::value_count);
        static_assert(Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 4: 多参数，void 返回
    {
        using Traits = detail::RpcMethodTraits<void(TestStruct, std::string)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<std::nullptr_t>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct, std::string>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<TestStruct, std::string>>);
        static_assert(Traits::NumParams == 2);
        static_assert(Traits::ParamsSize == 2);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }

    // Case 5: 无参，可序列化返回
    {
        using Traits = detail::RpcMethodTraits<TestStruct()>;
        static_assert(std::is_same_v<Traits::RawReturnType, TestStruct>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<TestStruct>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<>>);
        static_assert(Traits::NumParams == 0);
        static_assert(Traits::ParamsSize == 0);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 6: 单参数不可序列化，可序列化返回
    {
        using Traits = detail::RpcMethodTraits<TestStruct(std::string)>;
        static_assert(std::is_same_v<Traits::RawReturnType, TestStruct>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<TestStruct>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<std::string>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<std::string>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 1);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 7: 单参数可序列化，可序列化返回
    {
        using Traits = detail::RpcMethodTraits<TestStruct(TestStruct)>;
        static_assert(std::is_same_v<Traits::RawReturnType, TestStruct>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<TestStruct>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, TestStruct>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == Reflect<TestStruct>::value_count);
        static_assert(Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 8: 多参数，可序列化返回
    {
        using Traits = detail::RpcMethodTraits<TestStruct(TestStruct, std::string)>;
        static_assert(std::is_same_v<Traits::RawReturnType, TestStruct>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<TestStruct>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct, std::string>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<TestStruct, std::string>>);
        static_assert(Traits::NumParams == 2);
        static_assert(Traits::ParamsSize == 2);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }

    // Case 9: 无参，不可序列化返回
    {
        using Traits = detail::RpcMethodTraits<int()>;
        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<int>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<>>);
        static_assert(Traits::NumParams == 0);
        static_assert(Traits::ParamsSize == 0);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 10: 单参数不可序列化，不可序列化返回
    {
        using Traits = detail::RpcMethodTraits<int(std::string)>;
        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<int>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<std::string>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<std::string>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 1);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 11: 单参数可序列化，不可序列化返回
    {
        using Traits = detail::RpcMethodTraits<int(TestStruct)>;
        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<int>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, TestStruct>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == Reflect<TestStruct>::value_count);
        static_assert(Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 12: 多参数，不可序列化返回
    {
        using Traits = detail::RpcMethodTraits<int(TestStruct, std::string)>;
        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<int>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct, std::string>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<TestStruct, std::string>>);
        static_assert(Traits::NumParams == 2);
        static_assert(Traits::ParamsSize == 2);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 13: 元组参数，可序列化返回
    {
        using Traits = detail::RpcMethodTraits<int(std::tuple<int, std::string>)>;
        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::ReturnType, std::optional<int>>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<std::tuple<int, std::string>>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<int, std::string>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 2);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(Traits::IsTopTuple);
    }
}

TEST(RpcMethodTraitsTest, FreeFunctions) {
    // Case 1: 无参，void 返回
    {
        using Traits = detail::RpcMethodTraits<decltype(&free_func_void)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<>>);
        static_assert(Traits::NumParams == 0);
        static_assert(Traits::ParamsSize == 0);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 2: 单参数，非 void 返回
    {
        using Traits = detail::RpcMethodTraits<decltype(&free_func_one_arg)>;
        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<int>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<int>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 1);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 3: 多参数
    {
        using Traits = detail::RpcMethodTraits<decltype(&free_func_multi_args)>;
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<int, const std::string&>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<int, std::string>>); // 检查是否移除了 const&
        static_assert(Traits::NumParams == 2);
        static_assert(Traits::ParamsSize == 2);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 4: 可自动展开的结构体参数
    {
        using Traits = detail::RpcMethodTraits<decltype(&free_func_auto_expand)>;
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, TestStruct>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == Reflect<TestStruct>::size()); // 应等于结构体成员数
        static_assert(Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 5: 单个 tuple 参数
    {
        using Traits = detail::RpcMethodTraits<decltype(&free_func_tuple_arg)>;
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<std::tuple<int, bool>>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<int, bool>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 2);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(Traits::IsTopTuple);
    }
}

TEST(RpcMethodTraitsTest, MemberFunctions) {
    // Case 1: 静态成员函数 (用法同自由函数)
    {
        using Traits = detail::RpcMethodTraits<decltype(&TestStructWithFunc::staticTestFunc)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<int>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 1);
        static_assert(!Traits::IsAutomaticExpansionAble);
        static_assert(!Traits::IsTopTuple);
    }
    // Case 2: 普通成员函数
    {
        using Traits = detail::RpcMethodTraits<decltype(&TestStructWithFunc::testFunc)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        // 注意：成员函数的第一个参数(this指针)不应被萃取出来
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<int>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 1);
    }
    // Case 3: 无参成员函数
    {
        using Traits = detail::RpcMethodTraits<decltype(&TestStructWithFunc::testFuncNoArgs)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<>>);
        static_assert(Traits::NumParams == 0);
        static_assert(Traits::ParamsSize == 0);
    }
    // Case 4: const 成员函数
    {
        using Traits = detail::RpcMethodTraits<decltype(&TestStructWithFunc::constTestFunc)>;
        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<int>>);
        static_assert(Traits::NumParams == 1);
        static_assert(Traits::ParamsSize == 1);
    }
}

TEST(RpcMethodTraitsTest, LambdasAndFunctors) {
    // Case 1: 无捕获 Lambda
    {
        auto ll      = [](const int& ii) -> double { return ii; };
        using Traits = detail::RpcMethodTraits<decltype(ll)>;
        static_assert(std::is_same_v<Traits::RawReturnType, double>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<const int&>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<int>>);
        static_assert(Traits::NumParams == 1);
    }
    // Case 2: 有捕获 Lambda
    {
        int xx  = 10;
        auto ll = [xx](std::string ss) {
            (void)ss;
            (void)xx;
        };
        using Traits = detail::RpcMethodTraits<decltype(ll)>;
        static_assert(std::is_same_v<Traits::RawReturnType, void>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<std::string>>);
        static_assert(Traits::NumParams == 1);
    }
    // Case 3: 函数对象 (Functor)
    {
        using Traits = detail::RpcMethodTraits<MyFunctor>;
        static_assert(std::is_same_v<Traits::RawReturnType, bool>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<int, const std::string&>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<int, std::string>>);
        static_assert(Traits::NumParams == 2);
    }
}

TEST(RpcMethodTraitsTest, StdFunction) {
    // std::function 的萃取结果应与其包装的签名一致
    {
        using FuncType = std::function<int(TestStruct, bool)>;
        using Traits   = detail::RpcMethodTraits<FuncType>;

        static_assert(std::is_same_v<Traits::RawReturnType, int>);
        static_assert(std::is_same_v<Traits::RawParamsType, std::tuple<TestStruct, bool>>);
        static_assert(std::is_same_v<Traits::ParamsTupleType, std::tuple<TestStruct, bool>>);
        static_assert(Traits::NumParams == 2);
        static_assert(Traits::ParamsSize == 2);
        static_assert(!Traits::IsAutomaticExpansionAble); // 因为有多个参数，所以不能自动展开
        static_assert(!Traits::IsTopTuple);
    }
}
#include "../common/common_main.cpp.in" // IWYU pragma: export
