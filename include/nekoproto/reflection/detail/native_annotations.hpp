#pragma once
#include "nekoproto/global/macros.hpp"
#include "nekoproto/reflection/features.hpp"
#include "nekoproto/reflection/tags.hpp"

#if NEKO_USE_NATIVE_REFLECTION
#include <meta>

NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
template <std::meta::info Item, std::size_t I>
consteval auto nativeAnnotationList() {
    constexpr auto annotation = std::meta::annotations_of(Item)[I];
    using RawValue = [:std::meta::type_of(annotation):];
    using Value = std::remove_cv_t<RawValue>;
    constexpr auto value = std::meta::extract<Value>(annotation);
    // Annotations are tag values. Consumers select the types they understand;
    // discovery preserves unrelated values instead of filtering or converting.
    return flattenTags<value>();
}

template <std::meta::info Item>
consteval auto nativeTagsOf() {
    constexpr auto list = []<std::size_t... I>(std::index_sequence<I...>) {
        return (TagList<>{} + ... + nativeAnnotationList<Item, I>());
    }(std::make_index_sequence<std::meta::annotations_of(Item).size()>{});
    return NormalizeTagList<std::remove_cv_t<decltype(list)>>::value;
}
}
NEKO_END_EXPORT
#endif
