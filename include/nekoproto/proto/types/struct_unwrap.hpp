/**
 * @file struct_unwrap.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024 llhsdmd BusyStudent
 *
 */
#pragma once
#include <cstring>

#include "../json_serializer.hpp"
#include "../private/global.hpp"
#include "../private/helpers.hpp"
#include "../serializer_base.hpp"

#if NEKO_CPP_PLUS >= 17
NEKO_BEGIN_NAMESPACE

/// ====================== struct to array =====================
namespace detail {
template <size_t N>
struct size {
    static constexpr auto value = N;
};

// Unwrap here
// N = 0 (虽然你没要求，但通常会包含空结构体的情况)
template <typename T>
constexpr auto unwrap_struct_impl(T& /*data*/, size<0> /*unused*/) noexcept {
    return std::forward_as_tuple(); // 返回空元组
}

// N = 1
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<1> /*unused*/) noexcept {
    auto& [m1] = data;
    return std::forward_as_tuple(m1);
}

// N = 2
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<2> /*unused*/) noexcept {
    auto& [m1, m2] = data;
    return std::forward_as_tuple(m1, m2);
}

// N = 3
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<3> /*unused*/) noexcept {
    auto& [m1, m2, m3] = data;
    return std::forward_as_tuple(m1, m2, m3);
}

// N = 4
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<4> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4] = data;
    return std::forward_as_tuple(m1, m2, m3, m4);
}

// N = 5
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<5> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5);
}

// N = 6
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<6> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6);
}

// N = 7
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<7> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7);
}

// N = 8
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<8> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8);
}

// N = 9
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<9> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9);
}

// N = 10
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<10> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
}

// N = 11
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<11> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11);
}

// N = 12
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<12> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12);
}

// N = 13
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<13> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13);
}

// N = 14
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<14> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14);
}

// N = 15
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<15> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15);
}

// N = 16
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<16> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16);
}

// N = 17
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<17> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17);
}

// N = 18
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<18> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18);
}

// N = 19
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<19> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19);
}

// N = 20
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<20> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20);
}

// ... (继续这个模式直到 N = 60) ...

// N = 21
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<21> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21);
}

// N = 22
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<22> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22);
}

// N = 23
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<23> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23] =
        data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23);
}

// N = 24
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<24> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24);
}

// N = 25
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<25> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25);
}

// N = 26
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<26> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26);
}

// N = 27
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<27> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27);
}

// N = 28
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<28> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28);
}

// N = 29
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<29> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29);
}

// N = 30
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<30> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30);
}

// N = 31
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<31> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31);
}

// N = 32
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<32> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32);
}

// N = 33
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<33> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33);
}

// N = 34
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<34> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34);
}

// N = 35
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<35> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35);
}

// N = 36
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<36> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36);
}

// N = 37
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<37> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37);
}

// N = 38
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<38> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38);
}

// N = 39
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<39> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39);
}

// N = 40
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<40> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40);
}

// N = 41
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<41> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41);
}

// N = 42
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<42> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42);
}

// N = 43
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<43> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43);
}

// N = 44
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<44> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44] =
        data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44);
}

// N = 45
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<45> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44,
           m45] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45);
}

// N = 46
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<46> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46);
}

// N = 47
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<47> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47);
}

// N = 48
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<48> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48);
}

// N = 49
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<49> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49);
}

// N = 50
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<50> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50);
}

// N = 51
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<51> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51);
}

// N = 52
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<52> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52);
}

// N = 53
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<53> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53);
}

// N = 54
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<54> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54);
}

// N = 55
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<55> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55);
}

// N = 56
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<56> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56);
}

// N = 57
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<57> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57);
}

// N = 58
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<58> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58);
}

// N = 59
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<59> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59);
}

// N = 60
template <typename T>
constexpr auto unwrap_struct_impl(T& data, size<60> /*unused*/) noexcept {
    auto& [m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23,
           m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36, m37, m38, m39, m40, m41, m42, m43, m44, m45,
           m46, m47, m48, m49, m50, m51, m52, m53, m54, m55, m56, m57, m58, m59, m60] = data;
    return std::forward_as_tuple(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19,
                                 m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32, m33, m34, m35, m36,
                                 m37, m38, m39, m40, m41, m42, m43, m44, m45, m46, m47, m48, m49, m50, m51, m52, m53,
                                 m54, m55, m56, m57, m58, m59, m60);
}

struct any_type {
    template <typename T>
    operator T() {}
};
template <typename T, typename _Cond = void, typename... Args>
struct can_aggregate_impl : std::false_type {};
template <typename T, typename... Args>
struct can_aggregate_impl<T, std::void_t<decltype(T{{std::declval<Args>()}...})>, Args...> : std::true_type {};
template <typename T, typename... Args>
struct can_aggregate : can_aggregate_impl<T, void, Args...> {};

/**
 * @brief Get the struct size at compile time
 *
 * @tparam T
 * @return the size of the
 */
template <typename T, typename... Args>
constexpr auto tuple_size(Args&&... args) noexcept {
    if constexpr (!can_aggregate<T, Args...>::value) {
        return sizeof...(args) - 1;
    } else {
        return tuple_size<T>(std::forward<Args>(args)..., any_type{});
    }
}

template <typename T>
constexpr size_t tuple_size_v = tuple_size<T>();

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
constexpr bool can_unwrap_v = std::is_aggregate_v<std::remove_cv_t<T>> && !is_std_array<T>::value;

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
    return unwrap_struct_impl(data, size<tuple_size_v<T>>{});
}

template <typename SerializerT, typename... Args>
static bool unfold_unwrap(SerializerT& sa, std::tuple<Args...>&& tp) {
    return std::apply([&](Args&&... args) { return sa(args...); }, std::forward<std::tuple<Args...>>(tp));
}
} // namespace detail

template <typename SerializerT, typename T,
          traits::enable_if_t<detail::can_unwrap_v<T>, !traits::has_method_const_save<T, SerializerT>::value,
                              !traits::has_method_const_serialize<T, SerializerT>::value,
                              !traits::has_method_serialize<T, SerializerT>::value> = traits::default_value_for_enable>
inline bool save(SerializerT& sa, const T& value) {
    sa.startArray(std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value);
    auto ret = detail::unfold_unwrap(sa, detail::unwrap_struct(value));
    sa.endArray();
    return ret;
}

template <typename SerializerT, typename T,
          traits::enable_if_t<detail::can_unwrap_v<T>, !traits::has_method_load<T, SerializerT>::value,
                              !traits::has_method_deserialize<T, SerializerT>::value,
                              !traits::has_method_serialize<T, SerializerT>::value> = traits::default_value_for_enable>
inline bool load(SerializerT& sa, T& value) {
    uint32_t s;
    sa(make_size_tag(s));
    if (s != std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value) {
        NEKO_LOG_ERROR("proto", "struct size mismatch: json object size {} != struct size {}", s,
                       std::tuple_size<decltype(detail::unwrap_struct(std::declval<T&>()))>::value);
        return false;
    }
    return detail::unfold_unwrap(sa, detail::unwrap_struct(value));
}

NEKO_END_NAMESPACE
/// ====================== end =================================
#endif