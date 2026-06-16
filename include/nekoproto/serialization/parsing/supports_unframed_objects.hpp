#pragma once

#include "nekoproto/global/global.hpp"

#include <concepts>
#include <string_view>

NEKO_BEGIN_NAMESPACE

namespace parsing {
template <typename R, typename W>
concept supports_unframed_objects =
    requires(W writer, typename W::OutputObjectType object,
             typename R::InputValueType input, std::string_view name) {
        { writer.addUnframedObjectToObject(name, &object) } -> std::same_as<void>;
        { R::next(input) } -> std::same_as<typename R::InputValueType>;
    };
} // namespace parsing

NEKO_END_NAMESPACE
