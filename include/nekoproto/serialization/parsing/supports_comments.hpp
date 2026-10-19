#pragma once
#include "nekoproto/global/macros.hpp"

#include "nekoproto/reflect.hpp"

#include <concepts>
#include <string_view>

NEKO_BEGIN_EXPORT
namespace nekoproto {

namespace parsing {
template <typename W>
concept SupportsComments =
    requires(W writer, typename W::OutputArrayType arr, typename W::OutputObjectType obj, std::string_view comment) {
        { writer.addCommentToArray(comment, &arr) } -> std::same_as<void>;
        { writer.addCommentToObject(comment, &obj) } -> std::same_as<void>;
    };
} // namespace parsing

} // namespace nekoproto
NEKO_END_EXPORT
