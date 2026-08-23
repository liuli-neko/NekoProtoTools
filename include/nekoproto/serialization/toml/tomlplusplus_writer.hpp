#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)

#if defined(TOML_EXCEPTIONS) && TOML_EXCEPTIONS
#error "NekoProto TOML backend requires toml++ with TOML_EXCEPTIONS=0"
#endif
#ifndef TOML_EXCEPTIONS
#define TOML_EXCEPTIONS 0
#endif

#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <toml++/toml.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nekoproto {
namespace tomlplusplus {

class Writer {
public:
    struct OutputArrayType {
        toml::array* node = nullptr;
    };

    struct OutputObjectType {
        toml::table* node = nullptr;
    };

    struct OutputValueType {
        toml::node* node = nullptr;
    };

    Writer() = default;
    explicit Writer(toml::table* root) noexcept : mRoot(root) {}

    void reset(toml::table* root) noexcept {
        mRoot   = root;
        mResult = sa::success();
    }

    auto result() const noexcept -> const sa::Result<void>& { return mResult; }

    template <typename Tags>
    auto arrayAsRoot(std::size_t /*size*/, const Tags& /*tags*/) -> OutputArrayType {
        remember(sa::ErrorCode::InvalidType, "TOML document root must be an object/table");
        return {};
    }

    template <typename Tags>
    auto objectAsRoot(std::size_t /*size*/, const Tags& tags) -> OutputObjectType {
        if (mRoot == nullptr) {
            remember(sa::ErrorCode::InvalidType, "TOML document is not initialized");
            return {};
        }
        mRoot->clear();
        applyTableTags(*mRoot, tags, true);
        return {mRoot};
    }

    template <typename Tags>
    auto nullAsRoot(const Tags& /*tags*/) -> OutputValueType {
        remember(sa::ErrorCode::InvalidType, "TOML does not support null values");
        return {};
    }

    template <typename T, typename Tags>
    auto valueAsRoot(const T& /*value*/, const Tags& /*tags*/) -> OutputValueType {
        remember(sa::ErrorCode::InvalidType, "TOML document root must be an object/table");
        return {};
    }

    template <typename Tags>
    auto addArrayToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& /*tags*/) -> OutputArrayType {
        if (parent == nullptr || parent->node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML array");
            return {};
        }
        auto& child = parent->node->template emplace_back<toml::array>();
        return {&child};
    }

    template <typename Tags>
    auto addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent, const Tags& /*tags*/)
        -> OutputArrayType {
        if (parent == nullptr || parent->node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML table");
            return {};
        }
        auto [it, inserted] = parent->node->insert_or_assign(std::string{name}, toml::array{});
        static_cast<void>(inserted);
        auto* array = it->second.as_array();
        if (array == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Could not create TOML array field '" + std::string{name} + "'");
            return {};
        }
        return {array};
    }

    template <typename Tags>
    auto addObjectToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) -> OutputObjectType {
        if (parent == nullptr || parent->node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML array");
            return {};
        }
        auto& child = parent->node->template emplace_back<toml::table>();
        applyTableTags(child, tags, false);
        return {&child};
    }

    template <typename Tags>
    auto addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent, const Tags& tags)
        -> OutputObjectType {
        if (parent == nullptr || parent->node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML table");
            return {};
        }
        auto [it, inserted] = parent->node->insert_or_assign(std::string{name}, toml::table{});
        static_cast<void>(inserted);
        auto* table = it->second.as_table();
        if (table == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Could not create TOML table field '" + std::string{name} + "'");
            return {};
        }
        applyTableTags(*table, tags, false);
        return {table};
    }

    template <typename T, typename Tags>
    auto addValueToArray(const T& value, OutputArrayType* parent, const Tags& /*tags*/) -> OutputValueType {
        if (parent == nullptr || parent->node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML array");
            return {};
        }
        return appendValue(*parent->node, value);
    }

    template <typename T, typename Tags>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent, const Tags& /*tags*/)
        -> OutputValueType {
        if (parent == nullptr || parent->node == nullptr) {
            remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML table");
            return {};
        }
        return insertValue(*parent->node, name, value);
    }

    template <typename Tags>
    auto addNullToArray(OutputArrayType* /*parent*/, const Tags& /*tags*/) -> OutputValueType {
        remember(sa::ErrorCode::InvalidType, "TOML does not support null values");
        return {};
    }

    template <typename Tags>
    auto addNullToObject(std::string_view name, OutputObjectType* /*parent*/, const Tags& /*tags*/) -> OutputValueType {
        remember(sa::ErrorCode::InvalidType, "TOML field '" + std::string{name} + "' cannot be null");
        return {};
    }

    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    void remember(sa::ErrorCode code, std::string message) {
        if (mResult) {
            mResult = sa::error(code, std::move(message));
        }
    }

    template <typename Tags>
    void applyTableTags(toml::table& table, const Tags& tags, bool isRoot) {
        if (isRoot) {
            return;
        }
        if constexpr (tag_query::has<tag_property::InlineTable>(Tags{})) {
            table.is_inline(tag_query::get<tag_property::InlineTable>(tags));
        }
    }

    template <typename T>
    auto validScalar(const T& value) -> bool {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_floating_point_v<U>) {
            if (!std::isfinite(value)) {
                remember(sa::ErrorCode::InvalidType, "Cannot serialize non-finite floating point value to TOML");
                return false;
            }
        } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && !std::is_same_v<U, bool>) {
            if (value > static_cast<U>(std::numeric_limits<std::int64_t>::max())) {
                remember(sa::ErrorCode::InvalidType, "TOML integer value is out of int64 range");
                return false;
            }
        }
        return true;
    }

    template <typename T>
    static auto toTomlScalar(const T& value) -> decltype(auto) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string>) {
            return value;
        } else if constexpr (std::is_same_v<U, std::string_view>) {
            return std::string{value};
        } else if constexpr (std::is_same_v<U, bool>) {
            return value;
        } else if constexpr (std::is_floating_point_v<U>) {
            return static_cast<double>(value);
        } else if constexpr (std::is_integral_v<U>) {
            return static_cast<std::int64_t>(value);
        } else if constexpr (std::is_enum_v<U>) {
            using I = std::underlying_type_t<U>;
            return static_cast<std::int64_t>(static_cast<I>(value));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported TOML scalar type");
        }
    }

    template <typename T>
    auto appendValue(toml::array& array, const T& value) -> OutputValueType {
        if (!validScalar(value)) {
            return {};
        }
        auto& child = array.emplace_back(toTomlScalar(value));
        return {&child};
    }

    template <typename T>
    auto insertValue(toml::table& table, std::string_view name, const T& value) -> OutputValueType {
        if (!validScalar(value)) {
            return {};
        }
        auto [it, inserted] = table.insert_or_assign(std::string{name}, toTomlScalar(value));
        static_cast<void>(inserted);
        return {&it->second};
    }

private:
    toml::table* mRoot = nullptr;
    sa::Result<void> mResult;
};

} // namespace tomlplusplus

} // namespace nekoproto

#endif
