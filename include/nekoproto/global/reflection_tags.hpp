#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/global/string_literal.hpp"

#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename T>
struct is_resolvable_without_context : std::false_type {}; // NOLINT

template <typename MemberType, typename ClassType>
struct is_resolvable_without_context<MemberType ClassType::*> : std::true_type {};

template <typename Fn>
struct is_resolvable_without_context<std::is_invocable<Fn>> : std::true_type {};

template <typename T>
inline constexpr bool is_resolvable_without_context_v = is_resolvable_without_context<T>::value; // NOLINT

template <typename T>
struct resolve_without_context;

template <typename T>
    requires(std::is_invocable_v<T>)
struct resolve_without_context<T> {
    using type = std::invoke_result_t<T>;
};

template <typename T>
    requires(!std::is_invocable_v<T>)
struct resolve_without_context<T> {
    using type = T;
};

template <typename MemberType, typename ClassType>
struct resolve_without_context<MemberType ClassType::*> {
    using type = MemberType;
};

template <typename T>
using resolve_without_context_t = typename resolve_without_context<T>::type;

template <typename T, auto tags>
constexpr bool perform_check() {
    using TagType = decltype(tags);
    if constexpr (requires { TagType::template constexpr_check<T, tags>(); }) {
        return TagType::template constexpr_check<T, tags>();
    } else {
        return true;
    }
}

template <typename T>
auto constexpr make_tag_value_common(T& value) {
    if constexpr (std::is_array_v<T>) {
        return std::vector<std::decay_t<T>>{std::begin(value), std::end(value)};
    } else if constexpr (NEKO_NAMESPACE::detail::is_std_array<std::decay_t<T>>::value) {
        return std::vector<typename std::decay_t<T>::value_type>{std::begin(value), std::end(value)};
    } else if constexpr (is_constexpr_string<T>::value) {
        return value.view();
    } else {
        return std::forward<T>(value);
    }
}

} // namespace detail

template <auto Tags, typename Accessor>
struct FieldSpec {
    using accessor_type        = Accessor;
    using raw_type             = Accessor;
    using tag_type             = decltype(Tags);
    constexpr static auto tags = Tags; // NOLINT
    Accessor accessor;
};

template <auto Tags, typename Accessor>
inline constexpr auto make_tags(Accessor&& accessor) { // NOLINT
    if constexpr (detail::is_resolvable_without_context_v<std::decay_t<Accessor>>) {
        using ResolvedType = detail::resolve_without_context_t<std::decay_t<Accessor>>;
        static_assert(detail::perform_check<ResolvedType, Tags>(),
                      "Tag check failed for a member of type, please check the tag definition");
    }
    return FieldSpec<Tags, Accessor>{std::forward<Accessor>(accessor)};
}

struct NoTags {
    template <typename T>
        requires(std::is_class_v<T> && std::is_default_constructible_v<T> && std::is_aggregate_v<T> &&
                 (!std::is_same_v<std::remove_cvref_t<T>, std::string_view>) &&
                 (!std::is_same_v<std::remove_cvref_t<T>, std::string>))
    operator T() const {
        return T{};
    }
};

template <typename T>
struct is_field_spec : std::false_type {}; // NOLINT

template <auto Tags, typename Accessor>
struct is_field_spec<FieldSpec<Tags, Accessor>> : std::true_type {};

template <auto Tags, typename Accessor>
struct is_field_spec<FieldSpec<Tags, Accessor>&> : std::true_type {};

template <auto Tags, typename Accessor>
struct is_field_spec<const FieldSpec<Tags, Accessor>> : std::true_type {};

template <auto Tags, typename Accessor>
struct is_field_spec<const FieldSpec<Tags, Accessor>&> : std::true_type {};

template <typename T>
inline constexpr bool is_field_spec_v = is_field_spec<std::decay_t<T>>::value; // NOLINT

template <typename T, class enable = void>
struct unwrap_tags { // NOLINT
    using type                 = T;
    constexpr static auto tags = NoTags{}; // NOLINT
};

template <typename T>
struct unwrap_tags<T, std::enable_if_t<is_field_spec_v<T>>> {
    using type                 = typename std::decay_t<T>::accessor_type;
    constexpr static auto tags = std::decay_t<T>::tags; // NOLINT
};

template <typename T>
using unwrap_tags_t = typename unwrap_tags<T>::type;

template <typename T>
inline constexpr auto unwrap_tags_v = unwrap_tags<T>::tags; // NOLINT

template <typename T>
constexpr decltype(auto) field_accessor(T&& value) noexcept {
    if constexpr (is_field_spec_v<std::remove_cvref_t<T>>) {
        return (std::forward<T>(value).accessor);
    } else {
        return std::forward<T>(value);
    }
}

template <typename T>
using field_accessor_t = unwrap_tags_t<T>;

template <typename T>
inline constexpr auto field_tags_v = unwrap_tags_v<T>; // NOLINT

template <typename Accessor, typename HostType>
struct resolve_member_type { // NOLINT
    using type = field_accessor_t<Accessor>;
};

template <typename Accessor, typename HostType>
    requires(std::is_invocable_v<Accessor, HostType&>)
struct resolve_member_type<Accessor, HostType> {
    using type = std::invoke_result_t<Accessor, HostType&>;
};

template <typename Accessor, typename HostType>
using resolve_member_type_t =
    typename std::decay_t<typename resolve_member_type<std::decay_t<Accessor>, HostType>::type>;

namespace detail {

template <typename ValuesTuple, typename ContextType, std::size_t... Is>
constexpr bool perform_all_checks_impl(std::index_sequence<Is...> /*unused*/) {
    return (perform_check<resolve_member_type_t<field_accessor_t<std::tuple_element_t<Is, ValuesTuple>>, ContextType>,
                          field_tags_v<std::tuple_element_t<Is, ValuesTuple>>>() &&
            ...);
}

template <typename ValuesTuple, typename ContextType>
constexpr bool perform_all_checks() {
    if constexpr (std::tuple_size<ValuesTuple>::value > 0) {
        return perform_all_checks_impl<ValuesTuple, ContextType>(
            std::make_index_sequence<std::tuple_size_v<ValuesTuple>>{});
    }
    return true;
}

} // namespace detail
NEKO_END_NAMESPACE

#define NEKO_DEFINE_NESTED_TAG(Type, member, fname)                                                                    \
    template <typename T>                                                                                              \
    constexpr Type fname(const T& tags) {                                                                              \
        static_cast<void>(tags);                                                                                       \
        if constexpr (requires { tags.member; }) {                                                                     \
            return NEKO_NAMESPACE::detail::make_tag_value_common(tags.member);                                         \
        } else if constexpr (requires { tags.base; }) {                                                                \
            return fname(tags.base);                                                                                   \
        } else {                                                                                                       \
            return {};                                                                                                 \
        }                                                                                                              \
    }                                                                                                                  \
    template <typename T>                                                                                              \
    constexpr bool has_##fname(const T& tags) {                                                                        \
        static_cast<void>(tags);                                                                                       \
        if constexpr (requires { tags.member; }) {                                                                     \
            return true;                                                                                               \
        } else if constexpr (requires { tags.base; }) {                                                                \
            return has_##fname(tags.base);                                                                             \
        } else {                                                                                                       \
            return false;                                                                                              \
        }                                                                                                              \
    }
