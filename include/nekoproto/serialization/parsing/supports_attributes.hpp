#pragma once

#include "nekoproto/global/global.hpp"

#include <concepts>
#include <string_view>

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
