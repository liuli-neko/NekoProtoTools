#pragma once

#include "nekoproto/serialization/binary/binary_reader.hpp"
#include "nekoproto/serialization/binary/binary_writer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"

#include <cstddef>
#include <vector>

NEKO_BEGIN_NAMESPACE

struct BinaryBackend {
    using Reader              = binary::Reader;
    using Writer              = binary::Writer;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        explicit OutputState(BufferT& buffer) noexcept : writer(buffer) {}

        binary::Writer writer;
    };

    template <typename SourceT>
    struct InputState {
        InputState(const char* data, std::size_t size) noexcept : reader(data, size) {}

        binary::Reader reader;
    };

    template <typename BufferT, typename T>
    static sa::Result<void> write(OutputState<BufferT>& state, const T& value) {
        return detail::parser_write<binary::Reader, binary::Writer>(
            state.writer, value, parsing::Parent<binary::Writer>::Root{});
    }

    template <typename SourceT, typename T>
    static sa::Result<void> read(InputState<SourceT>& state, T& value) {
        return detail::parser_read<binary::Reader, binary::Writer>(state.reader.root(), value);
    }

    template <typename SourceT>
    static std::size_t offset(const InputState<SourceT>& state) noexcept {
        return state.reader.offset();
    }
};

using BinaryOutputSerializer = detail::OutputSerializerAdapter<BinaryBackend, BinaryBackend::DefaultOutputBuffer>;
using BinaryInputSerializer  = detail::InputSerializerAdapter<BinaryBackend, BinaryBackend::DefaultInputSource>;

struct BinarySerializer {
    using OutputSerializer = BinaryOutputSerializer;
    using InputSerializer  = BinaryInputSerializer;
    using Reader           = binary::Reader;
    using Writer           = binary::Writer;
};

NEKO_END_NAMESPACE
