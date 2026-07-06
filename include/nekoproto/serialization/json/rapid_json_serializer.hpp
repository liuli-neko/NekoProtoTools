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

NEKO_BEGIN_NAMESPACE

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
struct json_output_argument { // NOLINT(readability-identifier-naming)
    using sink_type              = T;
    static constexpr bool pretty = false;
};

template <typename T>
struct json_output_argument<PrettyJsonWriter<T>> { // NOLINT(readability-identifier-naming)
    using sink_type              = T;
    static constexpr bool pretty = true;
};

template <typename SinkT, class enable = void>
struct json_output_sink_traits { // NOLINT(readability-identifier-naming)
    using output_buffer_type = void;
    using wrapper_type       = void;
};

template <typename T>
struct json_output_sink_traits<T, std::enable_if_t<std::is_base_of_v<std::ostream, std::remove_reference_t<T>>>> {
    using output_buffer_type = std::remove_reference_t<T>;
    using wrapper_type       = rapidjson::BasicOStreamWrapper<output_buffer_type>;
};

template <>
struct json_output_sink_traits<std::vector<char>, void> {
    using output_buffer_type = std::vector<char>;
    using wrapper_type       = OutBufferWrapper;
};

template <>
struct json_output_sink_traits<std::vector<std::byte>, void> {
    using output_buffer_type = std::vector<std::byte>;
    using wrapper_type       = ByteOutBufferWrapper;
};

template <>
struct json_output_sink_traits<OutBufferWrapper, void> {
    using output_buffer_type = std::vector<char>;
    using wrapper_type       = OutBufferWrapper;
};

template <typename BufferT>
struct json_output_traits { // NOLINT(readability-identifier-naming)
    using argument               = json_output_argument<std::remove_cvref_t<BufferT>>;
    using sink_type              = typename argument::sink_type;
    using sink_traits            = json_output_sink_traits<sink_type>;
    using output_buffer_type     = typename sink_traits::output_buffer_type;
    using wrapper_type           = typename sink_traits::wrapper_type;
    static constexpr bool pretty = argument::pretty;
    static_assert(!std::is_void_v<output_buffer_type>, "Unsupported JSON output buffer");
    using writer_type = std::conditional_t<pretty, rapidjson::PrettyWriter<wrapper_type>, JsonWriter<wrapper_type>>;
};

template <typename T, class enable = void>
struct json_input_buffer_type // NOLINT(readability-identifier-naming)
    : std::false_type {
    using input_buffer_type = void;
};

template <typename T>
struct json_input_buffer_type<T, typename std::enable_if<std::is_base_of<std::istream, T>::value>::type>
    : std::true_type {
    using input_buffer_type = T;
};

template <typename T, class enable = void>
struct is_pretty_json_writer // NOLINT(readability-identifier-naming)
    : std::false_type {};

template <typename T>
struct is_pretty_json_writer<rapidjson::PrettyWriter<T>> : std::true_type {};
} // namespace detail

struct JsonOutputFormatOptions {
public:
    enum class Indent : char {
        Space   = ' ',
        Newline = '\n',
        Tab     = '\t',
    };
    using FormatOptions = rapidjson::PrettyFormatOptions;
    static JsonOutputFormatOptions Default() { // NOLINT(readability-identifier-naming)
        return JsonOutputFormatOptions();
    }
    static JsonOutputFormatOptions Compact() { // NOLINT(readability-identifier-naming)
        return JsonOutputFormatOptions(Indent::Space, 0);
    }
    explicit JsonOutputFormatOptions(Indent indentChar = Indent::Space, uint32_t indentLength = 4,
                                     FormatOptions formatOptions = FormatOptions::kFormatSingleLineArray,
                                     int precision               = detail::JsonWriter<>::kDefaultMaxDecimalPlaces)
        : indentChar(static_cast<char>(indentChar)), indentLength(indentLength), formatOptions(formatOptions),
          precision(precision) {}

    char indentChar             = static_cast<char>(Indent::Space);
    int indentLength            = 4;
    FormatOptions formatOptions = FormatOptions::kFormatDefault;
    int precision               = rapidjson::PrettyWriter<detail::OutBufferWrapper>::kDefaultMaxDecimalPlaces;
};

namespace detail {
template <typename T, class enable = void>
struct set_json_format_option { // NOLINT(readability-identifier-naming)
    static void setting(T& /* unused */, const JsonOutputFormatOptions& /* unused */) {
        NEKO_LOG_INFO("rapidjson", "No output format options support for this writer({})", class_nameof<T>);
    };
};

template <typename T>
struct set_json_format_option<T, typename std::enable_if<is_pretty_json_writer<T>::value>::type> {
    static void setting(T& writer, const JsonOutputFormatOptions& options) {
        writer.SetIndent(options.indentChar, options.indentLength);
        writer.SetFormatOptions(options.formatOptions);
        writer.SetMaxDecimalPlaces(options.precision);
    }
};

class RapidJsonValue {
public:
    RapidJsonValue() = default;
    explicit RapidJsonValue(const JsonValue& value) {
        mValue = std::make_shared<JsonDocument>();
        mValue->CopyFrom(value, mValue->GetAllocator());
    }
    auto hasValue() const -> bool { return mValue != nullptr; }
    operator bool() const { return hasValue(); }
    auto nativeValue() const -> const JsonValue& { return *mValue; }
    auto nativeValue() -> JsonValue& { return *mValue; }
    auto isObject() const -> bool { return mValue && mValue->IsObject(); }
    auto isArray() const -> bool { return mValue && mValue->IsArray(); }
    auto isString() const -> bool { return mValue && mValue->IsString(); }
    auto isNumber() const -> bool { return mValue && mValue->IsNumber(); }
    auto isBool() const -> bool { return mValue && mValue->IsBool(); }
    auto isNull() const -> bool { return mValue && mValue->IsNull(); }

    template <typename T>
    auto value(T& value) const -> bool {
        if (mValue && mValue->template Is<T>()) {
            value = mValue->template Get<T>();
            return true;
        }
        return false;
    }

    auto size() const -> std::size_t {
        if (isArray()) {
            return mValue->Size();
        }
        if (isObject()) {
            return mValue->MemberCount();
        }
        return 0;
    }

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    auto operator[](const T& name) const -> RapidJsonValue {
        if (isObject()) {
            auto view  = std::string_view(name);
            auto value = mValue->FindMember(JsonValue(view.data(), view.size()));
            if (value != mValue->MemberEnd()) {
                return RapidJsonValue(value->value);
            }
        }
        return RapidJsonValue();
    }

    auto operator[](std::size_t index) const -> RapidJsonValue {
        if (isArray()) {
            if (index < mValue->Size()) {
                return RapidJsonValue(mValue->GetArray()[(int)index]);
            }
        }
        if (isObject()) {
            if (index < mValue->MemberCount()) {
                return RapidJsonValue((mValue->MemberBegin() + index)->value);
            }
        }
        return RapidJsonValue();
    }

private:
    std::shared_ptr<JsonDocument> mValue;
};

} // namespace detail

namespace detail {
template <>
struct WriteParser<rapid::Writer, RapidJsonValue, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(rapid::Writer& writer, const RapidJsonValue& value, const ParentType& parent,
                              const Tags& tags) {
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
    static ParserResult read(rapid::Reader::InputValueType in, RapidJsonValue& value, const Tags& /*tags*/) {
        if (in == nullptr) {
            return parser_error(sa::ErrorCode::InvalidType, "Cannot read RapidJsonValue from a null input handle");
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
        using OutputTraits = detail::json_output_traits<BufferT>;
        using WriterType   = typename OutputTraits::writer_type;

        explicit OutputState(typename OutputTraits::output_buffer_type& buffer) noexcept : stream(buffer) {}

        OutputState(typename OutputTraits::output_buffer_type& buffer, WriterType&& writer) noexcept : stream(buffer) {
            static_cast<void>(writer);
        }

        OutputState(typename OutputTraits::output_buffer_type& buffer,
                    const JsonOutputFormatOptions& formatOptions) noexcept
            : stream(buffer), options(formatOptions), hasFormatOptions(true) {}

        typename OutputTraits::wrapper_type stream;
        rapid::Writer writer;
        JsonOutputFormatOptions options = JsonOutputFormatOptions::Default();
        bool hasFormatOptions           = false;
        bool hasRoot                    = false;
        bool flushed                    = false;
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
    static sa::Result<void> write(OutputState<BufferT>& state, const T& value) {
        state.writer.doc()->SetNull();
        auto result   = parser_write<rapid::Writer>(state.writer, value, parsing::Parent<rapid::Writer>::Root{});
        state.hasRoot = static_cast<bool>(result);
        state.flushed = false;
        return result;
    }

    template <typename BufferT>
    static sa::Result<void> finish(OutputState<BufferT>& state, sa::Result<void> result) {
        if (!state.hasRoot || !result) {
            return result;
        }
        if (state.flushed) {
            return result;
        }

        typename OutputState<BufferT>::WriterType writer(state.stream);
        if (state.hasFormatOptions) {
            detail::set_json_format_option<typename OutputState<BufferT>::WriterType>::setting(writer, state.options);
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
    static bool outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept {
        return state.hasRoot && static_cast<bool>(result);
    }

    template <typename BufferT>
    static sa::Result<void> inputResult(const InputState<BufferT>& state) {
        return state.result;
    }

    template <typename BufferT, typename T>
    static sa::Result<void> read(InputState<BufferT>& state, T& value) {
        return parser_read<rapid::Reader>(&state.document, value);
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
    using OutputSerializer     = RapidJsonOutputSerializer<>;
    using ByteOutputSerializer = RapidJsonByteOutputSerializer;
    using InputSerializer      = RapidJsonInputSerializer<>;
    using JsonValue            = detail::RapidJsonValue;
    using Reader               = rapid::Reader;
    using Writer               = rapid::Writer;
};

NEKO_END_NAMESPACE

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif
