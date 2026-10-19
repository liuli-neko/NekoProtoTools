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
#include "nekoproto/global/macros.hpp"

#include <string>
#include <string_view>

NEKO_BEGIN_EXPORT
namespace nekoproto {
template <typename>
inline constexpr bool always_false_v = false;
} // namespace nekoproto
NEKO_END_EXPORT
