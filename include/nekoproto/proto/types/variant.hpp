/**
 * @file variant.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include "../private/global.hpp"

#if NEKO_CPP_PLUS >= 17
#include <variant>

#include "../serializer_base.hpp"
NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename... Ts>
struct unfold_variant_helper { // NOLINT(readability-identifier-naming)
    template <typename T, size_t N>
    static bool unfoldValueImp1(Serializer& sa, std::variant<Ts...>& value) {
        T tvalue = {};
        if (sa(tvalue)) {
            value = tvalue;
            return true;
        }
        return false;
    }

    template <size_t... Ns>
    static bool unfoldValue(Serializer& sa, std::variant<Ts...>& value, std::index_sequence<Ns...> /*unused*/) {
        return (unfoldValueImp1<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(sa, value) || ...);
    }
    template <typename T, size_t N>
    static bool unfoldValueImp2(Serializer& sa, const std::variant<Ts...>& value) {
        if (value.index() != N) {
            return false;
        }
        return sa(std::get<N>(value));
    }

    template <size_t... Ns>
    static bool unfoldValue(Serializer& sa, const std::variant<Ts...>& value, std::index_sequence<Ns...> /*unused*/) {
        return (unfoldValueImp2<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(sa, value) || ...);
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

template <typename... Ts>
struct is_minimal_serializable<std::variant<Ts...>, void> : std::true_type {};

NEKO_END_NAMESPACE
#endif