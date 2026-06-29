#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/global/string_literal.hpp"

#include <concepts>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
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
constexpr bool tag_value_declared(const T& value) {
    if constexpr (requires { value.declared; }) {
        return value.declared;
    } else {
        return true;
    }
}

template <typename T>
constexpr decltype(auto) tag_value_value(T&& value) {
    if constexpr (requires { std::forward<T>(value).declared; std::forward<T>(value).value; }) {
        return (std::forward<T>(value).value);
    } else {
        return std::forward<T>(value);
    }
}

template <typename T>
auto constexpr make_tag_value_common(T&& value) {
    decltype(auto) rawValue = tag_value_value(std::forward<T>(value));
    using RawValue          = std::remove_reference_t<decltype(rawValue)>;
    if constexpr (std::is_array_v<RawValue>) {
        return std::vector<std::decay_t<RawValue>>{std::begin(rawValue), std::end(rawValue)};
    } else if constexpr (NEKO_NAMESPACE::detail::is_std_array<std::decay_t<RawValue>>::value) {
        return std::vector<typename std::decay_t<RawValue>::value_type>{std::begin(rawValue), std::end(rawValue)};
    } else if constexpr (is_constexpr_string<RawValue>::value) {
        return rawValue.view();
    } else {
        return std::forward<decltype(rawValue)>(rawValue);
    }
}

} // namespace detail

template <typename T>
struct TagValue {
    using value_type = T;

    T value{};            // NOLINT
    bool declared = false; // NOLINT

    constexpr TagValue() = default;
    constexpr TagValue(const T& input) : value(input), declared(true) {}
    constexpr TagValue(T&& input) : value(std::move(input)), declared(true) {}

    constexpr TagValue& operator=(const T& input) {
        value    = input;
        declared = true;
        return *this;
    }

    constexpr TagValue& operator=(T&& input) {
        value    = std::move(input);
        declared = true;
        return *this;
    }

    constexpr operator T() const { return value; }

    constexpr bool operator==(const TagValue&) const = default;
};

template <auto Tags, typename Accessor>
struct FieldSpec {
    using accessor_type        = Accessor;
    using raw_type             = Accessor;
    using tag_type             = decltype(Tags);
    constexpr static auto tags = Tags; // NOLINT
    Accessor accessor;
};

struct NoTags {
    template <typename T>
        requires(std::is_class_v<T> && std::is_default_constructible_v<T> && std::is_aggregate_v<T> &&
                 (!std::is_same_v<std::remove_cvref_t<T>, std::string_view>) &&
                 (!std::is_same_v<std::remove_cvref_t<T>, std::string>))
    operator T() const {
        return T{};
    }
};

template <auto... Tags>
struct TagList {
    using type = std::tuple<decltype(Tags)...>;
    constexpr auto size() const { return sizeof...(Tags); }
    constexpr auto tuple() const { return std::make_tuple(Tags...); }
    template <typename T, auto /*self*/>
    constexpr static bool constexpr_check() {
        return (detail::perform_check<T, Tags>() && ...);
    }
};

template <typename T>
struct is_tag_list : std::false_type {};

template <auto... Tags>
struct is_tag_list<TagList<Tags...>> : std::true_type {};

template <typename T>
inline constexpr bool is_tag_list_v = is_tag_list<T>::value;

namespace detail {

template <auto... Tags>
struct normalize_tags {
    constexpr static auto value = TagList<Tags...>{};
};

template <>
struct normalize_tags<> {
    constexpr static auto value = NoTags{};
};

template <auto Tag>
struct normalize_tags<Tag> {
    constexpr static auto value = Tag;
};

template <auto... Tags>
inline constexpr auto normalize_tags_v = normalize_tags<Tags...>::value;

} // namespace detail

template <auto... Tags, typename Accessor>
inline constexpr auto make_tags(Accessor&& accessor) { // NOLINT
    constexpr auto NormalizedTags = detail::normalize_tags_v<Tags...>;

    if constexpr (detail::is_resolvable_without_context_v<std::decay_t<Accessor>>) {
        using ResolvedType = detail::resolve_without_context_t<std::decay_t<Accessor>>;
        static_assert(detail::perform_check<ResolvedType, NormalizedTags>(),
                      "Tag check failed for a member of type, please check the tag definition");
    }
    return FieldSpec<NormalizedTags, Accessor>{std::forward<Accessor>(accessor)};
}

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

template <typename Prop, typename Tag>
inline constexpr bool tag_prop_has_available_v = requires(const Tag& tag) {
    { Prop::has(tag) } -> std::convertible_to<bool>;
};

template <typename Prop, typename Tag>
inline constexpr bool tag_prop_get_available_v = tag_prop_has_available_v<Prop, Tag> && requires(const Tag& tag) {
    Prop::get(tag);
};

template <typename Prop, typename Tags>
constexpr bool tag_has(const Tags& tags);

template <typename Prop, typename Tags>
constexpr typename Prop::type tag_get(const Tags& tags);

template <typename Prop, typename Tags>
constexpr decltype(auto) tag_get_existing(const Tags& tags);

template <typename Prop, auto Head, auto... Tail>
constexpr bool tag_list_has_impl() {
    if constexpr (tag_has<Prop>(Head)) {
        return true;
    } else if constexpr (sizeof...(Tail) > 0) {
        return tag_list_has_impl<Prop, Tail...>();
    } else {
        return false;
    }
}

template <typename Prop, auto Head, auto... Tail>
constexpr typename Prop::type tag_list_get_impl() {
    if constexpr (tag_has<Prop>(Head)) {
        return tag_get<Prop>(Head);
    } else if constexpr (sizeof...(Tail) > 0) {
        return tag_list_get_impl<Prop, Tail...>();
    } else {
        return Prop::missing();
    }
}

template <typename Prop, auto Head, auto... Tail>
constexpr decltype(auto) tag_list_get_existing_impl() {
    if constexpr (tag_has<Prop>(Head)) {
        return tag_get_existing<Prop>(Head);
    } else if constexpr (sizeof...(Tail) > 0) {
        return tag_list_get_existing_impl<Prop, Tail...>();
    } else {
        static_assert(NEKO_NAMESPACE::always_false_v<std::remove_cvref_t<decltype(Head)>>,
                      "requested tag property is missing");
    }
}

template <typename Prop, auto... Tags>
constexpr bool tag_has(const TagList<Tags...>& tags) {
    static_cast<void>(tags);

    if constexpr (sizeof...(Tags) == 0) {
        return false;
    } else {
        return tag_list_has_impl<Prop, Tags...>();
    }
}

template <typename Prop, auto... Tags>
constexpr typename Prop::type tag_get(const TagList<Tags...>& tags) {
    static_cast<void>(tags);

    if constexpr (sizeof...(Tags) == 0) {
        return Prop::missing();
    } else {
        return tag_list_get_impl<Prop, Tags...>();
    }
}

template <typename Prop, auto... Tags>
constexpr decltype(auto) tag_get_existing(const TagList<Tags...>& tags) {
    static_cast<void>(tags);

    if constexpr (sizeof...(Tags) == 0) {
        static_assert(NEKO_NAMESPACE::always_false_v<std::remove_cvref_t<decltype(tags)>>,
                      "requested tag property is missing");
    } else {
        return tag_list_get_existing_impl<Prop, Tags...>();
    }
}

template <typename Prop, typename Tag>
constexpr bool tag_has(const Tag& tag) {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (tag_prop_has_available_v<Prop, RawTag>) {
        if (Prop::has(tag)) {
            return true;
        }
    }

    if constexpr (requires { tag.base; }) {
        return tag_has<Prop>(tag.base);
    } else {
        return false;
    }
}

template <typename Prop, typename Tag>
constexpr typename Prop::type tag_get(const Tag& tag) {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (tag_prop_get_available_v<Prop, RawTag>) {
        if (Prop::has(tag)) {
            return Prop::get(tag);
        }
    }

    if constexpr (requires { tag.base; }) {
        return tag_get<Prop>(tag.base);
    } else {
        return Prop::missing();
    }
}

template <typename Prop, typename Tag>
constexpr decltype(auto) tag_get_existing(const Tag& tag) {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (tag_prop_get_available_v<Prop, RawTag>) {
        return Prop::get(tag);
    } else if constexpr (requires { tag.base; }) {
        return tag_get_existing<Prop>(tag.base);
    } else {
        static_assert(NEKO_NAMESPACE::always_false_v<RawTag>, "requested tag property is missing");
    }
}

} // namespace detail

namespace tag_query {
template <typename Prop, typename Tags>
constexpr bool has(const Tags& tags) {
    return detail::tag_has<Prop>(tags);
}

template <typename Prop, typename Tags>
constexpr decltype(auto) get(const Tags& tags) {
    if constexpr (requires { typename Prop::type; }) {
        return detail::tag_get<Prop>(tags);
    } else {
        return detail::tag_get_existing<Prop>(tags);
    }
}
} // namespace tag_query
NEKO_END_NAMESPACE

#define NEKO_DEFINE_TAG_PROP(Type, member, fname)                                                                      \
    struct fname {                                                                                                     \
        using type = Type;                                                                                             \
        static constexpr type missing() noexcept { return {}; }                                                        \
        template <typename Tag>                                                                                        \
        static constexpr bool has(const Tag& tag) {                                                                    \
            if constexpr (requires { tag.member; }) {                                                                  \
                return NEKO_NAMESPACE::detail::tag_value_declared(tag.member);                                         \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr type get(const Tag& tag)                                                                      \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return NEKO_NAMESPACE::detail::make_tag_value_common(tag.member);                                          \
        }                                                                                                              \
    };

#define NEKO_DEFINE_TAG_VALUE_PROP(member, fname)                                                                      \
    struct fname {                                                                                                     \
        template <typename Tag>                                                                                        \
        static constexpr bool has(const Tag& tag) {                                                                    \
            if constexpr (requires { tag.member; }) {                                                                  \
                return NEKO_NAMESPACE::detail::tag_value_declared(tag.member);                                         \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr decltype(auto) get(const Tag& tag)                                                            \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return NEKO_NAMESPACE::detail::make_tag_value_common(tag.member);                                          \
        }                                                                                                              \
    };

#define NEKO_DEFINE_TYPE_TAG_PROP(Type, member, fname)                                                                 \
    template <typename Value>                                                                                          \
    struct fname {                                                                                                     \
        using type = Type;                                                                                             \
        static constexpr type missing() noexcept {                                                                     \
            if constexpr (requires { is_##member##_tag<Value>::value; }) {                                             \
                return is_##member##_tag<Value>::value;                                                                \
            } else {                                                                                                   \
                return {};                                                                                             \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr bool has(const Tag& tag) {                                                                    \
            if constexpr (requires { tag.member; }) {                                                                  \
                return NEKO_NAMESPACE::detail::tag_value_declared(tag.member);                                         \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr type get(const Tag& tag)                                                                      \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return static_cast<type>(tag.member);                                                                      \
        }                                                                                                              \
    };
