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
    std::optional<std::string> type{};
    std::optional<std::string> schema{};
    std::optional<std::variant<bool, std::shared_ptr<JsonSchema>>> additionalProperties{}; // map
    // std::optional<std::map<std::string, JsonSchema, std::less<>>> defs{};
    std::optional<std::vector<JsonSchema>> oneOf{};
    std::optional<std::vector<std::string>> required{};
    // std::optional<std::vector<std::string>> examples{};

    std::optional<std::string> ref{};
    using schema_number = std::optional<std::variant<int64_t, uint64_t, double>>;
    using schema_any    = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string>;
    // meta data keywords, ref: https://www.learnjsonschema.com/2020-12/meta-data/
    // std::optional<std::string> title{};
    std::optional<std::string> description{};
    std::optional<schema_any> defaultValue{};
    std::optional<bool> deprecated{};
    std::optional<bool> readOnly{};
    std::optional<bool> writeOnly{};
    // hereafter validation keywords, ref: https://www.learnjsonschema.com/2020-12/validation/
    std::optional<schema_any> constant{};
    // string only keywords
    std::optional<uint64_t> minLength{};
    std::optional<uint64_t> maxLength{};
    std::optional<std::string> pattern{};
    // https://www.learnjsonschema.com/2020-12/format-annotation/format/
    std::optional<DefinedFormats> format{};
    // number only keywords
    schema_number minimum{};
    schema_number maximum{};
    schema_number exclusiveMinimum{};
    schema_number exclusiveMaximum{};
    schema_number multipleOf{};
    // object only keywords
    std::optional<std::map<std::string, JsonSchema>> properties{}; // object
    std::optional<uint64_t> minProperties{};
    std::optional<uint64_t> maxProperties{};
    // array only keywords
    std::optional<std::variant<std::shared_ptr<JsonSchema>, std::vector<JsonSchema>>> items{}; // array
    std::optional<bool> additionalItems{};
    std::optional<uint64_t> minItems{};
    std::optional<uint64_t> maxItems{};
    std::optional<uint64_t> minContains{};
    std::optional<uint64_t> maxContains{};
    std::optional<bool> uniqueItems{};
    // properties
    std::optional<std::vector<std::string>> enumeration{}; // enum

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
                // "title",            //
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
                // &T::title,                //
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
JsonSchema generate_schema(const std::decay_t<T>& obj) {
    auto schema   = SchemaGenerator<std::decay_t<T>>::get(obj);
    schema.schema = "http://json-schema.org/draft-07/schema#";
    return schema;
}

// --- Specializations for primitive types ---
template <typename T>
    requires std::is_signed_v<T> && (!std::is_enum_v<T>) && std::is_integral_v<T>
struct SchemaGenerator<T, void> {
    static JsonSchema get(const T& obj) {
        return {.type         = "integer",
                .defaultValue = obj,
                .minimum      = std::numeric_limits<T>::min(),
                .maximum      = std::numeric_limits<T>::max()};
    }
};

template <typename T>
    requires std::is_unsigned_v<T> && (!std::is_enum_v<T>) && std::is_integral_v<T>
struct SchemaGenerator<T, void> {
    static JsonSchema get(const T& obj) {
        return {.type = "integer", .defaultValue = obj, .minimum = 0, .maximum = std::numeric_limits<T>::max()};
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct SchemaGenerator<T, void> {
    static JsonSchema get([[maybe_unused]] const T& obj) {
        const auto& vm = Reflect<T>::valueMap();
        std::vector<std::string> names;
        for (const auto& [key, value] : vm) {
            names.push_back(std::string(value));
        }
        if (auto it = vm.find(obj); it != vm.end()) {
            return {.type = "string", .defaultValue = std::string(it->second), .enumeration = names};
        }
        return {.type = "string", .enumeration = names};
    }
};

template <typename T>
    requires std::is_floating_point_v<T>
struct SchemaGenerator<T, void> {
    static JsonSchema get(const T& obj) {
        return {.type         = "number",
                .defaultValue = obj,
                .minimum      = std::numeric_limits<T>::min(),
                .maximum      = std::numeric_limits<T>::max()};
    }
};

template <>
struct SchemaGenerator<bool, void> {
    static JsonSchema get(bool obj) { return {.type = "boolean", .defaultValue = obj}; }
};

template <>
struct SchemaGenerator<std::string, void> {
    static JsonSchema get(const std::string& obj) { return {.type = "string", .defaultValue = obj}; }
};

template <>
struct SchemaGenerator<std::nullptr_t, void> {
    static JsonSchema get(std::nullptr_t) { return {.type = "null"}; }
};

template <typename T>
struct SchemaGenerator<std::atomic<T>, void> {
    static JsonSchema get(const std::atomic<T>& obj) { return SchemaGenerator<T>::get(obj.load()); }
};

template <typename T>
struct SchemaGenerator<std::shared_ptr<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::shared_ptr<T>& obj) { return SchemaGenerator<T>::get(T{}); }
};

template <typename T>
struct SchemaGenerator<std::unique_ptr<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::unique_ptr<T>& obj) { return SchemaGenerator<T>::get(T{}); }
};

// --- Partial specializations for containers ---

// For containers.

template <template <typename...> class Array, typename V, typename... Ts>
JsonSchema generate_array_schema([[maybe_unused]] const Array<V, Ts...>& obj, int size) {
    JsonSchema schema;
    schema.type  = "array";
    schema.items = std::make_unique<JsonSchema>(SchemaGenerator<V>::get(V{}));
    if (size > 0) {
        schema.maxItems = size;
        schema.minItems = size;
    }
    return schema;
}

template <typename T>
struct SchemaGenerator<std::vector<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::vector<T>& obj) { return generate_array_schema(obj, -1); }
};

template <typename T, std::size_t N>
struct SchemaGenerator<std::array<T, N>, void> {
    static JsonSchema get([[maybe_unused]] const std::array<T, N>& obj) {
        return generate_array_schema(std::vector<T>{}, N);
    }
};

template <typename T>
struct SchemaGenerator<std::set<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::set<T>& obj) { return generate_array_schema(obj, -1); }
};

template <typename T>
struct SchemaGenerator<std::list<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::list<T>& obj) { return generate_array_schema(obj, -1); }
};

template <typename T>
struct SchemaGenerator<std::unordered_set<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::unordered_set<T>& obj) { return generate_array_schema(obj, -1); }
};

template <typename T>
struct SchemaGenerator<std::multiset<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::multiset<T>& obj) { return generate_array_schema(obj, -1); }
};

template <typename T>
struct SchemaGenerator<std::unordered_multiset<T>, void> {
    static JsonSchema get([[maybe_unused]] const std::unordered_multiset<T>& obj) {
        return generate_array_schema(obj, -1);
    }
};

template <typename... Ts>
struct SchemaGenerator<std::tuple<Ts...>> {
    static JsonSchema get([[maybe_unused]] const std::tuple<Ts...>& obj) {
        JsonSchema schema;
        schema.type = "array";

        // 创建一个用于存储每个元素 schema 的 vector
        std::vector<JsonSchema> itemSchemas;

        // 使用 C++17 折叠表达式遍历所有类型 Ts...
        // 对于每个类型 T，调用 SchemaGenerator<T>::get(obj) 并将结果存入 vector
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (itemSchemas.push_back(SchemaGenerator<std::decay_t<Ts>>::get(std::get<Is>(obj))), ...);
        }(std::make_index_sequence<sizeof...(Ts)>());

        schema.items           = std::move(itemSchemas);
        schema.additionalItems = false;

        // 设置数组的长度限制，确保其大小是固定的
        constexpr size_t tuple_size = sizeof...(Ts);
        schema.minItems             = tuple_size;
        schema.maxItems             = tuple_size;

        return schema;
    }
};

template <typename T>
    requires detail::has_values_meta<T> && (!detail::has_names_meta<T>)
struct SchemaGenerator<T, void> {
    static JsonSchema get(const T& obj) {
        JsonSchema schema;
        schema.type = "array";
        std::vector<JsonSchema> itemSchemas;
        Reflect<T>::forEachWithoutName(obj, [&](auto& member) mutable {
            using value_type = decltype(member);
            itemSchemas.push_back(SchemaGenerator<std::decay_t<value_type>>::get(member));
        });
        return schema;
    }
};

// For std::optional<T>
// The schema for optional<T> is the same as for T, but it affects the `required` field of the parent struct.
template <typename T>
struct SchemaGenerator<std::optional<T>, void> {
    static JsonSchema get(const std::optional<T>& obj) {
        return SchemaGenerator<std::decay_t<T>>::get(obj.has_value() ? obj.value() : T{});
    }
};

// --- The generic template for structs (SFINAE can be used for better error messages) ---
template <typename T>
    requires std::is_class_v<T> &&
             (!is_minimal_serializable<T>::value) && detail::has_values_meta<T> && detail::has_names_meta<T>
struct SchemaGenerator<T, void> {
    static JsonSchema get(const T& obj) {
        JsonSchema schema;
        schema.type       = "object";
        schema.properties = std::map<std::string, JsonSchema>();
        schema.required   = std::vector<std::string>();

        // Use the reflection helper
        Reflect<T>::forEachWithName(obj, [&](auto& member, std::string_view name) {
            using ValueType = decltype(member);

            // Recursively generate schema for the member
            (*schema.properties)[std::string(name)] = SchemaGenerator<std::decay_t<ValueType>>::get(member);

            // If the member is not an std::optional, it's required.
            if constexpr (!detail::is_optional<ValueType>::value) {
                schema.required->push_back(std::string(name));
            }
        });

        return schema;
    }
};

template <typename T>
struct is_string_like : std::is_same<T, std::string> {};

template <>
struct is_string_like<std::string_view> : std::true_type {};

template <template <typename...> class MapT, typename K, typename V, typename... Ts>
static JsonSchema generate_map_schema([[maybe_unused]] const MapT<K, V, Ts...>& obj) {
    // 使用 C++17 的 if constexpr 进行编译期分支
    if constexpr (is_string_like<K>::value) {
        // Case 1: 键是 std::string，映射到 JSON object
        JsonSchema schema;
        schema.type                 = "object";
        schema.additionalProperties = std::make_unique<JsonSchema>(SchemaGenerator<std::decay_t<V>>::get(V{}));
        return schema;
    } else {
        // Case 2: 键不是 std::string，映射到键值对数组
        JsonSchema schema;
        schema.type = "array";

        // 定义数组元素的 schema：一个包含 'key' 和 'value' 的对象
        JsonSchema itemSchema;
        itemSchema.type       = "object";
        itemSchema.properties = std::map<std::string, JsonSchema>{
            {"key", SchemaGenerator<std::decay_t<K>>::get(K{})}, {"value", SchemaGenerator<std::decay_t<V>>::get(V{})}};
        itemSchema.required = {"key", "value"};

        schema.items = std::make_unique<JsonSchema>(std::move(itemSchema));
        return schema;
    }
}

template <typename K, typename V>
struct SchemaGenerator<std::map<K, V>, void> {
    static JsonSchema get(const std::map<K, V>& obj) { return generate_map_schema(obj); }
};

template <typename K, typename V>
struct SchemaGenerator<std::unordered_map<K, V>, void> {
    static JsonSchema get(const std::unordered_map<K, V>& obj) { return generate_map_schema(obj); }
};

template <typename K, typename V>
struct SchemaGenerator<std::multimap<K, V>, void> {
    static JsonSchema get(const std::multimap<K, V>& obj) { return generate_map_schema(obj); }
};

template <typename K, typename V>
struct SchemaGenerator<std::unordered_multimap<K, V>, void> {
    static JsonSchema get(const std::unordered_multimap<K, V>& obj) { return generate_map_schema(obj); }
};

template <>
struct SchemaGenerator<std::monostate, void> {
    static JsonSchema get(const std::monostate& /*unused*/) {
        JsonSchema schema;
        schema.type = "null";
        return schema;
    }
};

template <typename... Ts>
struct SchemaGenerator<std::variant<Ts...>, void> {
    static JsonSchema get([[maybe_unused]] const std::variant<Ts...>& var) {
        JsonSchema schema;
        // 创建一个 vector 用于存储每个候选项的 schema
        std::vector<JsonSchema> subSchemas;
        // 使用 C++17 折叠表达式，为参数包中每个类型生成 schema 并存入 vector
        (subSchemas.push_back(SchemaGenerator<std::decay_t<Ts>>::get(Ts{})), ...);
        schema.oneOf = std::move(subSchemas);
        return schema;
    }
};
NEKO_END_NAMESPACE