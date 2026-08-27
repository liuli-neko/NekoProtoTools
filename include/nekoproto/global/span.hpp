#pragma once

#include "global.hpp"
#include "log.hpp"

#include <array>
#include <string>
#include <vector>
#if NEKO_CPP_PLUS >= 20
#include <version>
#if __cpp_lib_span >= 202002
#include <span>
#ifndef NEKO_HAS_STD_SPAN
#define NEKO_HAS_STD_SPAN
#endif
#endif
#endif

namespace nekoproto {
namespace detail {
#if !defined(NEKO_HAS_STD_SPAN)
template <typename T = char>
class Span {
public:
    using element_type           = T;
    using value_type             = typename std::remove_reference<typename std::remove_const<T>::type>::type;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using pointer                = T*;
    using const_pointer          = const T*;
    using reference              = T&;
    using const_reference        = const T&;
    using iterator               = T*;
    using const_iterator         = const T*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    Span() : size_(0), data_(nullptr) {}

    template <typename U = T>
    explicit Span(const U* data, size_type size)
        : size_(size * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(data)) {}

    template <typename It, typename End>
    Span(It begin, End end)
        : size_(std::distance(begin, end)), data_(reinterpret_cast<pointer>(&(*begin))) {}

    template <typename U = T, size_type N>
    Span(const U (&data)[N]) : size_(N * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(data)) {}

    template <typename U = T, size_type N>
    Span(const std::array<U, N>& data)
        : size_(data.size() * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(data.data())) {}
    template <typename U = T, size_type N>
    Span(std::array<U, N>& data)
        : size_(data.size() * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(data.data())) {}

    template <typename U = T>
    explicit Span(const std::basic_string<U>& data)
        : size_(data.size() * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(data.data())) {}

    template <typename U = T>
    Span(const Span<U>& other)
        : size_(other.size() * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(other.data)) {}

    template <typename U = T>
    explicit Span(const std::vector<U>& other)
        : size_(other.size() * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(other.data)) {}
    template <typename U = T>
    explicit Span(std::vector<U>& data)
        : size_(data.size() * sizeof(U) / sizeof(T)), data_(reinterpret_cast<pointer>(data.data())) {}

    auto operator[](const size_type index) -> reference { return data_[index]; }
    auto operator[](const size_type index) const -> const_reference { return data_[index]; }
    auto at(const size_type index) const -> const_reference {
        NEKO_ASSERT(index < size_, "span", "Index out of span view");
        return data_[index];
    }
    auto at(const size_type index) -> reference {
        NEKO_ASSERT(index < size_, "span", "Index out of span view");
        return data_[index];
    }

    auto begin() const -> iterator { return data_; }
    auto end() const -> iterator { return data_ + size_; }
    auto cbegin() const -> const_iterator { return data_; }
    auto cend() const -> const_iterator { return data_ + size_; }
    auto rbegin() const -> reverse_iterator { return reverse_iterator(end()); }
    auto rend() const -> reverse_iterator { return reverse_iterator(begin()); }
    auto crbegin() const -> const_reverse_iterator { return const_reverse_iterator(end()); }
    auto crend() const -> const_reverse_iterator { return const_reverse_iterator(begin()); }

    auto size_bytes() const -> size_type {
        return size_ * sizeof(T);
    }
    auto size() const -> size_type { return size_; }
    auto empty() const -> bool { return size_ == 0; }

    auto front() const -> const_reference { return data_[0]; }
    auto front() -> reference { return data_[0]; }
    auto back() const -> const_reference { return data_[size_ - 1]; }
    auto back() -> reference { return data_[size_ - 1]; }

    auto data() -> pointer { return data_; }
    auto data() const -> const_pointer { return data_; }

    auto subspan(size_type offset, size_type count) const -> Span { return Span(data_ + offset, count); }
    auto subspan(size_type offset) const -> Span { return Span(data_ + offset, size_ - offset); }
    auto first(size_type count) const -> Span { return Span(data_, count); }
    auto last(size_type count) const -> Span { return Span(data_ + size_ - count, count); }

private:
    size_type size_;
    pointer   data_;
};
#else
template <typename T = char>
using Span = std::span<T>;
#endif
} // namespace detail

} // namespace nekoproto