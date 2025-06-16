/**
 * @file json_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
// #####################################################
// default JsonSerializer type definition
#include "nekoproto/global/global.hpp"
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "json/rapid_json_serializer.hpp"
NEKO_BEGIN_NAMESPACE
using JsonSerializer = RapidJsonSerializer;
NEKO_END_NAMESPACE
#elif defined(NEKO_PROTO_ENABLE_SIMDJSON)
#include "json/simd_json_serializer.hpp"
NEKO_BEGIN_NAMESPACE
using JsonSerializer = SimdJsonSerializer;
NEKO_END_NAMESPACE
#else
#define NEKO_PROTO_NO_JSON_SERIALIZER
#endif