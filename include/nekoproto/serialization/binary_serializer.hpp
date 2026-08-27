#pragma once

#include "nekoproto/serialization/binary/binary_reader.hpp"
#include "nekoproto/serialization/binary/binary_writer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"
#include "nekoproto/serialization/serializer_base.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace nekoproto {

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
        bool wrote_root = false;
    };

    template <typename SourceT>
    struct InputState {
        InputState(const char* data, std::size_t size, binary::ParseLimits limits = {}) : reader(data, size, limits) {}

        binary::Reader reader;
    };

    template <typename BufferT, typename T>
    static auto write(OutputState<BufferT>& state, const T& value) -> sa::Result<void> {
        if (state.wrote_root) {
            return sa::error(sa::ErrorCode::InvalidLength,
                             "Binary V2 accepts exactly one root value; wrap multiple values in a tuple or object");
        }
        state.wrote_root = true;
        using ValueType = std::remove_cvref_t<T>;
        if constexpr (is_tagged_field_v<ValueType>) {
            if constexpr (tag_query::get<tag_property::RawFixedData>(ValueType::tags)) {
                using RawType = std::remove_cvref_t<typename ValueType::accessor_type>;
                if constexpr (!(detail::HasValuesMeta<RawType> && detail::HasNamesMeta<RawType>)) {
                    return sa::error(sa::ErrorCode::InvalidType,
                                     "raw_fixed_data requires a reflected object with named fixed-width fields");
                }
            }
        }
        using WriterType = typename OutputState<BufferT>::WriterType;
        using Root       = typename parsing::Parent<WriterType>::Root;
        auto result = parserWrite<WriterType>(state.writer, value, Root{});
        return result ? state.writer.result() : result;
    }

    template <typename SourceT, typename T>
    static auto read(InputState<SourceT>& state, T& value) -> sa::Result<void> {
        using ValueType = std::remove_cvref_t<T>;
        if constexpr (is_tagged_field_v<ValueType>) {
            if constexpr (tag_query::get<tag_property::RawFixedData>(ValueType::tags)) {
                using RawType = std::remove_cvref_t<typename ValueType::accessor_type>;
                if constexpr (!(detail::HasValuesMeta<RawType> && detail::HasNamesMeta<RawType>)) {
                    return sa::error(sa::ErrorCode::InvalidType,
                                     "raw_fixed_data requires a reflected object with named fixed-width fields");
                }
                state.reader.beginRawFixedDataAsRoot();
            }
        }
        if constexpr (std::is_copy_constructible_v<T> && std::is_move_assignable_v<T>) {
            T parsed = value;
            auto result = parserRead<binary::Reader>(state.reader.root(), parsed);
            if (result) {
                result = state.reader.finish();
            }
            if (result) {
                value = std::move(parsed);
            }
            return result;
        } else {
            auto result = parserRead<binary::Reader>(state.reader.root(), value);
            return result ? state.reader.finish() : result;
        }
    }

    template <typename BufferT>
    static auto finish(OutputState<BufferT>& state, sa::Result<void> result) -> sa::Result<void> {
        if (!result) {
            return result;
        }
        return state.writer.result();
    }

    template <typename SourceT>
    static auto finish(InputState<SourceT>& state, sa::Result<void> result) -> sa::Result<void> {
        if (!result) {
            return result;
        }
        return state.reader.finish();
    }

    template <typename SourceT>
    static auto inputResult(const InputState<SourceT>& state) -> sa::Result<void> {
        return state.reader.inputResult();
    }

    template <typename SourceT>
    static auto offset(const InputState<SourceT>& state) noexcept -> std::size_t {
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

} // namespace nekoproto
