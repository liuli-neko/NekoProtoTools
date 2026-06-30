#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/global/string_literal.hpp"

#include <concepts>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace detail {

inline constexpr std::size_t max_flattened_tag_count = 128; // NOLINT

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

} // namespace detail

namespace tag_detail {

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
    if constexpr (requires {
                      std::forward<T>(value).declared;
                      std::forward<T>(value).value;
                  }) {
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

template <typename T>
struct tag_value {
    using value_type = T;

    T value{};
    bool declared = false;

    constexpr tag_value() = default;
    constexpr tag_value(const T& input) : value(input), declared(true) {}
    constexpr tag_value(T&& input) : value(std::move(input)), declared(true) {}

    constexpr tag_value& operator=(const T& input) {
        value    = input;
        declared = true;
        return *this;
    }

    constexpr tag_value& operator=(T&& input) {
        value    = std::move(input);
        declared = true;
        return *this;
    }

    constexpr operator T() const { return value; }

    constexpr bool operator==(const tag_value&) const = default;
};

} // namespace tag_detail

template <auto Tags, typename Accessor>
struct TaggedField {
    using accessor_type        = Accessor;
    using raw_type             = Accessor;
    using tag_type             = decltype(Tags);
    constexpr static auto tags = Tags;
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
    static_assert(sizeof...(Tags) <= detail::max_flattened_tag_count,
                  "too many reflection tags in TagList; split the metadata or reduce nested TagList depth");
    using type = std::tuple<decltype(Tags)...>;
    constexpr auto size() const { return sizeof...(Tags); }
    constexpr auto tuple() const { return std::make_tuple(Tags...); }
    template <std::size_t I>
    constexpr auto get() const;
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

template <std::size_t I, auto... Tags>
struct tag_list_element {
    static_assert(NEKO_NAMESPACE::always_false_v<std::integral_constant<std::size_t, I>>, "TagList index out of range");
};

template <auto Head, auto... Tail>
struct tag_list_element<0, Head, Tail...> {
    using type                  = decltype(Head);
    constexpr static auto value = Head; // NOLINT
};

template <std::size_t I, auto Head, auto... Tail>
struct tag_list_element<I, Head, Tail...> : tag_list_element<I - 1, Tail...> {};

template <std::size_t I, auto... Tags>
using tag_list_element_t = typename tag_list_element<I, Tags...>::type;

template <typename T, auto Head, auto... Tail>
constexpr bool tag_list_has_type_impl() {
    if constexpr (std::is_same_v<std::decay_t<T>, std::decay_t<decltype(Head)>>) {
        return true;
    } else if constexpr (sizeof...(Tail) > 0) {
        return tag_list_has_type_impl<T, Tail...>();
    } else {
        return false;
    }
}

template <typename T, auto Head, auto... Tail>
constexpr auto tag_list_get_type_impl() {
    if constexpr (sizeof...(Tail) > 0) {
        if constexpr (tag_list_has_type_impl<T, Tail...>()) {
            return tag_list_get_type_impl<T, Tail...>();
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::decay_t<decltype(Head)>>) {
            return Head;
        } else {
            static_assert(std::is_default_constructible_v<T>, "Tag not found and requested tag type is not default constructible");
            return T{};
        }
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::decay_t<decltype(Head)>>) {
        return Head;
    } else {
        static_assert(std::is_default_constructible_v<T>, "Tag not found and requested tag type is not default constructible");
        return T{};
    }
}

template <typename T, typename Tags>
constexpr bool tag_has_type(const Tags& tags);

template <typename T, typename Tags>
constexpr T tag_get_type(const Tags& tags);

template <auto Tag>
consteval auto flatten_one_tag();

template <auto... Tags>
consteval auto flatten_tags();

template <auto... Left, auto... Right>
consteval auto concat_tag_lists(TagList<Left...>, TagList<Right...>) {
    return TagList<Left..., Right...>{};
}

template <auto Tag>
consteval auto flatten_one_tag() {
    if constexpr (is_tag_list_v<std::remove_cvref_t<decltype(Tag)>>) {
        return []<auto... Inner>(TagList<Inner...>) consteval { return flatten_tags<Inner...>(); }(Tag);
    } else {
        return TagList<Tag>{};
    }
}

template <auto... Tags>
consteval auto flatten_tags() {
    if constexpr (sizeof...(Tags) == 0) {
        return TagList<>{};
    } else {
        return []<auto Head, auto... Tail>() consteval {
            if constexpr (sizeof...(Tail) == 0) {
                return flatten_one_tag<Head>();
            } else {
                return concat_tag_lists(flatten_one_tag<Head>(), flatten_tags<Tail...>());
            }
        }.template operator()<Tags...>();
    }
}

template <typename List>
struct normalize_tag_list;

template <>
struct normalize_tag_list<TagList<>> {
    constexpr static auto value = NoTags{};
};

template <auto... Tags>
struct normalize_tag_list<TagList<Tags...>> {
    static_assert(sizeof...(Tags) <= max_flattened_tag_count,
                  "too many reflection tags after flattening; split the metadata or reduce nested TagList depth");
    constexpr static auto value = TagList<Tags...>{};
};

template <auto... Tags>
struct normalize_tags {
    using flattened             = decltype(flatten_tags<Tags...>());
    constexpr static auto value = normalize_tag_list<flattened>::value;
};

template <auto... Tags>
inline constexpr auto normalize_tags_v = normalize_tags<Tags...>::value;

} // namespace detail

template <std::size_t I, auto... Tags>
constexpr auto get(TagList<Tags...> tags) noexcept -> detail::tag_list_element_t<I, Tags...> {
    static_cast<void>(tags);
    return detail::tag_list_element<I, Tags...>::value;
}

template <auto... Tags>
template <std::size_t I>
constexpr auto TagList<Tags...>::get() const {
    return NEKO_NAMESPACE::get<I>(*this);
}

template <auto... Tags, typename Accessor>
inline constexpr auto make_tags(Accessor&& accessor) { // NOLINT
    constexpr auto NormalizedTags = detail::normalize_tags_v<Tags...>;

    if constexpr (detail::is_resolvable_without_context_v<std::decay_t<Accessor>>) {
        using ResolvedType = detail::resolve_without_context_t<std::decay_t<Accessor>>;
        static_assert(detail::perform_check<ResolvedType, NormalizedTags>(),
                      "Tag check failed for a member of type, please check the tag definition");
    }
    return TaggedField<NormalizedTags, Accessor>{std::forward<Accessor>(accessor)};
}

template <typename T>
struct is_tagged_field : std::false_type {}; // NOLINT

template <auto Tags, typename Accessor>
struct is_tagged_field<TaggedField<Tags, Accessor>> : std::true_type {};

template <auto Tags, typename Accessor>
struct is_tagged_field<TaggedField<Tags, Accessor>&> : std::true_type {};

template <auto Tags, typename Accessor>
struct is_tagged_field<const TaggedField<Tags, Accessor>> : std::true_type {};

template <auto Tags, typename Accessor>
struct is_tagged_field<const TaggedField<Tags, Accessor>&> : std::true_type {};

template <typename T>
inline constexpr bool is_tagged_field_v = is_tagged_field<std::decay_t<T>>::value; // NOLINT

template <typename T, class enable = void>
struct unwrap_tagged_field { // NOLINT
    using type                 = T;
    constexpr static auto tags = NoTags{}; // NOLINT
};

template <typename T>
struct unwrap_tagged_field<T, std::enable_if_t<is_tagged_field_v<T>>> {
    using type                 = typename std::decay_t<T>::accessor_type;
    constexpr static auto tags = std::decay_t<T>::tags; // NOLINT
};

template <typename T>
using unwrap_tagged_field_t = typename unwrap_tagged_field<T>::type;

template <typename T>
inline constexpr auto unwrap_tagged_field_tags_v = unwrap_tagged_field<T>::tags; // NOLINT

template <typename T>
constexpr decltype(auto) field_accessor(T&& value) noexcept {
    if constexpr (is_tagged_field_v<std::remove_cvref_t<T>>) {
        return (std::forward<T>(value).accessor);
    } else {
        return std::forward<T>(value);
    }
}

template <typename T>
using field_accessor_t = unwrap_tagged_field_t<T>;

template <typename T>
inline constexpr auto field_tags_v = unwrap_tagged_field_tags_v<T>; // NOLINT

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
inline constexpr bool tag_property_has_available_v = requires(const Tag& tag) {
    { Prop::has(tag) } -> std::convertible_to<bool>;
};

template <typename Prop, typename Tag>
inline constexpr bool tag_property_get_available_v =
    tag_property_has_available_v<Prop, Tag> && requires(const Tag& tag) { Prop::get(tag); };

template <typename Prop, typename Tags>
constexpr bool tag_has(const Tags& tags);

template <typename Prop, typename Tags>
constexpr typename Prop::type tag_get(const Tags& tags);

template <typename Prop, typename Tags>
constexpr decltype(auto) tag_get_existing(const Tags& tags);

template <typename T, auto... Tags>
constexpr bool tag_has_type(const TagList<Tags...>& tags) {
    static_cast<void>(tags);

    if constexpr (sizeof...(Tags) == 0) {
        return false;
    } else {
        return tag_list_has_type_impl<T, Tags...>();
    }
}

template <typename T, auto... Tags>
constexpr T tag_get_type(const TagList<Tags...>& tags) {
    static_cast<void>(tags);

    if constexpr (sizeof...(Tags) == 0) {
        static_assert(std::is_default_constructible_v<T>, "Tag not found and requested tag type is not default constructible");
        return T{};
    } else {
        return tag_list_get_type_impl<T, Tags...>();
    }
}

template <typename T>
constexpr bool tag_has_type(const NoTags& tags) {
    static_cast<void>(tags);
    return false;
}

template <typename T>
constexpr T tag_get_type(const NoTags& tags) {
    static_cast<void>(tags);
    static_assert(std::is_default_constructible_v<T>, "Tag not found and requested tag type is not default constructible");
    return T{};
}

template <typename T, typename Tag>
constexpr bool tag_has_type(const Tag& tag) {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (std::is_same_v<std::decay_t<T>, RawTag>) {
        return true;
    } else if constexpr (requires { tag.base; }) {
        return tag_has_type<T>(tag.base);
    } else {
        return false;
    }
}

template <typename T, typename Tag>
constexpr T tag_get_type(const Tag& tag) {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (std::is_same_v<std::decay_t<T>, RawTag>) {
        return tag;
    } else if constexpr (requires { tag.base; }) {
        return tag_get_type<T>(tag.base);
    } else {
        static_assert(std::is_default_constructible_v<T>, "Tag not found and requested tag type is not default constructible");
        return T{};
    }
}

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
    if constexpr (sizeof...(Tail) > 0) {
        if constexpr (tag_list_has_impl<Prop, Tail...>()) {
            return tag_list_get_impl<Prop, Tail...>();
        } else if constexpr (tag_has<Prop>(Head)) {
            return tag_get<Prop>(Head);
        } else {
            return Prop::missing();
        }
    } else if constexpr (tag_has<Prop>(Head)) {
        return tag_get<Prop>(Head);
    } else {
        return Prop::missing();
    }
}

template <typename Prop, auto Head, auto... Tail>
constexpr decltype(auto) tag_list_get_existing_impl() {
    if constexpr (sizeof...(Tail) > 0) {
        if constexpr (tag_list_has_impl<Prop, Tail...>()) {
            return tag_list_get_existing_impl<Prop, Tail...>();
        } else if constexpr (tag_has<Prop>(Head)) {
            return tag_get_existing<Prop>(Head);
        } else {
            static_assert(NEKO_NAMESPACE::always_false_v<std::remove_cvref_t<decltype(Head)>>,
                          "requested tag property is missing");
        }
    } else if constexpr (tag_has<Prop>(Head)) {
        return tag_get_existing<Prop>(Head);
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

    if constexpr (tag_property_has_available_v<Prop, RawTag>) {
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

    if constexpr (tag_property_get_available_v<Prop, RawTag>) {
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

    if constexpr (tag_property_get_available_v<Prop, RawTag>) {
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

template <typename Tag, typename Tags>
constexpr bool has_tag(const Tags& tags) {
    return detail::tag_has_type<Tag>(tags);
}

template <typename Prop, typename Tags>
constexpr decltype(auto) get(const Tags& tags) {
    if constexpr (requires { typename Prop::type; }) {
        return detail::tag_get<Prop>(tags);
    } else {
        return detail::tag_get_existing<Prop>(tags);
    }
}

template <typename Tag, typename Tags>
constexpr Tag get_tag(const Tags& tags) {
    return detail::tag_get_type<Tag>(tags);
}
} // namespace tag_query
NEKO_END_NAMESPACE

namespace std {
template <auto... Tags>
struct tuple_size<NEKO_NAMESPACE::TagList<Tags...>> : integral_constant<size_t, sizeof...(Tags)> {};

template <size_t I, auto... Tags>
struct tuple_element<I, NEKO_NAMESPACE::TagList<Tags...>> {
    using type = NEKO_NAMESPACE::detail::tag_list_element_t<I, Tags...>;
};

template <size_t I, auto... Tags>
constexpr auto get(const NEKO_NAMESPACE::TagList<Tags...>& tags) noexcept
    -> NEKO_NAMESPACE::detail::tag_list_element_t<I, Tags...> {
    return NEKO_NAMESPACE::get<I>(tags);
}

template <size_t I, auto... Tags>
constexpr auto get(NEKO_NAMESPACE::TagList<Tags...>& tags) noexcept
    -> NEKO_NAMESPACE::detail::tag_list_element_t<I, Tags...> {
    return NEKO_NAMESPACE::get<I>(tags);
}

template <size_t I, auto... Tags>
constexpr auto get(NEKO_NAMESPACE::TagList<Tags...>&& tags) noexcept
    -> NEKO_NAMESPACE::detail::tag_list_element_t<I, Tags...> {
    return NEKO_NAMESPACE::get<I>(tags);
}
} // namespace std

#ifdef NEKO_PROTO_USE_FMT
#include <fmt/format.h>
namespace fmt {
template <typename T>
struct formatter<NEKO_NAMESPACE::tag_detail::tag_value<T>> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const NEKO_NAMESPACE::tag_detail::tag_value<T>& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        if (!value.declared) {
            return fmt::format_to(ctx.out(), "<undeclared>");
        }
        return fmt::format_to(ctx.out(), "{}", value.value);
    }
};
} // namespace fmt
#elif defined(NEKO_PROTO_USE_STD_FORMAT)
#include <format>
namespace std {
template <typename T>
struct formatter<NEKO_NAMESPACE::tag_detail::tag_value<T>> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const NEKO_NAMESPACE::tag_detail::tag_value<T>& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        if (!value.declared) {
            return std::format_to(ctx.out(), "<undeclared>");
        }
        return std::format_to(ctx.out(), "{}", value.value);
    }
};
} // namespace std
#endif
#define NEKO_DETAIL_DEFINE_TAG_PROPERTY(Type, member, fname)                                                           \
    struct fname {                                                                                                     \
        using type = Type;                                                                                             \
        static constexpr type missing() noexcept { return {}; }                                                        \
        template <typename Tag>                                                                                        \
        static constexpr bool has(const Tag& tag) {                                                                    \
            if constexpr (requires { tag.member; }) {                                                                  \
                return NEKO_NAMESPACE::tag_detail::tag_value_declared(tag.member);                                     \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr type get(const Tag& tag)                                                                      \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return NEKO_NAMESPACE::tag_detail::make_tag_value_common(tag.member);                                      \
        }                                                                                                              \
    };

#define NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(member, fname)                                                           \
    struct fname {                                                                                                     \
        template <typename Tag>                                                                                        \
        static constexpr bool has(const Tag& tag) {                                                                    \
            if constexpr (requires { tag.member; }) {                                                                  \
                return NEKO_NAMESPACE::tag_detail::tag_value_declared(tag.member);                                     \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr decltype(auto) get(const Tag& tag)                                                            \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return NEKO_NAMESPACE::tag_detail::make_tag_value_common(tag.member);                                      \
        }                                                                                                              \
    };

#define NEKO_DETAIL_DEFINE_TYPE_TAG_PROPERTY(Type, member, fname)                                                      \
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
                return NEKO_NAMESPACE::tag_detail::tag_value_declared(tag.member);                                     \
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
