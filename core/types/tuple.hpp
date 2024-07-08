#pragma once
#include <tuple>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename SerializerT, typename T, size_t N, class enable = void>
struct unfold_tuple_helper1 {
    static bool unfold_value(SerializerT& sa, const T& value) {
        auto ret = unfold_tuple_helper1<SerializerT, T, N - 1>::unfold_value(sa, value);
        return sa(std::get<N>(value)) && ret;
    }
};
template <typename SerializerT, typename T, size_t N, class enable = void>
struct unfold_tuple_helper2 {
    static bool unfold_value(SerializerT& sa, T& value) {
        auto ret = unfold_tuple_helper2<SerializerT, T, N - 1>::unfold_value(sa, value);
        return sa(std::get<N>(value)) && ret;
    }
};

template <typename SerializerT, typename T>
struct unfold_tuple_helper1<SerializerT, T, 0, void> {
    static bool unfold_value(SerializerT& sa, const T& value) { return sa(std::get<0>(value)); }
};
template <typename SerializerT, typename T>
struct unfold_tuple_helper2<SerializerT, T, 0, void> {
    static bool unfold_value(SerializerT& sa, T& value) { return sa(std::get<0>(value)); }
};

template <typename SerializerT, typename... Ts>
inline bool unfold_tuple_value(SerializerT& sa, std::tuple<Ts...>& value) {
    return unfold_tuple_helper2<SerializerT, std::tuple<Ts...>, sizeof...(Ts) - 1>::unfold_value(sa, value);
}

template <typename SerializerT, typename... Ts>
inline bool unfold_tuple_value(SerializerT& sa, const std::tuple<Ts...>& value) {
    return unfold_tuple_helper1<SerializerT, std::tuple<Ts...>, sizeof...(Ts) - 1>::unfold_value(sa, value);
}
} // namespace detail

template <typename SerializerT, typename... Ts>
inline bool save(SerializerT& sa, const std::tuple<Ts...>& value) {
    sa.startArray(sizeof...(Ts));
    auto ret = detail::unfold_tuple_value(sa, value);
    sa.endArray();
    return ret;
}

template <typename SerializerT, typename... Ts>
inline bool load(SerializerT& sa, std::tuple<Ts...>& value) {
    uint32_t size = 0;
    sa(makeSizeTag(size));
    if (size != sizeof...(Ts)) {
        NEKO_LOG_ERROR("tuple size mismatch, expected: {}, got: {}", sizeof...(Ts), size);
        return false;
    }
    return unfold_tuple_value(sa, value);
}

NEKO_END_NAMESPACE
