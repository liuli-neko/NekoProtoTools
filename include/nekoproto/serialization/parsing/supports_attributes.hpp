#pragma once
#include "nekoproto/global/macros.hpp"

#include "nekoproto/reflect.hpp"

#include <concepts>
#include <string_view>

NEKO_BEGIN_EXPORT
namespace nekoproto {

namespace parsing {
template <typename W>
concept SupportsAttributes =
    requires(W writer, std::string_view name, typename W::OutputObjectType obj, bool is_attribute) {
        { writer.addValueToObject(name, name, &obj, is_attribute) } -> std::same_as<typename W::OutputVarType>;

        { writer.addNullToObject(name, &obj, is_attribute) } -> std::same_as<typename W::OutputVarType>;
    };
} // namespace parsing

} // namespace nekoproto
NEKO_END_EXPORT
