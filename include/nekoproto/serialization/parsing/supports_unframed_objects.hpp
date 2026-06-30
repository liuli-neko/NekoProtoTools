#pragma once

#include "nekoproto/global/global.hpp"

#include <concepts>
#include <string_view>

NEKO_BEGIN_NAMESPACE

namespace parsing {
template <typename W>
concept supports_unframed_object_writer =
    requires(W writer, typename W::OutputObjectType object, std::string_view name) {
        { writer.addUnframedObjectToObject(name, &object) } -> std::same_as<void>;
    };

template <typename R>
concept supports_unframed_object_reader = requires(typename R::InputValueType input) {
    { R::next(input) } -> std::same_as<typename R::InputValueType>;
};

template <typename R, typename W>
concept supports_unframed_objects = supports_unframed_object_reader<R> && supports_unframed_object_writer<W>;
} // namespace parsing

NEKO_END_NAMESPACE
