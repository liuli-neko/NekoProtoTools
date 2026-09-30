#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)

#include "nekoproto/serialization/json/text_json_writer.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <simdjson.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nekoproto {
namespace detail::simd {

class Writer {
public:
    static constexpr bool requires_field_count = false;

    using RawValueType = json::RawValue;

    struct OutputArrayType {
        Writer* writer = nullptr;
        bool    first  = true;
        bool    closed = false;

        OutputArrayType() = default;
        explicit OutputArrayType(Writer* w) noexcept : writer(w), first(true), closed(false) {}
        OutputArrayType(OutputArrayType&& other) noexcept
            : writer(other.writer), first(other.first), closed(other.closed) {
            other.writer = nullptr;
            other.closed = true;
        }
        OutputArrayType& operator=(OutputArrayType&& other) noexcept {
            if (this != &other) {
                close();
                writer       = other.writer;
                first        = other.first;
                closed       = other.closed;
                other.writer = nullptr;
                other.closed = true;
            }
            return *this;
        }
        OutputArrayType(const OutputArrayType&)            = delete;
        OutputArrayType& operator=(const OutputArrayType&) = delete;

        ~OutputArrayType() { close(); }

        void close() noexcept {
            if (writer != nullptr && !closed) {
                writer->buffer_.push_back(']');
                closed = true;
            }
        }
    };

    struct OutputObjectType {
        Writer* writer = nullptr;
        bool    first  = true;
        bool    closed = false;

        OutputObjectType() = default;
        explicit OutputObjectType(Writer* w) noexcept : writer(w), first(true), closed(false) {}
        OutputObjectType(OutputObjectType&& other) noexcept
            : writer(other.writer), first(other.first), closed(other.closed) {
            other.writer = nullptr;
            other.closed = true;
        }
        OutputObjectType& operator=(OutputObjectType&& other) noexcept {
            if (this != &other) {
                close();
                writer       = other.writer;
                first        = other.first;
                closed       = other.closed;
                other.writer = nullptr;
                other.closed = true;
            }
            return *this;
        }
        OutputObjectType(const OutputObjectType&)            = delete;
        OutputObjectType& operator=(const OutputObjectType&) = delete;

        ~OutputObjectType() { close(); }

        void close() noexcept {
            if (writer != nullptr && !closed) {
                writer->buffer_.push_back('}');
                closed = true;
            }
        }
    };

    struct OutputValueType {
        Writer* writer = nullptr;
    };

    void reset() { buffer_.clear(); }

    auto str() const noexcept -> const std::string& { return buffer_; }

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

    auto arrayAsRoot(std::size_t /*size*/ = 0) -> OutputArrayType {
        buffer_.push_back('[');
        return OutputArrayType{this};
    }

    auto objectAsRoot(std::size_t /*size*/ = 0) -> OutputObjectType {
        buffer_.push_back('{');
        return OutputObjectType{this};
    }

    auto nullAsRoot() -> OutputValueType {
        buffer_.append("null");
        return {this};
    }

    template <typename T>
    auto valueAsRoot(const T& value) -> OutputValueType {
        appendValue(value);
        return {this};
    }

    auto addArrayToArray(std::size_t /*size*/, OutputArrayType* parent) -> OutputArrayType {
        handleArrayElementPrefix(parent);
        buffer_.push_back('[');
        return OutputArrayType{this};
    }

    auto addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent) -> OutputArrayType {
        handleObjectMemberPrefix(name, parent);
        buffer_.push_back('[');
        return OutputArrayType{this};
    }

    auto addObjectToArray(std::size_t /*size*/, OutputArrayType* parent) -> OutputObjectType {
        handleArrayElementPrefix(parent);
        buffer_.push_back('{');
        return OutputObjectType{this};
    }

    auto addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent) -> OutputObjectType {
        handleObjectMemberPrefix(name, parent);
        buffer_.push_back('{');
        return OutputObjectType{this};
    }

    template <typename T>
    auto addValueToArray(const T& value, OutputArrayType* parent) -> OutputValueType {
        handleArrayElementPrefix(parent);
        appendValue(value);
        return {this};
    }

    template <typename T>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) -> OutputValueType {
        handleObjectMemberPrefix(name, parent);
        appendValue(value);
        return {this};
    }

    auto addNullToArray(OutputArrayType* parent) -> OutputValueType {
        handleArrayElementPrefix(parent);
        buffer_.append("null");
        return {this};
    }

    auto addNullToObject(std::string_view name, OutputObjectType* parent) -> OutputValueType {
        handleObjectMemberPrefix(name, parent);
        buffer_.append("null");
        return {this};
    }

    void endArray(OutputArrayType* array) {
        if (array != nullptr) {
            array->close();
        }
    }

    void endObject(OutputObjectType* object) {
        if (object != nullptr) {
            object->close();
        }
    }

private:
    void handleArrayElementPrefix(OutputArrayType* parent) {
        if (parent != nullptr) {
            if (parent->first) {
                parent->first = false;
            } else {
                buffer_.push_back(',');
            }
        }
    }

    void handleObjectMemberPrefix(std::string_view name, OutputObjectType* parent) {
        if (parent != nullptr) {
            if (parent->first) {
                parent->first = false;
            } else {
                buffer_.push_back(',');
            }
        }
        buffer_.push_back('"');
        escapeStringToBuffer(name, buffer_);
        buffer_.append("\":");
    }

    template <typename T>
    void appendValue(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, RawValueType>) {
            buffer_.append(value.text);
        } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            buffer_.push_back('"');
            escapeStringToBuffer(value, buffer_);
            buffer_.push_back('"');
        } else if constexpr (std::is_same_v<U, const char*>) {
            buffer_.push_back('"');
            if (value != nullptr) {
                escapeStringToBuffer(std::string_view{value}, buffer_);
            }
            buffer_.push_back('"');
        } else if constexpr (std::is_same_v<U, bool>) {
            buffer_.append(value ? "true" : "false");
        } else if constexpr (std::is_integral_v<U>) {
            char num_buf[32];
            auto [ptr, ec] = std::to_chars(num_buf, num_buf + sizeof(num_buf), value);
            buffer_.append(num_buf, ptr);
        } else if constexpr (std::is_floating_point_v<U>) {
            if (!std::isfinite(value)) {
                buffer_.append("null");
                return;
            }
            char num_buf[64];
            auto [ptr, ec] = std::to_chars(num_buf, num_buf + sizeof(num_buf), value);
            if (ec == std::errc{}) {
                buffer_.append(num_buf, ptr);
            } else {
                buffer_.append(std::to_string(value));
            }
        } else {
            static_assert(always_false_v<U>, "Unsupported JSON value type in simd::Writer");
        }
    }

    static void escapeStringToBuffer(std::string_view value, std::string& out) {
        for (const unsigned char ch : value) {
            switch (ch) {
            case '"':
                out.append("\\\"");
                break;
            case '\\':
                out.append("\\\\");
                break;
            case '\b':
                out.append("\\b");
                break;
            case '\f':
                out.append("\\f");
                break;
            case '\n':
                out.append("\\n");
                break;
            case '\r':
                out.append("\\r");
                break;
            case '\t':
                out.append("\\t");
                break;
            default:
                if (ch < 0x20U) {
                    constexpr char hex[] = "0123456789abcdef";
                    out.append("\\u00");
                    out.push_back(hex[(ch >> 4U) & 0x0FU]);
                    out.push_back(hex[ch & 0x0FU]);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
            }
        }
    }

    std::string buffer_;
};

} // namespace detail::simd
} // namespace nekoproto

#endif
