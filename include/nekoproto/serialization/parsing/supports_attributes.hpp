#pragma once

#include "nekoproto/global/global.hpp"

#include <concepts>
#include <string_view>

NEKO_BEGIN_NAMESPACE

namespace parsing {
template <typename W>
concept supports_attributes =
    requires(W writer, std::string_view name, typename W::OutputObjectType obj, bool isAttribute) {
        { writer.addValueToObject(name, name, &obj, isAttribute) } -> std::same_as<typename W::OutputVarType>;

        { writer.addNullToObject(name, &obj, isAttribute) } -> std::same_as<typename W::OutputVarType>;
    };
} // namespace parsing

NEKO_END_NAMESPACE
