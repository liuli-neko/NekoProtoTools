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
#include "nekoproto/serialization/serializer_base.hpp"
#if defined(NEKO_PROTO_ENABLE_YYJSON)
#include "json/yyjson_serializer.hpp"
#endif

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "json/rapid_json_serializer.hpp"
#endif

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)
#include "json/simd_json_serializer.hpp"
#endif

namespace nekoproto {
#if defined(NEKO_PROTO_ENABLE_YYJSON)
using JsonSerializer = YyJsonSerializer;
#elif defined(NEKO_PROTO_ENABLE_RAPIDJSON)
using JsonSerializer = RapidJsonSerializer;
#elif defined(NEKO_PROTO_ENABLE_SIMDJSON)
using JsonSerializer = SimdJsonSerializer;
#else
#define NEKO_PROTO_NO_JSON_SERIALIZER
#endif
} // namespace nekoproto

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
namespace nekoproto {
template <typename T>
auto toJsonValue(const T& obj) -> sa::Result<typename JsonSerializer::JsonValue> {
    JsonSerializer::JsonValue json;
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer out(buffer);
    if (!out(obj) || !out.end()) {
        if (const auto* error = out.error()) {
            return sa::err(*error);
        }
        return sa::err(sa::ErrorCode::ParseError, "Failed to serialize JSON value");
    }
    JsonSerializer::InputSerializer in(buffer.data(), buffer.size());
    if (!in(json)) {
        if (const auto* error = in.error()) {
            return sa::err(*error);
        }
        return sa::err(sa::ErrorCode::ParseError, "Failed to parse serialized JSON value");
    }
    return json;
}
} // namespace nekoproto
#endif
