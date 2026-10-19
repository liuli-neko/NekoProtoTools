#pragma once
#include "nekoproto/global/macros.hpp"

#include "nekoproto/reflect.hpp"

#include <concepts>
#include <string_view>

NEKO_BEGIN_EXPORT
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
NEKO_END_EXPORT
