/**
 * @file zip.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <tuple>

#include "global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename... Args>
class Zip {
public:
    template <typename... Iterators>
    class Iterator {
    public:
        Iterator(Iterators... iterables);
        bool operator==(const Iterator& other) const noexcept;
        bool operator!=(const Iterator& other) const noexcept;
        Iterator& operator++() noexcept;
        Iterator operator++(int) noexcept;
        Iterator& operator--() noexcept;
        Iterator operator--(int) noexcept;
        auto operator*() noexcept;
        const auto& operator*() const noexcept;

    private:
        template <std::size_t... Indices>
        bool _compare(std::index_sequence<Indices...> /*unused*/, const Iterator& other) const;

    private:
        std::tuple<Iterators...> mIterators;
    };

public:
    Zip(Args&&... args) : mIterable(std::make_tuple(std::forward<Args>(args)...)) {}
    Zip(Args&... args) : mIterable(std::make_tuple(args...)) {}

    auto begin() noexcept;
    auto end() noexcept;
    auto cbegin() noexcept;
    auto cend() noexcept;
    auto rbegin() noexcept;
    auto rend() noexcept;
    auto crbegin() noexcept;
    auto crend() noexcept;

protected:
    template <std::size_t... Indices>
    auto _makeBegin(std::index_sequence<Indices...> /*unused*/) noexcept;
    template <std::size_t... Indices>
    auto _makeEnd(std::index_sequence<Indices...> /*unused*/) noexcept;
    template <std::size_t... Indices>
    auto _makeCBegin(std::index_sequence<Indices...> /*unused*/) noexcept;
    template <std::size_t... Indices>
    auto _makeCEnd(std::index_sequence<Indices...> /*unused*/) noexcept;
    template <std::size_t... Indices>
    auto _makeRBegin(std::index_sequence<Indices...> /*unused*/) noexcept;
    template <std::size_t... Indices>
    auto _makeREnd(std::index_sequence<Indices...> /*unused*/) noexcept;
    template <std::size_t... Indices>
    auto _makeCrBegin(std::index_sequence<Indices...> /*unused*/) noexcept;
    template <std::size_t... Indices>
    auto _makeCrEnd(std::index_sequence<Indices...> /*unused*/) noexcept;

private:
    std::tuple<Args...> mIterable;
};

template <typename... Args>
class ReverseZip {
public:
    ReverseZip(Args&&... args) : mZip(std::forward<Args>(args)...) {}
    ReverseZip(Args&... args) : mZip(args...) {}

    auto begin() noexcept;
    auto end() noexcept;
    auto cbegin() noexcept;
    auto cend() noexcept;
    auto rbegin() noexcept;
    auto rend() noexcept;
    auto crbegin() noexcept;
    auto crend() noexcept;

private:
    Zip<Args...> mZip;
};

template <typename... Args>
template <typename... Iterators>
template <std::size_t... Indices>
bool Zip<Args...>::Iterator<Iterators...>::_compare(std::index_sequence<Indices...> /*unused*/,
                                                    const Iterator& other) const {
    return ((std::get<Indices>(mIterators) == std::get<Indices>(other.mIterators)) || ...);
}

template <typename... Args>
template <typename... Iterators>
Zip<Args...>::Iterator<Iterators...>::Iterator(Iterators... iterables) : mIterators(iterables...) {}

template <typename... Args>
template <typename... Iterators>
bool Zip<Args...>::Iterator<Iterators...>::operator==(const Iterator& other) const noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _compare(std::make_index_sequence<Size>(), other);
}

template <typename... Args>
template <typename... Iterators>
bool Zip<Args...>::Iterator<Iterators...>::operator!=(const Iterator& other) const noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return !_compare(std::make_index_sequence<Size>(), other);
}

template <typename... Args>
template <typename... Iterators>
auto Zip<Args...>::Iterator<Iterators...>::operator++() noexcept -> Zip<Args...>::Iterator<Iterators...>& {
    std::apply([](auto&... iterators) { ((++iterators), ...); }, mIterators);
    return *this;
}

template <typename... Args>
template <typename... Iterators>
auto Zip<Args...>::Iterator<Iterators...>::operator++(int) noexcept -> Zip<Args...>::Iterator<Iterators...> {
    auto ret = ZipValueIterator(*this);
    std::apply([](auto&... iterators) { ((++iterators), ...); }, mIterators);
    return ret;
}

template <typename... Args>
template <typename... Iterators>
auto Zip<Args...>::Iterator<Iterators...>::operator--() noexcept -> Zip<Args...>::Iterator<Iterators...>& {
    std::apply([](auto&... iterators) { ((--iterators), ...); }, mIterators);
    return *this;
}

template <typename... Args>
template <typename... Iterators>
auto Zip<Args...>::Iterator<Iterators...>::operator--(int) noexcept -> Zip<Args...>::Iterator<Iterators...> {
    auto ret = ZipValueIterator(*this);
    std::apply([](auto&... iterators) { ((--iterators), ...); }, mIterators);
    return ret;
}

template <typename... Args>
template <typename... Iterators>
auto Zip<Args...>::Iterator<Iterators...>::operator*() noexcept {
    return std::apply([](auto&... iterators) { return std::make_tuple(*iterators...); }, mIterators);
}

template <typename... Args>
template <typename... Iterators>
const auto& Zip<Args...>::Iterator<Iterators...>::operator*() const noexcept {
    return std::apply([](auto&... iterators) { return std::tuple<const typename Args::value_type&...>(*iterators...); },
                      mIterators);
}

template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeBegin(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::begin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeEnd(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::end(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeCBegin(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::cbegin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeCEnd(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::cend(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeRBegin(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::rbegin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeREnd(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::rend(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeCrBegin(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::crbegin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t... Indices>
auto Zip<Args...>::_makeCrEnd(std::index_sequence<Indices...> /*unused*/) noexcept {
    return Iterator(std::crend(std::get<Indices>(mIterable))...);
}

template <typename... Args>
auto Zip<Args...>::begin() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeBegin(std::make_index_sequence<Size>());
}

template <typename... Args>
auto Zip<Args...>::end() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeEnd(std::make_index_sequence<Size>());
}

template <typename... Args>
auto Zip<Args...>::cbegin() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeCBegin(std::make_index_sequence<Size>());
}

template <typename... Args>
auto Zip<Args...>::cend() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeCEnd(std::make_index_sequence<Size>());
}

template <typename... Args>
auto Zip<Args...>::rbegin() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeRBegin(std::make_index_sequence<Size>());
}

template <typename... Args>
auto Zip<Args...>::rend() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeREnd(std::make_index_sequence<Size>());
}

template <typename... Args>
auto Zip<Args...>::crbegin() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeCRBegin(std::make_index_sequence<Size>());
}

template <typename... Args>
auto Zip<Args...>::crend() noexcept {
    constexpr std::size_t Size = sizeof...(Args);
    return _makeCREnd(std::make_index_sequence<Size>());
}

template <typename... Args>
auto ReverseZip<Args...>::begin() noexcept {
    return mZip.rbegin();
}

template <typename... Args>
auto ReverseZip<Args...>::end() noexcept {
    return mZip.rend();
}

template <typename... Args>
auto ReverseZip<Args...>::cbegin() noexcept {
    return mZip.crbegin();
}

template <typename... Args>
auto ReverseZip<Args...>::cend() noexcept {
    return mZip.crend();
}

template <typename... Args>
auto ReverseZip<Args...>::rbegin() noexcept {
    return mZip.begin();
}

template <typename... Args>
auto ReverseZip<Args...>::rend() noexcept {
    return mZip.end();
}

template <typename... Args>
auto ReverseZip<Args...>::crbegin() noexcept {
    return mZip.cbegin();
}

template <typename... Args>
auto ReverseZip<Args...>::crend() noexcept {
    return mZip.cend();
}

NEKO_END_NAMESPACE