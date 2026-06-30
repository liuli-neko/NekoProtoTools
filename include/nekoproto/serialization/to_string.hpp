/**
 * @file to_string.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief Human-readable serialization helpers.
 * @version 0.2
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/print/text_writer.hpp"

#include <string>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE

class PrintSerializer {
public:
    PrintSerializer() = default;

    template <typename T>
    bool operator()(const T& value) {
        mWriter.reset();
        mLastResult = parser_write<print::TextWriter>(mWriter, value, parsing::Parent<print::TextWriter>::Root{});
        return static_cast<bool>(mLastResult);
    }

    bool end() const noexcept { return static_cast<bool>(mLastResult); }
    explicit operator bool() const noexcept { return static_cast<bool>(mLastResult); }
    const sa::Error* error() const noexcept { return sa::error_ptr(mLastResult); }
    std::string str() const { return mWriter.str(); }

private:
    print::TextWriter mWriter;
    sa::Result<void> mLastResult;
};

template <typename T>
inline std::string serializable_to_string(T&& value) {
    PrintSerializer serializer;
    if (!serializer(value)) {
        return {};
    }
    return serializer.str();
}

#ifndef NEKO_PROTO_NO_JSON_SERIALIZER
template <typename T>
inline std::string to_json_string(T&& value) {
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer json(buffer);
    if (!json(value) || !json.end()) {
        return {};
    }
    return {buffer.begin(), buffer.end()};
}
#endif

NEKO_END_NAMESPACE
