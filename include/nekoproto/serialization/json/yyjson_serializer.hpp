#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_YYJSON)

#include <concepts>
#include <cstddef>
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <yyjson.h>

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/json/yyjson_reader.hpp"
#include "nekoproto/serialization/json/yyjson_writer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/tags.hpp"

namespace nekoproto {
namespace detail::yyjson {

class YyJsonValue {
public:
    YyJsonValue() = default;

    YyJsonValue(const yyjson_val* val, std::shared_ptr<yyjson_doc> doc)
        : doc_(std::move(doc)), value_(val) {}

    explicit YyJsonValue(const yyjson_val* val)
        : value_(val) {}

    auto hasValue() const noexcept -> bool { return value_ != nullptr; }
    explicit operator bool() const noexcept { return hasValue(); }

    auto nativeValue() const noexcept -> const yyjson_val* { return value_; }
    auto nativeValue() noexcept -> yyjson_val* { return const_cast<yyjson_val*>(value_); }

    auto doc() const noexcept -> const std::shared_ptr<yyjson_doc>& { return doc_; }

    auto isObject() const noexcept -> bool {
        return value_ && yyjson_is_obj(const_cast<yyjson_val*>(value_));
    }
    auto isArray() const noexcept -> bool {
        return value_ && yyjson_is_arr(const_cast<yyjson_val*>(value_));
    }
    auto isString() const noexcept -> bool {
        return value_ && yyjson_is_str(const_cast<yyjson_val*>(value_));
    }
    auto isNumber() const noexcept -> bool {
        return value_ && yyjson_is_num(const_cast<yyjson_val*>(value_));
    }
    auto isBool() const noexcept -> bool {
        return value_ && yyjson_is_bool(const_cast<yyjson_val*>(value_));
    }
    auto isNull() const noexcept -> bool {
        return value_ && yyjson_is_null(const_cast<yyjson_val*>(value_));
    }

    auto size() const noexcept -> std::size_t {
        if (isArray()) {
            return yyjson_arr_size(const_cast<yyjson_val*>(value_));
        }
        if (isObject()) {
            return yyjson_obj_size(const_cast<yyjson_val*>(value_));
        }
        return 0;
    }

    template <typename T>
    auto value(T& output) const -> bool {
        if (!value_) {
            return false;
        }
        auto result = Reader::toBasicType<T>(value_);
        if (!result) {
            return false;
        }
        output = std::move(result.value());
        return true;
    }

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    auto operator[](const T& name) const -> YyJsonValue {
        if (!isObject()) {
            return {};
        }
        std::string_view sv{name};
        auto* child = yyjson_obj_getn(const_cast<yyjson_val*>(value_), sv.data(), sv.size());
        if (child == nullptr) {
            return {};
        }
        return YyJsonValue(child, doc_);
    }

    auto operator[](std::size_t index) const -> YyJsonValue {
        if (isArray() && index < size()) {
            return YyJsonValue(yyjson_arr_get(const_cast<yyjson_val*>(value_), index), doc_);
        }
        if (isObject() && index < size()) {
            yyjson_obj_iter iter = yyjson_obj_iter_with(const_cast<yyjson_val*>(value_));
            yyjson_val* k = nullptr;
            size_t i = 0;
            while ((k = yyjson_obj_iter_next(&iter))) {
                if (i == index) {
                    return YyJsonValue(yyjson_obj_iter_get_val(k), doc_);
                }
                ++i;
            }
        }
        return {};
    }

private:
    std::shared_ptr<yyjson_doc> doc_;
    const yyjson_val* value_ = nullptr;
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
    } else if constexpr (requires { buffer.append(json.data(), json.size()); }) {
        buffer.append(json.data(), json.size());
    } else if constexpr (requires { buffer.write(json.data(), static_cast<std::streamsize>(json.size())); }) {
        buffer.write(json.data(), static_cast<std::streamsize>(json.size()));
    } else {
        static_assert(always_false_v<BufferT>, "Unsupported yyjson output buffer");
    }
}

} // namespace detail::yyjson

namespace detail {

#ifndef NEKO_JSON_PRETTY_WRITER_DEFINED
#define NEKO_JSON_PRETTY_WRITER_DEFINED
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
#endif

template <>
struct WriteParser<detail::yyjson::Writer, detail::yyjson::YyJsonValue, void> {
    template <typename ParentType, typename Tags>
    static auto write(detail::yyjson::Writer& writer, const detail::yyjson::YyJsonValue& value,
                      const ParentType& parent, const Tags& tags) -> ParserResult {
        if (value.hasValue()) {
            auto* copied = yyjson_val_mut_copy(writer.doc(), const_cast<yyjson_val*>(value.nativeValue()));
            parsing::Parent<detail::yyjson::Writer>::addValue(writer, copied, parent, tags);
            return sa::success();
        }
        parsing::Parent<detail::yyjson::Writer>::addNull(writer, parent, tags);
        return sa::success();
    }
};

template <>
struct ReadParser<detail::yyjson::Reader, detail::yyjson::YyJsonValue, void> {
    template <typename Tags>
    static auto read(detail::yyjson::Reader::InputValueType input, detail::yyjson::YyJsonValue& value,
                     const Tags& /*tags*/) -> ParserResult {
        if (input == nullptr) {
            return makeParserError(sa::ErrorCode::InvalidType, "Cannot read YyJsonValue from a null input handle");
        }
        auto* mdoc = yyjson_mut_doc_new(nullptr);
        auto* mval = yyjson_val_mut_copy(mdoc, const_cast<yyjson_val*>(input));
        auto* new_doc = yyjson_mut_val_imut_copy(mval, nullptr);
        yyjson_mut_doc_free(mdoc);
        if (new_doc == nullptr) {
            return makeParserError(sa::ErrorCode::ParseError, "Failed to copy yyjson value into owned document");
        }
        value = detail::yyjson::YyJsonValue(yyjson_doc_get_root(new_doc),
                                            std::shared_ptr<yyjson_doc>(new_doc, yyjson_doc_free));
        return sa::success();
    }
};

} // namespace detail

struct YyJsonBackend {
    using Reader              = detail::yyjson::Reader;
    using Writer              = detail::yyjson::Writer;
    using JsonValue           = detail::yyjson::YyJsonValue;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = std::istream;

    template <typename BufferT>
    class OutputState {
    public:
        using OutputTraits = detail::JsonOutputArgument<std::remove_cvref_t<BufferT>>;
        using BufferType   = typename OutputTraits::sink_type;
        static constexpr bool pretty = OutputTraits::pretty;

        explicit OutputState(BufferType& buffer) noexcept
            : buffer_(buffer),
              flags_(pretty ? (YYJSON_WRITE_PRETTY | YYJSON_WRITE_PRETTY_TWO_SPACES) : 0) {}

        OutputState(BufferType& buffer, yyjson_write_flag flags) noexcept
            : buffer_(buffer), flags_(flags) {}

        BufferType& buffer_;
        yyjson_write_flag flags_ = 0;
        detail::yyjson::Writer writer_;
        bool has_root = false;
        bool flushed  = false;
    };

    template <typename BufferT>
    class InputState {
    public:
        explicit InputState(const char* buffer, std::size_t size) noexcept {
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            if (size == 0) {
                result = sa::error(sa::ErrorCode::ParseError, "yyjson input is empty");
                return;
            }
            yyjson_read_err err;
            yyjson_doc* doc = yyjson_read_opts(const_cast<char*>(buffer), size, 0, nullptr, &err);
            if (doc == nullptr) {
                result = sa::error(sa::ErrorCode::ParseError,
                                   "yyjson parse error at offset " + std::to_string(err.pos) + ": " +
                                       (err.msg ? err.msg : "unknown"));
                return;
            }
            doc_ = std::shared_ptr<yyjson_doc>(doc, yyjson_doc_free);
            root_ = yyjson_doc_get_root(doc);
        }

        explicit InputState(const detail::yyjson::YyJsonValue& value) noexcept {
            if (!value.hasValue()) {
                result = sa::error(sa::ErrorCode::InvalidType, "YyJsonValue does not contain a value");
                return;
            }
            retained_value_ = value;
            root_ = retained_value_.nativeValue();
        }

        template <typename StreamT>
            requires std::is_base_of_v<std::istream, std::remove_cvref_t<StreamT>>
        explicit InputState(StreamT& stream) noexcept {
            stream_content_ = std::string((std::istreambuf_iterator<char>(stream)),
                                          std::istreambuf_iterator<char>());
            auto size = stream_content_.size();
            const char* buffer = stream_content_.data();
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            if (size == 0) {
                result = sa::error(sa::ErrorCode::ParseError, "yyjson input is empty");
                return;
            }
            yyjson_read_err err;
            yyjson_doc* doc = yyjson_read_opts(const_cast<char*>(buffer), size, 0, nullptr, &err);
            if (doc == nullptr) {
                result = sa::error(sa::ErrorCode::ParseError,
                                   "yyjson parse error at offset " + std::to_string(err.pos) + ": " +
                                       (err.msg ? err.msg : "unknown"));
                return;
            }
            doc_ = std::shared_ptr<yyjson_doc>(doc, yyjson_doc_free);
            root_ = yyjson_doc_get_root(doc);
        }

        std::shared_ptr<yyjson_doc> doc_;
        const yyjson_val* root_ = nullptr;
        detail::yyjson::YyJsonValue retained_value_;
        std::string stream_content_;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static auto write(OutputState<BufferT>& state, const T& value) -> sa::Result<void> {
        auto result = parserWrite<detail::yyjson::Writer>(
            state.writer_, value, typename parsing::Parent<detail::yyjson::Writer>::Root{});
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
        size_t len = 0;
        yyjson_write_err err;
        char* json_str = yyjson_mut_write_opts(state.writer_.doc(), state.flags_, nullptr, &len, &err);
        if (json_str == nullptr) {
            return sa::error(sa::ErrorCode::Unknown,
                             err.msg ? std::string(err.msg) : "yyjson serialization failed");
        }
        detail::yyjson::appendJson(state.buffer_, std::string_view(json_str, len));
        free(json_str);
        state.flushed = true;
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
        return parserRead<detail::yyjson::Reader>(state.root_, value);
    }
};

template <typename BufferT = YyJsonBackend::DefaultOutputBuffer>
class YyJsonOutputSerializer : public detail::OutputSerializerAdapter<YyJsonBackend, BufferT> {
public:
    using Base = detail::OutputSerializerAdapter<YyJsonBackend, BufferT>;
    using Base::Base;
};

template <typename BufferT>
YyJsonOutputSerializer(BufferT&) -> YyJsonOutputSerializer<BufferT>;

using YyJsonByteOutputSerializer = YyJsonOutputSerializer<std::vector<std::byte>>;

template <typename BufferT = YyJsonBackend::DefaultOutputBuffer>
using YyJsonPrettyOutputSerializer = YyJsonOutputSerializer<detail::PrettyJsonWriter<BufferT>>;

template <typename BufferT = YyJsonBackend::DefaultInputSource>
class YyJsonInputSerializer : public detail::InputSerializerAdapter<YyJsonBackend, BufferT> {
public:
    using Base = detail::InputSerializerAdapter<YyJsonBackend, BufferT>;
    using Base::Base;
};

YyJsonInputSerializer(const char*, std::size_t) -> YyJsonInputSerializer<>;
YyJsonInputSerializer(const detail::yyjson::YyJsonValue&) -> YyJsonInputSerializer<>;

template <typename BufferT>
YyJsonInputSerializer(BufferT&) -> YyJsonInputSerializer<BufferT>;

struct YyJsonSerializer {
    using OutputSerializer       = YyJsonOutputSerializer<>;
    using PrettyOutputSerializer = YyJsonPrettyOutputSerializer<>;
    using ByteOutputSerializer   = YyJsonByteOutputSerializer;
    using InputSerializer        = YyJsonInputSerializer<>;
    using JsonValue              = detail::yyjson::YyJsonValue;
    using Reader                 = detail::yyjson::Reader;
    using Writer                 = detail::yyjson::Writer;
};

} // namespace nekoproto

#endif
