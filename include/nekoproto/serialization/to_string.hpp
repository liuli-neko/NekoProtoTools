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

namespace nekoproto {

class PrintSerializer {
public:
    PrintSerializer() = default;

    template <typename T>
    auto operator()(const T& value) -> bool {
        writer_.reset();
        last_result_ = parserWrite<print::TextWriter>(writer_, value, parsing::Parent<print::TextWriter>::Root{});
        return static_cast<bool>(last_result_);
    }

    auto end() const noexcept -> bool { return static_cast<bool>(last_result_); }
    explicit operator bool() const noexcept { return static_cast<bool>(last_result_); }
    auto error() const noexcept -> const sa::Error* { return sa::errorPtr(last_result_); }
    auto str() const -> std::string { return writer_.str(); }

private:
    print::TextWriter writer_;
    sa::Result<void>  last_result_;
};

template <typename T>
inline auto serializableToString(T&& value) -> std::string {
    PrintSerializer serializer;
    if (!serializer(value)) {
        return {};
    }
    return serializer.str();
}

#ifndef NEKO_PROTO_NO_JSON_SERIALIZER
template <typename T>
inline auto toJsonString(T&& value) -> std::string {
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer json(buffer);
    if (!json(value) || !json.end()) {
        return {};
    }
    return {buffer.begin(), buffer.end()};
}
#endif

} // namespace nekoproto
