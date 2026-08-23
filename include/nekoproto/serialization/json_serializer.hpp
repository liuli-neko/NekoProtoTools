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
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "json/rapid_json_serializer.hpp"
namespace nekoproto {
using JsonSerializer = RapidJsonSerializer;
} // namespace nekoproto
#elif defined(NEKO_PROTO_ENABLE_SIMDJSON)
#include "json/simd_json_serializer.hpp"
namespace nekoproto {
using JsonSerializer = SimdJsonSerializer;
} // namespace nekoproto
#else
#define NEKO_PROTO_NO_JSON_SERIALIZER
#endif

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
