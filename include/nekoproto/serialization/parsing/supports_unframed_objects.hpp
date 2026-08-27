#pragma once

#include "nekoproto/global/global.hpp"

#include <concepts>
#include <string_view>

namespace nekoproto {

namespace parsing {
template <typename W>
concept SupportsUnframedObjectWriter =
    requires(W writer, typename W::OutputObjectType object, std::string_view name) {
        { writer.addUnframedObjectToObject(name, &object) } -> std::same_as<void>;
    };

template <typename R>
concept SupportsUnframedObjectReader = requires(typename R::InputValueType input) {
    { R::next(input) } -> std::same_as<typename R::InputValueType>;
};

template <typename R, typename W>
concept SupportsUnframedObjects = SupportsUnframedObjectReader<R> && SupportsUnframedObjectWriter<W>;
} // namespace parsing

} // namespace nekoproto
