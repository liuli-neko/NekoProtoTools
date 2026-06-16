#pragma once

#include "nekoproto/global/global.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

NEKO_BEGIN_NAMESPACE

namespace parsing::schema {

struct Type {
    using Number = std::variant<std::int64_t, std::uint64_t, double>;

    struct Null {};
    struct Boolean {};

    struct Integer {
        std::optional<Number> minimum;
        std::optional<Number> maximum;
    };

    struct FloatingPoint {
        std::optional<Number> minimum;
        std::optional<Number> maximum;
    };

    struct String {
        std::vector<std::string> enumeration;
    };

    struct Array {
        std::shared_ptr<Type> items;
        std::vector<Type> prefixItems;
        std::optional<std::size_t> minItems;
        std::optional<std::size_t> maxItems;
        std::optional<bool> additionalItems;
        std::optional<bool> uniqueItems;
    };

    struct Object {
        std::map<std::string, Type> properties;
        std::vector<std::string> required;
        std::shared_ptr<Type> additionalProperties;
    };

    struct AnyOf {
        std::vector<Type> types;
    };

    struct Optional {
        std::shared_ptr<Type> type;
    };

    using Value = std::variant<Null, Boolean, Integer, FloatingPoint, String, Array, Object, AnyOf, Optional>;

    Type() : value(Null{}) {}

    template <typename T>
    Type(T schema) : value(std::move(schema)) {}

    Value value;
    std::optional<std::size_t> fixedLength;
    bool unframed = false;
};

inline const Type& unwrapOptional(const Type& type) {
    if (const auto* optional = std::get_if<Type::Optional>(&type.value)) {
        return *optional->type;
    }
    return type;
}

} // namespace parsing::schema

NEKO_END_NAMESPACE
