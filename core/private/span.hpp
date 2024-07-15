#pragma once

#include "global.hpp"

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

NEKO_BEGIN_NAMESPACE
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
    Span() : mData(nullptr), mSize(0) {}

    template <typename U = T>
    explicit Span(const U* data, size_type size)
        : mData(reinterpret_cast<pointer>(data)), mSize(size * sizeof(U) / sizeof(T)) {}

    template <typename It, typename End>
    Span(It begin, End end)
        : mData(reinterpret_cast<pointer>(std::addressof(*begin))), mSize(std::distance(begin, end)) {}

    template <typename U = T, size_type N>
    Span(const U (&data)[N]) : mData(reinterpret_cast<pointer>(data)), mSize(N * sizeof(U) / sizeof(T)) {}

    template <typename U = T, size_type N>
    Span(const std::array<U, N>& data)
        : mData(reinterpret_cast<pointer>(data.data())), mSize(data.size() * sizeof(U) / sizeof(T)) {}
    template <typename U = T, size_type N>
    Span(std::array<U, N>& data)
        : mData(reinterpret_cast<pointer>(data.data())), mSize(data.size() * sizeof(U) / sizeof(T)) {}

    template <typename U = T>
    explicit Span(const std::basic_string<U>& data)
        : mData(reinterpret_cast<pointer>(data.data())), mSize(data.size() * sizeof(U) / sizeof(T)) {}

    template <typename U = T>
    Span(const Span<U>& other)
        : mData(reinterpret_cast<pointer>(other.data)), mSize(other.size() * sizeof(U) / sizeof(T)) {}

    template <typename U = T>
    explicit Span(const std::vector<U>& other)
        : mData(reinterpret_cast<pointer>(other.data)), mSize(other.size() * sizeof(U) / sizeof(T)) {}
    template <typename U = T>
    explicit Span(std::vector<U>& data)
        : mData(reinterpret_cast<pointer>(data.data())), mSize(data.size() * sizeof(U) / sizeof(T)) {}

    reference operator[](const size_type index) { return mData[index]; }
    const_reference operator[](const size_type index) const { return mData[index]; }
    const_reference at(const size_type index) const {
        NEKO_ASSERT(index < mSize, "Index out of span view");
        return mData[index];
    }
    reference at(const size_type index) {
        NEKO_ASSERT(index < mSize, "Index out of span view");
        return mData[index];
    }

    iterator begin() const { return mData; }
    iterator end() const { return mData + mSize; }
    const_iterator cbegin() const { return mData; }
    const_iterator cend() const { return mData + mSize; }
    reverse_iterator rbegin() const { return reverse_iterator(end()); }
    reverse_iterator rend() const { return reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    size_type size_bytes() const { return mSize * sizeof(T); }
    size_type size() const { return mSize; }
    bool empty() const { return mSize == 0; }

    const_reference front() const { return mData[0]; }
    reference front() { return mData[0]; }
    const_reference back() const { return mData[mSize - 1]; }
    reference back() { return mData[mSize - 1]; }

    pointer data() { return mData; }
    const_pointer data() const { return mData; }

    Span subspan(size_type offset, size_type count) const { return Span(mData + offset, count); }
    Span subspan(size_type offset) const { return Span(mData + offset, mSize - offset); }
    Span first(size_type count) const { return Span(mData, count); }
    Span last(size_type count) const { return Span(mData + mSize - count, count); }

private:
    size_type mSize;
    pointer mData;
};
#else
template <typename T = char>
using Span = std::span<T>;
#endif
} // namespace detail

NEKO_END_NAMESPACE