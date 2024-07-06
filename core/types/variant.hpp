#pragma once
#include <variant>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename... Ts>
struct unfold_variant_helper {
    template <size_t N>
    static bool unfoldValueImp(Serializer& sa, std::variant<Ts...>&& value) {
        if (value.index() != N)
            return false;
        return sa(std::get<N>(std::forward<std::variant<Ts...>>(value)));
    }

    template <size_t... Ns>
    static bool unfoldValue(Serializer& sa, std::variant<Ts...>&& value, std::index_sequence<Ns...>) {
        return (unfoldValueImp<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(
                    sa, std::forward<std::variant<Ts...>>(value)) ||
                ...);
    }
};

template <typename Serializer, typename... Ts>
inline bool save(Serializer& sa, const std::variant<Ts...>& value) {
    return unfold_variant_helper<Serializer, Ts...>::unfoldValue(sa, value, std::make_index_sequence<sizeof...(Ts)>());
}

template <typename Serializer, typename... Ts>
inline bool load(Serializer& sa, std::variant<Ts...>& value) {
    return unfold_variant_helper<Serializer, Ts...>::unfoldValue(sa, value, std::make_index_sequence<sizeof...(Ts)>());
}

NEKO_END_NAMESPACE
