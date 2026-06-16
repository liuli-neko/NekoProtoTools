#pragma once

#include "nekoproto/global/global.hpp"

#include <cstddef>
#include <concepts>
#include <string>
#include <string_view>

NEKO_BEGIN_NAMESPACE

namespace parsing::schemaful {
template <typename W>
concept IsSchemafulWriter =
    requires(W writer, typename W::OutputArrayType array, typename W::OutputMapType map,
             typename W::OutputObjectType object, typename W::OutputUnionType unionValue,
             std::size_t index, std::size_t size, std::string_view name, std::string value) {
        { writer.mapAsRoot(size) } -> std::same_as<typename W::OutputMapType>;
        { writer.unionAsRoot() } -> std::same_as<typename W::OutputUnionType>;

        { writer.addArrayToMap(name, size, &map) } -> std::same_as<typename W::OutputArrayType>;
        { writer.addArrayToUnion(index, size, &unionValue) } -> std::same_as<typename W::OutputArrayType>;

        { writer.addMapToArray(size, &array) } -> std::same_as<typename W::OutputMapType>;
        { writer.addMapToMap(name, size, &map) } -> std::same_as<typename W::OutputMapType>;
        { writer.addMapToObject(name, size, &object) } -> std::same_as<typename W::OutputMapType>;
        { writer.addMapToUnion(index, size, &unionValue) } -> std::same_as<typename W::OutputMapType>;

        { writer.addObjectToMap(name, size, &map) } -> std::same_as<typename W::OutputObjectType>;
        { writer.addObjectToUnion(index, size, &unionValue) } -> std::same_as<typename W::OutputObjectType>;

        { writer.addUnionToArray(&array) } -> std::same_as<typename W::OutputUnionType>;
        { writer.addUnionToMap(name, &map) } -> std::same_as<typename W::OutputUnionType>;
        { writer.addUnionToObject(name, &object) } -> std::same_as<typename W::OutputUnionType>;
        { writer.addUnionToUnion(index, &unionValue) } -> std::same_as<typename W::OutputUnionType>;

        { writer.addNullToMap(name, &map) } -> std::same_as<typename W::OutputValueType>;
        { writer.addValueToMap(name, value, &map) } -> std::same_as<typename W::OutputValueType>;
        { writer.addNullToUnion(index, &unionValue) } -> std::same_as<typename W::OutputValueType>;
        { writer.addValueToUnion(index, value, &unionValue) } -> std::same_as<typename W::OutputValueType>;

        { writer.endMap(&map) } -> std::same_as<void>;
    };
}

NEKO_END_NAMESPACE
