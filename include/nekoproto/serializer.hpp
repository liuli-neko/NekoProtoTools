/**
 * @file serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief Top-level aggregate header for NekoProto serialization base and adapters.
 * @version 0.1
 * @date 2026-08-26
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#include "nekoproto/global/macros.hpp"

#if !defined(NEKO_BUILDING_MODULE) || NEKO_BUILDING_MODULE == NEKO_MODULE_SERIALIZER

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/reflect.hpp"
#endif // current component: serializer
