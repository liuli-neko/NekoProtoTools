#pragma once
#include "nekoproto/global/macros.hpp"
#include "provider.hpp"
NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
template <typename T>
struct ReflectModel {
    using provider = ReflectProvider<T>;
private:
    template <std::size_t... I>
    static auto types(std::index_sequence<I...>) -> std::tuple<typename provider::template field_type<I>...>;
    template <std::size_t... I>
    static consteval bool validate(std::index_sequence<I...>) {
        return (performCheck<typename provider::template field_type<I>, provider::template tag<I>()>() && ...);
    }
public:
    using value_types = decltype(types(std::make_index_sequence<provider::value_count>{}));
    static constexpr auto field_tags = provider::tags();
    static constexpr int value_count = static_cast<int>(provider::value_count);
    static_assert(std::tuple_size_v<decltype(field_tags)> == provider::value_count, "field tags/count mismatch");
    static_assert(!provider::has_names || provider::names().size() == provider::value_count, "field names/count mismatch");
    static_assert(validate(std::make_index_sequence<provider::value_count>{}), "tags checks failed");
};
}
NEKO_END_EXPORT
