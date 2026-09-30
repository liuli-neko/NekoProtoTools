#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "nekoproto/global/log.hpp"
#include "nekoproto/serialization/error.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <rapidjson/rapidjson.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifdef _WIN32
#pragma push_macro("GetObject")
#ifdef GetObject
#undef GetObject
#endif
#endif

namespace nekoproto {
namespace rapid {

template <typename RapidJsonWriterT>
class StreamWriter {
public:
    static constexpr bool requires_field_count = false;

    using RawValueType = rapidjson::Document;

    static auto parseRawValue(std::string_view text, RawValueType& value) -> bool {
        value.Parse(text.data(), text.size());
        return !value.HasParseError();
    }

    struct OutputArrayType {
        StreamWriter* writer  = nullptr;
        bool          closed  = false;

        OutputArrayType() = default;
        explicit OutputArrayType(StreamWriter* w) noexcept : writer(w) {}
        OutputArrayType(OutputArrayType&& other) noexcept
            : writer(other.writer), closed(other.closed) {
            other.writer = nullptr;
            other.closed = true;
        }
        OutputArrayType& operator=(OutputArrayType&& other) noexcept {
            if (this != &other) {
                close();
                writer       = other.writer;
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
                writer->rawWriter()->EndArray();
                closed = true;
            }
        }
    };

    struct OutputObjectType {
        StreamWriter* writer  = nullptr;
        bool          closed  = false;

        OutputObjectType() = default;
        explicit OutputObjectType(StreamWriter* w) noexcept : writer(w) {}
        OutputObjectType(OutputObjectType&& other) noexcept
            : writer(other.writer), closed(other.closed) {
            other.writer = nullptr;
            other.closed = true;
        }
        OutputObjectType& operator=(OutputObjectType&& other) noexcept {
            if (this != &other) {
                close();
                writer       = other.writer;
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
                writer->rawWriter()->EndObject();
                closed = true;
            }
        }
    };

    struct OutputValueType {};

    explicit StreamWriter(RapidJsonWriterT* raw_writer) noexcept : writer_(raw_writer) {}
    ~StreamWriter() = default;

    auto rawWriter() noexcept -> RapidJsonWriterT* { return writer_; }
    auto rawWriter() const noexcept -> const RapidJsonWriterT* { return writer_; }

    auto arrayAsRoot(std::size_t /*size*/ = 0) -> OutputArrayType {
        writer_->StartArray();
        return OutputArrayType{this};
    }

    auto objectAsRoot(std::size_t /*size*/ = 0) -> OutputObjectType {
        writer_->StartObject();
        return OutputObjectType{this};
    }

    auto nullAsRoot() -> OutputValueType {
        writer_->Null();
        return {};
    }

    template <typename T>
    auto valueAsRoot(const T& value) -> OutputValueType {
        writeBasicType(value);
        return {};
    }

    auto addArrayToArray(std::size_t /*size*/, OutputArrayType* /*parent*/) -> OutputArrayType {
        writer_->StartArray();
        return OutputArrayType{this};
    }

    auto addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* /*parent*/)
        -> OutputArrayType {
        writer_->Key(name.data(), static_cast<rapidjson::SizeType>(name.size()), false);
        writer_->StartArray();
        return OutputArrayType{this};
    }

    auto addObjectToArray(std::size_t /*size*/, OutputArrayType* /*parent*/) -> OutputObjectType {
        writer_->StartObject();
        return OutputObjectType{this};
    }

    auto addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* /*parent*/)
        -> OutputObjectType {
        writer_->Key(name.data(), static_cast<rapidjson::SizeType>(name.size()), false);
        writer_->StartObject();
        return OutputObjectType{this};
    }

    template <typename T>
    auto addValueToArray(const T& value, OutputArrayType* /*parent*/) -> OutputValueType {
        writeBasicType(value);
        return {};
    }

    template <typename T>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* /*parent*/)
        -> OutputValueType {
        writer_->Key(name.data(), static_cast<rapidjson::SizeType>(name.size()), false);
        writeBasicType(value);
        return {};
    }

    auto addNullToArray(OutputArrayType* /*parent*/) -> OutputValueType {
        writer_->Null();
        return {};
    }

    auto addNullToObject(std::string_view name, OutputObjectType* /*parent*/) -> OutputValueType {
        writer_->Key(name.data(), static_cast<rapidjson::SizeType>(name.size()), false);
        writer_->Null();
        return {};
    }

    void endArray(OutputArrayType* array) noexcept {
        if (array != nullptr) {
            array->close();
        }
    }

    void endObject(OutputObjectType* object) noexcept {
        if (object != nullptr) {
            object->close();
        }
    }

    template <typename T>
    void writeBasicType(const T& value) {
        using U = std::remove_cv_t<std::remove_reference_t<T>>;
        if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
            writer_->String(value.data(), static_cast<rapidjson::SizeType>(value.size()), false);
        } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
            writer_->String(value, static_cast<rapidjson::SizeType>(std::strlen(value)), false);
        } else if constexpr (requires { const_cast<U&>(value).Accept(*writer_); }) {
            const_cast<U&>(value).Accept(*writer_);
        } else if constexpr (std::is_same_v<U, bool>) {
            writer_->Bool(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            writer_->Double(static_cast<double>(value));
        } else if constexpr (std::is_enum_v<U>) {
            using I = std::underlying_type_t<U>;
            writeBasicType(static_cast<I>(value));
        } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
            if constexpr (sizeof(U) <= sizeof(int)) {
                writer_->Int(static_cast<int>(value));
            } else {
                writer_->Int64(static_cast<std::int64_t>(value));
            }
        } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>) {
            if constexpr (sizeof(U) <= sizeof(unsigned)) {
                writer_->Uint(static_cast<unsigned>(value));
            } else {
                writer_->Uint64(static_cast<std::uint64_t>(value));
            }
        } else {
            static_assert(always_false_v<U>, "Unsupported JSON basic type for RapidJSON streaming writer");
        }
    }

private:
    RapidJsonWriterT* writer_ = nullptr;
};

} // namespace rapid
} // namespace nekoproto

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif
#endif // NEKO_PROTO_ENABLE_RAPIDJSON
