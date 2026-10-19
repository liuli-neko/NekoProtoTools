#pragma once
#include "nekoproto/global/macros.hpp"

// Compatibility entry point: the reflection core is independent of format policy.
#include "nekoproto/reflection/reflect.hpp"
#include "nekoproto/reflection/field.hpp"
#include "nekoproto/serialization/tags.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto {
template <typename T, std::size_t I>
struct FieldDescriptor : BasicFieldDescriptor<T, I> {
    using context_type = std::decay_t<T>;
    using provider     = detail::ReflectProvider<context_type>;
    using field_type   = typename provider::template field_type<I>;

    static constexpr auto tags = BasicFieldDescriptor<T, I>::tags;
    using tags_type            = std::decay_t<decltype(tags)>;
    static constexpr std::string_view raw_name = []() constexpr -> std::string_view {
        if constexpr (provider::has_names) {
            return provider::template name<I>();
        } else {
            return std::string_view{};
        }
    }();
    static constexpr bool has_custom_name = tag_query::has<tag_property::Name>(tags);
    static constexpr std::string_view name = has_custom_name ? tag_query::get<tag_property::Name>(tags) : raw_name;

    static constexpr bool is_ignored       = tag_query::get<tag_property::Ignore>(tags);
    static constexpr bool is_skippable     = tag_query::get<tag_property::Skippable>(tags);
    static constexpr bool has_fixed_length = tag_query::has<tag_property::FixedLength<void>>(tags);
    static constexpr std::size_t fixed_length =
        has_fixed_length ? tag_query::get<tag_property::FixedLength<field_type>>(tags) : 0;
};

} // namespace nekoproto

NEKO_END_EXPORT
