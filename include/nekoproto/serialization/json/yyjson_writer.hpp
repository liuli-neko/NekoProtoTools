#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_YYJSON)

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <yyjson.h>

#include "nekoproto/serialization/error.hpp"

namespace nekoproto {
namespace detail::yyjson {

class Writer {
public:
    struct RawValue {
        std::string text;
    };
    using RawValueType = RawValue;

    struct OutputArrayType {
        yyjson_mut_val* value = nullptr;
    };

    struct OutputObjectType {
        yyjson_mut_val* value = nullptr;
    };

    struct OutputValueType {
        yyjson_mut_val* value = nullptr;
    };

    static constexpr bool requires_field_count = false;

    Writer() : doc_(yyjson_mut_doc_new(nullptr)) {}
    ~Writer() {
        if (doc_ != nullptr) {
            yyjson_mut_doc_free(doc_);
            doc_ = nullptr;
        }
    }

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    Writer(Writer&& other) noexcept : doc_(other.doc_) {
        other.doc_ = nullptr;
    }
    Writer& operator=(Writer&& other) noexcept {
        if (this != &other) {
            if (doc_ != nullptr) {
                yyjson_mut_doc_free(doc_);
            }
            doc_ = other.doc_;
            other.doc_ = nullptr;
        }
        return *this;
    }

    auto doc() noexcept -> yyjson_mut_doc* { return doc_; }
    auto doc() const noexcept -> const yyjson_mut_doc* { return doc_; }

    static auto parseRawValue(std::string_view text, RawValueType& value) -> bool {
        yyjson_doc* doc = yyjson_read(text.data(), text.size(), 0);
        if (doc == nullptr) {
            return false;
        }
        yyjson_doc_free(doc);
        value.text = std::string(text);
        return true;
    }

    auto arrayAsRoot(const std::size_t /*size*/) noexcept -> OutputArrayType {
        auto* arr = yyjson_mut_arr(doc_);
        yyjson_mut_doc_set_root(doc_, arr);
        return {arr};
    }

    auto objectAsRoot(const std::size_t /*size*/) noexcept -> OutputObjectType {
        auto* obj = yyjson_mut_obj(doc_);
        yyjson_mut_doc_set_root(doc_, obj);
        return {obj};
    }

    auto nullAsRoot() noexcept -> OutputValueType {
        auto* val = yyjson_mut_null(doc_);
        yyjson_mut_doc_set_root(doc_, val);
        return {val};
    }

    auto valueAsRoot(const RawValue& value) noexcept -> OutputValueType {
        auto* val = yyjson_mut_rawncpy(doc_, value.text.data(), value.text.size());
        yyjson_mut_doc_set_root(doc_, val);
        return {val};
    }

    template <typename T>
    auto valueAsRoot(const T& value) noexcept -> OutputValueType {
        auto* val = fromBasicType(value);
        yyjson_mut_doc_set_root(doc_, val);
        return {val};
    }

    auto addArrayToArray(const std::size_t /*size*/, OutputArrayType* parent) noexcept -> OutputArrayType {
        auto* child = yyjson_mut_arr_add_arr(doc_, parent->value);
        return {child};
    }

    auto addArrayToObject(std::string_view name, const std::size_t /*size*/, OutputObjectType* parent) noexcept -> OutputArrayType {
        auto* key = yyjson_mut_strncpy(doc_, name.data(), name.size());
        auto* child = yyjson_mut_arr(doc_);
        yyjson_mut_obj_add(parent->value, key, child);
        return {child};
    }

    auto addObjectToArray(const std::size_t /*size*/, OutputArrayType* parent) noexcept -> OutputObjectType {
        auto* child = yyjson_mut_arr_add_obj(doc_, parent->value);
        return {child};
    }

    auto addObjectToObject(std::string_view name, const std::size_t /*size*/, OutputObjectType* parent) noexcept -> OutputObjectType {
        auto* key = yyjson_mut_strncpy(doc_, name.data(), name.size());
        auto* child = yyjson_mut_obj(doc_);
        yyjson_mut_obj_add(parent->value, key, child);
        return {child};
    }

    template <typename T>
    auto addValueToArray(const T& value, OutputArrayType* parent) noexcept -> OutputValueType {
        auto* val = fromBasicType(value);
        yyjson_mut_arr_append(parent->value, val);
        return {val};
    }

    auto addValueToArray(const RawValue& value, OutputArrayType* parent) noexcept -> OutputValueType {
        auto* val = yyjson_mut_rawncpy(doc_, value.text.data(), value.text.size());
        yyjson_mut_arr_append(parent->value, val);
        return {val};
    }

    template <typename T>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) noexcept -> OutputValueType {
        auto* key = yyjson_mut_strncpy(doc_, name.data(), name.size());
        auto* val = fromBasicType(value);
        yyjson_mut_obj_add(parent->value, key, val);
        return {val};
    }

    auto addValueToObject(std::string_view name, const RawValue& value, OutputObjectType* parent) noexcept -> OutputValueType {
        auto* key = yyjson_mut_strncpy(doc_, name.data(), name.size());
        auto* val = yyjson_mut_rawncpy(doc_, value.text.data(), value.text.size());
        yyjson_mut_obj_add(parent->value, key, val);
        return {val};
    }

    auto addNullToArray(OutputArrayType* parent) noexcept -> OutputValueType {
        auto* val = yyjson_mut_null(doc_);
        yyjson_mut_arr_append(parent->value, val);
        return {val};
    }

    auto addNullToObject(std::string_view name, OutputObjectType* parent) noexcept -> OutputValueType {
        auto* key = yyjson_mut_strncpy(doc_, name.data(), name.size());
        auto* val = yyjson_mut_null(doc_);
        yyjson_mut_obj_add(parent->value, key, val);
        return {val};
    }

    void endObject(OutputObjectType*) noexcept {}
    void endArray(OutputArrayType*) noexcept {}

    template <typename T>
    auto fromBasicType(const T& value) noexcept -> yyjson_mut_val* {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, bool>) {
            return yyjson_mut_bool(doc_, value);
        } else if constexpr (std::is_floating_point_v<U>) {
            return yyjson_mut_real(doc_, static_cast<double>(value));
        } else if constexpr (std::is_unsigned_v<U>) {
            return yyjson_mut_uint(doc_, static_cast<uint64_t>(value));
        } else if constexpr (std::is_integral_v<U>) {
            return yyjson_mut_sint(doc_, static_cast<int64_t>(value));
        } else if constexpr (std::is_convertible_v<U, std::string_view>) {
            std::string_view sv{value};
            return yyjson_mut_strncpy(doc_, sv.data(), sv.size());
        } else {
            static_assert(always_false_v<U>, "Unsupported basic type in yyjson Writer");
        }
    }

private:
    yyjson_mut_doc* doc_ = nullptr;
};

} // namespace detail::yyjson
} // namespace nekoproto

#endif
