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

NEKO_BEGIN_NAMESPACE

namespace detail {
template <typename BufferT>
void append_toml(BufferT& buffer, std::string_view toml) {
    if constexpr (requires { buffer.insert(buffer.end(), toml.begin(), toml.end()); }) {
        buffer.insert(buffer.end(), toml.begin(), toml.end());
    } else if constexpr (requires { buffer.write(toml.data(), static_cast<std::streamsize>(toml.size())); }) {
        buffer.write(toml.data(), static_cast<std::streamsize>(toml.size()));
    } else {
        static_assert(always_false_v<BufferT>, "Unsupported TOML output buffer");
    }
}

inline std::string toml_parse_error_message(const toml::parse_error& error) {
    std::ostringstream stream;
    stream << error;
    return stream.str();
}
} // namespace detail

struct TomlplusplusBackend {
    using Reader              = tomlplusplus::Reader;
    using Writer              = tomlplusplus::Writer;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        explicit OutputState(BufferT& outputBuffer) : buffer(outputBuffer), writer(&document) {}

        BufferT& buffer;
        toml::table document;
        tomlplusplus::Writer writer;
        bool hasRoot = false;
        bool flushed = false;
    };

    template <typename SourceT>
    struct InputState {
        explicit InputState(const char* buffer, std::size_t size) { parse(buffer, size); }

        explicit InputState(std::istream& stream) {
            ownedInput.assign(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
            parse(ownedInput.data(), ownedInput.size(), false);
        }

        void parse(const char* buffer, std::size_t size, bool copy = true) {
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            if (copy) {
                ownedInput.assign(buffer, size);
                buffer = ownedInput.data();
            }
            auto parsed = toml::parse(std::string_view{buffer, size});
            if (!parsed) {
                result = sa::error(sa::ErrorCode::ParseError, detail::toml_parse_error_message(parsed.error()));
                return;
            }
            document = std::move(parsed).table();
            result   = sa::success();
        }

        std::string ownedInput;
        toml::table document;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static sa::Result<void> write(OutputState<BufferT>& state, const T& value) {
        state.document.clear();
        state.writer.reset(&state.document);
        auto result = parser_write<tomlplusplus::Writer>(state.writer, value,
                                                         parsing::Parent<tomlplusplus::Writer>::Root{});
        state.hasRoot = static_cast<bool>(result) && static_cast<bool>(state.writer.result());
        state.flushed = false;
        if (!state.writer.result()) {
            return state.writer.result();
        }
        return result;
    }

    template <typename BufferT>
    static sa::Result<void> finish(OutputState<BufferT>& state, sa::Result<void> result) {
        if (!result) {
            return result;
        }
        if (!state.writer.result()) {
            return state.writer.result();
        }
        if (!state.hasRoot) {
            return result;
        }
        if (!state.flushed) {
            std::ostringstream stream;
            stream << state.document;
            const auto output = stream.str();
            detail::append_toml(state.buffer, output);
            state.flushed = true;
        }
        return result;
    }

    template <typename BufferT>
    static bool outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept {
        return state.hasRoot && static_cast<bool>(result) && static_cast<bool>(state.writer.result());
    }

    template <typename SourceT>
    static sa::Result<void> inputResult(const InputState<SourceT>& state) {
        return state.result;
    }

    template <typename SourceT, typename T>
    static sa::Result<void> read(InputState<SourceT>& state, T& value) {
        return parser_read<tomlplusplus::Reader>(&state.document, value);
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

NEKO_END_NAMESPACE

#else
#define NEKO_PROTO_NO_TOML_SERIALIZER
#endif
