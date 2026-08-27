/**
 * @file global.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief NekoProtoTools
 *
 * @version 0.1
 * @date 2024-05-25
 *
 * @par license
 *  MIT License
 *
 * @copyright Copyright (c) 2024 by llhsdmd
 *
 */
#pragma once

#include "config.h"

#include <string>
#include <string_view>

namespace nekoproto {
template <typename>
inline constexpr bool always_false_v = false;
} // namespace nekoproto

#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
    #define NEKO_DETAIL_CPP_RAW_VER _MSVC_LANG
#else
    #define NEKO_DETAIL_CPP_RAW_VER __cplusplus
#endif
#define NEKO_DETAIL_CPP_RAW_VER_YEAR ((NEKO_DETAIL_CPP_RAW_VER / 100) % 100)
#if NEKO_DETAIL_CPP_RAW_VER >= 201103L
    #define NEKO_CPP_PLUS (NEKO_DETAIL_CPP_RAW_VER_YEAR - (NEKO_DETAIL_CPP_RAW_VER_YEAR % 3) + 2)
#else
    #define NEKO_CPP_PLUS 1
#endif

#ifdef _WIN32
#define NEKO_DECL_EXPORT __declspec(dllexport)
#define NEKO_DECL_IMPORT __declspec(dllimport)
#define NEKO_DECL_LOCAL
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define NEKO_DECL_EXPORT __attribute__((visibility("default")))
#define NEKO_DECL_IMPORT __attribute__((visibility("default")))
#define NEKO_DECL_LOCAL  __attribute__((visibility("hidden")))
#else
#define NEKO_DECL_EXPORT
#define NEKO_DECL_IMPORT
#define NEKO_DECL_LOCAL
#endif

#ifndef NEKO_PROTO_STATIC
#ifdef NEKO_PROTO_LIBRARY
#define NEKO_PROTO_API NEKO_DECL_EXPORT
#else
#define NEKO_PROTO_API NEKO_DECL_IMPORT
#endif
#else
#define NEKO_PROTO_API
#endif

#define NEKO_PP_NARG(...)  NEKO_PP_NARG_(__VA_ARGS__, NEKO_PP_RSEQ_N())
#define NEKO_PP_NARG_(...) NEKO_PP_ARG_N(__VA_ARGS__)
// clang-format off
#define NEKO_PP_ARG_N(                                  \
    _1, _2, _3, _4, _5, _6, _7, _8, _9,_10,             \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20,            \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,            \
    _31,_32,_33,_34,_35,_36,_37,_38,_39,_40,            \
    _41,_42,_43,_44,_45,_46,_47,_48,_49,_50,            \
    _51,_52,_53,_54,_55,_56,_57,_58,_59,_60,            \
    _61,_62,_63,_64,_65,_66,_67,_68,_69,_70,            \
    _71,_72,_73,_74,_75,_76,_77,_78,_79,_80,            \
    _81,_82,_83,_84,_85,_86,_87,_88,_89,_90,            \
    _91,_92,_93,_94,_95,_96,_97,_98,_99,_100,           \
    _101,_102,_103,_104,_105,_106,_107,_108,_109,_110,  \
    _111,_112,_113,_114,_115,_116,_117,_118,_119,_120,  \
    _121,_122,_123,_124,_125,_126,_127,_128,_129,N,...) N

#define NEKO_PP_RSEQ_N()                        \
    129,128,127,126,125,124,123,122,121,120,    \
    119,118,117,116,115,114,113,112,111,110,    \
    109,108,107,106,105,104,103,102,101,100,    \
    99,98,97,96,95,94,93,92,91,90,              \
    89,88,87,86,85,84,83,82,81,80,              \
    79,78,77,76,75,74,73,72,71,70,              \
    69,68,67,66,65,64,63,62,61,60,              \
    59,58,57,56,55,54,53,52,51,50,              \
    49,48,47,46,45,44,43,42,41,40,              \
    39,38,37,36,35,34,33,32,31,30,              \
    29,28,27,26,25,24,23,22,21,20,              \
    19,18,17,16,15,14,13,12,11,10,              \
    9,8,7,6,5,4,3,2,1,0

// clang-format on
#define NEKO_VA_ARGS_SIZE(...) NEKO_PP_NARG(__VA_ARGS__)
