/**
 * @file name_reflect.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-29
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "global.hpp"
#include "string_literal.hpp"

#include <array>
#ifdef __GNUC__
#include <cxxabi.h>
#include <list>
#include <map>
#include <vector>
#endif

NEKO_BEGIN_NAMESPACE
namespace detail {
#if defined(__GNUC__) || defined(__MINGW__) || defined(__clang__)
#define NEKO_PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(_WIN32)
#define NEKO_PRETTY_FUNCTION_NAME __FUNCSIG__
#else
#define NEKO_PRETTY_FUNCTION_NAME __func__
#endif

inline constexpr static int max_unwrap_struct_size = 60;

template <std::size_t N, typename T>
    requires(N <= max_unwrap_struct_size)
constexpr auto unwrap_struct_impl(T& data) noexcept {
    if constexpr (N == 0) {
        return std::forward_as_tuple();
    } else if constexpr (N == 1) {
        auto& [m1] = data;
        return std::forward_as_tuple(m1);
    } else if constexpr (N == 2) {
        auto& [m1, m2] = data;
        return std::forward_as_tuple(m1, m2);
    } else if constexpr (N == 3) {
        auto& [m1, m2, m3] = data;
        return std::forward_as_tuple(m1, m2, m3);
    } else if constexpr (N == 4) {
        auto& [m1, m2, m3, m4] = data;
        return std::forward_as_tuple(m1, m2, m3, m4);
    } else if constexpr (N == 5) {
        auto& [m1, m2, m3, m4, m5] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5);
    } else if constexpr (N == 6) {
        auto& [m1, m2, m3, m4, m5, m6] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6);
    } else if constexpr (N == 7) {
        auto& [m1, m2, m3, m4, m5, m6, m7] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7);
    } else if constexpr (N == 8) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8);
    } else if constexpr (N == 9) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9);
    } else if constexpr (N == 10) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
    } else if constexpr (N == 11) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11);
    } else if constexpr (N == 12) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12);
    } else if constexpr (N == 13) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13);
    } else if constexpr (N == 14) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14);
    } else if constexpr (N == 15) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15);
    } else if constexpr (N == 16) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16);
    } else if constexpr (N == 17) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17);
    } else if constexpr (N == 18) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18);
    } else if constexpr (N == 19) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19);
    } else if constexpr (N == 20) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20);
    } else if constexpr (N == 21) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21);
    } else if constexpr (N == 22) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22] =
            data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22);
    } else if constexpr (N == 23) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22,
               m23] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23);
    } else if constexpr (N == 24) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24);
    } else if constexpr (N == 25) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25);
    } else if constexpr (N == 26) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26);
    } else if constexpr (N == 27) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27);
    } else if constexpr (N == 28) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28);
    } else if constexpr (N == 29) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29);
    } else if constexpr (N == 30) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30);
    } else if constexpr (N == 31) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31);
    } else if constexpr (N == 32) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32);
    } else if constexpr (N == 33) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33);
    } else if constexpr (N == 34) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34);
    } else if constexpr (N == 35) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35);
    } else if constexpr (N == 36) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36);
    } else if constexpr (N == 37) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37);
    } else if constexpr (N == 38) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38);
    } else if constexpr (N == 39) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39);
    } else if constexpr (N == 40) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40);
    } else if constexpr (N == 41) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41);
    } else if constexpr (N == 42) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42);
    } else if constexpr (N == 43) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43] =
            data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43);
    } else if constexpr (N == 44) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43,
               m44] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44);
    } else if constexpr (N == 45) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45);
    } else if constexpr (N == 46) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46);
    } else if constexpr (N == 47) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47);
    } else if constexpr (N == 48) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48);
    } else if constexpr (N == 49) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49);
    } else if constexpr (N == 50) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50);
    } else if constexpr (N == 51) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51);
    } else if constexpr (N == 52) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52);
    } else if constexpr (N == 53) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53);
    } else if constexpr (N == 54) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53, m54] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53, m54);
    } else if constexpr (N == 55) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53, m54, m55] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53, m54, m55);
    } else if constexpr (N == 56) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53, m54, m55, m56);
    } else if constexpr (N == 57) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53, m54, m55, m56, m57);
    } else if constexpr (N == 58) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53, m54, m55, m56, m57, m58);
    } else if constexpr (N == 59) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53, m54, m55, m56, m57, m58, m59);
    } else if constexpr (N == 60) {
        auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
               m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
               m45, m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60] = data;
        return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18,
                                     m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34,
                                     m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50,
                                     m51, m52, m53, m54, m55, m56, m57, m58, m59, m60);
    }
}

struct any_type {
    template <typename T>
    operator T() {}
};
template <typename T, typename _Cond = void, typename... Args>
struct can_aggregate_impl : std::false_type {};
template <typename T, typename... Args>
struct can_aggregate_impl<T, std::void_t<decltype(T{std::declval<Args>()...})>, Args...> : std::true_type {};
template <typename T, typename... Args>
struct can_aggregate : can_aggregate_impl<T, void, Args...> {};

/**
 * @brief Get the struct size at compile time
 *
 * @tparam T
 * @return the size of the
 */
template <typename T, typename... Args>
constexpr auto member_count([[maybe_unused]] Args&&... args) noexcept {
    if constexpr (!can_aggregate<T, any_type, Args...>::value) {
        return sizeof...(args);
    } else {
        return member_count<T>(std::forward<Args>(args)..., any_type{});
    }
}

template <typename T>
static constexpr size_t member_count_v = member_count<T>();

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
static constexpr bool can_unwrap_v = std::is_aggregate_v<std::remove_cv_t<T>> && !is_std_array<T>::value;

/**
 * @brief Convert the struct reference to tuple
 *
 * @tparam T
 * @param data
 * @return constexpr auto
 */
template <typename T>
constexpr auto unwrap_struct(T& data) noexcept {
    static_assert(can_unwrap_v<T>, "The struct must be aggregate");
    static_assert(member_count_v<T> > 0, "The struct must have at least one member");
    static_assert(member_count_v<T> <= 60, "The struct must have at most 60 members");
    return unwrap_struct_impl<member_count_v<T>>(data);
}

template <class T>
inline static T external;

template <class T>
struct ptr_t final { // NOLINT
    const T* ptr;
};

template <size_t N, class T>
constexpr auto get_ptr(T&& dt) noexcept {
    auto& val = get<N>(unwrap_struct(dt));
    return ptr_t<std::remove_cvref_t<decltype(val)>>{&val};
}

template <auto Ptr>
[[nodiscard]] consteval auto mangled_name() {
    return NEKO_PRETTY_FUNCTION_NAME;
}

template <class T>
[[nodiscard]] consteval auto mangled_name() {
    return NEKO_PRETTY_FUNCTION_NAME;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
template <auto N, class T>
constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#pragma clang diagnostic pop
#elif __GNUC__
template <auto N, class T>
constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#else
template <auto N, class T>
constexpr std::string_view get_name_impl = mangled_name<get_ptr<N>(external<std::remove_volatile_t<T>>)>();
#endif

struct NekoReflector {
    int nekoField;
};

struct reflect_type {                                                                                  // NOLINT
    static constexpr std::string_view name = mangled_name<NekoReflector>();                            // NOLINT
    static constexpr auto end = name.substr(name.find("NekoReflector") + sizeof("NekoReflector") - 1); // NOLINT
#if defined(__GNUC__) || defined(__clang__)
    static constexpr auto begin = std::string_view{"T = "}; // NOLINT
#else
    static constexpr auto begin = std::string_view{"mangled_name<"};
#endif
};

struct reflect_field {                                                                           // NOLINT
    static constexpr auto name  = get_name_impl<0, NekoReflector>;                               // NOLINT
    static constexpr auto end   = name.substr(name.find("nekoField") + sizeof("nekoField") - 1); // NOLINT
    static constexpr auto begin = name[name.find("nekoField") - 1];                              // NOLINT
};

template <std::size_t N, class T>
struct member_nameof_impl {                                                                  // NOLINT
    static constexpr auto name     = get_name_impl<N, T>;                                    // NOLINT
    static constexpr auto begin    = name.find(reflect_field::end);                          // NOLINT
    static constexpr auto tmp      = name.substr(0, begin);                                  // NOLINT
    static constexpr auto stripped = tmp.substr(tmp.find_last_of(reflect_field::begin) + 1); // NOLINT

    static constexpr std::string_view stripped_literal = join_v<stripped>; // NOLINT
};

template <const std::string_view& str>
constexpr std::string_view parser_class_name_with_type() {
    std::string_view string;
    string = str.find_last_of(' ') == std::string_view::npos ? str : str.substr(str.find_last_of(' ') + 1);
    string = string.find_last_of(':') == std::string_view::npos ? string : string.substr(string.find_last_of(':') + 1);
    return string;
}

template <class T>
struct class_nameof_impl {                                                     // NOLINT
    static constexpr std::string_view name     = mangled_name<T>();            // NOLINT
    static constexpr auto begin                = name.find(reflect_type::end); // NOLINT
    static constexpr auto tmp                  = name.substr(0, begin);        // NOLINT
    static constexpr auto class_name_with_type =                               // NOLINT
        tmp.substr(tmp.find(reflect_type::begin) + reflect_type::begin.size());
    static constexpr auto stripped = // NOLINT
        parser_class_name_with_type<class_name_with_type>();

    static constexpr std::string_view stripped_literal = join_v<stripped>; // NOLINT
};

template <std::size_t N, class T>
inline constexpr auto member_nameof = []() constexpr { return member_nameof_impl<N, T>::stripped_literal; }(); // NOLINT

template <class T>
inline constexpr auto class_nameof = []() constexpr { return class_nameof_impl<T>::stripped_literal; }(); // NOLINT

template <class T, std::size_t... Is>
[[nodiscard]] constexpr auto member_names_impl(std::index_sequence<Is...> /*unused*/) {
    if constexpr (sizeof...(Is) == 0) {
        return std::array<std::string_view, 0>{};
    } else {
        return std::array{member_nameof<Is, T>...};
    }
}

/// ====================== enum string =========================
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#endif
#ifndef NEKO_ENUM_SEARCH_DEPTH
#define NEKO_ENUM_SEARCH_DEPTH 60
#endif
#if defined(__GNUC__) || defined(__MINGW__) || defined(__clang__)
template <typename T, T Value>
constexpr auto neko_get_enum_name() noexcept {
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value = MyValues]
    // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value =
    // (MyEnum)114514]"
    std::string_view name(__PRETTY_FUNCTION__);
    std::size_t eqBegin   = name.find_last_of(' ');
    std::size_t end       = name.find_last_of(']');
    std::string_view body = name.substr(eqBegin + 1, end - eqBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
#elif defined(_MSC_VER)
#define NEKO_ENUM_TO_NAME(enumType)
template <typename T, T Value>
constexpr auto neko_get_enum_name() noexcept {
    // auto __cdecl _Neko_GetEnumName<enum main::MyEnum,(enum
    // main::MyEnum)0x2>(void) auto __cdecl _Neko_GetEnumName<enum
    // main::MyEnum,main::MyEnum::Wtf>(void)
    std::string_view name(__FUNCSIG__);
    std::size_t dotBegin  = name.find_first_of(',');
    std::size_t end       = name.find_last_of('>');
    std::string_view body = name.substr(dotBegin + 1, end - dotBegin - 1);
    if (body[0] == '(') {
        // Failed
        return std::string_view();
    }
    return body;
}
#else
template <typename T, T Value>
constexpr auto neko_get_enum_name() noexcept {
    // Unsupported
    return std::string_view();
}
#endif
template <typename T, T Value>
constexpr bool neko_is_valid_enum() noexcept {
    return !neko_get_enum_name<T, Value>().empty();
}
template <typename T, std::size_t... N>
constexpr std::size_t neko_get_valid_enum_count(std::index_sequence<N...> /*unused*/) noexcept {
    return (... + neko_is_valid_enum<T, T(N)>());
}
template <typename T, std::size_t... N>
constexpr auto neko_get_valid_enum_names(std::index_sequence<N...> seq) noexcept {
    constexpr auto ValidCount = neko_get_valid_enum_count<T>(seq);

    std::array<std::pair<T, std::string_view>, ValidCount> arr;
    std::string_view vstr[sizeof...(N)]{neko_get_enum_name<T, T(N)>()...};

    std::size_t ns   = 0;
    std::size_t left = ValidCount;
    auto iter        = arr.begin();

    for (auto idx : vstr) {
        if (!idx.empty()) {
            // Valid name
            iter->first  = T(ns);
            iter->second = idx;
            ++iter;
        }
        if (left == 0) {
            break;
        }

        ns += 1;
    }
    return arr;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
} // namespace detail
NEKO_END_NAMESPACE