#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/supports_fixed_length.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace nekoproto {
namespace detail {

template <typename CharT>
inline constexpr bool ParserIsByteCharV = sizeof(CharT) == sizeof(char);

template <typename T>
auto parserArithmeticSchema() -> parsing::schema::Type {
    using Schema = parsing::schema::Type;
    if constexpr (std::is_same_v<T, bool>) {
        return Schema::Boolean{};
    } else if constexpr (std::is_floating_point_v<T>) {
        return Schema::FloatingPoint{
            .minimum = static_cast<double>(std::numeric_limits<T>::lowest()),
            .maximum = static_cast<double>(std::numeric_limits<T>::max()),
        };
    } else if constexpr (std::is_signed_v<T>) {
        return Schema::Integer{
            .minimum = static_cast<std::int64_t>(std::numeric_limits<T>::min()),
            .maximum = static_cast<std::int64_t>(std::numeric_limits<T>::max()),
        };
    } else {
        return Schema::Integer{
            .minimum = static_cast<std::uint64_t>(std::numeric_limits<T>::min()),
            .maximum = static_cast<std::uint64_t>(std::numeric_limits<T>::max()),
        };
    }
}

template <typename W, typename ParentType, typename Tags>
auto parserWriteString(W& writer, std::string_view value, const ParentType& parent, const Tags& tags) -> ParserResult {
    if (tag_query::get<tag_property::RawString>(tags)) {
        if constexpr (requires(std::string_view text, typename W::RawValueType& raw) { W::parseRawValue(text, raw); }) {
            typename W::RawValueType raw;
            if (!W::parseRawValue(value, raw)) {
                return parserError(sa::ErrorCode::ParseError, "Invalid raw value");
            }
            parsing::Parent<W>::addValue(writer, raw, parent, tags);
            return sa::success();
        }
    }
    parsing::Parent<W>::addValue(writer, value, parent, tags);
    return sa::success();
}

template <typename R, typename Tags>
auto parserReadString(typename R::InputValueType in, std::string& value, const Tags& tags) -> ParserResult {
    if (tag_query::get<tag_property::RawString>(tags)) {
        if constexpr (requires { parsing::readerToRawString<R>(in, tags); }) {
            auto raw = parsing::readerToRawString<R>(in, tags);
            if (!raw) {
                return raw.error();
            }
            value = raw.value();
            return sa::success();
        }
    }
    auto result = parsing::readerToBasic<R, std::string>(in, tags);
    if (!result) {
        return result.error();
    }
    value = result.value();
    return sa::success();
}

template <typename W, typename T>
struct WriteParser<W, T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const T& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if (tag_query::has<tag_property::FixedLength<void>>(tags)) {
            if constexpr (parsing::supports_fixed_length_writer<W, T, Tags>) {
                const auto fixed_length = tag_query::get<tag_property::FixedLength<T>>(tags);
                if (fixed_length != sizeof(T)) {
                    return parserError(sa::ErrorCode::InvalidLength,
                                        "Fixed-length arithmetic field requires " + std::to_string(sizeof(T)) +
                                            " bytes for its C++ type, got " + std::to_string(fixed_length));
                }
                parsing::Parent<W>::addFixedValue(writer, value, fixed_length, parent, tags);
                return sa::success();
            }
        }
        parsing::Parent<W>::addValue(writer, value, parent, tags);
        return sa::success();
    }
};

template <typename R, typename T>
struct ReadParser<R, T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
        if (tag_query::has<tag_property::FixedLength<void>>(tags)) {
            if constexpr (parsing::supports_fixed_length_reader<R, T, Tags>) {
                const auto fixed_length = tag_query::get<tag_property::FixedLength<T>>(tags);
                if (fixed_length != sizeof(T)) {
                    return parserError(sa::ErrorCode::InvalidLength,
                                        "Fixed-length arithmetic field requires " + std::to_string(sizeof(T)) +
                                            " bytes for its C++ type, got " + std::to_string(fixed_length));
                }
                auto result = parsing::readerToFixedBasic<R, T>(in, fixed_length, tags);
                if (!result) {
                    return result.error();
                }
                value = result.value();
                return sa::success();
            }
        }
        auto result = parsing::readerToBasic<R, T>(in, tags);
        if (!result) {
            return result.error();
        }
        value = result.value();
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static auto toSchema() -> parsing::schema::Type { return parserArithmeticSchema<T>(); }
};

template <typename W>
struct WriteParser<W, std::nullptr_t, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, std::nullptr_t, const ParentType& parent, const Tags& tags) -> ParserResult {
        parsing::Parent<W>::addNull(writer, parent, tags);
        return sa::success();
    }
};

template <typename R>
struct ReadParser<R, std::nullptr_t, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, std::nullptr_t& /*value*/, const Tags& tags) -> ParserResult {
        if (!parsing::readerIsEmpty<R>(in, tags)) {
            return parserError(sa::ErrorCode::InvalidType, "Expected null");
        }
        return sa::success();
    }
};

template <>
struct SchemaParser<std::nullptr_t, void> {
    static auto toSchema() -> parsing::schema::Type { return parsing::schema::Type::Null{}; }
};

template <typename W>
struct WriteParser<W, const char*, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const char* value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if (value == nullptr) {
            parsing::Parent<W>::addNull(writer, parent, tags);
            return sa::success();
        }
        return parserWriteString(writer, std::string_view{value}, parent, tags);
    }
};

template <typename R>
struct ReadParser<R, const char*, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType /*in*/, const char*& /*value*/, const Tags& /*tags*/) -> ParserResult {
        return parserError(sa::ErrorCode::InvalidType, "Reading into const char* is unsupported");
    }
};

template <>
struct SchemaParser<const char*, void> {
    static auto toSchema() -> parsing::schema::Type { return parsing::schema::Type::String{}; }
};

template <typename W>
struct WriteParser<W, char*, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const char* value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return WriteParser<W, const char*>::write(writer, value, parent, tags);
    }
};

template <typename R>
struct ReadParser<R, char*, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType /*in*/, char*& /*value*/, const Tags& /*tags*/) -> ParserResult {
        return parserError(sa::ErrorCode::InvalidType, "Reading into char* is unsupported");
    }
};

template <>
struct SchemaParser<char*, void> {
    static auto toSchema() -> parsing::schema::Type { return parsing::schema::Type::String{}; }
};

template <typename W, typename CharT, typename Traits, typename Alloc>
struct WriteParser<W, std::basic_string<CharT, Traits, Alloc>, void> {
    using String = std::basic_string<CharT, Traits, Alloc>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const String& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if constexpr (std::is_convertible_v<String, std::string_view>) {
            return parserWriteString(writer, std::string_view{value}, parent, tags);
        } else {
            static_assert(ParserIsByteCharV<CharT>, "Serialized strings must use byte-sized characters");
            return parserWriteString(
                writer, std::string_view{reinterpret_cast<const char*>(value.data()), value.size()}, parent, tags);
        }
    }
};

template <typename R, typename CharT, typename Traits, typename Alloc>
struct ReadParser<R, std::basic_string<CharT, Traits, Alloc>, void> {
    using String = std::basic_string<CharT, Traits, Alloc>;

    template <typename Tags>
    static auto read(typename R::InputValueType in, String& value, const Tags& tags) -> ParserResult {
        static_assert(ParserIsByteCharV<CharT>, "Serialized strings must use byte-sized characters");
        std::string tmp;
        auto result = parserReadString<R>(in, tmp, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse string: ");
        }
        value.assign(reinterpret_cast<const CharT*>(tmp.data()), tmp.size());
        return sa::success();
    }
};

template <typename CharT, typename Traits, typename Alloc>
struct SchemaParser<std::basic_string<CharT, Traits, Alloc>, void> {
    static auto toSchema() -> parsing::schema::Type { return parsing::schema::Type::String{}; }
};

template <typename W, typename CharT, typename Traits>
struct WriteParser<W, std::basic_string_view<CharT, Traits>, void> {
    using StringView = std::basic_string_view<CharT, Traits>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, StringView value, const ParentType& parent, const Tags& tags) -> ParserResult {
        static_assert(ParserIsByteCharV<CharT>, "Serialized string views must use byte-sized characters");
        return parserWriteString(writer, std::string_view{reinterpret_cast<const char*>(value.data()), value.size()},
                                   parent, tags);
    }
};

template <typename R, typename CharT, typename Traits>
struct ReadParser<R, std::basic_string_view<CharT, Traits>, void> {
    using StringView = std::basic_string_view<CharT, Traits>;

    template <typename Tags>
    static auto read(typename R::InputValueType in, StringView& value, const Tags& tags) -> ParserResult {
        static_assert(ParserIsByteCharV<CharT>, "Serialized string views must use byte-sized characters");
        if (tag_query::get<tag_property::RawString>(tags)) {
            if constexpr (requires { parsing::readerToRawString<R>(in, tags); }) {
                return parserError(sa::ErrorCode::InvalidType,
                                    "raw_string cannot be read into string_view without owned storage");
            }
        }
        if constexpr (requires { parsing::readerToStringView<R, CharT, Traits>(in, tags); }) {
            auto result = parsing::readerToStringView<R, CharT, Traits>(in, tags);
            if (!result) {
                return result.error();
            }
            value = result.value();
            return sa::success();
        }
        return parserError(sa::ErrorCode::InvalidType, "Reader does not support string_view");
    }
};

template <typename CharT, typename Traits>
struct SchemaParser<std::basic_string_view<CharT, Traits>, void> {
    static auto toSchema() -> parsing::schema::Type { return parsing::schema::Type::String{}; }
};

template <typename W, typename T>
struct WriteParser<W, T, std::enable_if_t<std::is_enum_v<T>>> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const T& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        auto name = Reflect<T>::name(value);
        if (!name.empty()) {
            return parserWriteString(writer, name, parent, tags);
        }
        return parserWrite<W>(writer, static_cast<std::underlying_type_t<T>>(value), parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, T, std::enable_if_t<std::is_enum_v<T>>> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, T& value, const Tags& tags) -> ParserResult {
        std::string enumName;
        if (parserReadString<R>(in, enumName, tags)) {
            if (auto it = Reflect<T>::nameMap().find(enumName); it != Reflect<T>::nameMap().end()) {
                value = it->second;
                return sa::success();
            }
            return parserError(sa::ErrorCode::InvalidType, "Unknown enum name '" + enumName + "'");
        }
        std::underlying_type_t<T> raw{};
        auto result = parserRead<R>(in, raw, tags);
        if (!result) {
            return parserContext(std::move(result), "Failed to parse enum value: ");
        }
        value = static_cast<T>(raw);
        return sa::success();
    }
};

template <typename T>
struct SchemaParser<T, std::enable_if_t<std::is_enum_v<T>>> {
    static auto toSchema() -> parsing::schema::Type {
        parsing::schema::Type::String schema;
        for (const auto name : Reflect<T>::names()) {
            schema.enumeration.emplace_back(name);
        }
        return schema;
    }
};

} // namespace detail
} // namespace nekoproto
