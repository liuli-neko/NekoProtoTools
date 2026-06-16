#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)

#include "nekoproto/serialization/json/text_json_writer.hpp"

#include <simdjson.h>
#include <string_view>

NEKO_BEGIN_NAMESPACE
namespace detail::simd {

class Writer : public json::TextWriter {
public:
    static bool parseRawValue(std::string_view text, RawValueType& value) {
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
NEKO_END_NAMESPACE

#endif
