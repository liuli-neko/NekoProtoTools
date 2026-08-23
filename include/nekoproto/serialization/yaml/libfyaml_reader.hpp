#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_LIBFYAML)

#include "nekoproto/serialization/error.hpp"

#include <libfyaml.h>

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace nekoproto {
namespace yaml {

class Reader {
public:
    struct InputArrayType {
        fy_node* node = nullptr;
    };

    struct InputObjectType {
        fy_node* node = nullptr;
    };

    using InputValueType = fy_node*;

    static auto arraySize(const InputArrayType& array) noexcept -> std::size_t {
        const int count = fy_node_sequence_item_count(array.node);
        return count < 0 ? 0U : static_cast<std::size_t>(count);
    }

    static auto arrayElement(const InputArrayType& array, std::size_t index) noexcept -> InputValueType {
        if (index > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return nullptr;
        }
        return fy_node_sequence_get_by_index(array.node, static_cast<int>(index));
    }

    static auto objectSize(const InputObjectType& object) noexcept -> std::size_t {
        const int count = fy_node_mapping_item_count(object.node);
        return count < 0 ? 0U : static_cast<std::size_t>(count);
    }

    static auto objectField(const InputObjectType& object, std::string_view name) noexcept
        -> sa::Result<InputValueType> {
        auto* node = fy_node_mapping_lookup_value_by_simple_key(object.node, name.data(), name.size());
        if (node != nullptr) {
            return node;
        }
        return sa::error(sa::ErrorCode::InvalidField, "Field '" + std::string{name} + "' not found");
    }

    template <typename Fn>
    static auto forEachObjectMember(const InputObjectType& object, Fn&& fn) -> bool {
        void* iter = nullptr;
        while (auto* pair = fy_node_mapping_iterate(object.node, &iter)) {
            auto* key        = fy_node_pair_key(pair);
            auto* value      = fy_node_pair_value(pair);
            size_t size      = 0;
            const char* text = fy_node_get_scalar(key, &size);
            if (text == nullptr) {
                return false;
            }
            if (!fn(std::string_view{text, size}, value)) {
                return false;
            }
        }
        return true;
    }

    static auto isEmpty(InputValueType value) noexcept -> bool { return value == nullptr || fy_node_is_null(value); }

    template <typename CharT, typename Traits>
    static auto toStringView(InputValueType value) noexcept -> sa::Result<std::basic_string_view<CharT, Traits>> {
        static_assert(sizeof(CharT) == sizeof(char), "YAML string views require byte-sized characters");
        auto text = scalarView(value);
        if (!text) {
            return text.error();
        }
        return std::basic_string_view<CharT, Traits>{reinterpret_cast<const CharT*>(text.value().data()),
                                                     text.value().size()};
    }

    template <typename T>
    static auto toBasicType(InputValueType value) noexcept -> sa::Result<T> {
        using U   = std::remove_cvref_t<T>;
        auto text = scalarView(value);
        if (!text) {
            return text.error();
        }
        if constexpr (std::is_same_v<U, std::string>) {
            return std::string{text.value()};
        } else if constexpr (std::is_same_v<U, bool>) {
            if (text.value() == "true" || text.value() == "True" || text.value() == "TRUE" || text.value() == "1") {
                return true;
            }
            if (text.value() == "false" || text.value() == "False" || text.value() == "FALSE" || text.value() == "0") {
                return false;
            }
            return sa::error(sa::ErrorCode::InvalidType,
                             "Could not cast '" + std::string{text.value()} + "' to boolean");
        } else if constexpr (std::is_integral_v<U>) {
            U parsed{};
            const auto [end, error] =
                std::from_chars(text.value().data(), text.value().data() + text.value().size(), parsed);
            if (error != std::errc{} || end != text.value().data() + text.value().size()) {
                return sa::error(sa::ErrorCode::InvalidType,
                                 "Could not cast '" + std::string{text.value()} + "' to integer");
            }
            return parsed;
        } else if constexpr (std::is_floating_point_v<U>) {
            const auto owned         = std::string{text.value()};
            char* end                = nullptr;
            errno                    = 0;
            const long double parsed = std::strtold(owned.c_str(), &end);
            if (errno == ERANGE || end != owned.c_str() + owned.size() || !std::isfinite(parsed) ||
                parsed < static_cast<long double>(std::numeric_limits<U>::lowest()) ||
                parsed > static_cast<long double>(std::numeric_limits<U>::max())) {
                return sa::error(sa::ErrorCode::InvalidType, "Could not cast '" + owned + "' to floating point value");
            }
            return static_cast<U>(parsed);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported YAML scalar type");
        }
    }

    static auto toArray(InputValueType value) noexcept -> sa::Result<InputArrayType> {
        if (value == nullptr || !fy_node_is_sequence(value)) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast YAML node to sequence");
        }
        return InputArrayType{value};
    }

    static auto toObject(InputValueType value) noexcept -> sa::Result<InputObjectType> {
        if (value == nullptr || !fy_node_is_mapping(value)) {
            return sa::error(sa::ErrorCode::InvalidType, "Could not cast YAML node to mapping");
        }
        return InputObjectType{value};
    }

private:
    static auto scalarView(InputValueType value) noexcept -> sa::Result<std::string_view> {
        if (isEmpty(value)) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected scalar, got null");
        }
        size_t size      = 0;
        const char* text = fy_node_get_scalar(value, &size);
        if (text == nullptr) {
            return sa::error(sa::ErrorCode::InvalidType, "Expected YAML scalar");
        }
        return std::string_view{text, size};
    }
};

} // namespace yaml
} // namespace nekoproto

#endif
