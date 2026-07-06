#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/parsing/schemaful/IsSchemafulWriter.hpp"
#include "nekoproto/serialization/parsing/supports_attributes.hpp"
#include "nekoproto/serialization/parsing/supports_comments.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE

namespace parsing {
#define NEKO_RETURN_TAGGED(with_tags_call, without_tags_call)                                                          \
    if constexpr (requires { with_tags_call; }) {                                                                      \
        return with_tags_call;                                                                                         \
    } else {                                                                                                           \
        return without_tags_call;                                                                                      \
    }
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

    template <class ParentType, typename Tags = NoTags>
    static OutputArrayType addArray(W& writer, const std::size_t size, const ParentType& parent,
                                    const Tags& tags = Tags{}) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            NEKO_RETURN_TAGGED(writer.addArrayToArray(size, parent.array, tags),
                               writer.addArrayToArray(size, parent.array));
        } else if constexpr (std::is_same<Type, Object>()) {
            NEKO_RETURN_TAGGED(writer.addArrayToObject(parent.name, size, parent.object, tags),
                               writer.addArrayToObject(parent.name, size, parent.object));
        } else if constexpr (std::is_same<Type, Root>()) {
            NEKO_RETURN_TAGGED(writer.arrayAsRoot(size, tags), writer.arrayAsRoot(size));
        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                NEKO_RETURN_TAGGED(writer.addArrayToMap(parent.name, size, parent.map, tags),
                                   writer.addArrayToMap(parent.name, size, parent.map));
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                NEKO_RETURN_TAGGED(writer.addArrayToUnion(parent.index, size, parent.value, tags),
                                   writer.addArrayToUnion(parent.index, size, parent.value));
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

    template <class ParentType, typename Tags = NoTags>
    static OutputObjectType addObject(W& writer, const size_t size, const ParentType& parent,
                                      const Tags& tags = Tags{}) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            NEKO_RETURN_TAGGED(writer.addObjectToArray(size, parent.array, tags),
                               writer.addObjectToArray(size, parent.array));
        } else if constexpr (std::is_same<Type, Object>()) {
            NEKO_RETURN_TAGGED(writer.addObjectToObject(parent.name, size, parent.object, tags),
                               writer.addObjectToObject(parent.name, size, parent.object));
        } else if constexpr (std::is_same<Type, Root>()) {
            NEKO_RETURN_TAGGED(writer.objectAsRoot(size, tags), writer.objectAsRoot(size));
        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                NEKO_RETURN_TAGGED(writer.addObjectToMap(parent.name, size, parent.map, tags),
                                   writer.addObjectToMap(parent.name, size, parent.map));
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                NEKO_RETURN_TAGGED(writer.addObjectToUnion(parent.index, size, parent.value, tags),
                                   writer.addObjectToUnion(parent.index, size, parent.value));
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

    template <class ParentType, typename Tags = NoTags>
    static OutputValueType addNull(W& writer, const ParentType& parent, const Tags& tags = Tags{}) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            NEKO_RETURN_TAGGED(writer.addNullToArray(parent.array, tags), writer.addNullToArray(parent.array));

        } else if constexpr (std::is_same<Type, Object>()) {
            if constexpr (supports_attributes<std::remove_cvref_t<W>>) {
                NEKO_RETURN_TAGGED(writer.addNullToObject(parent.name, parent.object, parent.isAttribute, tags),
                                   writer.addNullToObject(parent.name, parent.object, parent.isAttribute));
            } else {
                NEKO_RETURN_TAGGED(writer.addNullToObject(parent.name, parent.object, tags),
                                   writer.addNullToObject(parent.name, parent.object));
            }

        } else if constexpr (std::is_same<Type, Root>()) {
            NEKO_RETURN_TAGGED(writer.nullAsRoot(tags), writer.nullAsRoot());

        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                NEKO_RETURN_TAGGED(writer.addNullToMap(parent.name, parent.map, tags),
                                   writer.addNullToMap(parent.name, parent.map));
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                NEKO_RETURN_TAGGED(writer.addNullToUnion(parent.index, parent.value, tags),
                                   writer.addNullToUnion(parent.index, parent.value));
            } else {
                static_assert(always_false_v<Type>, "Unsupported option.");
            }

        } else {
            static_assert(always_false_v<Type>, "Unsupported option.");
        }
    }

    template <class ParentType, class T, typename Tags = NoTags>
    static OutputValueType addValue(W& writer, const T& var, const ParentType& parent, const Tags& tags = Tags{}) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            NEKO_RETURN_TAGGED(writer.addValueToArray(var, parent.array, tags),
                               writer.addValueToArray(var, parent.array));

        } else if constexpr (std::is_same<Type, Object>()) {
            if constexpr (supports_attributes<std::remove_cvref_t<W>>) {
                NEKO_RETURN_TAGGED(writer.addValueToObject(parent.name, var, parent.object, parent.isAttribute, tags),
                                   writer.addValueToObject(parent.name, var, parent.object, parent.isAttribute));
            } else {
                NEKO_RETURN_TAGGED(writer.addValueToObject(parent.name, var, parent.object, tags),
                                   writer.addValueToObject(parent.name, var, parent.object));
            }

        } else if constexpr (std::is_same<Type, Root>()) {
            NEKO_RETURN_TAGGED(writer.valueAsRoot(var, tags), writer.valueAsRoot(var));

        } else if constexpr (schemaful::IsSchemafulWriter<W>) {
            if constexpr (std::is_same<Type, Map<typename W::OutputMapType>>()) {
                NEKO_RETURN_TAGGED(writer.addValueToMap(parent.name, var, parent.map, tags),
                                   writer.addValueToMap(parent.name, var, parent.map));
            } else if constexpr (std::is_same<Type, Union<typename W::OutputUnionType>>()) {
                NEKO_RETURN_TAGGED(writer.addValueToUnion(parent.index, var, parent.value, tags),
                                   writer.addValueToUnion(parent.index, var, parent.value));
            } else {
                static_assert(always_false_v<Type>, "Unsupported option.");
            }

        } else {
            static_assert(always_false_v<Type>, "Unsupported option.");
        }
    }

    template <class ParentType, class T, typename Tags = NoTags>
    static OutputValueType addFixedValue(W& writer, const T& var, std::size_t size, const ParentType& parent,
                                         const Tags& tags = Tags{}) {
        using Type = std::remove_cvref_t<ParentType>;
        if constexpr (std::is_same<Type, Array>()) {
            NEKO_RETURN_TAGGED(writer.addFixedValueToArray(var, size, parent.array, tags),
                               writer.addFixedValueToArray(var, size, parent.array));

        } else if constexpr (std::is_same<Type, Object>()) {
            NEKO_RETURN_TAGGED(writer.addFixedValueToObject(parent.name, var, size, parent.object, tags),
                               writer.addFixedValueToObject(parent.name, var, size, parent.object));

        } else if constexpr (std::is_same<Type, Root>()) {
            NEKO_RETURN_TAGGED(writer.fixedValueAsRoot(var, size, tags), writer.fixedValueAsRoot(var, size));

        } else {
            static_assert(always_false_v<Type>, "Unsupported fixed-value parent.");
        }
    }
};
#undef NEKO_RETURN_TAGGED
} // namespace parsing

NEKO_END_NAMESPACE
