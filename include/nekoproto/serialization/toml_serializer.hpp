#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)

#if defined(TOML_EXCEPTIONS) && TOML_EXCEPTIONS
#error "NekoProto TOML backend requires toml++ with TOML_EXCEPTIONS=0"
#endif
#ifndef TOML_EXCEPTIONS
#define TOML_EXCEPTIONS 0
#endif

#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"
#include "nekoproto/serialization/toml/tomlplusplus_reader.hpp"
#include "nekoproto/serialization/toml/tomlplusplus_writer.hpp"

#include <toml++/toml.h>

#include <cstddef>
#include <istream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nekoproto {

namespace detail {
template <typename BufferT>
void appendToml(BufferT& buffer, std::string_view toml) {
    if constexpr (requires { buffer.insert(buffer.end(), toml.begin(), toml.end()); }) {
        buffer.insert(buffer.end(), toml.begin(), toml.end());
    } else if constexpr (requires { buffer.write(toml.data(), static_cast<std::streamsize>(toml.size())); }) {
        buffer.write(toml.data(), static_cast<std::streamsize>(toml.size()));
    } else {
        static_assert(always_false_v<BufferT>, "Unsupported TOML output buffer");
    }
}

inline auto tomlParseErrorMessage(const toml::parse_error& error) -> std::string {
    std::ostringstream stream;
    stream << error;
    return stream.str();
}

inline auto removeBlankTomlLines(std::string_view toml) -> std::string {
    std::string result;
    result.reserve(toml.size());

    std::size_t line_begin = 0;
    while (line_begin < toml.size()) {
        auto line_end = toml.find('\n', line_begin);
        if (line_end == std::string_view::npos) {
            line_end = toml.size();
        }

        bool has_content = false;
        for (std::size_t index = line_begin; index < line_end; ++index) {
            if (toml[index] != ' ' && toml[index] != '\t' && toml[index] != '\r') {
                has_content = true;
                break;
            }
        }

        if (has_content) {
            result.append(toml.substr(line_begin, line_end - line_begin));
            if (line_end < toml.size()) {
                result.push_back('\n');
            }
        }

        line_begin = line_end + 1;
    }

    return result;
}
} // namespace detail

struct TomlOutputFormatOptions {
    using FormatOptions = toml::format_flags;

    static constexpr auto compactFlags() noexcept -> FormatOptions {
        return toml::format_flags::allow_literal_strings | toml::format_flags::allow_unicode_strings |
               toml::format_flags::allow_real_tabs_in_strings | toml::format_flags::allow_binary_integers |
               toml::format_flags::allow_octal_integers | toml::format_flags::allow_hexadecimal_integers |
               toml::format_flags::terse_key_value_pairs;
    }

    static auto compact() -> TomlOutputFormatOptions {
        return TomlOutputFormatOptions(compactFlags(), true);
    }

    static auto pretty() -> TomlOutputFormatOptions {
        return TomlOutputFormatOptions(toml::toml_formatter::default_flags, false);
    }

    explicit TomlOutputFormatOptions(FormatOptions flags = compactFlags(), bool strip_blank_lines = true) noexcept
        : flags(flags), strip_blank_lines(strip_blank_lines) {}

    FormatOptions flags             = compactFlags();
    bool          strip_blank_lines = true;
};

struct TomlplusplusBackend {
    using Reader              = tomlplusplus::Reader;
    using Writer              = tomlplusplus::Writer;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        explicit OutputState(BufferT& outputBuffer) : buffer(outputBuffer), writer(&document) {}

        OutputState(BufferT& outputBuffer, const TomlOutputFormatOptions& format_options)
            : buffer(outputBuffer), writer(&document), options(format_options) {}

        BufferT&                buffer;
        toml::table             document;
        tomlplusplus::Writer    writer;
        TomlOutputFormatOptions options;
        bool                    has_root = false;
        bool                    flushed  = false;
    };

    template <typename SourceT>
    struct InputState {
        explicit InputState(const char* buffer, std::size_t size) { parse(buffer, size); }

        explicit InputState(std::istream& stream) {
            owned_input.assign(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
            parse(owned_input.data(), owned_input.size(), false);
        }

        void parse(const char* buffer, std::size_t size, bool copy = true) {
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            if (copy) {
                owned_input.assign(buffer, size);
                buffer = owned_input.data();
            }
            auto parsed = toml::parse(std::string_view{buffer, size});
            if (!parsed) {
                result = sa::error(sa::ErrorCode::ParseError, detail::tomlParseErrorMessage(parsed.error()));
                return;
            }
            document = std::move(parsed).table();
            result   = sa::success();
        }

        std::string      owned_input;
        toml::table      document;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static auto write(OutputState<BufferT>& state, const T& value) -> sa::Result<void> {
        state.document.clear();
        state.writer.reset(&state.document);
        auto result = parserWrite<tomlplusplus::Writer>(state.writer, value,
                                                         parsing::Parent<tomlplusplus::Writer>::Root{});
        state.has_root = static_cast<bool>(result) && static_cast<bool>(state.writer.result());
        state.flushed  = false;
        if (!state.writer.result()) {
            return state.writer.result();
        }
        return result;
    }

    template <typename BufferT>
    static auto finish(OutputState<BufferT>& state, sa::Result<void> result) -> sa::Result<void> {
        if (!result) {
            return result;
        }
        if (!state.writer.result()) {
            return state.writer.result();
        }
        if (!state.has_root) {
            return result;
        }
        if (!state.flushed) {
            std::ostringstream stream;
            stream << toml::toml_formatter{state.document, state.options.flags};
            auto output = stream.str();
            if (state.options.strip_blank_lines) {
                output = detail::removeBlankTomlLines(output);
            }
            detail::appendToml(state.buffer, output);
            state.flushed = true;
        }
        return result;
    }

    template <typename BufferT>
    static auto outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept -> bool {
        return state.has_root && static_cast<bool>(result) && static_cast<bool>(state.writer.result());
    }

    template <typename SourceT>
    static auto inputResult(const InputState<SourceT>& state) -> sa::Result<void> {
        return state.result;
    }

    template <typename SourceT, typename T>
    static auto read(InputState<SourceT>& state, T& value) -> sa::Result<void> {
        return parserRead<tomlplusplus::Reader>(&state.document, value);
    }
};

template <typename BufferT = TomlplusplusBackend::DefaultOutputBuffer>
class TomlplusplusOutputSerializer : public detail::OutputSerializerAdapter<TomlplusplusBackend, BufferT> {
public:
    using Base = detail::OutputSerializerAdapter<TomlplusplusBackend, BufferT>;
    using Base::Base;
};

template <typename BufferT>
TomlplusplusOutputSerializer(BufferT&) -> TomlplusplusOutputSerializer<BufferT>;

template <typename SourceT = TomlplusplusBackend::DefaultInputSource>
class TomlplusplusInputSerializer : public detail::InputSerializerAdapter<TomlplusplusBackend, SourceT> {
public:
    using Base = detail::InputSerializerAdapter<TomlplusplusBackend, SourceT>;
    using Base::Base;
};

TomlplusplusInputSerializer(const char*, std::size_t) -> TomlplusplusInputSerializer<>;
TomlplusplusInputSerializer(std::istream&) -> TomlplusplusInputSerializer<>;

struct TomlplusplusSerializer {
    using OutputSerializer = TomlplusplusOutputSerializer<>;
    using InputSerializer  = TomlplusplusInputSerializer<>;
    using Reader           = tomlplusplus::Reader;
    using Writer           = tomlplusplus::Writer;
};

using TomlSerializer = TomlplusplusSerializer;

} // namespace nekoproto

#else
#define NEKO_PROTO_NO_TOML_SERIALIZER
#endif
