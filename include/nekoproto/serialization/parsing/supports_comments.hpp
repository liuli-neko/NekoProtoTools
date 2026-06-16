#pragma once

#include "nekoproto/global/global.hpp"

#include <concepts>
#include <string_view>

NEKO_BEGIN_NAMESPACE

namespace parsing {
template <typename W>
concept supports_comments =
    requires(W writer, typename W::OutputArrayType arr, typename W::OutputObjectType obj, std::string_view comment) {
        { writer.addCommentToArray(comment, &arr) } -> std::same_as<void>;
        { writer.addCommentToObject(comment, &obj) } -> std::same_as<void>;
    };
} // namespace parsing

NEKO_END_NAMESPACE
