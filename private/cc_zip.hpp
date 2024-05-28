#pragma once

#include <tuple>
#include <type_traits>

template <typename... Args>
class CCZip {
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
        bool compare(std::index_sequence<Indices...>, const Iterator& other) const;

    private:
        std::tuple<Iterators...> mIterators;
    };

public:
    CCZip(Args&&... args) : mIterable(std::make_tuple(std::forward<Args>(args)...)) {}
    CCZip(Args&... args) : mIterable(std::make_tuple(args...)) {}

    auto begin() noexcept;
    auto end() noexcept;
    auto cbegin() noexcept;
    auto cend() noexcept;
    auto rbegin() noexcept;
    auto rend() noexcept;
    auto crbegin() noexcept;
    auto crend() noexcept;

public:
    template <std::size_t ...Indices>
    auto _makeBegin(std::index_sequence<Indices...>) noexcept;
    template <std::size_t ...Indices>
    auto _makeEnd(std::index_sequence<Indices...>) noexcept;
    template <std::size_t ...Indices>
    auto _makeCBegin(std::index_sequence<Indices...>) noexcept;
    template <std::size_t ...Indices>
    auto _makeCEnd(std::index_sequence<Indices...>) noexcept;
    template <std::size_t ...Indices>
    auto _makeRBegin(std::index_sequence<Indices...>) noexcept;
    template <std::size_t ...Indices>
    auto _makeREnd(std::index_sequence<Indices...>) noexcept;
    template <std::size_t ...Indices>
    auto _makeCRBegin(std::index_sequence<Indices...>) noexcept;
    template <std::size_t ...Indices>
    auto _makeCREnd(std::index_sequence<Indices...>) noexcept;

private:
    std::tuple<Args...> mIterable;
};

template <typename... Args>
class CCReverseZip {
public:
    CCReverseZip(Args&&... args) : mZip(std::forward<Args>(args)...) {}
    CCReverseZip(Args&... args) : mZip(args...) {}

    auto begin() noexcept;
    auto end() noexcept;
    auto cbegin() noexcept;
    auto cend() noexcept;
    auto rbegin() noexcept;
    auto rend() noexcept;
    auto crbegin() noexcept;
    auto crend() noexcept;

private:
    CCZip<Args...> mZip;
};

template <typename... Args>
template <typename... Iterators>
template <std::size_t... Indices>
bool CCZip<Args...>::Iterator<Iterators...>::compare(std::index_sequence<Indices...>, const Iterator& other) const {
    return ((std::get<Indices>(mIterators) == std::get<Indices>(other.mIterators)) || ...);
}

template <typename... Args>
template <typename... Iterators>
CCZip<Args...>::Iterator<Iterators...>::Iterator(Iterators... iterables)
    : mIterators(iterables...) {}

template <typename... Args>
template <typename... Iterators>
bool CCZip<Args...>::Iterator<Iterators...>::operator==(const Iterator& other) const noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return compare(std::make_index_sequence<size>(), other);
}

template <typename... Args>
template <typename... Iterators>
bool CCZip<Args...>::Iterator<Iterators...>::operator!=(const Iterator& other) const noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return !compare(std::make_index_sequence<size>(), other);
}

template <typename... Args>
template <typename... Iterators>
CCZip<Args...>::Iterator<Iterators...>& CCZip<Args...>::Iterator<Iterators...>::operator++() noexcept {
    std::apply([](auto&... iterators) { ((++iterators), ...); }, mIterators);
    return *this;
}

template <typename... Args>
template <typename... Iterators>
CCZip<Args...>::Iterator<Iterators...> CCZip<Args...>::Iterator<Iterators...>::operator++(int) noexcept {
    auto ret = ZipValueIterator(*this);
    std::apply([](auto&... iterators) { ((++iterators), ...); }, mIterators);
    return ret;
}

template <typename... Args>
template <typename... Iterators>
CCZip<Args...>::Iterator<Iterators...>& CCZip<Args...>::Iterator<Iterators...>::operator--() noexcept {
    std::apply([](auto&... iterators) { ((--iterators), ...); }, mIterators);
    return *this;
}

template <typename... Args>
template <typename... Iterators>
CCZip<Args...>::Iterator<Iterators...> CCZip<Args...>::Iterator<Iterators...>::operator--(int) noexcept {
    auto ret = ZipValueIterator(*this);
    std::apply([](auto&... iterators) { ((--iterators), ...); }, mIterators);
    return ret;
}

template <typename... Args>
template <typename... Iterators>
auto CCZip<Args...>::Iterator<Iterators...>::operator*() noexcept {
    return std::apply([](auto&... iterators) { return std::make_tuple(*iterators...); }, mIterators);
}

template <typename... Args>
template <typename... Iterators>
const auto& CCZip<Args...>::Iterator<Iterators...>::operator*() const noexcept {
    return std::apply([](auto&... iterators) { return std::make_tuple(*iterators...); }, mIterators);
}

template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeBegin(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::begin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeEnd(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::end(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeCBegin(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::cbegin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeCEnd(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::cend(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeRBegin(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::rbegin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeREnd(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::rend(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeCRBegin(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::crbegin(std::get<Indices>(mIterable))...);
}
template <typename... Args>
template <std::size_t ...Indices>
auto CCZip<Args...>::_makeCREnd(std::index_sequence<Indices...>) noexcept {
    return Iterator(std::crend(std::get<Indices>(mIterable))...);
}

template <typename... Args>
auto CCZip<Args...>::begin() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeBegin(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCZip<Args...>::end() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeEnd(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCZip<Args...>::cbegin() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeCBegin(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCZip<Args...>::cend() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeCEnd(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCZip<Args...>::rbegin() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeRBegin(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCZip<Args...>::rend() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeREnd(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCZip<Args...>::crbegin() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeCRBegin(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCZip<Args...>::crend() noexcept {
    constexpr std::size_t size = sizeof...(Args);
    return _makeCREnd(std::make_index_sequence<size>());
}

template <typename... Args>
auto CCReverseZip<Args...>::begin() noexcept {
    return mZip.rbegin();
}

template <typename... Args>
auto CCReverseZip<Args...>::end() noexcept {
    return mZip.rend();
}

template <typename... Args>
auto CCReverseZip<Args...>::cbegin() noexcept {
    return mZip.crbegin();
}

template <typename... Args>
auto CCReverseZip<Args...>::cend() noexcept {
    return mZip.crend();
}

template <typename... Args>
auto CCReverseZip<Args...>::rbegin() noexcept {
    return mZip.begin();
}

template <typename... Args>
auto CCReverseZip<Args...>::rend() noexcept {
    return mZip.end();
}

template <typename... Args>
auto CCReverseZip<Args...>::crbegin() noexcept {
    return mZip.cbegin();
}

template <typename... Args>
auto CCReverseZip<Args...>::crend() noexcept {
    return mZip.cend();
}
