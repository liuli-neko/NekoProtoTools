#pragma once

#include "global.hpp"
#include "nekoproto/global/reflect.hpp"

#include <concepts>
#include <cstdint> // for uintptr_t
#include <iostream>
#include <iterator> // for iterator tags
#include <type_traits>

namespace nekoproto {
// --- 主模板声明 ---
template <typename T, typename = void>
class XorList;
/**
 * @brief (核心) XOR 链表节点
 *
 * 这是所有侵入式节点的【唯一】基类。它只提供链接功能，不关心如何存储值。
 * 这就是实现多态和行为特化的基础。
 */
class XorNodeBase {
private:
    template <typename T, typename>
    friend class XorList;

    // 指针类型是 void* 以保持通用性，在 list 层面进行转换
    XorNodeBase* link_ = nullptr;

    static auto xorPtr(const XorNodeBase* a, const XorNodeBase* b) -> XorNodeBase* {
        return reinterpret_cast<XorNodeBase*>(reinterpret_cast<uintptr_t>(a) ^ reinterpret_cast<uintptr_t>(b));
    }

public:
    // 返回类型也是基类指针
    auto next(const XorNodeBase* pre) const -> XorNodeBase* { return xorPtr(pre, link_); }
    auto pre(const XorNodeBase* next) const -> XorNodeBase* { return xorPtr(next, link_); }
    auto link() const -> XorNodeBase* { return link_; }
    void set(const XorNodeBase* pre, const XorNodeBase* next) { link_ = xorPtr(pre, next); }
    void set(XorNodeBase* link) { link_ = link; }
};

template <typename Node>
class XorListIterator;

/**
 * @brief XOR 链表节点模板
 * @tparam T 节点存储的数据类型
 *
 * 用户自定义的类需要公有继承此类，以成为链表的一部分。
 */
template <typename T>
    requires(!std::derived_from<T, XorNodeBase>)
class XorNode final : public XorNodeBase {
private:
    T value_;

public:
    using value_type = T;
    XorNode()       = default;

    // 带值的构造函数
    explicit XorNode(const T& value) : XorNodeBase(), value_(value) {}
    explicit XorNode(T&& value) : XorNodeBase(), value_(std::move(value)) {}

    /**
     * @brief 获取节点存储的值的引用
     */
    auto value() -> T& { return value_; }

    /**
     * @brief 获取节点存储的值的 const 引用
     */
    auto value() const -> const T& { return value_; }
};

namespace detail {
template <typename Node>
struct IsXorNodeTemplate : std::false_type {};
template <typename T>
struct IsXorNodeTemplate<XorNode<T>> : std::true_type {};

template <typename Node>
struct XorNodeTrait {};

template <typename Node>
    requires(std::derived_from<Node, XorNodeBase> && IsXorNodeTemplate<Node>::value)
struct XorNodeTrait<Node> {
    using value_type = typename Node::value_type;
    static auto value(Node* node) -> value_type& { return node->value(); }
    static auto value(const Node* node) -> const value_type& { return node->value(); }
};

template <typename Node>
    requires(std::derived_from<Node, XorNodeBase> && !IsXorNodeTemplate<Node>::value)
struct XorNodeTrait<Node> {
    using value_type = Node;
    static auto value(Node* node) -> value_type& { return *node; }
    static auto value(const Node* node) -> const value_type& { return *node; }
};

template <typename Node>
auto xorNodeGetValue(Node* node) -> decltype(auto) {
    return XorNodeTrait<Node>::value(node);
}

template <typename Node>
auto xorNodeGetValue(const Node* node) -> decltype(auto) {
    return XorNodeTrait<Node>::value(node);
}
} // namespace detail

/**
 * @brief XOR 链表的双向迭代器
 * @tparam Node 节点类型 (即 XorNode<T> 或其派生类)
 */
template <typename Node>
class XorListIterator {
public:
    //--- 标准库兼容性所需的类型定义 ---
    using iterator_category = std::bidirectional_iterator_tag;
    using raw_value_type    = std::decay_t<decltype(detail::xorNodeGetValue(std::declval<Node*>()))>;
    using value_type        = std::remove_const_t<raw_value_type>;

    using difference_type = std::ptrdiff_t;

    // 使用 std::conditional 根据 Node 是否为 const 来决定 pointer 和 reference 的类型
    using pointer = typename std::conditional<std::is_const<Node>::value, const value_type*, value_type*>::type;

    using reference = typename std::conditional<std::is_const<Node>::value, const value_type&, value_type&>::type;

private:
    Node* prev_    = nullptr;
    Node* current_ = nullptr;

    template <typename T, typename>
    friend class XorList;

public:
    // 构造函数
    XorListIterator() = default;
    XorListIterator(Node* prev, Node* current) : prev_(prev), current_(current) {}

    //--- 迭代器核心操作 ---

    // 解引用
    auto operator*() const -> reference { return detail::xorNodeGetValue(current_); }

    auto operator->() const -> pointer { return &detail::xorNodeGetValue(current_); }

    // 前置递增
    auto operator++() -> XorListIterator& {
        Node* next = static_cast<Node*>(current_->next(prev_));
        prev_      = current_;
        current_   = next;
        return *this;
    }

    // 后置递增
    auto operator++(int) -> XorListIterator {
        XorListIterator temp = *this;
        ++(*this);
        return temp;
    }

    // 前置递减
    auto operator--() -> XorListIterator& {
        Node* prev_of_prev = static_cast<Node*>(prev_->pre(current_));
        current_           = prev_;
        prev_              = prev_of_prev;
        return *this;
    }

    // 后置递减
    auto operator--(int) -> XorListIterator {
        XorListIterator temp = *this;
        --(*this);
        return temp;
    }

    // 比较操作
    auto operator==(const XorListIterator& other) const -> bool { return current_ == other.current_; }

    auto operator!=(const XorListIterator& other) const -> bool { return current_ != other.current_; }
};

// --- 偏特化 1: 侵入式版本 ---
// 当 T 继承自 XorNodeBase 时启用
template <typename Node>
class XorList<Node, std::enable_if_t<std::is_base_of_v<XorNodeBase, Node>>> {
private:
    Node* head_  = nullptr;
    Node* tail_  = nullptr;
    size_t size_ = 0;

public:
    using iterator        = XorListIterator<Node>;
    using const_iterator  = XorListIterator<const Node>; // 简单起见，这里可以用同样的实现
    using value_type      = typename iterator::value_type;
    using reference       = value_type&;
    using const_reference = const value_type&;

    // 构造函数和析构函数
    XorList() noexcept = default;
    ~XorList()         = default; // 不管理内存

    // 禁止拷贝和赋值，因为链表不拥有节点
    XorList(const XorList&)            = delete;
    auto operator=(const XorList&) -> XorList& = delete;

    //--- 迭代器访问 ---
    auto begin() noexcept -> iterator { return iterator(nullptr, head_); }
    auto begin() const noexcept -> const_iterator { return const_iterator(nullptr, head_); }

    auto end() noexcept -> iterator { return iterator(tail_, nullptr); }
    auto end() const noexcept -> const_iterator { return const_iterator(tail_, nullptr); }

    //--- 容量查询 ---
    auto empty() const noexcept -> bool { return head_ == nullptr; }

    auto size() const noexcept -> size_t { return size_; }

    //--- 元素操作 ---

    void pushFront(Node* node) noexcept {
        if (empty()) {
            head_ = tail_ = node;
            node->set(nullptr, nullptr);
        } else {
            // 1. 更新新节点的链接
            // pre=nullptr, next=旧head
            node->set(nullptr, head_);

            // 2. 更新旧head节点的链接
            // 它的后继不变 (head_->next(nullptr))，前驱从 nullptr 变为 node
            Node* old_next = static_cast<Node*>(head_->next(nullptr));
            head_->set(node, old_next); // 新pre是node, 旧next是old_next

            // 3. 更新链表的头指针
            head_ = node;
        }
        size_++;
    }

    void pushBack(Node* node) noexcept {
        if (empty()) {
            head_ = tail_ = node;
            node->set(nullptr, nullptr);
        } else {
            // 1. 更新新节点的链接
            // pre=旧tail, next=nullptr
            node->set(tail_, nullptr);

            // 2. 更新旧tail节点的链接
            // 它的前驱不变 (tail_->pre(nullptr))，后继从 nullptr 变为 node
            Node* old_prev = static_cast<Node*>(tail_->pre(nullptr));
            tail_->set(old_prev, node); // 旧pre是old_prev, 新next是node

            // 3. 更新链表的尾指针
            tail_ = node;
        }
        size_++;
    }

    auto popFront() noexcept -> Node* {
        if (empty()) {
            return nullptr;
        }

        Node* front = head_;
        Node* next  = static_cast<Node*>(head_->next(nullptr));

        if (next) {
            auto next_of_next = static_cast<Node*>(next->next(front));
            next->set(nullptr, next_of_next);
        } else {
            tail_ = nullptr;
        }

        head_ = next;
        size_--;

        return front;
    }

    auto popBack() noexcept -> Node* {
        if (empty()) {
            return nullptr;
        }

        Node* back = tail_;
        Node* prev = static_cast<Node*>(tail_->pre(nullptr));

        if (prev) {
            auto prev_of_prev = static_cast<Node*>(prev->pre(back));
            prev->set(prev_of_prev, nullptr);
        } else {
            head_ = nullptr;
        }

        tail_ = prev;
        size_--;

        return back;
    }

    auto front() noexcept -> reference { return detail::xorNodeGetValue(head_); }
    auto front() const noexcept -> const_reference { return detail::xorNodeGetValue(head_); }

    auto back() noexcept -> reference { return detail::xorNodeGetValue(tail_); }
    auto back() const noexcept -> const_reference { return detail::xorNodeGetValue(tail_); }

    /**
     * @brief 移除位于迭代器位置的节点
     * @param pos 指向要移除的节点的迭代器
     * @return 指向被移除节点之后节点的迭代器
     */
    auto erase(iterator pos) noexcept -> iterator {
        Node* prev    = pos.prev_;
        Node* current = pos.current_;
        Node* next    = static_cast<Node*>(current->next(prev));

        if (prev) { // 不是头节点
            Node* prev_of_prev = static_cast<Node*>(prev->pre(current));
            prev->set(prev_of_prev, next);
        } else { // 是头节点
            head_ = next;
        }

        if (next) { // 不是尾节点
            Node* next_of_next = static_cast<Node*>(next->next(current));
            next->set(prev, next_of_next);
        } else { // 是尾节点
            tail_ = prev;
        }

        size_--;
        // 将被移除的节点链接清空
        current->set(nullptr, nullptr);

        return iterator(prev, next);
    }

    /**
     * @brief 清空链表（仅重置指针，不释放节点内存）
     */
    void clear() noexcept {
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }
};

template <typename T>
class XorList<T, std::enable_if_t<!std::is_base_of_v<XorNodeBase, T>>> {
public:
    // --- 类型定义 ---
    using value_type      = T;
    using allocator_type  = std::allocator<T>;
    using reference       = value_type&;
    using const_reference = const value_type&;

    // 内部节点类型是 XorNode<T>
    using Node = XorNode<T>;
    // 迭代器直接复用 intrusive 版本的迭代器
    using iterator       = XorListIterator<Node>;
    using const_iterator = XorListIterator<const Node>;

private:
    // 使用 intrusive list 作为底层引擎
    using intrusive_list_type = XorList<Node>;

    // 需要一个 allocator 来管理 Node 的内存
    using NodeAllocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<Node>;

    intrusive_list_type list_impl_;
    NodeAllocator node_alloc_;

public:
    // --- 构造函数和析构函数 ---

    XorList() noexcept = default;

    ~XorList() { clear(); }

    // --- 元素访问 ---
    auto begin() noexcept -> iterator { return list_impl_.begin(); }
    auto begin() const noexcept -> const_iterator { return list_impl_.begin(); }
    auto end() noexcept -> iterator { return list_impl_.end(); }
    auto end() const noexcept -> const_iterator { return list_impl_.end(); }
    auto front() noexcept -> reference { return list_impl_.front(); }
    auto front() const noexcept -> const_reference { return list_impl_.front(); }
    auto back() noexcept -> reference { return list_impl_.back(); }
    auto back() const noexcept -> const_reference { return list_impl_.back(); }

    // --- 容量 ---
    auto empty() const noexcept -> bool { return list_impl_.empty(); }
    auto size() const noexcept -> size_t { return list_impl_.size(); }

    // --- 修改器 ---

    void pushBack(const value_type& value) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, value);
        list_impl_.pushBack(new_node);
    }

    void pushBack(value_type&& value) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, std::move(value));
        list_impl_.pushBack(new_node);
    }

    auto popFront() -> T {
        if (empty()) {
            throw std::runtime_error("pop_front() called on empty list");
        }
        Node* front = list_impl_.popFront();
        T value     = std::move(front->value());
        std::allocator_traits<NodeAllocator>::destroy(node_alloc_, front);
        std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, front, 1);
        return value;
    }

    auto popBack() -> T {
        if (empty()) {
            throw std::runtime_error("pop_back() called on empty list");
        }
        Node* back = list_impl_.popBack();
        T value    = std::move(back->value());
        std::allocator_traits<NodeAllocator>::destroy(node_alloc_, back);
        std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, back, 1);
        return value;
    }

    template <typename... Args>
    auto emplaceBack(Args&&... args) -> reference {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        // 直接在节点的 value_ 成员上构造对象
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, std::forward<Args>(args)...);
        list_impl_.push_back(new_node);
        return new_node->value();
    }

    void pushFront(const value_type& value) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, value);
        list_impl_.pushFront(new_node);
    }

    auto erase(iterator pos) -> iterator {
        Node* node_to_delete = pos.current_;
        iterator next_it     = list_impl_.erase(pos);

        std::allocator_traits<NodeAllocator>::destroy(node_alloc_, node_to_delete);
        std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, node_to_delete, 1);

        return next_it;
    }

    void clear() noexcept {
        while (!list_impl_.empty()) {
            Node* node_to_delete = list_impl_.popFront(); // 获取节点指针

            std::allocator_traits<NodeAllocator>::destroy(node_alloc_, node_to_delete);
            std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, node_to_delete, 1);
        }
    }
};

} // namespace nekoproto