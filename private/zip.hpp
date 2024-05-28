#pragma once

#include <type_traits>
#include <tuple>

// zip_iterator
// if
template <typename ...Ts>
struct zip_iterator {
    zip_iterator(Ts ...args) noexcept : mTuple(args...) { }
    zip_iterator(const zip_iterator &) = default;
    ~zip_iterator() = default;

    auto operator !=(const zip_iterator &other) const noexcept {
        return _compare_all_nequals(other, std::make_index_sequence<sizeof ...(Ts)>());
    }
    auto operator ==(const zip_iterator &other) const noexcept {
        // Only one equals, we think the are same
        return !_compare_all_nequals(other, std::make_index_sequence<sizeof ...(Ts)>());
    }
    auto operator ++() -> zip_iterator & {
        std::apply([](auto &... iters) {
            (++iters, ...);
        }, mTuple);
        return *this;
    }
    auto operator --() -> zip_iterator & {
        std::apply([](auto &... iters) {
            (--iters, ...);
        }, mTuple);
        return *this;
    }
    auto operator *() const {
        return _pack_as_tuple(std::make_index_sequence<sizeof ...(Ts)>());
    }

    template <size_t ...Idx>
    auto _pack_as_tuple(std::index_sequence<Idx...>) const noexcept {
        return std::forward_as_tuple(*std::get<Idx>(mTuple)...);
    }

    template <size_t I>
    auto _compare_nequals(const zip_iterator &other) const -> bool {
        return std::get<I>(mTuple) != std::get<I>(other.mTuple);
    }
    template <size_t ...Idx>
    auto _compare_all_nequals(const zip_iterator &other, std::index_sequence<Idx...>) const -> bool {
        return (_compare_nequals<Idx>(other) && ...);
    }

    std::tuple<Ts...> mTuple;
};

template <typename ...Ts>
struct zip {
    zip(Ts &...args) noexcept : mTuple(args...) { }
    zip(const zip &) = default;
    ~zip() = default;

    auto begin() const noexcept {
        return _pack_begin(std::make_index_sequence<sizeof ...(Ts)>());
    }
    auto end() const noexcept {
        return _pack_end(std::make_index_sequence<sizeof ...(Ts)>());
    }

    auto rbegin() const noexcept {
        return _pack_rbegin(std::make_index_sequence<sizeof ...(Ts)>());
    }
    auto rend() const noexcept {
        return _pack_rend(std::make_index_sequence<sizeof ...(Ts)>());
    }

    template <size_t ...Idx>
    auto _pack_begin(std::index_sequence<Idx...>) const noexcept {
        return zip_iterator {std::begin(std::get<Idx>(mTuple))... };
    }
    template <size_t ...Idx>
    auto _pack_end(std::index_sequence<Idx...>) const noexcept {
        return zip_iterator {std::end(std::get<Idx>(mTuple))... };
    }

    template <size_t ...Idx>
    auto _pack_rend(std::index_sequence<Idx...>) const noexcept {
        return zip_iterator {std::rend(std::get<Idx>(mTuple))... };
    }
    template <size_t ...Idx>
    auto _pack_rbegin(std::index_sequence<Idx...>) const noexcept {
        return zip_iterator {std::rbegin(std::get<Idx>(mTuple))... };
    }

    std::tuple<Ts &...> mTuple;
};

template <typename T>
struct reverse {
    reverse(T &arg) noexcept : mContainer(arg) { }
    reverse(const reverse &) = default;
    ~reverse() = default;

    auto begin() const noexcept {
        return std::rbegin(mContainer);
    }
    auto end() const noexcept {
        return std::rend(mContainer);
    }

    auto rbegin() const noexcept {
        return std::begin(mContainer);
    }
    auto rend() const noexcept {
        return std::rend(mContainer);
    }

    T &mContainer;
};