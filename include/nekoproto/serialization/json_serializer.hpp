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

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
NEKO_BEGIN_NAMESPACE
template <typename T>
JsonSerializer::JsonValue from_object(const T& obj) {
    JsonSerializer::JsonValue json;
    std::vector<char> buffer;
    NEKO_NAMESPACE::JsonSerializer::OutputSerializer out(buffer);
    out(obj);
    out.end();
    NEKO_LOG_INFO("mcp server", "Tool call info: {}", std::string_view{buffer.data(), buffer.size()});
    NEKO_NAMESPACE::JsonSerializer::InputSerializer in(buffer.data(), buffer.size());
    in(json);
    return json;
}
NEKO_END_NAMESPACE
#endif