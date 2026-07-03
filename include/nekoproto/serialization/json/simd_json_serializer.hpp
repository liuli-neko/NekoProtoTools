#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_SIMDJSON)

#include "nekoproto/serialization/json/simd_json_reader.hpp"
#include "nekoproto/serialization/json/simd_json_writer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"

#include <memory>
#include <ostream>
#include <simdjson.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE

namespace detail::simd {

using RawJsonValue = simdjson::dom::element;

class SimdJsonValue {
public:
    SimdJsonValue() = default;

    explicit SimdJsonValue(const InputValue& value)
        : mValue(std::make_shared<RawJsonValue>(value.value)), mParser(value.owner) {}

    SimdJsonValue(const RawJsonValue& value, std::shared_ptr<JsonParser> parser)
        : mValue(std::make_shared<RawJsonValue>(value)), mParser(std::move(parser)) {}

    bool hasValue() const noexcept { return mValue != nullptr; }
    explicit operator bool() const noexcept { return hasValue(); }

    const RawJsonValue& nativeValue() const { return *mValue; }
    RawJsonValue& nativeValue() { return *mValue; }

    bool isObject() const { return mValue && mValue->is_object(); }
    bool isArray() const { return mValue && mValue->is_array(); }
    bool isString() const { return mValue && mValue->is_string(); }
    bool isNumber() const { return mValue && mValue->is_number(); }
    bool isBool() const { return mValue && mValue->is_bool(); }
    bool isNull() const { return mValue && mValue->is_null(); }

    template <typename T>
    bool value(T& output) const {
        if (!mValue) {
            return false;
        }
        auto result = Reader::template toBasicType<T>(InputValue{*mValue, mParser});
        if (!result) {
            return false;
        }
        output = result.value();
        return true;
    }

    std::size_t size() const {
        if (isArray()) {
            return mValue->get_array().value_unsafe().size();
        }
        if (isObject()) {
            return mValue->get_object().value_unsafe().size();
        }
        return 0;
    }

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    SimdJsonValue operator[](const T& name) const {
        if (!isObject()) {
            return {};
        }
        auto value = mValue->get_object().value_unsafe().at_key(std::string_view{name});
        if (value.error() != simdjson::SUCCESS) {
            return {};
        }
        return SimdJsonValue(value.value_unsafe(), mParser);
    }

    SimdJsonValue operator[](std::size_t index) const {
        if (isArray() && index < size()) {
            return SimdJsonValue(mValue->get_array().value_unsafe().at(index).value_unsafe(), mParser);
        }
        if (isObject() && index < size()) {
            auto iterator = mValue->get_object().value_unsafe().begin();
            while (index-- > 0) {
                ++iterator;
            }
            return SimdJsonValue(iterator.value(), mParser);
        }
        return {};
    }

private:
    std::shared_ptr<RawJsonValue> mValue;
    std::shared_ptr<JsonParser> mParser;
};

template <typename BufferT, typename = void>
struct JsonOutputValueType {
    using type = void;
};

template <typename BufferT>
struct JsonOutputValueType<BufferT, std::void_t<typename BufferT::value_type>> {
    using type = typename BufferT::value_type;
};

template <typename BufferT>
void appendJson(BufferT& buffer, std::string_view json) {
    using ValueType = typename JsonOutputValueType<std::remove_cvref_t<BufferT>>::type;
    if constexpr (std::is_same_v<ValueType, std::byte>) {
        if constexpr (requires { buffer.reserve(buffer.size() + json.size()); }) {
            buffer.reserve(buffer.size() + json.size());
        }
        buffer.insert(buffer.end(), reinterpret_cast<const std::byte*>(json.data()),
                      reinterpret_cast<const std::byte*>(json.data()) + json.size());
    } else if constexpr (requires { buffer.insert(buffer.end(), json.begin(), json.end()); }) {
        buffer.insert(buffer.end(), json.begin(), json.end());
    } else if constexpr (requires { buffer.write(json.data(), static_cast<std::streamsize>(json.size())); }) {
        buffer.write(json.data(), static_cast<std::streamsize>(json.size()));
    } else {
        static_assert(always_false_v<BufferT>, "Unsupported simdjson output buffer");
    }
}

} // namespace detail::simd

namespace detail {

template <>
struct WriteParser<simd::Writer, simd::SimdJsonValue, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(simd::Writer& writer, const simd::SimdJsonValue& value, const ParentType& parent,
                              const Tags& /*tags*/) {
        if (!value.hasValue()) {
            parsing::Parent<simd::Writer>::addNull(writer, parent);
            return sa::success();
        }
        json::RawValue raw{simdjson::minify(value.nativeValue())};
        parsing::Parent<simd::Writer>::addValue(writer, raw, parent);
        return sa::success();
    }
};

template <>
struct ReadParser<simd::Reader, simd::SimdJsonValue, void> {
    template <typename Tags>
    static ParserResult read(simd::Reader::InputValueType input, simd::SimdJsonValue& value, const Tags& /*tags*/) {
        value = simd::SimdJsonValue(input);
        return sa::success();
    }
};

} // namespace detail

struct SimdJsonBackend {
    using Reader              = detail::simd::Reader;
    using Writer              = detail::simd::Writer;
    using JsonValue           = detail::simd::SimdJsonValue;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        explicit OutputState(BufferT& outputBuffer) noexcept : buffer(outputBuffer) {}

        BufferT& buffer;
        detail::simd::Writer writer;
        bool hasRoot = false;
        bool flushed = false;
    };

    template <typename SourceT>
    struct InputState {
        explicit InputState(const detail::simd::SimdJsonValue& value) noexcept {
            if (!value.hasValue()) {
                result = sa::error(sa::ErrorCode::InvalidType, "SimdJsonValue does not contain a value");
                return;
            }
            retainedValue = value;
            root          = detail::simd::InputValue{retainedValue.nativeValue(), nullptr};
        }

        explicit InputState(const char* buffer, std::size_t size) noexcept
            : parser(std::make_shared<detail::simd::JsonParser>()) {
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            if (size == 0) {
                result = sa::error(sa::ErrorCode::ParseError, "simdjson input is empty");
                return;
            }
            auto parsed = parser->parse(buffer, size);
            if (parsed.error() != simdjson::SUCCESS) {
                result = sa::error(sa::ErrorCode::ParseError,
                                   "simdjson parse error: " + std::string(simdjson::error_message(parsed.error())));
                return;
            }
            root = detail::simd::InputValue{parsed.value_unsafe(), parser};
        }

        detail::simd::InputValue root;
        std::shared_ptr<detail::simd::JsonParser> parser;
        detail::simd::SimdJsonValue retainedValue;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static sa::Result<void> write(OutputState<BufferT>& state, const T& value) {
        state.writer.reset();
        auto result =
            parser_write<detail::simd::Writer>(state.writer, value, parsing::Parent<detail::simd::Writer>::Root{});
        state.hasRoot = static_cast<bool>(result);
        state.flushed = false;
        return result;
    }

    template <typename BufferT>
    static sa::Result<void> finish(OutputState<BufferT>& state, sa::Result<void> result) {
        if (!state.hasRoot || !result) {
            return result;
        }
        if (!state.flushed) {
            const auto json = state.writer.str();
            detail::simd::appendJson(state.buffer, json);
            state.flushed = true;
        }
        return result;
    }

    template <typename BufferT>
    static bool outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept {
        return state.hasRoot && static_cast<bool>(result);
    }

    template <typename SourceT>
    static sa::Result<void> inputResult(const InputState<SourceT>& state) {
        return state.result;
    }

    template <typename SourceT, typename T>
    static sa::Result<void> read(InputState<SourceT>& state, T& value) {
        return parser_read<detail::simd::Reader>(state.root, value);
    }
};

template <typename BufferT = SimdJsonBackend::DefaultOutputBuffer>
class SimdJsonOutputSerializer : public detail::OutputSerializerAdapter<SimdJsonBackend, BufferT> {
public:
    using Base = detail::OutputSerializerAdapter<SimdJsonBackend, BufferT>;
    using Base::Base;
};

template <typename BufferT>
SimdJsonOutputSerializer(BufferT&) -> SimdJsonOutputSerializer<BufferT>;

using SimdJsonByteOutputSerializer = SimdJsonOutputSerializer<std::vector<std::byte>>;

class SimdJsonInputSerializer
    : public detail::InputSerializerAdapter<SimdJsonBackend, SimdJsonBackend::DefaultInputSource> {
public:
    using Base = detail::InputSerializerAdapter<SimdJsonBackend, SimdJsonBackend::DefaultInputSource>;
    using Base::Base;
};

struct SimdJsonSerializer {
    using OutputSerializer     = SimdJsonOutputSerializer<>;
    using ByteOutputSerializer = SimdJsonByteOutputSerializer;
    using InputSerializer      = SimdJsonInputSerializer;
    using JsonValue            = detail::simd::SimdJsonValue;
    using Reader               = detail::simd::Reader;
    using Writer               = detail::simd::Writer;
};

NEKO_END_NAMESPACE

#endif
