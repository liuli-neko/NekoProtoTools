/**
 * @file tuple.hpp
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

#include "../private/helpers.hpp"
#include "nekoproto/global/log.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename SerializerT, typename T, int N, class enable = void>
struct UnfoldTupleHelper1 {
    static bool unfoldValue(SerializerT& sa, const T& value) {
        auto ret = UnfoldTupleHelper1<SerializerT, T, N - 1>::unfoldValue(sa, value);
        return sa(std::get<N>(value)) && ret;
    }
};
template <typename SerializerT, typename T, int N, class enable = void>
struct UnfoldTupleHelper2 {
    static bool unfoldValue(SerializerT& sa, T& value) {
        auto ret = UnfoldTupleHelper2<SerializerT, T, N - 1>::unfoldValue(sa, value);
        return sa(std::get<N>(value)) && ret;
    }
};
template <typename SerializerT, typename T>
struct UnfoldTupleHelper1<SerializerT, T, -1, void> {
    static bool unfoldValue(SerializerT& /*unused*/, const T& /*unused*/) { return true; }
};
template <typename SerializerT, typename T>
struct UnfoldTupleHelper1<SerializerT, T, 0, void> {
    static bool unfoldValue(SerializerT& sa, const T& value) { return sa(std::get<0>(value)); }
};
template <typename SerializerT, typename T>
struct UnfoldTupleHelper2<SerializerT, T, -1, void> {
    static bool unfoldValue(SerializerT& /*unused*/, T& /*unused*/) { return true; }
};
template <typename SerializerT, typename T>
struct UnfoldTupleHelper2<SerializerT, T, 0, void> {
    static bool unfoldValue(SerializerT& sa, T& value) { return sa(std::get<0>(value)); }
};

template <typename SerializerT, typename... Ts>
inline bool unfold_tuple_value(SerializerT& sa, std::tuple<Ts...>& value) {
    return UnfoldTupleHelper2<SerializerT, std::tuple<Ts...>, (int)sizeof...(Ts) - 1>::unfoldValue(sa, value);
}

template <typename SerializerT, typename... Ts>
inline bool unfold_tuple_value(SerializerT& sa, const std::tuple<Ts...>& value) {
    return UnfoldTupleHelper1<SerializerT, std::tuple<Ts...>, (int)sizeof...(Ts) - 1>::unfoldValue(sa, value);
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
    auto ret      = sa.startNode();
    if (!ret) {
        NEKO_LOG_INFO("xxx", "why");
    }
    ret = ret && sa(make_size_tag(size));
    if (size != sizeof...(Ts)) {
        NEKO_LOG_ERROR("proto", "tuple size mismatch, expected: {}, got: {}", sizeof...(Ts), size);
        sa.finishNode();
        return false;
    }
    ret = ret && unfold_tuple_value(sa, value);
    return sa.finishNode() && ret;
}

NEKO_END_NAMESPACE
