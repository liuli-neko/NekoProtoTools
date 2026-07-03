#pragma once

#include "nekoproto/serialization/binary/binary_reader.hpp"
#include "nekoproto/serialization/binary/binary_writer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

#include <cstddef>
#include <vector>

NEKO_BEGIN_NAMESPACE

struct BinaryBackend {
    using Reader              = binary::Reader;
    using Writer              = binary::Writer<>;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        using WriterType = binary::Writer<BufferT>;

        explicit OutputState(BufferT& buffer) noexcept : writer(buffer) {}

        WriterType writer;
    };

    template <typename SourceT>
    struct InputState {
        InputState(const char* data, std::size_t size) noexcept : reader(data, size) {}

        binary::Reader reader;
    };

    template <typename BufferT, typename T>
    static sa::Result<void> write(OutputState<BufferT>& state, const T& value) {
        using WriterType = typename OutputState<BufferT>::WriterType;
        using Root       = typename parsing::Parent<WriterType>::Root;
        return parser_write<WriterType>(state.writer, value, Root{});
    }

    template <typename SourceT, typename T>
    static sa::Result<void> read(InputState<SourceT>& state, T& value) {
        return parser_read<binary::Reader>(state.reader.root(), value);
    }

    template <typename SourceT>
    static std::size_t offset(const InputState<SourceT>& state) noexcept {
        return state.reader.offset();
    }
};

using BinaryOutputSerializer     = detail::OutputSerializerAdapter<BinaryBackend, BinaryBackend::DefaultOutputBuffer>;
using BinaryByteOutputSerializer = detail::OutputSerializerAdapter<BinaryBackend, std::vector<std::byte>>;
using BinaryInputSerializer      = detail::InputSerializerAdapter<BinaryBackend, BinaryBackend::DefaultInputSource>;

struct BinarySerializer {
    using OutputSerializer     = BinaryOutputSerializer;
    using ByteOutputSerializer = BinaryByteOutputSerializer;
    using InputSerializer      = BinaryInputSerializer;
    using Reader               = binary::Reader;
    using Writer               = binary::Writer<>;
};

NEKO_END_NAMESPACE
