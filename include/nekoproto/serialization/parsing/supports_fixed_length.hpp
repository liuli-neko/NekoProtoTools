#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <concepts>
#include <cstddef>
#include <string_view>

NEKO_BEGIN_NAMESPACE

namespace parsing {
template <typename W, typename T, typename Tags = NoTags>
concept supports_fixed_length_writer =
    (requires(W writer, const T& value, std::size_t size) {
        { writer.fixedValueAsRoot(value, size) } -> std::same_as<typename W::OutputValueType>;
    } || requires(W writer, const T& value, std::size_t size, const Tags& tags) {
        { writer.fixedValueAsRoot(value, size, tags) } -> std::same_as<typename W::OutputValueType>;
    }) &&
    (requires(W writer, const T& value, std::size_t size, typename W::OutputArrayType array) {
        { writer.addFixedValueToArray(value, size, &array) } -> std::same_as<typename W::OutputValueType>;
    } || requires(W writer, const T& value, std::size_t size, typename W::OutputArrayType array, const Tags& tags) {
        { writer.addFixedValueToArray(value, size, &array, tags) } -> std::same_as<typename W::OutputValueType>;
    }) &&
    (requires(W writer, const T& value, std::size_t size, typename W::OutputObjectType object,
              std::string_view name) {
        { writer.addFixedValueToObject(name, value, size, &object) } -> std::same_as<typename W::OutputValueType>;
    } || requires(W writer, const T& value, std::size_t size, typename W::OutputObjectType object,
                  std::string_view name, const Tags& tags) {
        { writer.addFixedValueToObject(name, value, size, &object, tags) } -> std::same_as<typename W::OutputValueType>;
    });

template <typename R, typename T, typename Tags = NoTags>
concept supports_fixed_length_reader = requires(typename R::InputValueType input, std::size_t size) {
    { R::template toFixedBasicType<T>(input, size) } -> std::same_as<sa::Result<T>>;
} || requires(typename R::InputValueType input, std::size_t size, const Tags& tags) {
    { R::template toFixedBasicType<T>(input, size, tags) } -> std::same_as<sa::Result<T>>;
};

template <typename R, typename W, typename T>
concept supports_fixed_length = supports_fixed_length_reader<R, T> && supports_fixed_length_writer<W, T>;
} // namespace parsing

NEKO_END_NAMESPACE
