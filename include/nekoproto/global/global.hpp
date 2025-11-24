/**
 * @file global.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief NekoProtoTools
 *
 * @version 0.1
 * @date 2024-05-25
 *
 * @par license
 *  GPL-2.0 license
 *
 * @copyright Copyright (c) 2024 by llhsdmd
 *
 */
#pragma once

#include "config.h"

#ifndef NEKO_NAMESPACE
#define NEKO_NAMESPACE NekoProto
#endif

#define NEKO_BEGIN_NAMESPACE namespace NEKO_NAMESPACE {
#define NEKO_END_NAMESPACE   }
#define NEKO_USE_NAMESPACE   using namespace NEKO_NAMESPACE;
#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
    #define _NEKO_CPP_RAW_VER _MSVC_LANG
#else
    #define _NEKO_CPP_RAW_VER __cplusplus
#endif
#if _NEKO_CPP_RAW_VER >= 201103L
    #define NEKO_CPP_PLUS ((_NEKO_CPP_RAW_VER / 100) % 100)
#else
    #define NEKO_CPP_PLUS 10
#endif

#if defined(__GNUC__) || defined(__MINGW32__)
#define NEKO_USED [[gnu::used]]
#elif defined(_WIN32)
#define NEKO_USED
#endif
#if NEKO_CPP_PLUS >= 20
#define NEKO_IF_LIKELY   [[likely]]
#define NEKO_IF_UNLIKELY [[unlikely]]
#else
#define NEKO_IF_IS_LIKELY
#define NEKO_IF_UNLIKELY
#endif
#if NEKO_CPP_PLUS >= 17
#include <string>
#define NEKO_CONSTEXPR_FUNC         constexpr
#define NEKO_CONSTEXPR_IF           if constexpr
#define NEKO_CONSTEXPR_VAR          constexpr
#define NEKO_STRING_VIEW            std::string_view
#define NEKO_MAKE_UNIQUE(type, ...) std::make_unique<type>(__VA_ARGS__)
#define NEKO_NOEXCEPT               noexcept
#else
#include <string>
#define NEKO_CONSTEXPR_FUNC
#define NEKO_CONSTEXPR_IF           if
#define NEKO_CONSTEXPR_VAR          constexpr
#define NEKO_STRING_VIEW            std::string
#define NEKO_MAKE_UNIQUE(type, ...) std::unique_ptr<type>(new type(__VA_ARGS__))
#define NEKO_NOEXCEPT               noexcept
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
// 辅助宏解决 MSVC 预处理器的一个 bug
#define NEKO_VA_ARGS_SIZE_HELPER(x) x
#define NEKO_VA_ARGS_SIZE(...)      NEKO_VA_ARGS_SIZE_HELPER(NEKO_PP_NARG(__VA_ARGS__))

#define NEKO_PP_SEP_EMPTY()
#define NEKO_PP_SEP_COMMA()             ,
#define NEKO_PP_PASTE_IMPL(a, b)        a##b
#define NEKO_PP_PASTE(a, b)             NEKO_PP_PASTE_IMPL(a, b)
#define NEKO_PP_LIST_1(var, s, macro)   macro(var, 1)
#define NEKO_PP_LIST_2(var, s, macro)   NEKO_PP_LIST_1(var, s, macro) s() macro(var, 2)
#define NEKO_PP_LIST_3(var, s, macro)   NEKO_PP_LIST_2(var, s, macro) s() macro(var, 3)
#define NEKO_PP_LIST_4(var, s, macro)   NEKO_PP_LIST_3(var, s, macro) s() macro(var, 4)
#define NEKO_PP_LIST_5(var, s, macro)   NEKO_PP_LIST_4(var, s, macro) s() macro(var, 5)
#define NEKO_PP_LIST_6(var, s, macro)   NEKO_PP_LIST_5(var, s, macro) s() macro(var, 6)
#define NEKO_PP_LIST_7(var, s, macro)   NEKO_PP_LIST_6(var, s, macro) s() macro(var, 7)
#define NEKO_PP_LIST_8(var, s, macro)   NEKO_PP_LIST_7(var, s, macro) s() macro(var, 8)
#define NEKO_PP_LIST_9(var, s, macro)   NEKO_PP_LIST_8(var, s, macro) s() macro(var, 9)
#define NEKO_PP_LIST_10(var, s, macro)  NEKO_PP_LIST_9(var, s, macro) s() macro(var, 10)
#define NEKO_PP_LIST_11(var, s, macro)  NEKO_PP_LIST_10(var, s, macro) s() macro(var, 11)
#define NEKO_PP_LIST_12(var, s, macro)  NEKO_PP_LIST_11(var, s, macro) s() macro(var, 12)
#define NEKO_PP_LIST_13(var, s, macro)  NEKO_PP_LIST_12(var, s, macro) s() macro(var, 13)
#define NEKO_PP_LIST_14(var, s, macro)  NEKO_PP_LIST_13(var, s, macro) s() macro(var, 14)
#define NEKO_PP_LIST_15(var, s, macro)  NEKO_PP_LIST_14(var, s, macro) s() macro(var, 15)
#define NEKO_PP_LIST_16(var, s, macro)  NEKO_PP_LIST_15(var, s, macro) s() macro(var, 16)
#define NEKO_PP_LIST_17(var, s, macro)  NEKO_PP_LIST_16(var, s, macro) s() macro(var, 17)
#define NEKO_PP_LIST_18(var, s, macro)  NEKO_PP_LIST_17(var, s, macro) s() macro(var, 18)
#define NEKO_PP_LIST_19(var, s, macro)  NEKO_PP_LIST_18(var, s, macro) s() macro(var, 19)
#define NEKO_PP_LIST_20(var, s, macro)  NEKO_PP_LIST_19(var, s, macro) s() macro(var, 20)
#define NEKO_PP_LIST_21(var, s, macro)  NEKO_PP_LIST_20(var, s, macro) s() macro(var, 21)
#define NEKO_PP_LIST_22(var, s, macro)  NEKO_PP_LIST_21(var, s, macro) s() macro(var, 22)
#define NEKO_PP_LIST_23(var, s, macro)  NEKO_PP_LIST_22(var, s, macro) s() macro(var, 23)
#define NEKO_PP_LIST_24(var, s, macro)  NEKO_PP_LIST_23(var, s, macro) s() macro(var, 24)
#define NEKO_PP_LIST_25(var, s, macro)  NEKO_PP_LIST_24(var, s, macro) s() macro(var, 25)
#define NEKO_PP_LIST_26(var, s, macro)  NEKO_PP_LIST_25(var, s, macro) s() macro(var, 26)
#define NEKO_PP_LIST_27(var, s, macro)  NEKO_PP_LIST_26(var, s, macro) s() macro(var, 27)
#define NEKO_PP_LIST_28(var, s, macro)  NEKO_PP_LIST_27(var, s, macro) s() macro(var, 28)
#define NEKO_PP_LIST_29(var, s, macro)  NEKO_PP_LIST_28(var, s, macro) s() macro(var, 29)
#define NEKO_PP_LIST_30(var, s, macro)  NEKO_PP_LIST_29(var, s, macro) s() macro(var, 30)
#define NEKO_PP_LIST_31(var, s, macro)  NEKO_PP_LIST_30(var, s, macro) s() macro(var, 31)
#define NEKO_PP_LIST_32(var, s, macro)  NEKO_PP_LIST_31(var, s, macro) s() macro(var, 32)
#define NEKO_PP_LIST_33(var, s, macro)  NEKO_PP_LIST_32(var, s, macro) s() macro(var, 33)
#define NEKO_PP_LIST_34(var, s, macro)  NEKO_PP_LIST_33(var, s, macro) s() macro(var, 34)
#define NEKO_PP_LIST_35(var, s, macro)  NEKO_PP_LIST_34(var, s, macro) s() macro(var, 35)
#define NEKO_PP_LIST_36(var, s, macro)  NEKO_PP_LIST_35(var, s, macro) s() macro(var, 36)
#define NEKO_PP_LIST_37(var, s, macro)  NEKO_PP_LIST_36(var, s, macro) s() macro(var, 37)
#define NEKO_PP_LIST_38(var, s, macro)  NEKO_PP_LIST_37(var, s, macro) s() macro(var, 38)
#define NEKO_PP_LIST_39(var, s, macro)  NEKO_PP_LIST_38(var, s, macro) s() macro(var, 39)
#define NEKO_PP_LIST_40(var, s, macro)  NEKO_PP_LIST_39(var, s, macro) s() macro(var, 40)
#define NEKO_PP_LIST_41(var, s, macro)  NEKO_PP_LIST_40(var, s, macro) s() macro(var, 41)
#define NEKO_PP_LIST_42(var, s, macro)  NEKO_PP_LIST_41(var, s, macro) s() macro(var, 42)
#define NEKO_PP_LIST_43(var, s, macro)  NEKO_PP_LIST_42(var, s, macro) s() macro(var, 43)
#define NEKO_PP_LIST_44(var, s, macro)  NEKO_PP_LIST_43(var, s, macro) s() macro(var, 44)
#define NEKO_PP_LIST_45(var, s, macro)  NEKO_PP_LIST_44(var, s, macro) s() macro(var, 45)
#define NEKO_PP_LIST_46(var, s, macro)  NEKO_PP_LIST_45(var, s, macro) s() macro(var, 46)
#define NEKO_PP_LIST_47(var, s, macro)  NEKO_PP_LIST_46(var, s, macro) s() macro(var, 47)
#define NEKO_PP_LIST_48(var, s, macro)  NEKO_PP_LIST_47(var, s, macro) s() macro(var, 48)
#define NEKO_PP_LIST_49(var, s, macro)  NEKO_PP_LIST_48(var, s, macro) s() macro(var, 49)
#define NEKO_PP_LIST_50(var, s, macro)  NEKO_PP_LIST_49(var, s, macro) s() macro(var, 50)
#define NEKO_PP_LIST_51(var, s, macro)  NEKO_PP_LIST_50(var, s, macro) s() macro(var, 51)
#define NEKO_PP_LIST_52(var, s, macro)  NEKO_PP_LIST_51(var, s, macro) s() macro(var, 52)
#define NEKO_PP_LIST_53(var, s, macro)  NEKO_PP_LIST_52(var, s, macro) s() macro(var, 53)
#define NEKO_PP_LIST_54(var, s, macro)  NEKO_PP_LIST_53(var, s, macro) s() macro(var, 54)
#define NEKO_PP_LIST_55(var, s, macro)  NEKO_PP_LIST_54(var, s, macro) s() macro(var, 55)
#define NEKO_PP_LIST_56(var, s, macro)  NEKO_PP_LIST_55(var, s, macro) s() macro(var, 56)
#define NEKO_PP_LIST_57(var, s, macro)  NEKO_PP_LIST_56(var, s, macro) s() macro(var, 57)
#define NEKO_PP_LIST_58(var, s, macro)  NEKO_PP_LIST_57(var, s, macro) s() macro(var, 58)
#define NEKO_PP_LIST_59(var, s, macro)  NEKO_PP_LIST_58(var, s, macro) s() macro(var, 59)
#define NEKO_PP_LIST_60(var, s, macro)  NEKO_PP_LIST_59(var, s, macro) s() macro(var, 60)
#define NEKO_PP_LIST_61(var, s, macro)  NEKO_PP_LIST_60(var, s, macro) s() macro(var, 61)
#define NEKO_PP_LIST_62(var, s, macro)  NEKO_PP_LIST_61(var, s, macro) s() macro(var, 62)
#define NEKO_PP_LIST_63(var, s, macro)  NEKO_PP_LIST_62(var, s, macro) s() macro(var, 63)
#define NEKO_PP_LIST_64(var, s, macro)  NEKO_PP_LIST_63(var, s, macro) s() macro(var, 64)
#define NEKO_PP_LIST_65(var, s, macro)  NEKO_PP_LIST_64(var, s, macro) s() macro(var, 65)
#define NEKO_PP_LIST_66(var, s, macro)  NEKO_PP_LIST_65(var, s, macro) s() macro(var, 66)
#define NEKO_PP_LIST_67(var, s, macro)  NEKO_PP_LIST_66(var, s, macro) s() macro(var, 67)
#define NEKO_PP_LIST_68(var, s, macro)  NEKO_PP_LIST_67(var, s, macro) s() macro(var, 68)
#define NEKO_PP_LIST_69(var, s, macro)  NEKO_PP_LIST_68(var, s, macro) s() macro(var, 69)
#define NEKO_PP_LIST_70(var, s, macro)  NEKO_PP_LIST_69(var, s, macro) s() macro(var, 70)
#define NEKO_PP_LIST_71(var, s, macro)  NEKO_PP_LIST_70(var, s, macro) s() macro(var, 71)
#define NEKO_PP_LIST_72(var, s, macro)  NEKO_PP_LIST_71(var, s, macro) s() macro(var, 72)
#define NEKO_PP_LIST_73(var, s, macro)  NEKO_PP_LIST_72(var, s, macro) s() macro(var, 73)
#define NEKO_PP_LIST_74(var, s, macro)  NEKO_PP_LIST_73(var, s, macro) s() macro(var, 74)
#define NEKO_PP_LIST_75(var, s, macro)  NEKO_PP_LIST_74(var, s, macro) s() macro(var, 75)
#define NEKO_PP_LIST_76(var, s, macro)  NEKO_PP_LIST_75(var, s, macro) s() macro(var, 76)
#define NEKO_PP_LIST_77(var, s, macro)  NEKO_PP_LIST_76(var, s, macro) s() macro(var, 77)
#define NEKO_PP_LIST_78(var, s, macro)  NEKO_PP_LIST_77(var, s, macro) s() macro(var, 78)
#define NEKO_PP_LIST_79(var, s, macro)  NEKO_PP_LIST_78(var, s, macro) s() macro(var, 79)
#define NEKO_PP_LIST_80(var, s, macro)  NEKO_PP_LIST_79(var, s, macro) s() macro(var, 80)
#define NEKO_PP_LIST_81(var, s, macro)  NEKO_PP_LIST_80(var, s, macro) s() macro(var, 81)
#define NEKO_PP_LIST_82(var, s, macro)  NEKO_PP_LIST_81(var, s, macro) s() macro(var, 82)
#define NEKO_PP_LIST_83(var, s, macro)  NEKO_PP_LIST_82(var, s, macro) s() macro(var, 83)
#define NEKO_PP_LIST_84(var, s, macro)  NEKO_PP_LIST_83(var, s, macro) s() macro(var, 84)
#define NEKO_PP_LIST_85(var, s, macro)  NEKO_PP_LIST_84(var, s, macro) s() macro(var, 85)
#define NEKO_PP_LIST_86(var, s, macro)  NEKO_PP_LIST_85(var, s, macro) s() macro(var, 86)
#define NEKO_PP_LIST_87(var, s, macro)  NEKO_PP_LIST_86(var, s, macro) s() macro(var, 87)
#define NEKO_PP_LIST_88(var, s, macro)  NEKO_PP_LIST_87(var, s, macro) s() macro(var, 88)
#define NEKO_PP_LIST_89(var, s, macro)  NEKO_PP_LIST_88(var, s, macro) s() macro(var, 89)
#define NEKO_PP_LIST_90(var, s, macro)  NEKO_PP_LIST_89(var, s, macro) s() macro(var, 90)
#define NEKO_PP_LIST_91(var, s, macro)  NEKO_PP_LIST_90(var, s, macro) s() macro(var, 91)
#define NEKO_PP_LIST_92(var, s, macro)  NEKO_PP_LIST_91(var, s, macro) s() macro(var, 92)
#define NEKO_PP_LIST_93(var, s, macro)  NEKO_PP_LIST_92(var, s, macro) s() macro(var, 93)
#define NEKO_PP_LIST_94(var, s, macro)  NEKO_PP_LIST_93(var, s, macro) s() macro(var, 94)
#define NEKO_PP_LIST_95(var, s, macro)  NEKO_PP_LIST_94(var, s, macro) s() macro(var, 95)
#define NEKO_PP_LIST_96(var, s, macro)  NEKO_PP_LIST_95(var, s, macro) s() macro(var, 96)
#define NEKO_PP_LIST_97(var, s, macro)  NEKO_PP_LIST_96(var, s, macro) s() macro(var, 97)
#define NEKO_PP_LIST_98(var, s, macro)  NEKO_PP_LIST_97(var, s, macro) s() macro(var, 98)
#define NEKO_PP_LIST_99(var, s, macro)  NEKO_PP_LIST_98(var, s, macro) s() macro(var, 99)
#define NEKO_PP_LIST_100(var, s, macro) NEKO_PP_LIST_99(var, s, macro) s() macro(var, 100)
#define NEKO_PP_LIST_101(var, s, macro) NEKO_PP_LIST_100(var, s, macro) s() macro(var, 101)
#define NEKO_PP_LIST_102(var, s, macro) NEKO_PP_LIST_101(var, s, macro) s() macro(var, 102)
#define NEKO_PP_LIST_103(var, s, macro) NEKO_PP_LIST_102(var, s, macro) s() macro(var, 103)
#define NEKO_PP_LIST_104(var, s, macro) NEKO_PP_LIST_103(var, s, macro) s() macro(var, 104)
#define NEKO_PP_LIST_105(var, s, macro) NEKO_PP_LIST_104(var, s, macro) s() macro(var, 105)
#define NEKO_PP_LIST_106(var, s, macro) NEKO_PP_LIST_105(var, s, macro) s() macro(var, 106)
#define NEKO_PP_LIST_107(var, s, macro) NEKO_PP_LIST_106(var, s, macro) s() macro(var, 107)
#define NEKO_PP_LIST_108(var, s, macro) NEKO_PP_LIST_107(var, s, macro) s() macro(var, 108)
#define NEKO_PP_LIST_109(var, s, macro) NEKO_PP_LIST_108(var, s, macro) s() macro(var, 109)
#define NEKO_PP_LIST_110(var, s, macro) NEKO_PP_LIST_109(var, s, macro) s() macro(var, 110)
#define NEKO_PP_LIST_111(var, s, macro) NEKO_PP_LIST_110(var, s, macro) s() macro(var, 111)
#define NEKO_PP_LIST_112(var, s, macro) NEKO_PP_LIST_111(var, s, macro) s() macro(var, 112)
#define NEKO_PP_LIST_113(var, s, macro) NEKO_PP_LIST_112(var, s, macro) s() macro(var, 113)
#define NEKO_PP_LIST_114(var, s, macro) NEKO_PP_LIST_113(var, s, macro) s() macro(var, 114)
#define NEKO_PP_LIST_115(var, s, macro) NEKO_PP_LIST_114(var, s, macro) s() macro(var, 115)
#define NEKO_PP_LIST_116(var, s, macro) NEKO_PP_LIST_115(var, s, macro) s() macro(var, 116)
#define NEKO_PP_LIST_117(var, s, macro) NEKO_PP_LIST_116(var, s, macro) s() macro(var, 117)
#define NEKO_PP_LIST_118(var, s, macro) NEKO_PP_LIST_117(var, s, macro) s() macro(var, 118)
#define NEKO_PP_LIST_119(var, s, macro) NEKO_PP_LIST_118(var, s, macro) s() macro(var, 119)
#define NEKO_PP_LIST_120(var, s, macro) NEKO_PP_LIST_119(var, s, macro) s() macro(var, 120)
#define NEKO_PP_LIST_121(var, s, macro) NEKO_PP_LIST_120(var, s, macro) s() macro(var, 121)
#define NEKO_PP_LIST_122(var, s, macro) NEKO_PP_LIST_121(var, s, macro) s() macro(var, 122)
#define NEKO_PP_LIST_123(var, s, macro) NEKO_PP_LIST_122(var, s, macro) s() macro(var, 123)
#define NEKO_PP_LIST_124(var, s, macro) NEKO_PP_LIST_123(var, s, macro) s() macro(var, 124)
#define NEKO_PP_LIST_125(var, s, macro) NEKO_PP_LIST_124(var, s, macro) s() macro(var, 125)
#define NEKO_PP_LIST_126(var, s, macro) NEKO_PP_LIST_125(var, s, macro) s() macro(var, 126)
#define NEKO_PP_LIST_127(var, s, macro) NEKO_PP_LIST_126(var, s, macro) s() macro(var, 127)
#define NEKO_PP_LIST_128(var, s, macro) NEKO_PP_LIST_127(var, s, macro) s() macro(var, 128)

#define NEKO_PP_REPEAT_1(var, s, macro)   macro(var, 1)
#define NEKO_PP_REPEAT_2(var, s, macro)   NEKO_PP_REPEAT_1(var, s, macro) s() macro(var, 2)
#define NEKO_PP_REPEAT_3(var, s, macro)   NEKO_PP_REPEAT_2(var, s, macro) s() macro(var, 3)
#define NEKO_PP_REPEAT_4(var, s, macro)   NEKO_PP_REPEAT_3(var, s, macro) s() macro(var, 4)
#define NEKO_PP_REPEAT_5(var, s, macro)   NEKO_PP_REPEAT_4(var, s, macro) s() macro(var, 5)
#define NEKO_PP_REPEAT_6(var, s, macro)   NEKO_PP_REPEAT_5(var, s, macro) s() macro(var, 6)
#define NEKO_PP_REPEAT_7(var, s, macro)   NEKO_PP_REPEAT_6(var, s, macro) s() macro(var, 7)
#define NEKO_PP_REPEAT_8(var, s, macro)   NEKO_PP_REPEAT_7(var, s, macro) s() macro(var, 8)
#define NEKO_PP_REPEAT_9(var, s, macro)   NEKO_PP_REPEAT_8(var, s, macro) s() macro(var, 9)
#define NEKO_PP_REPEAT_10(var, s, macro)  NEKO_PP_REPEAT_9(var, s, macro) s() macro(var, 10)
#define NEKO_PP_REPEAT_11(var, s, macro)  NEKO_PP_REPEAT_10(var, s, macro) s() macro(var, 11)
#define NEKO_PP_REPEAT_12(var, s, macro)  NEKO_PP_REPEAT_11(var, s, macro) s() macro(var, 12)
#define NEKO_PP_REPEAT_13(var, s, macro)  NEKO_PP_REPEAT_12(var, s, macro) s() macro(var, 13)
#define NEKO_PP_REPEAT_14(var, s, macro)  NEKO_PP_REPEAT_13(var, s, macro) s() macro(var, 14)
#define NEKO_PP_REPEAT_15(var, s, macro)  NEKO_PP_REPEAT_14(var, s, macro) s() macro(var, 15)
#define NEKO_PP_REPEAT_16(var, s, macro)  NEKO_PP_REPEAT_15(var, s, macro) s() macro(var, 16)
#define NEKO_PP_REPEAT_17(var, s, macro)  NEKO_PP_REPEAT_16(var, s, macro) s() macro(var, 17)
#define NEKO_PP_REPEAT_18(var, s, macro)  NEKO_PP_REPEAT_17(var, s, macro) s() macro(var, 18)
#define NEKO_PP_REPEAT_19(var, s, macro)  NEKO_PP_REPEAT_18(var, s, macro) s() macro(var, 19)
#define NEKO_PP_REPEAT_20(var, s, macro)  NEKO_PP_REPEAT_19(var, s, macro) s() macro(var, 20)
#define NEKO_PP_REPEAT_21(var, s, macro)  NEKO_PP_REPEAT_20(var, s, macro) s() macro(var, 21)
#define NEKO_PP_REPEAT_22(var, s, macro)  NEKO_PP_REPEAT_21(var, s, macro) s() macro(var, 22)
#define NEKO_PP_REPEAT_23(var, s, macro)  NEKO_PP_REPEAT_22(var, s, macro) s() macro(var, 23)
#define NEKO_PP_REPEAT_24(var, s, macro)  NEKO_PP_REPEAT_23(var, s, macro) s() macro(var, 24)
#define NEKO_PP_REPEAT_25(var, s, macro)  NEKO_PP_REPEAT_24(var, s, macro) s() macro(var, 25)
#define NEKO_PP_REPEAT_26(var, s, macro)  NEKO_PP_REPEAT_25(var, s, macro) s() macro(var, 26)
#define NEKO_PP_REPEAT_27(var, s, macro)  NEKO_PP_REPEAT_26(var, s, macro) s() macro(var, 27)
#define NEKO_PP_REPEAT_28(var, s, macro)  NEKO_PP_REPEAT_27(var, s, macro) s() macro(var, 28)
#define NEKO_PP_REPEAT_29(var, s, macro)  NEKO_PP_REPEAT_28(var, s, macro) s() macro(var, 29)
#define NEKO_PP_REPEAT_30(var, s, macro)  NEKO_PP_REPEAT_29(var, s, macro) s() macro(var, 30)
#define NEKO_PP_REPEAT_31(var, s, macro)  NEKO_PP_REPEAT_30(var, s, macro) s() macro(var, 31)
#define NEKO_PP_REPEAT_32(var, s, macro)  NEKO_PP_REPEAT_31(var, s, macro) s() macro(var, 32)
#define NEKO_PP_REPEAT_33(var, s, macro)  NEKO_PP_REPEAT_32(var, s, macro) s() macro(var, 33)
#define NEKO_PP_REPEAT_34(var, s, macro)  NEKO_PP_REPEAT_33(var, s, macro) s() macro(var, 34)
#define NEKO_PP_REPEAT_35(var, s, macro)  NEKO_PP_REPEAT_34(var, s, macro) s() macro(var, 35)
#define NEKO_PP_REPEAT_36(var, s, macro)  NEKO_PP_REPEAT_35(var, s, macro) s() macro(var, 36)
#define NEKO_PP_REPEAT_37(var, s, macro)  NEKO_PP_REPEAT_36(var, s, macro) s() macro(var, 37)
#define NEKO_PP_REPEAT_38(var, s, macro)  NEKO_PP_REPEAT_37(var, s, macro) s() macro(var, 38)
#define NEKO_PP_REPEAT_39(var, s, macro)  NEKO_PP_REPEAT_38(var, s, macro) s() macro(var, 39)
#define NEKO_PP_REPEAT_40(var, s, macro)  NEKO_PP_REPEAT_39(var, s, macro) s() macro(var, 40)
#define NEKO_PP_REPEAT_41(var, s, macro)  NEKO_PP_REPEAT_40(var, s, macro) s() macro(var, 41)
#define NEKO_PP_REPEAT_42(var, s, macro)  NEKO_PP_REPEAT_41(var, s, macro) s() macro(var, 42)
#define NEKO_PP_REPEAT_43(var, s, macro)  NEKO_PP_REPEAT_42(var, s, macro) s() macro(var, 43)
#define NEKO_PP_REPEAT_44(var, s, macro)  NEKO_PP_REPEAT_43(var, s, macro) s() macro(var, 44)
#define NEKO_PP_REPEAT_45(var, s, macro)  NEKO_PP_REPEAT_44(var, s, macro) s() macro(var, 45)
#define NEKO_PP_REPEAT_46(var, s, macro)  NEKO_PP_REPEAT_45(var, s, macro) s() macro(var, 46)
#define NEKO_PP_REPEAT_47(var, s, macro)  NEKO_PP_REPEAT_46(var, s, macro) s() macro(var, 47)
#define NEKO_PP_REPEAT_48(var, s, macro)  NEKO_PP_REPEAT_47(var, s, macro) s() macro(var, 48)
#define NEKO_PP_REPEAT_49(var, s, macro)  NEKO_PP_REPEAT_48(var, s, macro) s() macro(var, 49)
#define NEKO_PP_REPEAT_50(var, s, macro)  NEKO_PP_REPEAT_49(var, s, macro) s() macro(var, 50)
#define NEKO_PP_REPEAT_51(var, s, macro)  NEKO_PP_REPEAT_50(var, s, macro) s() macro(var, 51)
#define NEKO_PP_REPEAT_52(var, s, macro)  NEKO_PP_REPEAT_51(var, s, macro) s() macro(var, 52)
#define NEKO_PP_REPEAT_53(var, s, macro)  NEKO_PP_REPEAT_52(var, s, macro) s() macro(var, 53)
#define NEKO_PP_REPEAT_54(var, s, macro)  NEKO_PP_REPEAT_53(var, s, macro) s() macro(var, 54)
#define NEKO_PP_REPEAT_55(var, s, macro)  NEKO_PP_REPEAT_54(var, s, macro) s() macro(var, 55)
#define NEKO_PP_REPEAT_56(var, s, macro)  NEKO_PP_REPEAT_55(var, s, macro) s() macro(var, 56)
#define NEKO_PP_REPEAT_57(var, s, macro)  NEKO_PP_REPEAT_56(var, s, macro) s() macro(var, 57)
#define NEKO_PP_REPEAT_58(var, s, macro)  NEKO_PP_REPEAT_57(var, s, macro) s() macro(var, 58)
#define NEKO_PP_REPEAT_59(var, s, macro)  NEKO_PP_REPEAT_58(var, s, macro) s() macro(var, 59)
#define NEKO_PP_REPEAT_60(var, s, macro)  NEKO_PP_REPEAT_59(var, s, macro) s() macro(var, 60)
#define NEKO_PP_REPEAT_61(var, s, macro)  NEKO_PP_REPEAT_60(var, s, macro) s() macro(var, 61)
#define NEKO_PP_REPEAT_62(var, s, macro)  NEKO_PP_REPEAT_61(var, s, macro) s() macro(var, 62)
#define NEKO_PP_REPEAT_63(var, s, macro)  NEKO_PP_REPEAT_62(var, s, macro) s() macro(var, 63)
#define NEKO_PP_REPEAT_64(var, s, macro)  NEKO_PP_REPEAT_63(var, s, macro) s() macro(var, 64)
#define NEKO_PP_REPEAT_65(var, s, macro)  NEKO_PP_REPEAT_64(var, s, macro) s() macro(var, 65)
#define NEKO_PP_REPEAT_66(var, s, macro)  NEKO_PP_REPEAT_65(var, s, macro) s() macro(var, 66)
#define NEKO_PP_REPEAT_67(var, s, macro)  NEKO_PP_REPEAT_66(var, s, macro) s() macro(var, 67)
#define NEKO_PP_REPEAT_68(var, s, macro)  NEKO_PP_REPEAT_67(var, s, macro) s() macro(var, 68)
#define NEKO_PP_REPEAT_69(var, s, macro)  NEKO_PP_REPEAT_68(var, s, macro) s() macro(var, 69)
#define NEKO_PP_REPEAT_70(var, s, macro)  NEKO_PP_REPEAT_69(var, s, macro) s() macro(var, 70)
#define NEKO_PP_REPEAT_71(var, s, macro)  NEKO_PP_REPEAT_70(var, s, macro) s() macro(var, 71)
#define NEKO_PP_REPEAT_72(var, s, macro)  NEKO_PP_REPEAT_71(var, s, macro) s() macro(var, 72)
#define NEKO_PP_REPEAT_73(var, s, macro)  NEKO_PP_REPEAT_72(var, s, macro) s() macro(var, 73)
#define NEKO_PP_REPEAT_74(var, s, macro)  NEKO_PP_REPEAT_73(var, s, macro) s() macro(var, 74)
#define NEKO_PP_REPEAT_75(var, s, macro)  NEKO_PP_REPEAT_74(var, s, macro) s() macro(var, 75)
#define NEKO_PP_REPEAT_76(var, s, macro)  NEKO_PP_REPEAT_75(var, s, macro) s() macro(var, 76)
#define NEKO_PP_REPEAT_77(var, s, macro)  NEKO_PP_REPEAT_76(var, s, macro) s() macro(var, 77)
#define NEKO_PP_REPEAT_78(var, s, macro)  NEKO_PP_REPEAT_77(var, s, macro) s() macro(var, 78)
#define NEKO_PP_REPEAT_79(var, s, macro)  NEKO_PP_REPEAT_78(var, s, macro) s() macro(var, 79)
#define NEKO_PP_REPEAT_80(var, s, macro)  NEKO_PP_REPEAT_79(var, s, macro) s() macro(var, 80)
#define NEKO_PP_REPEAT_81(var, s, macro)  NEKO_PP_REPEAT_80(var, s, macro) s() macro(var, 81)
#define NEKO_PP_REPEAT_82(var, s, macro)  NEKO_PP_REPEAT_81(var, s, macro) s() macro(var, 82)
#define NEKO_PP_REPEAT_83(var, s, macro)  NEKO_PP_REPEAT_82(var, s, macro) s() macro(var, 83)
#define NEKO_PP_REPEAT_84(var, s, macro)  NEKO_PP_REPEAT_83(var, s, macro) s() macro(var, 84)
#define NEKO_PP_REPEAT_85(var, s, macro)  NEKO_PP_REPEAT_84(var, s, macro) s() macro(var, 85)
#define NEKO_PP_REPEAT_86(var, s, macro)  NEKO_PP_REPEAT_85(var, s, macro) s() macro(var, 86)
#define NEKO_PP_REPEAT_87(var, s, macro)  NEKO_PP_REPEAT_86(var, s, macro) s() macro(var, 87)
#define NEKO_PP_REPEAT_88(var, s, macro)  NEKO_PP_REPEAT_87(var, s, macro) s() macro(var, 88)
#define NEKO_PP_REPEAT_89(var, s, macro)  NEKO_PP_REPEAT_88(var, s, macro) s() macro(var, 89)
#define NEKO_PP_REPEAT_90(var, s, macro)  NEKO_PP_REPEAT_89(var, s, macro) s() macro(var, 90)
#define NEKO_PP_REPEAT_91(var, s, macro)  NEKO_PP_REPEAT_90(var, s, macro) s() macro(var, 91)
#define NEKO_PP_REPEAT_92(var, s, macro)  NEKO_PP_REPEAT_91(var, s, macro) s() macro(var, 92)
#define NEKO_PP_REPEAT_93(var, s, macro)  NEKO_PP_REPEAT_92(var, s, macro) s() macro(var, 93)
#define NEKO_PP_REPEAT_94(var, s, macro)  NEKO_PP_REPEAT_93(var, s, macro) s() macro(var, 94)
#define NEKO_PP_REPEAT_95(var, s, macro)  NEKO_PP_REPEAT_94(var, s, macro) s() macro(var, 95)
#define NEKO_PP_REPEAT_96(var, s, macro)  NEKO_PP_REPEAT_95(var, s, macro) s() macro(var, 96)
#define NEKO_PP_REPEAT_97(var, s, macro)  NEKO_PP_REPEAT_96(var, s, macro) s() macro(var, 97)
#define NEKO_PP_REPEAT_98(var, s, macro)  NEKO_PP_REPEAT_97(var, s, macro) s() macro(var, 98)
#define NEKO_PP_REPEAT_99(var, s, macro)  NEKO_PP_REPEAT_98(var, s, macro) s() macro(var, 99)
#define NEKO_PP_REPEAT_100(var, s, macro) NEKO_PP_REPEAT_99(var, s, macro) s() macro(var, 100)
#define NEKO_PP_REPEAT_101(var, s, macro) NEKO_PP_REPEAT_100(var, s, macro) s() macro(var, 101)
#define NEKO_PP_REPEAT_102(var, s, macro) NEKO_PP_REPEAT_101(var, s, macro) s() macro(var, 102)
#define NEKO_PP_REPEAT_103(var, s, macro) NEKO_PP_REPEAT_102(var, s, macro) s() macro(var, 103)
#define NEKO_PP_REPEAT_104(var, s, macro) NEKO_PP_REPEAT_103(var, s, macro) s() macro(var, 104)
#define NEKO_PP_REPEAT_105(var, s, macro) NEKO_PP_REPEAT_104(var, s, macro) s() macro(var, 105)
#define NEKO_PP_REPEAT_106(var, s, macro) NEKO_PP_REPEAT_105(var, s, macro) s() macro(var, 106)
#define NEKO_PP_REPEAT_107(var, s, macro) NEKO_PP_REPEAT_106(var, s, macro) s() macro(var, 107)
#define NEKO_PP_REPEAT_108(var, s, macro) NEKO_PP_REPEAT_107(var, s, macro) s() macro(var, 108)
#define NEKO_PP_REPEAT_109(var, s, macro) NEKO_PP_REPEAT_108(var, s, macro) s() macro(var, 109)
#define NEKO_PP_REPEAT_110(var, s, macro) NEKO_PP_REPEAT_109(var, s, macro) s() macro(var, 110)
#define NEKO_PP_REPEAT_111(var, s, macro) NEKO_PP_REPEAT_110(var, s, macro) s() macro(var, 111)
#define NEKO_PP_REPEAT_112(var, s, macro) NEKO_PP_REPEAT_111(var, s, macro) s() macro(var, 112)
#define NEKO_PP_REPEAT_113(var, s, macro) NEKO_PP_REPEAT_112(var, s, macro) s() macro(var, 113)
#define NEKO_PP_REPEAT_114(var, s, macro) NEKO_PP_REPEAT_113(var, s, macro) s() macro(var, 114)
#define NEKO_PP_REPEAT_115(var, s, macro) NEKO_PP_REPEAT_114(var, s, macro) s() macro(var, 115)
#define NEKO_PP_REPEAT_116(var, s, macro) NEKO_PP_REPEAT_115(var, s, macro) s() macro(var, 116)
#define NEKO_PP_REPEAT_117(var, s, macro) NEKO_PP_REPEAT_116(var, s, macro) s() macro(var, 117)
#define NEKO_PP_REPEAT_118(var, s, macro) NEKO_PP_REPEAT_117(var, s, macro) s() macro(var, 118)
#define NEKO_PP_REPEAT_119(var, s, macro) NEKO_PP_REPEAT_118(var, s, macro) s() macro(var, 119)
#define NEKO_PP_REPEAT_120(var, s, macro) NEKO_PP_REPEAT_119(var, s, macro) s() macro(var, 120)
#define NEKO_PP_REPEAT_121(var, s, macro) NEKO_PP_REPEAT_120(var, s, macro) s() macro(var, 121)
#define NEKO_PP_REPEAT_122(var, s, macro) NEKO_PP_REPEAT_121(var, s, macro) s() macro(var, 122)
#define NEKO_PP_REPEAT_123(var, s, macro) NEKO_PP_REPEAT_122(var, s, macro) s() macro(var, 123)
#define NEKO_PP_REPEAT_124(var, s, macro) NEKO_PP_REPEAT_123(var, s, macro) s() macro(var, 124)
#define NEKO_PP_REPEAT_125(var, s, macro) NEKO_PP_REPEAT_124(var, s, macro) s() macro(var, 125)
#define NEKO_PP_REPEAT_126(var, s, macro) NEKO_PP_REPEAT_125(var, s, macro) s() macro(var, 126)
#define NEKO_PP_REPEAT_127(var, s, macro) NEKO_PP_REPEAT_126(var, s, macro) s() macro(var, 127)
#define NEKO_PP_REPEAT_128(var, s, macro) NEKO_PP_REPEAT_127(var, s, macro) s() macro(var, 128)
#define NEKO_PP_CALL(macro, num)          macro(num)

#define NEKO_PP_LIST_PARAMS(var, N)    NEKO_PP_PASTE(NEKO_PP_LIST_, N)(var, NEKO_PP_SEP_COMMA, NEKO_PP_PASTE)
#define NEKO_PP_REPEAT_MACRO(macro, N) NEKO_PP_PASTE(NEKO_PP_REPEAT_, N)(macro, NEKO_PP_SEP_EMPTY, NEKO_PP_CALL)