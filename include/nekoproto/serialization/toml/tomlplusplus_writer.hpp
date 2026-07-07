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

NEKO_BEGIN_NAMESPACE
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

    const sa::Result<void>& result() const noexcept { return mResult; }

    template <typename Tags>
    OutputArrayType arrayAsRoot(std::size_t /*size*/, const Tags& /*tags*/) {
        _remember(sa::ErrorCode::InvalidType, "TOML document root must be an object/table");
        return {};
    }

    template <typename Tags>
    OutputObjectType objectAsRoot(std::size_t /*size*/, const Tags& tags) {
        if (mRoot == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "TOML document is not initialized");
            return {};
        }
        mRoot->clear();
        _applyTableTags(*mRoot, tags, true);
        return {mRoot};
    }

    template <typename Tags>
    OutputValueType nullAsRoot(const Tags& /*tags*/) {
        _remember(sa::ErrorCode::InvalidType, "TOML does not support null values");
        return {};
    }

    template <typename T, typename Tags>
    OutputValueType valueAsRoot(const T& /*value*/, const Tags& /*tags*/) {
        _remember(sa::ErrorCode::InvalidType, "TOML document root must be an object/table");
        return {};
    }

    template <typename Tags>
    OutputArrayType addArrayToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& /*tags*/) {
        if (parent == nullptr || parent->node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML array");
            return {};
        }
        auto& child = parent->node->template emplace_back<toml::array>();
        return {&child};
    }

    template <typename Tags>
    OutputArrayType addArrayToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent,
                                     const Tags& /*tags*/) {
        if (parent == nullptr || parent->node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML table");
            return {};
        }
        auto [it, inserted] = parent->node->insert_or_assign(std::string{name}, toml::array{});
        static_cast<void>(inserted);
        auto* array = it->second.as_array();
        if (array == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Could not create TOML array field '" + std::string{name} + "'");
            return {};
        }
        return {array};
    }

    template <typename Tags>
    OutputObjectType addObjectToArray(std::size_t /*size*/, OutputArrayType* parent, const Tags& tags) {
        if (parent == nullptr || parent->node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML array");
            return {};
        }
        auto& child = parent->node->template emplace_back<toml::table>();
        _applyTableTags(child, tags, false);
        return {&child};
    }

    template <typename Tags>
    OutputObjectType addObjectToObject(std::string_view name, std::size_t /*size*/, OutputObjectType* parent,
                                       const Tags& tags) {
        if (parent == nullptr || parent->node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML table");
            return {};
        }
        auto [it, inserted] = parent->node->insert_or_assign(std::string{name}, toml::table{});
        static_cast<void>(inserted);
        auto* table = it->second.as_table();
        if (table == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Could not create TOML table field '" + std::string{name} + "'");
            return {};
        }
        _applyTableTags(*table, tags, false);
        return {table};
    }

    template <typename T, typename Tags>
    OutputValueType addValueToArray(const T& value, OutputArrayType* parent, const Tags& /*tags*/) {
        if (parent == nullptr || parent->node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML array");
            return {};
        }
        return _appendValue(*parent->node, value);
    }

    template <typename T, typename Tags>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* parent,
                                     const Tags& /*tags*/) {
        if (parent == nullptr || parent->node == nullptr) {
            _remember(sa::ErrorCode::InvalidType, "Cannot append to an empty TOML table");
            return {};
        }
        return _insertValue(*parent->node, name, value);
    }

    template <typename Tags>
    OutputValueType addNullToArray(OutputArrayType* /*parent*/, const Tags& /*tags*/) {
        _remember(sa::ErrorCode::InvalidType, "TOML does not support null values");
        return {};
    }

    template <typename Tags>
    OutputValueType addNullToObject(std::string_view name, OutputObjectType* /*parent*/, const Tags& /*tags*/) {
        _remember(sa::ErrorCode::InvalidType, "TOML field '" + std::string{name} + "' cannot be null");
        return {};
    }

    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    void _remember(sa::ErrorCode code, std::string message) {
        if (mResult) {
            mResult = sa::error(code, std::move(message));
        }
    }

    template <typename Tags>
    void _applyTableTags(toml::table& table, const Tags& tags, bool isRoot) {
        if (isRoot) {
            return;
        }
        if constexpr (tag_query::has<tag_property::inline_table>(Tags{})) {
            table.is_inline(tag_query::get<tag_property::inline_table>(tags));
        }
    }

    template <typename T>
    bool _validScalar(const T& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_floating_point_v<U>) {
            if (!std::isfinite(value)) {
                _remember(sa::ErrorCode::InvalidType, "Cannot serialize non-finite floating point value to TOML");
                return false;
            }
        } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U> && !std::is_same_v<U, bool>) {
            if (value > static_cast<U>(std::numeric_limits<std::int64_t>::max())) {
                _remember(sa::ErrorCode::InvalidType, "TOML integer value is out of int64 range");
                return false;
            }
        }
        return true;
    }

    template <typename T>
    static decltype(auto) _toTomlScalar(const T& value) {
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
    OutputValueType _appendValue(toml::array& array, const T& value) {
        if (!_validScalar(value)) {
            return {};
        }
        auto& child = array.emplace_back(_toTomlScalar(value));
        return {&child};
    }

    template <typename T>
    OutputValueType _insertValue(toml::table& table, std::string_view name, const T& value) {
        if (!_validScalar(value)) {
            return {};
        }
        auto [it, inserted] = table.insert_or_assign(std::string{name}, _toTomlScalar(value));
        static_cast<void>(inserted);
        return {&it->second};
    }

private:
    toml::table* mRoot = nullptr;
    sa::Result<void> mResult;
};

} // namespace tomlplusplus

NEKO_END_NAMESPACE

#endif
