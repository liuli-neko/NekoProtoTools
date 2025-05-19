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
        : mData(reinterpret_cast<pointer>(&(*begin))), mSize(std::distance(begin, end)) {}

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

    auto operator[](const size_type index) -> reference { return mData[index]; }
    auto operator[](const size_type index) const -> const_reference { return mData[index]; }
    auto at(const size_type index) const -> const_reference {
        NEKO_ASSERT(index < mSize, "span", "Index out of span view");
        return mData[index];
    }
    auto at(const size_type index) -> reference {
        NEKO_ASSERT(index < mSize, "span", "Index out of span view");
        return mData[index];
    }

    auto begin() const -> iterator { return mData; }
    auto end() const -> iterator { return mData + mSize; }
    auto cbegin() const -> const_iterator { return mData; }
    auto cend() const -> const_iterator { return mData + mSize; }
    auto rbegin() const -> reverse_iterator { return reverse_iterator(end()); }
    auto rend() const -> reverse_iterator { return reverse_iterator(begin()); }
    auto crbegin() const -> const_reverse_iterator { return const_reverse_iterator(end()); }
    auto crend() const -> const_reverse_iterator { return const_reverse_iterator(begin()); }

    auto size_bytes() const -> size_type // NOLINT(readability-identifier-naming)
    {
        return mSize * sizeof(T);
    }
    auto size() const -> size_type { return mSize; }
    auto empty() const -> bool { return mSize == 0; }

    auto front() const -> const_reference { return mData[0]; }
    auto front() -> reference { return mData[0]; }
    auto back() const -> const_reference { return mData[mSize - 1]; }
    auto back() -> reference { return mData[mSize - 1]; }

    auto data() -> pointer { return mData; }
    auto data() const -> const_pointer { return mData; }

    auto subspan(size_type offset, size_type count) const -> Span { return Span(mData + offset, count); }
    auto subspan(size_type offset) const -> Span { return Span(mData + offset, mSize - offset); }
    auto first(size_type count) const -> Span { return Span(mData, count); }
    auto last(size_type count) const -> Span { return Span(mData + mSize - count, count); }

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