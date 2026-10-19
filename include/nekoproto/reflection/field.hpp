#pragma once
#include "model.hpp"
#include "nekoproto/global/macros.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto {
// Format-independent field information. Serialization policy extends this view.
template <typename T, std::size_t I>
struct BasicFieldDescriptor {
    using context_type = std::decay_t<T>;
    using provider = detail::ReflectProvider<context_type>;
    using model = detail::ReflectModel<context_type>;
    using field_type = typename provider::template field_type<I>;
    static constexpr auto tags = std::get<I>(model::field_tags);
    using tags_type = std::decay_t<decltype(tags)>;
    static constexpr std::string_view raw_name = []() constexpr -> std::string_view {
        if constexpr (provider::has_names) return provider::template name<I>();
        else return {};
    }();
};
}
NEKO_END_EXPORT
