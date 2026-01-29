#include "nekoproto/global/global.hpp"
#include "nekoproto/global/xor_list.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <array>

NEKO_USE_NAMESPACE

//--- 使用示例 ---

// 1. 定义你的数据结构，公有继承自 xor_node
struct MyData : public xor_node_base {
    // 可以添加额外的数据或方法
    std::string name;
    int value;

    MyData(std::string n, int v) : name(std::move(n)), value(v) {}
};


// 帮助函数：验证链表内容与一个 vector 是否一致
template <typename T, typename U>
void VerifyListContent(const T& list, const std::vector<U>& expected_values, const std::function<U(const typename T::value_type&)>& accessor) {
    ASSERT_EQ(list.size(), expected_values.size());
    if (list.empty()) {
        ASSERT_TRUE(expected_values.empty());
        return;
    }

    size_t i = 0;
    for (const auto& item : list) {
        ASSERT_EQ(accessor(item), expected_values[i++]);
    }
}


//================================================================================
// 您的原始测试（稍作清理和增强）
//================================================================================

TEST(XorList, IntrusiveListBasic) {
    xor_list<MyData> list;

    MyData node1("ten", 10);
    MyData node2("twenty", 20);
    MyData node3("thirty", 30);

    // 测试 push_back 和 push_front
    list.push_back(&node2);    // list: {20}
    list.push_front(&node1);   // list: {10, 20}
    list.push_back(&node3);    // list: {10, 20, 30}

    EXPECT_EQ(list.size(), 3);
    EXPECT_FALSE(list.empty());

    // 验证正向迭代
    std::vector<std::string> expected_names = {"ten", "twenty", "thirty"};
    VerifyListContent<xor_list<MyData>, std::string>(list, expected_names, [](const MyData& item) { return item.name; });

    // 验证反向迭代
    auto it = list.end();
    std::vector<std::string> expected_names_rev = {"thirty", "twenty", "ten"};
    size_t i = 0;
    if (!list.empty()) {
        --it;
        while (true) {
            EXPECT_EQ(it->name, expected_names_rev[i++]);
            if (it == list.begin()) break;
            --it;
        }
    }
}

TEST(XorList, IntrusiveErase) {
    xor_list<MyData> list;
    MyData node1("ten", 10);
    MyData node2("twenty", 20);
    MyData node3("thirty", 30);
    list.push_back(&node1);
    list.push_back(&node2);
    list.push_back(&node3);

    // 移除中间的节点 node2
    auto it_to_erase = ++list.begin(); // 指向 node2
    EXPECT_EQ(it_to_erase->name, "twenty");
    auto next_it = list.erase(it_to_erase); // next_it 应指向 node3
    
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(next_it->name, "thirty");
    VerifyListContent<xor_list<MyData>, std::string>(list, {"ten", "thirty"}, [](const MyData& item) { return item.name; });

    // 节点的 value 可以被修改
    node1.name = "one hundred";
    VerifyListContent<xor_list<MyData>, std::string>(list, {"one hundred", "thirty"}, [](const MyData& item) { return item.name; });
}


TEST(XorList, NonIntrusiveBasic) {
    xor_list<int> list;

    list.push_back(10);
    list.push_front(20);
    list.push_back(30);

    EXPECT_EQ(list.size(), 3);
    VerifyListContent<xor_list<int>, int>(list, {20, 10, 30}, [](int item){ return item; });
    
    EXPECT_EQ(list.front(), 20);
    EXPECT_EQ(list.back(), 30);
    
    auto front = list.pop_front();
    EXPECT_EQ(front, 20);
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list.front(), 10);
    EXPECT_EQ(list.back(), 30);

    auto back = list.pop_back();
    EXPECT_EQ(back, 30);
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(list.front(), 10);
    EXPECT_EQ(list.back(), 10);

    list.clear();
    EXPECT_EQ(list.size(), 0);
    EXPECT_TRUE(list.empty());
}


//================================================================================
// 新增的全面性测试
//================================================================================

TEST(XorList, EdgeCases) {
    // 1. 对空链表的操作
    xor_list<int> list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0);
    EXPECT_EQ(list.begin(), list.end());

    // 对空链表调用 pop/front/back 通常是未定义行为或会触发断言
    // 如果您的实现会抛出异常，可以使用 EXPECT_THROW
    // 如果会触发断言，可以使用 EXPECT_DEATH (在Debug模式下)
    // EXPECT_DEATH(list.pop_front(), ".*");
    // EXPECT_DEATH(list.front(), ".*");

    // 2. 单元素链表
    list.push_back(100);
    EXPECT_EQ(list.size(), 1);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.front(), 100);
    EXPECT_EQ(list.back(), 100);

    // 移除唯一的元素
    int val = list.pop_back();
    EXPECT_EQ(val, 100);
    EXPECT_TRUE(list.empty());

    // 3. 再次添加并用 erase 删除
    list.push_front(200);
    auto it = list.erase(list.begin());
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(it, list.end()); // erase 最后一个元素后，迭代器应等于 end()

    // 4. 清空一个空链表
    list.clear();
    EXPECT_TRUE(list.empty());
}

TEST(XorList, EraseComprehensive) {
    xor_list<std::string> list;
    list.push_back("A");
    list.push_back("B");
    list.push_back("C");
    list.push_back("D");

    // 1. 删除头节点 ("A")
    auto it = list.erase(list.begin());
    EXPECT_STREQ(it->c_str(), "B"); // 返回的迭代器应指向 "B"
    VerifyListContent<xor_list<std::string>, std::string>(list, {"B", "C", "D"}, [](const std::string& s){ return s; });

    // 2. 删除尾节点 ("D")
    auto tail_it = --list.end();
    EXPECT_STREQ(tail_it->c_str(), "D");
    it = list.erase(tail_it);
    EXPECT_EQ(it, list.end()); // 删除尾节点，返回的迭代器应是 end()
    VerifyListContent<xor_list<std::string>, std::string>(list, {"B", "C"}, [](const std::string& s){ return s; });

    // 3. 重新插入并清空
    list.push_front("Z"); // list: {"Z", "B", "C"}
    list.clear();
    EXPECT_TRUE(list.empty());
}

TEST(XorList, IteratorValidity) {
    xor_list<int> list;
    list.push_back(10);
    list.push_back(20);
    list.push_back(30);

    // 获取指向中间元素 "20" 的迭代器
    auto it_to_20 = ++list.begin();
    EXPECT_EQ(*it_to_20, 20);

    // 在链表头尾插入元素
    list.push_front(5);
    list.push_back(40);

    // 验证旧的迭代器依然有效，这是链式数据结构的重要特性
    EXPECT_EQ(*it_to_20, 20);

    // 验证链表现状
    VerifyListContent<xor_list<int>, int>(list, {5, 10, 20, 30, 40}, [](int item){ return item; });
}

TEST(XorList, ConstCorrectness) {
    xor_list<int> list;
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);

    // 将链表转换为 const 引用
    const xor_list<int>& const_list = list;

    EXPECT_EQ(const_list.size(), 3);
    EXPECT_FALSE(const_list.empty());
    EXPECT_EQ(const_list.front(), 1);
    EXPECT_EQ(const_list.back(), 3);
    
    // 使用 const_iterator 遍历
    std::vector<int> expected_values = {1, 2, 3};
    size_t i = 0;
    for (const auto& item : const_list) {
        EXPECT_EQ(item, expected_values[i++]);
    }
}

TEST(XorList, NonIntrusiveComplexType) {
    xor_list<std::string> list;
    
    // 测试 push_back 右值
    list.push_back("Hello");
    
    // 测试 push_front 右值
    list.push_front("World"); // list: {"World", "Hello"}

    // 测试 push_back 左值
    std::string s1 = "Cpp";
    list.push_back(s1); // list: {"World", "Hello", "Cpp"}
    EXPECT_EQ(s1, "Cpp"); // s1 本身不应被改变

    // 测试 push_back 移动
    std::string s2 = "Rocks";
    list.push_back(std::move(s2)); // list: {"World", "Hello", "Cpp", "Rocks"}
    // s2 的状态现在是未指定的，但通常为空
    // EXPECT_TRUE(s2.empty()); // 这取决于 std::string 的实现

    VerifyListContent<xor_list<std::string>, std::string>(list, {"World", "Hello", "Cpp", "Rocks"}, [](const std::string& s){ return s; });
    
    // pop_front 应该返回一个可用的 string
    std::string popped = list.pop_front();
    EXPECT_EQ(popped, "World");

    // clear 方法应该正确调用所有 string 的析构函数（由 valgrind 或内存泄漏工具验证）
    list.clear();
    EXPECT_TRUE(list.empty());

    // 再次使用
    list.push_back("New Life");
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(list.front(), "New Life");
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}