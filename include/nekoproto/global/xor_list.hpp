#pragma once

#include "global.hpp"
#include "nekoproto/global/reflect.hpp"

#include <concepts>
#include <cstdint> // for uintptr_t
#include <iostream>
#include <iterator> // for iterator tags
#include <type_traits>

NEKO_BEGIN_NAMESPACE
// --- 主模板声明 ---
template <typename T, typename = void>
class xor_list;
/**
 * @brief (核心) XOR 链表节点
 *
 * 这是所有侵入式节点的【唯一】基类。它只提供链接功能，不关心如何存储值。
 * 这就是实现多态和行为特化的基础。
 */
class xor_node_base {
private:
    template <typename T, typename>
    friend class xor_list;

    // 指针类型是 void* 以保持通用性，在 list 层面进行转换
    xor_node_base* link_ = nullptr;

    static xor_node_base* xor_ptr(const xor_node_base* a, const xor_node_base* b) {
        return reinterpret_cast<xor_node_base*>(reinterpret_cast<uintptr_t>(a) ^ reinterpret_cast<uintptr_t>(b));
    }

public:
    // 返回类型也是基类指针
    xor_node_base* next(const xor_node_base* pre) const { return xor_ptr(pre, link_); }
    xor_node_base* pre(const xor_node_base* next) const { return xor_ptr(next, link_); }
    xor_node_base* link() const { return link_; }
    void set(const xor_node_base* pre, const xor_node_base* next) { link_ = xor_ptr(pre, next); }
    void set(xor_node_base* link) { link_ = link; }
};

template <typename Node>
class xor_list_iterator;

/**
 * @brief XOR 链表节点模板
 * @tparam T 节点存储的数据类型
 *
 * 用户自定义的类需要公有继承此类，以成为链表的一部分。
 */
template <typename T>
    requires(!std::derived_from<T, xor_node_base>)
class xor_node final : public xor_node_base {
private:
    T value_;

public:
    using value_type = T;
    xor_node()       = default;

    // 带值的构造函数
    explicit xor_node(const T& value) : xor_node_base(), value_(value) {}
    explicit xor_node(T&& value) : xor_node_base(), value_(std::move(value)) {}

    /**
     * @brief 获取节点存储的值的引用
     */
    T& value() { return value_; }

    /**
     * @brief 获取节点存储的值的 const 引用
     */
    const T& value() const { return value_; }
};

namespace detail {
template <typename Node>
struct is_xor_node_template : std::false_type {};
template <typename T>
struct is_xor_node_template<xor_node<T>> : std::true_type {};

template <typename Node>
struct xor_node_trait {};

template <typename Node>
    requires(std::derived_from<Node, xor_node_base> && is_xor_node_template<Node>::value)
struct xor_node_trait<Node> {
    using value_type = typename Node::value_type;
    static value_type& value(Node* node) { return node->value(); }
    static const value_type& value(const Node* node) { return node->value(); }
};

template <typename Node>
    requires(std::derived_from<Node, xor_node_base> && !is_xor_node_template<Node>::value)
struct xor_node_trait<Node> {
    using value_type = Node;
    static value_type& value(Node* node) { return *node; }
    static const value_type& value(const Node* node) { return *node; }
};

template <typename Node>
decltype(auto) xor_node_get_value(Node* node) {
    return xor_node_trait<Node>::value(node);
}

template <typename Node>
decltype(auto) xor_node_get_value(const Node* node) {
    return xor_node_trait<Node>::value(node);
}
} // namespace detail

/**
 * @brief XOR 链表的双向迭代器
 * @tparam Node 节点类型 (即 xor_node<T> 或其派生类)
 */
template <typename Node>
class xor_list_iterator {
public:
    //--- 标准库兼容性所需的类型定义 ---
    using iterator_category = std::bidirectional_iterator_tag;
    using raw_value_type    = std::decay_t<decltype(detail::xor_node_get_value(std::declval<Node*>()))>;
    using value_type        = std::remove_const_t<raw_value_type>;

    using difference_type = std::ptrdiff_t;

    // 使用 std::conditional 根据 Node 是否为 const 来决定 pointer 和 reference 的类型
    using pointer = typename std::conditional<std::is_const<Node>::value, const value_type*, value_type*>::type;

    using reference = typename std::conditional<std::is_const<Node>::value, const value_type&, value_type&>::type;

private:
    Node* prev_    = nullptr;
    Node* current_ = nullptr;

    template <typename T, typename>
    friend class xor_list;

public:
    // 构造函数
    xor_list_iterator() = default;
    xor_list_iterator(Node* prev, Node* current) : prev_(prev), current_(current) {}

    //--- 迭代器核心操作 ---

    // 解引用
    reference operator*() const { return detail::xor_node_get_value(current_); }

    pointer operator->() const { return &detail::xor_node_get_value(current_); }

    // 前置递增
    xor_list_iterator& operator++() {
        Node* next = static_cast<Node*>(current_->next(prev_));
        prev_      = current_;
        current_   = next;
        return *this;
    }

    // 后置递增
    xor_list_iterator operator++(int) {
        xor_list_iterator temp = *this;
        ++(*this);
        return temp;
    }

    // 前置递减
    xor_list_iterator& operator--() {
        Node* prev_of_prev = static_cast<Node*>(prev_->pre(current_));
        current_           = prev_;
        prev_              = prev_of_prev;
        return *this;
    }

    // 后置递减
    xor_list_iterator operator--(int) {
        xor_list_iterator temp = *this;
        --(*this);
        return temp;
    }

    // 比较操作
    bool operator==(const xor_list_iterator& other) const { return current_ == other.current_; }

    bool operator!=(const xor_list_iterator& other) const { return current_ != other.current_; }
};

// --- 偏特化 1: 侵入式版本 ---
// 当 T 继承自 xor_node_base 时启用
template <typename Node>
class xor_list<Node, std::enable_if_t<std::is_base_of_v<xor_node_base, Node>>> {
private:
    Node* head_  = nullptr;
    Node* tail_  = nullptr;
    size_t size_ = 0;

public:
    using iterator        = xor_list_iterator<Node>;
    using const_iterator  = xor_list_iterator<const Node>; // 简单起见，这里可以用同样的实现
    using value_type      = typename iterator::value_type;
    using reference       = value_type&;
    using const_reference = const value_type&;

    // 构造函数和析构函数
    xor_list() noexcept = default;
    ~xor_list()         = default; // 不管理内存

    // 禁止拷贝和赋值，因为链表不拥有节点
    xor_list(const xor_list&)            = delete;
    xor_list& operator=(const xor_list&) = delete;

    //--- 迭代器访问 ---
    iterator begin() noexcept { return iterator(nullptr, head_); }
    const_iterator begin() const noexcept { return const_iterator(nullptr, head_); }

    iterator end() noexcept { return iterator(tail_, nullptr); }
    const_iterator end() const noexcept { return const_iterator(tail_, nullptr); }

    //--- 容量查询 ---
    bool empty() const noexcept { return head_ == nullptr; }

    size_t size() const noexcept { return size_; }

    //--- 元素操作 ---

    void push_front(Node* node) noexcept {
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

    void push_back(Node* node) noexcept {
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

    Node* pop_front() noexcept {
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

    Node* pop_back() noexcept {
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

    reference front() noexcept { return detail::xor_node_get_value(head_); }
    const_reference front() const noexcept { return detail::xor_node_get_value(head_); }

    reference back() noexcept { return detail::xor_node_get_value(tail_); }
    const_reference back() const noexcept { return detail::xor_node_get_value(tail_); }

    /**
     * @brief 移除位于迭代器位置的节点
     * @param pos 指向要移除的节点的迭代器
     * @return 指向被移除节点之后节点的迭代器
     */
    iterator erase(iterator pos) noexcept {
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
class xor_list<T, std::enable_if_t<!std::is_base_of_v<xor_node_base, T>>> {
public:
    // --- 类型定义 ---
    using value_type      = T;
    using allocator_type  = std::allocator<T>;
    using reference       = value_type&;
    using const_reference = const value_type&;

    // 内部节点类型是 xor_node<T>
    using Node = xor_node<T>;
    // 迭代器直接复用 intrusive 版本的迭代器
    using iterator       = xor_list_iterator<Node>;
    using const_iterator = xor_list_iterator<const Node>;

private:
    // 使用 intrusive list 作为底层引擎
    using intrusive_list_type = xor_list<Node>;

    // 需要一个 allocator 来管理 Node 的内存
    using NodeAllocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<Node>;

    intrusive_list_type list_impl_;
    NodeAllocator node_alloc_;

public:
    // --- 构造函数和析构函数 ---

    xor_list() noexcept = default;

    ~xor_list() { clear(); }

    // --- 元素访问 ---
    iterator begin() noexcept { return list_impl_.begin(); }
    const_iterator begin() const noexcept { return list_impl_.begin(); }
    iterator end() noexcept { return list_impl_.end(); }
    const_iterator end() const noexcept { return list_impl_.end(); }
    reference front() noexcept { return list_impl_.front(); }
    const_reference front() const noexcept { return list_impl_.front(); }
    reference back() noexcept { return list_impl_.back(); }
    const_reference back() const noexcept { return list_impl_.back(); }

    // --- 容量 ---
    bool empty() const noexcept { return list_impl_.empty(); }
    size_t size() const noexcept { return list_impl_.size(); }

    // --- 修改器 ---

    void push_back(const value_type& value) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, value);
        list_impl_.push_back(new_node);
    }

    void push_back(value_type&& value) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, std::move(value));
        list_impl_.push_back(new_node);
    }

    T pop_front() {
        if (empty()) {
            throw std::runtime_error("pop_front() called on empty list");
        }
        Node* front = list_impl_.pop_front();
        T value     = std::move(front->value());
        std::allocator_traits<NodeAllocator>::destroy(node_alloc_, front);
        std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, front, 1);
        return value;
    }

    T pop_back() {
        if (empty()) {
            throw std::runtime_error("pop_back() called on empty list");
        }
        Node* back = list_impl_.pop_back();
        T value    = std::move(back->value());
        std::allocator_traits<NodeAllocator>::destroy(node_alloc_, back);
        std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, back, 1);
        return value;
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        // 直接在节点的 value_ 成员上构造对象
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, std::forward<Args>(args)...);
        list_impl_.push_back(new_node);
        return new_node->value();
    }

    void push_front(const value_type& value) {
        Node* new_node = std::allocator_traits<NodeAllocator>::allocate(node_alloc_, 1);
        std::allocator_traits<NodeAllocator>::construct(node_alloc_, new_node, value);
        list_impl_.push_front(new_node);
    }

    iterator erase(iterator pos) {
        Node* node_to_delete = pos.current_;
        iterator next_it     = list_impl_.erase(pos);

        std::allocator_traits<NodeAllocator>::destroy(node_alloc_, node_to_delete);
        std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, node_to_delete, 1);

        return next_it;
    }

    void clear() noexcept {
        while (!list_impl_.empty()) {
            Node* node_to_delete = list_impl_.pop_front(); // 获取节点指针

            std::allocator_traits<NodeAllocator>::destroy(node_alloc_, node_to_delete);
            std::allocator_traits<NodeAllocator>::deallocate(node_alloc_, node_to_delete, 1);
        }
    }
};

NEKO_END_NAMESPACE