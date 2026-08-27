/**
 * @file rapid_json_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "nekoproto/global/log.hpp"
#include "nekoproto/global/reflect.hpp"

#include <cstddef>
#include <cstring>
#include <memory>
#include <ostream>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <type_traits>
#include <utility>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#include <variant>
#endif

#ifdef _WIN32
#pragma push_macro("GetObject")
#ifdef GetObject
#undef GetObject
#endif
#endif

#include "nekoproto/serialization/json/rapid_json_reader.hpp"
#include "nekoproto/serialization/json/rapid_json_writer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/private/helpers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"

namespace nekoproto {

namespace detail {

using JsonValue       = rapidjson::Value;
using ConstJsonValue  = rapidjson::Value;
using JsonObject      = rapidjson::Value::Object;
using ConstJsonObject = rapidjson::Value::ConstObject;
using JsonArray       = rapidjson::Value::Array;
using ConstJsonArray  = rapidjson::Value::ConstArray;
using JsonDocument    = rapidjson::Document;
using OStreamWrapper  = rapidjson::OStreamWrapper;
template <typename BufferT = OutBufferWrapper>
using JsonWriter = rapidjson::Writer<BufferT>;
template <typename BufferT = std::vector<char>>
struct PrettyJsonWriter {};

template <typename T>
struct JsonOutputArgument {
    using sink_type              = T;
    static constexpr bool pretty = false;
};

template <typename T>
struct JsonOutputArgument<PrettyJsonWriter<T>> {
    using sink_type              = T;
    static constexpr bool pretty = true;
};

template <typename SinkT, class enable = void>
struct JsonOutputSinkTraits {
    using output_buffer_type = void;
    using wrapper_type       = void;
};

template <typename T>
struct JsonOutputSinkTraits<T, std::enable_if_t<std::is_base_of_v<std::ostream, std::remove_reference_t<T>>>> {
    using output_buffer_type = std::remove_reference_t<T>;
    using wrapper_type       = rapidjson::BasicOStreamWrapper<output_buffer_type>;
};

template <>
struct JsonOutputSinkTraits<std::vector<char>, void> {
    using output_buffer_type = std::vector<char>;
    using wrapper_type       = OutBufferWrapper;
};

template <>
struct JsonOutputSinkTraits<std::vector<std::byte>, void> {
    using output_buffer_type = std::vector<std::byte>;
    using wrapper_type       = ByteOutBufferWrapper;
};

template <>
struct JsonOutputSinkTraits<OutBufferWrapper, void> {
    using output_buffer_type = std::vector<char>;
    using wrapper_type       = OutBufferWrapper;
};

template <typename BufferT>
struct JsonOutputTraits {
    using argument               = JsonOutputArgument<std::remove_cvref_t<BufferT>>;
    using sink_type              = typename argument::sink_type;
    using sink_traits            = JsonOutputSinkTraits<sink_type>;
    using output_buffer_type     = typename sink_traits::output_buffer_type;
    using wrapper_type           = typename sink_traits::wrapper_type;
    static constexpr bool pretty = argument::pretty;
    static_assert(!std::is_void_v<output_buffer_type>, "Unsupported JSON output buffer");
    using writer_type = std::conditional_t<pretty, rapidjson::PrettyWriter<wrapper_type>, JsonWriter<wrapper_type>>;
};

template <typename T, class enable = void>
struct JsonInputBufferType
    : std::false_type {
    using input_buffer_type = void;
};

template <typename T>
struct JsonInputBufferType<T, typename std::enable_if<std::is_base_of<std::istream, T>::value>::type>
    : std::true_type {
    using input_buffer_type = T;
};

template <typename T, class enable = void>
struct IsPrettyJsonWriter
    : std::false_type {};

template <typename T>
struct IsPrettyJsonWriter<rapidjson::PrettyWriter<T>> : std::true_type {};
} // namespace detail

struct JsonOutputFormatOptions {
public:
    enum class Indent : char {
        Space   = ' ',
        Newline = '\n',
        Tab     = '\t',
    };
    using FormatOptions = rapidjson::PrettyFormatOptions;
    static auto defaultOptions() -> JsonOutputFormatOptions {
        return JsonOutputFormatOptions();
    }
    static auto compact() -> JsonOutputFormatOptions {
        return JsonOutputFormatOptions(Indent::Space, 0);
    }
    explicit JsonOutputFormatOptions(Indent indent_char = Indent::Space, uint32_t indent_length = 4,
                                     FormatOptions format_options = FormatOptions::kFormatSingleLineArray,
                                     int precision               = detail::JsonWriter<>::kDefaultMaxDecimalPlaces)
        : indent_char(static_cast<char>(indent_char)), indent_length(indent_length), format_options(format_options),
          precision(precision) {}

    char          indent_char    = static_cast<char>(Indent::Space);
    int           indent_length  = 4;
    FormatOptions format_options = FormatOptions::kFormatDefault;
    int           precision      = rapidjson::PrettyWriter<detail::OutBufferWrapper>::kDefaultMaxDecimalPlaces;
};

namespace detail {
template <typename T, class enable = void>
struct SetJsonFormatOption {
    static void setting(T& /* unused */, const JsonOutputFormatOptions& /* unused */) {
        NEKO_LOG_INFO("rapidjson", "No output format options support for this writer({})", class_nameof<T>);
    };
};

template <typename T>
struct SetJsonFormatOption<T, typename std::enable_if<IsPrettyJsonWriter<T>::value>::type> {
    static void setting(T& writer, const JsonOutputFormatOptions& options) {
        writer.SetIndent(options.indent_char, options.indent_length);
        writer.SetFormatOptions(options.format_options);
        writer.SetMaxDecimalPlaces(options.precision);
    }
};

class RapidJsonValue {
public:
    RapidJsonValue() = default;
    explicit RapidJsonValue(const JsonValue& value) {
        value_ = std::make_shared<JsonDocument>();
        value_->CopyFrom(value, value_->GetAllocator());
    }
    auto hasValue() const -> bool { return value_ != nullptr; }
    operator bool() const { return hasValue(); }
    auto nativeValue() const -> const JsonValue& { return *value_; }
    auto nativeValue() -> JsonValue& { return *value_; }
    auto isObject() const -> bool { return value_ && value_->IsObject(); }
    auto isArray() const -> bool { return value_ && value_->IsArray(); }
    auto isString() const -> bool { return value_ && value_->IsString(); }
    auto isNumber() const -> bool { return value_ && value_->IsNumber(); }
    auto isBool() const -> bool { return value_ && value_->IsBool(); }
    auto isNull() const -> bool { return value_ && value_->IsNull(); }

    template <typename T>
    auto value(T& value) const -> bool {
        if (value_ && value_->template Is<T>()) {
            value = value_->template Get<T>();
            return true;
        }
        return false;
    }

    auto size() const -> std::size_t {
        if (isArray()) {
            return value_->Size();
        }
        if (isObject()) {
            return value_->MemberCount();
        }
        return 0;
    }

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    auto operator[](const T& name) const -> RapidJsonValue {
        if (isObject()) {
            auto view  = std::string_view(name);
            auto value = value_->FindMember(JsonValue(view.data(), view.size()));
            if (value != value_->MemberEnd()) {
                return RapidJsonValue(value->value);
            }
        }
        return RapidJsonValue();
    }

    auto operator[](std::size_t index) const -> RapidJsonValue {
        if (isArray()) {
            if (index < value_->Size()) {
                return RapidJsonValue(value_->GetArray()[(int)index]);
            }
        }
        if (isObject()) {
            if (index < value_->MemberCount()) {
                return RapidJsonValue((value_->MemberBegin() + index)->value);
            }
        }
        return RapidJsonValue();
    }

private:
    std::shared_ptr<JsonDocument> value_;
};

} // namespace detail

namespace detail {
template <>
struct WriteParser<rapid::Writer, RapidJsonValue, void> {
    template <typename ParentType, typename Tags>
    static auto write(rapid::Writer& writer, const RapidJsonValue& value, const ParentType& parent,
                              const Tags& tags) -> ParserResult {
        if (value.hasValue()) {
            parsing::Parent<rapid::Writer>::addValue(writer, value.nativeValue(), parent, tags);
            return sa::success();
        }
        parsing::Parent<rapid::Writer>::addNull(writer, parent, tags);
        return sa::success();
    }
};

template <>
struct ReadParser<rapid::Reader, RapidJsonValue, void> {
    template <typename Tags>
    static auto read(rapid::Reader::InputValueType in, RapidJsonValue& value, const Tags& /*tags*/) -> ParserResult {
        if (in == nullptr) {
            return makeParserError(sa::ErrorCode::InvalidType, "Cannot read RapidJsonValue from a null input handle");
        }
        value = RapidJsonValue(*in);
        return sa::success();
    }
};
} // namespace detail

struct RapidJsonBackend {
    using Reader              = rapid::Reader;
    using Writer              = rapid::Writer;
    using JsonValue           = detail::RapidJsonValue;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = std::istream;

    template <typename BufferT>
    class OutputState {
    public:
        using OutputTraits = detail::JsonOutputTraits<BufferT>;
        using WriterType   = typename OutputTraits::writer_type;

        explicit OutputState(typename OutputTraits::output_buffer_type& buffer) noexcept : stream(buffer) {}

        OutputState(typename OutputTraits::output_buffer_type& buffer, WriterType&& writer) noexcept : stream(buffer) {
            static_cast<void>(writer);
        }

        OutputState(typename OutputTraits::output_buffer_type& buffer,
                    const JsonOutputFormatOptions& format_options) noexcept
            : stream(buffer), options(format_options), has_format_options(true) {}

        typename OutputTraits::wrapper_type stream;
        rapid::Writer                       writer;
        JsonOutputFormatOptions             options            = JsonOutputFormatOptions::defaultOptions();
        bool                                has_format_options = false;
        bool                                has_root           = false;
        bool                                flushed            = false;
    };

    template <typename BufferT>
    class InputState {
    public:
        explicit InputState(const char* buffer, std::size_t size) noexcept {
            document.Parse(buffer, size);
            if (document.HasParseError()) {
                result = sa::error(sa::ErrorCode::ParseError,
                                   "RapidJSON parse error at offset " + std::to_string(document.GetErrorOffset()) +
                                       ": " + rapidjson::GetParseError_En(document.GetParseError()));
            }
        }

        explicit InputState(const detail::RapidJsonValue& value) noexcept {
            if (value.hasValue()) {
                document.CopyFrom(value.nativeValue(), document.GetAllocator());
            } else {
                result = sa::error(sa::ErrorCode::InvalidType, "RapidJsonValue does not contain a value");
            }
        }

        explicit InputState(BufferT& inputStream) noexcept
            : stream(std::make_unique<rapidjson::BasicIStreamWrapper<BufferT>>(inputStream)) {
            document.ParseStream(*stream);
            if (document.HasParseError()) {
                result = sa::error(sa::ErrorCode::ParseError,
                                   "RapidJSON parse error at offset " + std::to_string(document.GetErrorOffset()) +
                                       ": " + rapidjson::GetParseError_En(document.GetParseError()));
            }
        }

        detail::JsonDocument document;
        std::unique_ptr<rapidjson::BasicIStreamWrapper<BufferT>> stream;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static auto write(OutputState<BufferT>& state, const T& value) -> sa::Result<void> {
        state.writer.doc()->SetNull();
        auto result    = parserWrite<rapid::Writer>(state.writer, value, parsing::Parent<rapid::Writer>::Root{});
        state.has_root = static_cast<bool>(result);
        state.flushed  = false;
        return result;
    }

    template <typename BufferT>
    static auto finish(OutputState<BufferT>& state, sa::Result<void> result) -> sa::Result<void> {
        if (!state.has_root || !result) {
            return result;
        }
        if (state.flushed) {
            return result;
        }

        typename OutputState<BufferT>::WriterType writer(state.stream);
        if (state.has_format_options) {
            detail::SetJsonFormatOption<typename OutputState<BufferT>::WriterType>::setting(writer, state.options);
        }
        const auto flushed = state.writer.doc()->Accept(writer);
        writer.Flush();
        state.flushed = flushed;
        if (!flushed) {
            return sa::error(sa::ErrorCode::ParseError, "RapidJSON failed to flush the output document");
        }
        return result;
    }

    template <typename BufferT>
    static auto outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept -> bool {
        return state.has_root && static_cast<bool>(result);
    }

    template <typename BufferT>
    static auto inputResult(const InputState<BufferT>& state) -> sa::Result<void> {
        return state.result;
    }

    template <typename BufferT, typename T>
    static auto read(InputState<BufferT>& state, T& value) -> sa::Result<void> {
        return parserRead<rapid::Reader>(&state.document, value);
    }
};

template <typename BufferT = RapidJsonBackend::DefaultOutputBuffer>
class RapidJsonOutputSerializer : public detail::OutputSerializerAdapter<RapidJsonBackend, BufferT> {
public:
    using Base = detail::OutputSerializerAdapter<RapidJsonBackend, BufferT>;
    using Base::Base;
};

template <typename BufferT>
RapidJsonOutputSerializer(BufferT&) -> RapidJsonOutputSerializer<BufferT>;

using RapidJsonByteOutputSerializer = RapidJsonOutputSerializer<std::vector<std::byte>>;

template <typename BufferT = RapidJsonBackend::DefaultOutputBuffer>
using RapidJsonPrettyOutputSerializer = RapidJsonOutputSerializer<detail::PrettyJsonWriter<BufferT>>;

template <typename BufferT = RapidJsonBackend::DefaultInputSource>
class RapidJsonInputSerializer : public detail::InputSerializerAdapter<RapidJsonBackend, BufferT> {
public:
    using Base = detail::InputSerializerAdapter<RapidJsonBackend, BufferT>;
    using Base::Base;
};

RapidJsonInputSerializer(const char*, std::size_t) -> RapidJsonInputSerializer<>;
RapidJsonInputSerializer(const detail::RapidJsonValue&) -> RapidJsonInputSerializer<>;

template <typename BufferT>
RapidJsonInputSerializer(BufferT&) -> RapidJsonInputSerializer<BufferT>;

// #####################################################
// default JsonSerializer type definition
struct RapidJsonSerializer {
    using OutputSerializer       = RapidJsonOutputSerializer<>;
    using PrettyOutputSerializer = RapidJsonPrettyOutputSerializer<>;
    using ByteOutputSerializer   = RapidJsonByteOutputSerializer;
    using InputSerializer        = RapidJsonInputSerializer<>;
    using JsonValue              = detail::RapidJsonValue;
    using Reader                 = rapid::Reader;
    using Writer                 = rapid::Writer;
};

} // namespace nekoproto

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif
