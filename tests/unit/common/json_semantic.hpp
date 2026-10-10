#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#include "nekoproto/serialization/json_serializer.hpp"

#if !defined(NEKO_PROTO_NO_JSON_SERIALIZER)
namespace neko_test {

using JsonValue = nekoproto::JsonSerializer::JsonValue;

struct JsonNumber {
    enum class Kind { Signed, Unsigned, Floating } kind = Kind::Floating;
    long double value = 0;
    std::int64_t signedValue = 0;
    std::uint64_t unsignedValue = 0;
};

inline auto jsonNumber(const JsonValue& value, JsonNumber& number) -> bool {
    std::int64_t signedValue = 0;
    if (value.value(signedValue)) {
        number = {JsonNumber::Kind::Signed, static_cast<long double>(signedValue), signedValue, 0};
        return true;
    }
    std::uint64_t unsignedValue = 0;
    if (value.value(unsignedValue)) {
        number = {JsonNumber::Kind::Unsigned, static_cast<long double>(unsignedValue), 0, unsignedValue};
        return true;
    }
    double floatingValue = 0;
    if (value.value(floatingValue)) {
        number = {JsonNumber::Kind::Floating, static_cast<long double>(floatingValue), 0, 0};
        return true;
    }
    return false;
}

inline auto jsonString(const JsonValue& value) -> std::string {
#if defined(NEKO_PROTO_ENABLE_YYJSON)
    const auto* native = value.nativeValue();
    return {yyjson_get_str(const_cast<yyjson_val*>(native)), yyjson_get_len(const_cast<yyjson_val*>(native))};
#elif defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    const auto& native = value.nativeValue();
    return {native.GetString(), native.GetStringLength()};
#else
    std::string result;
    value.value(result);
    return result;
#endif
}

inline auto equalJsonIntegers(const JsonNumber& lhs, const JsonNumber& rhs) -> bool {
    if (lhs.kind == JsonNumber::Kind::Signed && rhs.kind == JsonNumber::Kind::Signed) {
        return lhs.signedValue == rhs.signedValue;
    }
    if (lhs.kind == JsonNumber::Kind::Unsigned && rhs.kind == JsonNumber::Kind::Unsigned) {
        return lhs.unsignedValue == rhs.unsignedValue;
    }
    if (lhs.kind == JsonNumber::Kind::Signed) {
        return lhs.signedValue >= 0 && static_cast<std::uint64_t>(lhs.signedValue) == rhs.unsignedValue;
    }
    return rhs.signedValue >= 0 && lhs.unsignedValue == static_cast<std::uint64_t>(rhs.signedValue);
}

template <typename Fn>
auto forEachJsonObjectKey(const JsonValue& value, Fn&& fn) -> void {
#if defined(NEKO_PROTO_ENABLE_YYJSON)
    auto* native = const_cast<yyjson_val*>(value.nativeValue());
    auto iter = yyjson_obj_iter_with(native);
    while (auto* key = yyjson_obj_iter_next(&iter)) {
        fn(std::string_view{yyjson_get_str(key), yyjson_get_len(key)});
    }
#elif defined(NEKO_PROTO_ENABLE_RAPIDJSON)
    const auto& native = value.nativeValue();
    for (auto member = native.MemberBegin(); member != native.MemberEnd(); ++member) {
        fn(std::string_view{member->name.GetString(), member->name.GetStringLength()});
    }
#else
    for (const auto field : value.nativeValue().get_object().value_unsafe()) {
        fn(field.key);
    }
#endif
}

inline auto compareJsonValues(const JsonValue& actual, const JsonValue& expected, const std::string& path)
    -> ::testing::AssertionResult {
    if (expected.isObject()) {
        if (!actual.isObject() || actual.size() != expected.size()) {
            return ::testing::AssertionFailure() << path << ": object shape differs";
        }
        auto result = ::testing::AssertionSuccess();
        forEachJsonObjectKey(expected, [&](std::string_view key) {
            if (!result) {
                return;
            }
            const auto child = actual[key];
            if (!child) {
                result = ::testing::AssertionFailure() << path << ": missing field " << key;
            } else {
                result = compareJsonValues(child, expected[key], path + "." + std::string(key));
            }
        });
        return result;
    }
    if (expected.isArray()) {
        if (!actual.isArray() || actual.size() != expected.size()) {
            return ::testing::AssertionFailure() << path << ": array shape differs";
        }
        for (std::size_t i = 0; i < expected.size(); ++i) {
            auto result = compareJsonValues(actual[i], expected[i], path + "[" + std::to_string(i) + "]");
            if (!result) {
                return result;
            }
        }
        return ::testing::AssertionSuccess();
    }
    if (expected.isString()) {
        if (!actual.isString() || jsonString(actual) != jsonString(expected)) {
            return ::testing::AssertionFailure() << path << ": string differs";
        }
        return ::testing::AssertionSuccess();
    }
    if (expected.isBool()) {
        bool actualBool = false;
        bool expectedBool = false;
        if (!actual.isBool() || !actual.value(actualBool) || !expected.value(expectedBool) ||
            actualBool != expectedBool) {
            return ::testing::AssertionFailure() << path << ": boolean differs";
        }
        return ::testing::AssertionSuccess();
    }
    if (expected.isNull()) {
        return actual.isNull() ? ::testing::AssertionSuccess()
                               : ::testing::AssertionFailure() << path << ": null differs";
    }
    if (expected.isNumber() && actual.isNumber()) {
        JsonNumber lhs, rhs;
        if (!jsonNumber(actual, lhs) || !jsonNumber(expected, rhs)) {
            return ::testing::AssertionFailure() << path << ": number could not be read";
        }
        if (lhs.kind != JsonNumber::Kind::Floating && rhs.kind != JsonNumber::Kind::Floating) {
            if (equalJsonIntegers(lhs, rhs)) {
                return ::testing::AssertionSuccess();
            }
        } else {
            // Some backends print the full binary32 value while others print its short decimal form.
            const auto scale = std::max(std::abs(lhs.value), std::abs(rhs.value));
            const auto tolerance = std::max(1e-12L, 1e-7L * scale);
            if (std::abs(lhs.value - rhs.value) <= tolerance) {
                return ::testing::AssertionSuccess();
            }
        }
        return ::testing::AssertionFailure() << path << ": number differs (" << lhs.value << " vs " << rhs.value << ")";
    }
    return ::testing::AssertionFailure() << path << ": value type differs";
}

inline auto jsonEquivalent(std::string_view actual, std::string_view expected) -> ::testing::AssertionResult {
    JsonValue actualValue, expectedValue;
    nekoproto::JsonSerializer::InputSerializer actualInput(actual.data(), actual.size());
    nekoproto::JsonSerializer::InputSerializer expectedInput(expected.data(), expected.size());
    if (!actualInput(actualValue)) {
        return ::testing::AssertionFailure() << "actual JSON did not parse";
    }
    if (!expectedInput(expectedValue)) {
        return ::testing::AssertionFailure() << "expected JSON did not parse";
    }
    return compareJsonValues(actualValue, expectedValue, "$");
}

} // namespace neko_test
#endif
