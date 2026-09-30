#pragma once

#include "nekoproto/serialization/reflection.hpp"

#include <cstddef>
#include <string_view>

namespace nekoproto::detail {

// Keep field policy in constant metadata. Node tags remain available to
// backends and custom parsers through tags.
template <typename T, std::size_t I>
struct ParserFieldPlan {
    using Descriptor = FieldDescriptor<T, I>;
    using FieldType = typename Descriptor::field_type;

    static constexpr auto tags = Descriptor::tags;
    static constexpr bool ignored = Descriptor::is_ignored;
    static constexpr bool skippable = Descriptor::is_skippable;
    static constexpr bool flat = tag_query::get<tag_property::Flat<FieldType>>(tags);
    static constexpr std::string_view name = [] {
        constexpr auto renamed = tag_query::get<tag_property::Name>(tags);
        return renamed.empty() ? Descriptor::raw_name : renamed;
    }();
    static constexpr std::string_view leading_comment = tag_query::get<tag_property::LeadingComment>(tags);
    static constexpr std::string_view trailing_comment = tag_query::get<tag_property::TrailingComment>(tags);
    static constexpr std::size_t fixed_length = Descriptor::fixed_length;
};

} // namespace nekoproto::detail
