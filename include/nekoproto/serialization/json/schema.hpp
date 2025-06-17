/**
 * @file schema.hpp
 * @author llhsdmd (llhsdmd@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-04-27
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include "nekoproto/global/global.hpp"

#include <array>
#include <map>
#include <optional>
#include <set>
#include <unordered_set>
#include <variant>
#include <vector>

#include "nekoproto/serialization/private/helpers.hpp"
#include "nekoproto/serialization/reflection.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {
#define NEKO_JSON_SCHEMA_TYPES_TABLE                                                                                   \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Null, "null")                                                                          \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Boolean, "boolean")                                                                    \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Integer, "integer")                                                                    \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Number, "number")                                                                      \
    NEKO_JSON_SCHEMA_TYPES_ENUM(String, "string")                                                                      \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Array, "array")                                                                        \
    NEKO_JSON_SCHEMA_TYPES_ENUM(Object, "object")

#define NEKO_JSON_SCHEMA_TYPES_ENUM(Name, Value) Name,
enum class JsonSchemaType { NEKO_JSON_SCHEMA_TYPES_TABLE };
#undef NEKO_JSON_SCHEMA_TYPES_ENUM
} // namespace detail

template <>
struct Meta<detail::JsonSchemaType> {
    using T                     = detail::JsonSchemaType;
    static constexpr auto value = Enumerate{// NOLINT
#define NEKO_JSON_SCHEMA_TYPES_ENUM(name, str) str, detail::JsonSchemaType::name,
                                            NEKO_JSON_SCHEMA_TYPES_TABLE
#undef NEKO_JSON_SCHEMA_TYPES_ENUM
    };
};
#undef NEKO_JSON_SCHEMA_TYPES_TABLE

#define NEKO_JSON_SCHEMA_FORMATS_TABLE                                                                                 \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Datetime, "date-time")                                                               \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Date, "date")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Time, "time")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Duration, "duration")                                                                \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Email, "email")                                                                      \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(IdnEmail, "idn-email")                                                               \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Hostname, "hostname")                                                                \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(IdnHostname, "idn-hostname")                                                         \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Ipv4, "ipv4")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Ipv6, "ipv6")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Uri, "uri")                                                                          \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(UriReference, "uri-reference")                                                       \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Iri, "iri")                                                                          \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(IriReference, "iri-reference")                                                       \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Uuid, "uuid")                                                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(UriTemplate, "uri-template")                                                         \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(JsonPointer, "json-pointer")                                                         \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(RelativeJsonPointer, "relative-json-pointer")                                        \
    NEKO_JSON_SCHEMA_FORMATS_ENUM(Regex, "regex")

enum struct DefinedFormats : uint32_t {
#define NEKO_JSON_SCHEMA_FORMATS_ENUM(name, _) name,
    NEKO_JSON_SCHEMA_FORMATS_TABLE
#undef NEKO_JSON_SCHEMA_FORMATS_ENUM
};

template <>
struct Meta<DefinedFormats> {
    using T                     = DefinedFormats;
    static constexpr auto value = Enumerate{// NOLINT
#define NEKO_JSON_SCHEMA_FORMATS_ENUM(name, str) str, DefinedFormats::name,
                                            NEKO_JSON_SCHEMA_FORMATS_TABLE
#undef NEKO_JSON_SCHEMA_FORMATS_ENUM
    };
};

#undef NEKO_JSON_SCHEMA_FORMATS_TABLE

// https://json-schema.org/draft-07/schema#
struct JsonSchema final {
    std::optional<std::string> type{std::nullopt};
    std::optional<std::string> schema{std::nullopt};
    std::variant<std::monostate, bool, std::unique_ptr<JsonSchema>> additionalProperties{}; // map
    // std::optional<std::map<std::string, JsonSchema, std::less<>>> defs{};
    std::optional<std::vector<JsonSchema>> oneOf{std::nullopt};
    std::optional<std::vector<std::string>> required{std::nullopt};
    // std::optional<std::vector<std::string>> examples{};

    std::optional<std::string> ref{std::nullopt};
    using schema_number = std::optional<std::variant<int64_t, uint64_t, double>>;
    using schema_any    = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string>;
    // meta data keywords, ref: https://www.learnjsonschema.com/2020-12/meta-data/
    std::optional<std::string> title{};
    std::optional<std::string> description{std::nullopt};
    std::optional<schema_any> defaultValue{std::nullopt};
    std::optional<bool> deprecated{std::nullopt};
    std::optional<bool> readOnly{std::nullopt};
    std::optional<bool> writeOnly{std::nullopt};
    // hereafter validation keywords, ref: https://www.learnjsonschema.com/2020-12/validation/
    std::optional<schema_any> constant{std::nullopt};
    // string only keywords
    std::optional<uint64_t> minLength{std::nullopt};
    std::optional<uint64_t> maxLength{std::nullopt};
    std::optional<std::string> pattern{std::nullopt};
    // https://www.learnjsonschema.com/2020-12/format-annotation/format/
    std::optional<DefinedFormats> format{std::nullopt};
    // number only keywords
    schema_number minimum{std::nullopt};
    schema_number maximum{std::nullopt};
    schema_number exclusiveMinimum{std::nullopt};
    schema_number exclusiveMaximum{std::nullopt};
    schema_number multipleOf{std::nullopt};
    // object only keywords
    std::optional<std::map<std::string, JsonSchema>> properties{std::nullopt}; // object
    std::optional<uint64_t> minProperties{std::nullopt};
    std::optional<uint64_t> maxProperties{std::nullopt};
    // array only keywords
    std::variant<std::monostate, std::unique_ptr<JsonSchema>, std::vector<JsonSchema>> items{}; // array
    std::optional<bool> additionalItems{std::nullopt};
    std::optional<uint64_t> minItems{std::nullopt};
    std::optional<uint64_t> maxItems{std::nullopt};
    std::optional<uint64_t> minContains{std::nullopt};
    std::optional<uint64_t> maxContains{std::nullopt};
    std::optional<bool> uniqueItems{std::nullopt};
    // properties
    std::optional<std::vector<std::string>> enumeration{std::nullopt}; // enum

    struct Neko {
        using T = JsonSchema;
        static constexpr std::array names // NOLINT
            = {
                "$schema",              //
                "type",                 //
                "properties",           //
                "items",                //
                "additionalItems",      //
                "additionalProperties", //
                // "$defs",                //
                "oneOf",    //
                "required", //
                // "examples",         //
                "$ref", //
                "title",            //
                "description",      //
                "default",          //
                "deprecated",       //
                "readOnly",         //
                "writeOnly",        //
                "const",            //
                "minLength",        //
                "maxLength",        //
                "pattern",          //
                "format",           //
                "minimum",          //
                "maximum",          //
                "exclusiveMinimum", //
                "exclusiveMaximum", //
                "multipleOf",       //
                "minProperties",    //
                "maxProperties",    //
                "minItems",         //
                "maxItems",         //
                "minContains",      //
                "maxContains",      //
                "uniqueItems",      //
                "enum",             //
        };

        static constexpr std::tuple values // NOLINT
            = {
                &T::schema, //
                &T::type,
                &T::properties,           //
                &T::items,                //
                &T::additionalItems,      //
                &T::additionalProperties, //
                // &T::defs,                 //
                &T::oneOf,    //
                &T::required, //
                // &T::examples,             //
                &T::ref, //
                &T::title,                //
                &T::description,      //
                &T::defaultValue,     //
                &T::deprecated,       //
                &T::readOnly,         //
                &T::writeOnly,        //
                &T::constant,         //
                &T::minLength,        //
                &T::maxLength,        //
                &T::pattern,          //
                &T::format,           //
                &T::minimum,          //
                &T::maximum,          //
                &T::exclusiveMinimum, //
                &T::exclusiveMaximum, //
                &T::multipleOf,       //
                &T::minProperties,    //
                &T::maxProperties,    //
                &T::minItems,         //
                &T::maxItems,         //
                &T::minContains,      //
                &T::maxContains,      //
                &T::uniqueItems,      //
                &T::enumeration,      //
        };
    };
};

// Forward declaration
template <typename T, class enable = void>
struct SchemaGenerator;

// Helper function to make it easy to call
template <typename T>
bool generate_schema(const std::decay_t<T>& obj, JsonSchema& def) {
    def.schema = "http://json-schema.org/draft-07/schema#";
    return SchemaGenerator<std::decay_t<T>>::get(obj, def);
}

// --- Specializations for primitive types ---
template <typename T>
    requires std::is_signed_v<T> && (!std::is_enum_v<T>) && std::is_integral_v<T>
struct SchemaGenerator<T, void> {
    static bool get(const T& obj, JsonSchema& def) {
        def.type = "integer";
        if (!def.defaultValue) {
            def.defaultValue = obj;
        }
        if (!def.minimum) {
            def.minimum = std::numeric_limits<T>::min();
        }
        if (!def.maximum) {
            def.maximum = std::numeric_limits<T>::max();
        }
        return true;
    }
};

template <typename T>
    requires std::is_unsigned_v<T> && (!std::is_enum_v<T>) && std::is_integral_v<T>
struct SchemaGenerator<T, void> {
    static bool get(const T& obj, JsonSchema& def) {
        def.type = "integer";
        if (!def.defaultValue) {
            def.defaultValue = obj;
        }
        if (!def.minimum) {
            def.minimum = 0;
        }
        if (!def.maximum) {
            def.maximum = std::numeric_limits<T>::max();
        }
        return true;
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct SchemaGenerator<T, void> {
    static bool get([[maybe_unused]] const T& obj, JsonSchema& def) {
        const auto& vm = Reflect<T>::valueMap();
        std::vector<std::string> names;
        for (const auto& [key, value] : vm) {
            names.push_back(std::string(value));
        }
        def.type        = "string";
        def.enumeration = names;
        if (auto it = vm.find(obj); it != vm.end() && !def.defaultValue) {
            def.defaultValue = std::string(it->second);
        }
        return true;
    }
};

template <typename T>
    requires std::is_floating_point_v<T>
struct SchemaGenerator<T, void> {
    static bool get(const T& obj, JsonSchema& def) {
        def.type = "number";
        if (!def.defaultValue) {
            def.defaultValue = obj;
        }
        if (!def.minimum) {
            def.minimum = std::numeric_limits<T>::min();
        }
        if (!def.maximum) {
            def.maximum = std::numeric_limits<T>::max();
        }
        return true;
    }
};

template <>
struct SchemaGenerator<bool, void> {
    static bool get(bool obj, JsonSchema& def) {
        def.type = "boolean";
        if (!def.defaultValue) {
            def.defaultValue = obj;
        }
        return true;
    }
};

template <>
struct SchemaGenerator<std::string, void> {
    static bool get(const std::string& obj, JsonSchema& def) {
        def.type = "string";
        if (!def.defaultValue) {
            def.defaultValue = obj;
        }
        return true;
    }
};

template <>
struct SchemaGenerator<std::nullptr_t, void> {
    static bool get(std::nullptr_t, JsonSchema& def) {
        def.type = "null";
        return true;
    }
};

template <typename T>
struct SchemaGenerator<std::atomic<T>, void> {
    static bool get(const std::atomic<T>& obj, JsonSchema& def) { return SchemaGenerator<T>::get(obj.load(), def); }
};

template <typename T>
struct SchemaGenerator<std::shared_ptr<T>, void> {
    static bool get([[maybe_unused]] const std::shared_ptr<T>& obj, JsonSchema& def) {
        if (!obj) {
            def.defaultValue = std::monostate{};
            return SchemaGenerator<T>::get(T{}, def);
        }
        return SchemaGenerator<T>::get(*obj, def);
    }
};

template <typename T>
struct SchemaGenerator<std::unique_ptr<T>, void> {
    static bool get([[maybe_unused]] const std::unique_ptr<T>& obj, JsonSchema& def) {
        if (!obj) {
            def.defaultValue = std::monostate{};
            return SchemaGenerator<T>::get(T{}, def);
        }
        return SchemaGenerator<T>::get(*obj, def);
    }
};

// --- Partial specializations for containers ---

// For containers.

template <template <typename...> class Array, typename V, typename... Ts>
bool generate_array_schema([[maybe_unused]] const Array<V, Ts...>& obj, int size, JsonSchema& def) {
    def.type = "array";
    bool ret = true;
    if (!std::holds_alternative<std::unique_ptr<JsonSchema>>(def.items)) {
        def.items = std::make_unique<JsonSchema>();
    }
    auto& item = std::get<std::unique_ptr<JsonSchema>>(def.items);
    if (item == nullptr) {
        def.items = std::make_unique<JsonSchema>();
    }
    ret = SchemaGenerator<V>::get(V{}, *item);
    if (size > 0) {
        if (!def.minItems.has_value()) {
            def.minItems = size;
        }
        if (!def.maxItems.has_value() || (int)def.maxItems.value() < size) {
            def.maxItems = size;
        }
    }
    return ret;
}

template <typename T>
struct SchemaGenerator<std::vector<T>, void> {
    static bool get([[maybe_unused]] const std::vector<T>& obj, JsonSchema& def) {
        return generate_array_schema(obj, -1, def);
    }
};

template <typename T, std::size_t N>
struct SchemaGenerator<std::array<T, N>, void> {
    static bool get([[maybe_unused]] const std::array<T, N>& obj, JsonSchema& def) {
        return generate_array_schema(std::vector<T>{}, N, def);
    }
};

template <typename T>
struct SchemaGenerator<std::set<T>, void> {
    static bool get([[maybe_unused]] const std::set<T>& obj, JsonSchema& def) {
        return generate_array_schema(obj, -1, def);
    }
};

template <typename T>
struct SchemaGenerator<std::list<T>, void> {
    static bool get([[maybe_unused]] const std::list<T>& obj, JsonSchema& def) {
        return generate_array_schema(obj, -1, def);
    }
};

template <typename T>
struct SchemaGenerator<std::unordered_set<T>, void> {
    static bool get([[maybe_unused]] const std::unordered_set<T>& obj, JsonSchema& def) {
        return generate_array_schema(obj, -1, def);
    }
};

template <typename T>
struct SchemaGenerator<std::multiset<T>, void> {
    static bool get([[maybe_unused]] const std::multiset<T>& obj, JsonSchema& def) {
        return generate_array_schema(obj, -1, def);
    }
};

template <typename T>
struct SchemaGenerator<std::unordered_multiset<T>, void> {
    static bool get([[maybe_unused]] const std::unordered_multiset<T>& obj, JsonSchema& def) {
        return generate_array_schema(obj, -1, def);
    }
};

template <typename... Ts>
struct SchemaGenerator<std::tuple<Ts...>> {
    static bool get([[maybe_unused]] const std::tuple<Ts...>& obj, JsonSchema& def) {
        def.type = "array";

        bool ret = true;
        // 使用 C++17 折叠表达式遍历所有类型 Ts...
        // 对于每个类型 T，调用 SchemaGenerator<T>::get(obj) 并将结果存入 vector
        if (!std::holds_alternative<std::vector<JsonSchema>>(def.items)) {
            def.items = std::vector<JsonSchema>();
        }
        auto& items = std::get<std::vector<JsonSchema>>(def.items);
        items.resize(sizeof...(Ts));
        ret = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return (SchemaGenerator<std::decay_t<Ts>>::get(std::get<Is>(obj), items[Is]) && ...);
        }(std::make_index_sequence<sizeof...(Ts)>());
        def.additionalItems = false;

        // 设置数组的长度限制，确保其大小是固定的
        constexpr size_t tuple_size = sizeof...(Ts);
        def.minItems                = tuple_size;
        def.maxItems                = tuple_size;

        return ret;
    }
};

template <typename T>
    requires detail::has_values_meta<T> && (!detail::has_names_meta<T>)
struct SchemaGenerator<T, void> {
    static bool get(const T& obj, JsonSchema& def) {
        def.type = "array";
        if (Reflect<T>::size() == 0) {
            return true;
        }
        bool ret = true;
        if (Reflect<T>::size() == 1) {
            if (!std::holds_alternative<std::unique_ptr<JsonSchema>>(def.items)) {
                def.items = std::make_unique<JsonSchema>();
            }
            auto& item = std::get<std::unique_ptr<JsonSchema>>(def.items);
            if (item == nullptr) {
                item = std::make_unique<JsonSchema>();
            }
            Reflect<T>::forEachWithoutName(obj, [&](auto& member) mutable {
                using value_type = decltype(member);
                ret              = SchemaGenerator<std::decay_t<value_type>>::get(member, *item) && ret;
            });
        } else {
            if (!std::holds_alternative<std::vector<JsonSchema>>(def.items)) {
                def.items = std::vector<JsonSchema>();
            }
            auto& item = std::get<std::vector<JsonSchema>>(def.items);
            item.resize(Reflect<T>::size());
            int idx = 0;
            Reflect<T>::forEachWithoutName(obj, [&](auto& member) mutable {
                using value_type = decltype(member);
                ret              = SchemaGenerator<std::decay_t<value_type>>::get(member, item[idx++]) && ret;
            });
        }
        def.additionalItems = false;
        def.minItems        = Reflect<T>::size();
        def.maxItems        = Reflect<T>::size();
        return ret;
    }
};

// For std::optional<T>
// The schema for optional<T> is the same as for T, but it affects the `required` field of the parent struct.
template <typename T>
struct SchemaGenerator<std::optional<T>, void> {
    static bool get(const std::optional<T>& obj, JsonSchema& def) {
        return SchemaGenerator<std::decay_t<T>>::get(obj.has_value() ? obj.value() : T{}, def);
    }
};

// --- The generic template for structs (SFINAE can be used for better error messages) ---
template <typename T>
    requires std::is_class_v<T> &&
             (!is_minimal_serializable<T>::value) && detail::has_values_meta<T> && detail::has_names_meta<T>
struct SchemaGenerator<T, void> {
    static bool get(const T& obj, JsonSchema& def) {
        def.type = "object";
        if (!def.properties.has_value()) {
            def.properties = std::map<std::string, JsonSchema>();
        }
        auto required = def.required.has_value() ? *std::move(def.required) : std::vector<std::string>();
        bool ret      = true;
        // Use the reflection helper
        Reflect<T>::forEachWithName(obj, [&](auto& member, std::string_view name) {
            using ValueType = decltype(member);
            auto mname      = std::string(name);
            // Recursively generate def for the member
            ret = SchemaGenerator<std::decay_t<ValueType>>::get(member, (*def.properties)[mname]) && ret;

            // If the member is not an std::optional, it's required.
            if constexpr (!detail::is_optional<ValueType>::value) {
                required.push_back(std::string(name));
            }
        });

        // 给required去重
        std::sort(required.begin(), required.end());
        required.erase(std::unique(required.begin(), required.end()), required.end());

        def.required = required;

        return ret;
    }
};

template <typename T>
struct is_string_like : std::is_same<T, std::string> {};

template <>
struct is_string_like<std::string_view> : std::true_type {};

template <template <typename...> class MapT, typename K, typename V, typename... Ts>
static bool generate_map_schema([[maybe_unused]] const MapT<K, V, Ts...>& obj, JsonSchema& def) {
    // 使用 C++17 的 if constexpr 进行编译期分支
    if constexpr (is_string_like<K>::value) {
        // Case 1: 键是 std::string，映射到 JSON object
        def.type = "object";
        if (!std::holds_alternative<std::unique_ptr<JsonSchema>>(def.additionalProperties)) {
            def.additionalProperties = std::make_unique<JsonSchema>();
        }
        auto& additionalProperties = std::get<std::unique_ptr<JsonSchema>>(def.additionalProperties);
        if (!additionalProperties) {
            additionalProperties = std::make_unique<JsonSchema>();
        }
        return SchemaGenerator<std::decay_t<V>>::get(V{}, *additionalProperties);
    } else {
        // Case 2: 键不是 std::string，映射到键值对数组
        def.type = "array";

        // 定义数组元素的 schema：一个包含 'key' 和 'value' 的对象
        JsonSchema itemSchema;
        itemSchema.type = "object";
        if (!std::holds_alternative<std::unique_ptr<JsonSchema>>(def.items)) {
            def.items = std::make_unique<JsonSchema>();
        }
        auto& item = std::get<std::unique_ptr<JsonSchema>>(def.items);
        if (item == nullptr) {
            item = std::make_unique<JsonSchema>();
        }
        if (!item->properties.has_value()) {
            item->properties = std::map<std::string, JsonSchema>{};
        }
        auto& properties    = item->properties.value();
        itemSchema.required = {"key", "value"};
        return SchemaGenerator<std::decay_t<K>>::get(K{}, properties["key"]) &&
               SchemaGenerator<std::decay_t<V>>::get(V{}, properties["value"]);
    }
}

template <typename K, typename V>
struct SchemaGenerator<std::map<K, V>, void> {
    static bool get(const std::map<K, V>& obj, JsonSchema& def) { return generate_map_schema(obj, def); }
};

template <typename K, typename V>
struct SchemaGenerator<std::unordered_map<K, V>, void> {
    static bool get(const std::unordered_map<K, V>& obj, JsonSchema& def) { return generate_map_schema(obj, def); }
};

template <typename K, typename V>
struct SchemaGenerator<std::multimap<K, V>, void> {
    static bool get(const std::multimap<K, V>& obj, JsonSchema& def) { return generate_map_schema(obj, def); }
};

template <typename K, typename V>
struct SchemaGenerator<std::unordered_multimap<K, V>, void> {
    static bool get(const std::unordered_multimap<K, V>& obj, JsonSchema& def) { return generate_map_schema(obj, def); }
};

template <>
struct SchemaGenerator<std::monostate, void> {
    static bool get(const std::monostate& /*unused*/, JsonSchema& def) {
        def.type = "null";
        return true;
    }
};

template <typename... Ts>
struct SchemaGenerator<std::variant<Ts...>, void> {
    static bool get([[maybe_unused]] const std::variant<Ts...>& var, JsonSchema& def) {
        bool ret = true;
        // 创建一个 vector 用于存储每个候选项的 schema
        std::vector<JsonSchema> subSchemas;
        if (!def.oneOf.has_value()) {
            def.oneOf = std::vector<JsonSchema>();
        }
        auto& oneOf = def.oneOf.value();
        oneOf.resize(sizeof...(Ts));
        ret = [&]<size_t... Is>(std::index_sequence<Is...>) {
            return (SchemaGenerator<std::decay_t<Ts>>::get(Ts{}, oneOf[Is]) && ...);
        };
        return ret;
    }
};
NEKO_END_NAMESPACE