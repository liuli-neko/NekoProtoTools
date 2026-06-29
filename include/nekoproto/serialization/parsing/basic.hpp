#pragma once

#include "nekoproto/serialization/parsing/parser.hpp"
#include "nekoproto/serialization/parsing/supports_fixed_length.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename CharT>
inline constexpr bool ParserIsByteCharV = sizeof(CharT) == sizeof(char);

template <typename T>
parsing::schema::Type parser_arithmetic_schema() {
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
ParserResult parser_write_string(W& writer, std::string_view value, const ParentType& parent, const Tags& tags) {
    if (tag_query::raw_string(tags)) {
        if constexpr (requires(std::string_view text, typename W::RawValueType& raw) {
                          W::parseRawValue(text, raw);
                      }) {
            typename W::RawValueType raw;
            if (!W::parseRawValue(value, raw)) {
                return parser_error(sa::ErrorCode::ParseError, "Invalid raw value");
            }
            parsing::Parent<W>::addValue(writer, raw, parent);
            return sa::success();
        }
    }
    parsing::Parent<W>::addValue(writer, value, parent);
    return sa::success();
}

template <typename R, typename Tags>
ParserResult parser_read_string(typename R::InputValueType in, std::string& value, const Tags& tags) {
    if (tag_query::raw_string(tags)) {
        if constexpr (requires { R::toRawString(in); }) {
            auto raw = R::toRawString(in);
            if (!raw) {
                return raw.error();
            }
            value = raw.value();
            return sa::success();
        }
    }
    auto result = R::template toBasicType<std::string>(in);
    if (!result) {
        return result.error();
    }
    value = result.value();
    return sa::success();
}

template <typename R, typename W, typename T>
struct Parser<R, W, T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
        if (tag_query::has_fixed_length(tags)) {
            if constexpr (parsing::supports_fixed_length<R, W, T>) {
                const auto fixedLength = tag_query::fixed_length<T>(tags);
                if (fixedLength != sizeof(T)) {
                    return parser_error(
                        sa::ErrorCode::InvalidLength,
                        "Fixed-length arithmetic field requires " + std::to_string(sizeof(T)) +
                            " bytes for its C++ type, got " + std::to_string(fixedLength));
                }
                parsing::Parent<W>::addFixedValue(writer, value, fixedLength, parent);
                return sa::success();
            }
        }
        parsing::Parent<W>::addValue(writer, value, parent);
        return sa::success();
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, T& value, const Tags& tags) {
        if (tag_query::has_fixed_length(tags)) {
            if constexpr (parsing::supports_fixed_length<R, W, T>) {
                const auto fixedLength = tag_query::fixed_length<T>(tags);
                if (fixedLength != sizeof(T)) {
                    return parser_error(
                        sa::ErrorCode::InvalidLength,
                        "Fixed-length arithmetic field requires " + std::to_string(sizeof(T)) +
                            " bytes for its C++ type, got " + std::to_string(fixedLength));
                }
                auto result = R::template toFixedBasicType<T>(in, fixedLength);
                if (!result) {
                    return result.error();
                }
                value = result.value();
                return sa::success();
            }
        }
        auto result = R::template toBasicType<T>(in);
        if (!result) {
            return result.error();
        }
        value = result.value();
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parser_arithmetic_schema<T>(); }
};

template <typename R, typename W>
struct Parser<R, W, std::nullptr_t, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, std::nullptr_t, const ParentType& parent, const Tags& /*tags*/) {
        parsing::Parent<W>::addNull(writer, parent);
        return sa::success();
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, std::nullptr_t& /*value*/, const Tags& /*tags*/) {
        if (!R::isEmpty(in)) {
            return parser_error(sa::ErrorCode::InvalidType, "Expected null");
        }
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parsing::schema::Type::Null{}; }
};

template <typename R, typename W>
struct Parser<R, W, const char*, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const char* value, const ParentType& parent, const Tags& tags) {
        if (value == nullptr) {
            parsing::Parent<W>::addNull(writer, parent);
            return sa::success();
        }
        return parser_write_string(writer, std::string_view{value}, parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType /*in*/, const char*& /*value*/, const Tags& /*tags*/) {
        return parser_error(sa::ErrorCode::InvalidType, "Reading into const char* is unsupported");
    }

    static parsing::schema::Type toSchema() { return parsing::schema::Type::String{}; }
};

template <typename R, typename W>
struct Parser<R, W, char*, void> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const char* value, const ParentType& parent, const Tags& tags) {
        return Parser<R, W, const char*>::write(writer, value, parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType /*in*/, char*& /*value*/, const Tags& /*tags*/) {
        return parser_error(sa::ErrorCode::InvalidType, "Reading into char* is unsupported");
    }

    static parsing::schema::Type toSchema() { return parsing::schema::Type::String{}; }
};

template <typename R, typename W, typename CharT, typename Traits, typename Alloc>
struct Parser<R, W, std::basic_string<CharT, Traits, Alloc>, void> {
    using String = std::basic_string<CharT, Traits, Alloc>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const String& value, const ParentType& parent, const Tags& tags) {
        if constexpr (std::is_convertible_v<String, std::string_view>) {
            return parser_write_string(writer, std::string_view{value}, parent, tags);
        } else {
            static_assert(ParserIsByteCharV<CharT>, "Serialized strings must use byte-sized characters");
            return parser_write_string(
                writer, std::string_view{reinterpret_cast<const char*>(value.data()), value.size()}, parent, tags);
        }
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, String& value, const Tags& tags) {
        static_assert(ParserIsByteCharV<CharT>, "Serialized strings must use byte-sized characters");
        std::string tmp;
        auto result = parser_read_string<R>(in, tmp, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse string: ");
        }
        value.assign(reinterpret_cast<const CharT*>(tmp.data()), tmp.size());
        return sa::success();
    }

    static parsing::schema::Type toSchema() { return parsing::schema::Type::String{}; }
};

template <typename R, typename W, typename CharT, typename Traits>
struct Parser<R, W, std::basic_string_view<CharT, Traits>, void> {
    using StringView = std::basic_string_view<CharT, Traits>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, StringView value, const ParentType& parent, const Tags& tags) {
        static_assert(ParserIsByteCharV<CharT>, "Serialized string views must use byte-sized characters");
        return parser_write_string(
            writer, std::string_view{reinterpret_cast<const char*>(value.data()), value.size()}, parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, StringView& value, const Tags& tags) {
        static_assert(ParserIsByteCharV<CharT>, "Serialized string views must use byte-sized characters");
        if (tag_query::raw_string(tags)) {
            if constexpr (requires { R::toRawString(in); }) {
                return parser_error(sa::ErrorCode::InvalidType,
                                    "rawString cannot be read into string_view without owned storage");
            }
        }
        if constexpr (requires { R::template toStringView<CharT, Traits>(in); }) {
            auto result = R::template toStringView<CharT, Traits>(in);
            if (!result) {
                return result.error();
            }
            value = result.value();
            return sa::success();
        }
        return parser_error(sa::ErrorCode::InvalidType, "Reader does not support string_view");
    }

    static parsing::schema::Type toSchema() { return parsing::schema::Type::String{}; }
};

template <typename R, typename W, typename T>
struct Parser<R, W, T, std::enable_if_t<std::is_enum_v<T>>> {
    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const T& value, const ParentType& parent, const Tags& tags) {
        auto name = Reflect<T>::name(value);
        if (!name.empty()) {
            return parser_write_string(writer, name, parent, tags);
        }
        return parser_write<R, W>(writer, static_cast<std::underlying_type_t<T>>(value), parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, T& value, const Tags& tags) {
        std::string enumName;
        if (parser_read_string<R>(in, enumName, tags)) {
            if (auto it = Reflect<T>::nameMap().find(enumName); it != Reflect<T>::nameMap().end()) {
                value = it->second;
                return sa::success();
            }
            return parser_error(sa::ErrorCode::InvalidType, "Unknown enum name '" + enumName + "'");
        }
        std::underlying_type_t<T> raw{};
        auto result = parser_read<R, W>(in, raw, tags);
        if (!result) {
            return parser_context(std::move(result), "Failed to parse enum value: ");
        }
        value = static_cast<T>(raw);
        return sa::success();
    }

    static parsing::schema::Type toSchema() {
        parsing::schema::Type::String schema;
        for (const auto name : Reflect<T>::names()) {
            schema.enumeration.emplace_back(name);
        }
        return schema;
    }
};

} // namespace detail
NEKO_END_NAMESPACE
