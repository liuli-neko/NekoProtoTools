#pragma once

#include "../global.hpp"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

inline constexpr std::size_t max_unwrap_struct_size = 128;

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 0>, T& /*data*/) noexcept {
    return std::tuple<>{};
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 1>, T& data) noexcept {
    auto& [m1] = data;
    return std::forward_as_tuple(m1);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 2>, T& data) noexcept {
    auto& [m1, m2] = data;
    return std::forward_as_tuple(m1, m2);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 3>, T& data) noexcept {
    auto& [m1, m2, m3] = data;
    return std::forward_as_tuple(m1, m2, m3);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 4>, T& data) noexcept {
    auto& [m1, m2, m3, m4] = data;
    return std::forward_as_tuple(m1, m2, m3, m4);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 5>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 6>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 7>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 8>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 9>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 10>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 11>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 12>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 13>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 14>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 15>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 16>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 17>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 18>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 19>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 20>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 21>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 22>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 23>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23] =
        data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 24>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 25>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 26>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 27>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 28>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 29>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 30>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 31>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 32>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 33>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 34>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 35>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 36>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 37>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 38>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 39>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 40>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 41>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 42>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 43>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 44>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44] =
        data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 45>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
           m45] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 46>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 47>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 48>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 49>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 50>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 51>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 52>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 53>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 54>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 55>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 56>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 57>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 58>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 59>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 60>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 61>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 62>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 63>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 64>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 65>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 66>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66] =
        data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 67>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66,
           m67] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 68>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 69>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 70>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 71>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 72>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 73>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 74>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 75>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 76>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 77>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 78>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 79>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 80>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 81>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 82>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 83>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 84>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 85>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 86>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 87>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 88>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88] =
        data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 89>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88,
           m89] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 90>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 91>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 92>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 93>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 94>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 95>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 96>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 97>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 98>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 99>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 100>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 101>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 102>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 103>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 104>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 105>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 106>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 107>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 108>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108] =
        data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 109>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108,
           m109] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 110>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 111>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103,
                                 m104, m105, m106, m107, m108, m109, m110, m111);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 112>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103,
                                 m104, m105, m106, m107, m108, m109, m110, m111, m112);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 113>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103,
                                 m104, m105, m106, m107, m108, m109, m110, m111, m112, m113);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 114>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103,
                                 m104, m105, m106, m107, m108, m109, m110, m111, m112, m113, m114);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 115>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103,
                                 m104, m105, m106, m107, m108, m109, m110, m111, m112, m113, m114, m115);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 116>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103,
                                 m104, m105, m106, m107, m108, m109, m110, m111, m112, m113, m114, m115, m116);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 117>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68, m69, m70,
                                 m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87,
                                 m88, m89, m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103,
                                 m104, m105, m106, m107, m108, m109, m110, m111, m112, m113, m114, m115, m116, m117);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 118>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 119>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 120>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 121>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 122>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 123>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 124>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 125>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 126>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125, m126] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125, m126);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 127>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125, m126, m127] =
        data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125, m126, m127);
}

template <typename T>
constexpr auto unwrap_struct_from_size(std::integral_constant<std::size_t, 128>, T& data) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67,
           m68, m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89,
           m90, m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109,
           m110, m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125, m126, m127,
           m128] = data;
    return std::forward_as_tuple(
        m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24,
        m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46,
        m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60, m61, m62, m63, m64, m65, m66, m67, m68,
        m69, m70, m71, m72, m73, m74, m75, m76, m77, m78, m79, m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90,
        m91, m92, m93, m94, m95, m96, m97, m98, m99, m100, m101, m102, m103, m104, m105, m106, m107, m108, m109, m110,
        m111, m112, m113, m114, m115, m116, m117, m118, m119, m120, m121, m122, m123, m124, m125, m126, m127, m128);
}

template <std::size_t N, typename T>
    requires(N <= max_unwrap_struct_size)
constexpr auto unwrap_struct_impl(T& data) noexcept {
    return unwrap_struct_from_size(std::integral_constant<std::size_t, N>{}, data);
}

} // namespace detail
NEKO_END_NAMESPACE
