#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)

#include "nekoproto/serialization/json/text_json_writer.hpp"

#include <simdjson.h>
#include <string_view>

namespace nekoproto {
namespace detail::simd {

class Writer : public json::TextWriter {
public:
    static auto parseRawValue(std::string_view text, RawValueType& value) -> bool {
        simdjson::dom::parser parser;
        simdjson::padded_string padded{text};
        auto parsed = parser.parse(padded);
        if (parsed.error() != simdjson::SUCCESS) {
            return false;
        }
        value.text = simdjson::minify(parsed);
        return true;
    }
};

} // namespace detail::simd
} // namespace nekoproto

#endif
