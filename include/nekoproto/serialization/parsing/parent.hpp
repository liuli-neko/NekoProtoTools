#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/parsing/schemaful/IsSchemafulWriter.hpp"
#include "nekoproto/serialization/parsing/supports_attributes.hpp"
#include "nekoproto/serialization/parsing/supports_comments.hpp"

#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE

namespace parsing {

template <typename W>
struct Parent {
    using OutputArrayType  = typename W::OutputArrayType;
    using OutputObjectType = typename W::OutputObjectType;
    using OutputValueType  = typename W::OutputValueType;

    struct Array {
        OutputArrayType* array;
    };

    template <class T>
    struct Map {
        std::string_view name;
        T* map;
    };

    struct Object {
        std::string_view name;
        OutputObjectType* object;
        bool isAttribute = false;
        Object asAttribute() { return {name, object, true}; }
    };

    template <typename T>
    struct Union {
        std::size_t index;
        T* value;
    };

    struct Root {};

    template <class ParentType>
    static OutputArrayType addArray(W& writer, const std::size_t size, const ParentType& parent) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            return writer.addArrayToArray(size, parent.array);

        } else if constexpr (std::is_same<Type, Object>()) {
            return writer.addArrayToObject(parent.name, size, parent.object);

        } else if constexpr (std::is_same<Type, Root>()) {
            return writer.arrayAsRoot(size);

        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                return writer.addArrayToMap(parent.name, size, parent.map);
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                return writer.addArrayToUnion(parent.index, size, parent.value);
            } else {
                static_assert(always_false_v<Type>, "Unsupported option.");
            }

        } else {
            static_assert(always_false_v<Type>, "Unsupported option.");
        }
    }

    template <typename ParentType>
    static void addComment(W& writer, std::string_view comment, const ParentType& parent) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (supports_comments<W>) {
            if constexpr (std::is_same<Type, Array>()) {
                writer.addCommentToArray(comment, parent.array);

            } else if constexpr (std::is_same<Type, Object>()) {
                writer.addCommentToObject(comment, parent.object);
            }
        }
    }

    template <class ParentType>
    static OutputObjectType addObject(W& writer, const size_t size, const ParentType& parent) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            return writer.addObjectToArray(size, parent.array);
        } else if constexpr (std::is_same<Type, Object>()) {
            return writer.addObjectToObject(parent.name, size, parent.object);

        } else if constexpr (std::is_same<Type, Root>()) {
            return writer.objectAsRoot(size);

        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                return writer.addObjectToMap(parent.name, size, parent.map);
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                return writer.addObjectToUnion(parent.index, size, parent.value);
            } else {
                static_assert(always_false_v<Type>, "Unsupported option.");
            }

        } else {
            static_assert(always_false_v<Type>, "Unsupported option.");
        }
    }

    template <class ParentType>
    static void beginUnframedObject(W& writer, const ParentType& parent) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Object>()) {
            writer.addUnframedObjectToObject(parent.name, parent.object);
        } else if constexpr (std::is_same<Type, Array>() || std::is_same<Type, Root>()) {
            static_cast<void>(writer);
            static_cast<void>(parent);
        } else {
            static_assert(always_false_v<Type>, "Unsupported unframed-object parent.");
        }
    }

    template <class ParentType>
    static OutputValueType addNull(W& writer, const ParentType& parent) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            return writer.addNullToArray(parent.array);

        } else if constexpr (std::is_same<Type, Object>()) {
            if constexpr (supports_attributes<std::remove_cvref_t<W>>) {
                return writer.addNullToObject(parent.name, parent.object, parent.isAttribute);
            } else {
                return writer.addNullToObject(parent.name, parent.object);
            }

        } else if constexpr (std::is_same<Type, Root>()) {
            return writer.nullAsRoot();

        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                return writer.addNullToMap(parent.name, parent.map);
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                return writer.addNullToUnion(parent.index, parent.value);
            } else {
                static_assert(always_false_v<Type>, "Unsupported option.");
            }

        } else {
            static_assert(always_false_v<Type>, "Unsupported option.");
        }
    }

    template <class ParentType, class T>
    static OutputValueType addValue(W& writer, const T& var, const ParentType& parent) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            return writer.addValueToArray(var, parent.array);

        } else if constexpr (std::is_same<Type, Object>()) {
            if constexpr (supports_attributes<std::remove_cvref_t<W>>) {
                return writer.addValueToObject(parent.name, var, parent.object, parent.isAttribute);
            } else {
                return writer.addValueToObject(parent.name, var, parent.object);
            }

        } else if constexpr (std::is_same<Type, Root>()) {
            return writer.valueAsRoot(var);

        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                return writer.addValueToMap(parent.name, var, parent.map);
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                return writer.addValueToUnion(parent.index, var, parent.value);
            } else {
                static_assert(always_false_v<Type>, "Unsupported option.");
            }

        } else {
            static_assert(always_false_v<Type>, "Unsupported option.");
        }
    }

    template <class ParentType, class T>
    static OutputValueType addFixedValue(W& writer, const T& var, std::size_t size,
                                         const ParentType& parent) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            return writer.addFixedValueToArray(var, size, parent.array);

        } else if constexpr (std::is_same<Type, Object>()) {
            return writer.addFixedValueToObject(parent.name, var, size, parent.object);

        } else if constexpr (std::is_same<Type, Root>()) {
            return writer.fixedValueAsRoot(var, size);

        } else {
            static_assert(always_false_v<Type>, "Unsupported fixed-value parent.");
        }
    }
};
} // namespace parsing

NEKO_END_NAMESPACE
