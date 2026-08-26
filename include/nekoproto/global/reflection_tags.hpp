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

namespace nekoproto {
namespace detail {

inline constexpr std::size_t max_flattened_tag_count = 128; // NOLINT

template <typename T>
struct IsResolvableWithoutContext : std::false_type {};

template <typename MemberType, typename ClassType>
struct IsResolvableWithoutContext<MemberType ClassType::*> : std::true_type {};

template <typename Fn>
struct IsResolvableWithoutContext<std::is_invocable<Fn>> : std::true_type {};

template <typename T>
inline constexpr bool is_resolvable_without_context_v = IsResolvableWithoutContext<T>::value; // NOLINT

template <typename T>
struct ResolveWithoutContext;

template <typename T>
    requires(std::is_invocable_v<T>)
struct ResolveWithoutContext<T> {
    using type = std::invoke_result_t<T>;
};

template <typename T>
    requires(!std::is_invocable_v<T>)
struct ResolveWithoutContext<T> {
    using type = T;
};

template <typename MemberType, typename ClassType>
struct ResolveWithoutContext<MemberType ClassType::*> {
    using type = MemberType;
};

template <typename T>
using resolve_without_context_t = typename ResolveWithoutContext<T>::type;

template <typename T, auto tags>
constexpr auto performCheck() -> bool {
    using TagType = decltype(tags);
    if constexpr (requires { TagType::template constexprCheck<T, tags>(); }) {
        return TagType::template constexprCheck<T, tags>();
    } else {
        return true;
    }
}

} // namespace detail

namespace tag_detail {

template <typename T>
constexpr auto tagValueDeclared(const T& value) -> bool {
    if constexpr (requires { value.declared; }) {
        return value.declared;
    } else {
        return true;
    }
}

template <typename T>
constexpr auto tagValueValue(T&& value) -> decltype(auto) {
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
auto constexpr makeTagValueCommon(T&& value) {
    decltype(auto) rawValue = tagValueValue(std::forward<T>(value));
    using RawValue          = std::remove_reference_t<decltype(rawValue)>;
    if constexpr (std::is_array_v<RawValue>) {
        return std::vector<std::decay_t<RawValue>>{std::begin(rawValue), std::end(rawValue)};
    } else if constexpr (nekoproto::detail::IsStdArray<std::decay_t<RawValue>>::value) {
        return std::vector<typename std::decay_t<RawValue>::value_type>{std::begin(rawValue), std::end(rawValue)};
    } else if constexpr (IsConstexprString<RawValue>::value) {
        return rawValue.view();
    } else {
        return std::forward<decltype(rawValue)>(rawValue);
    }
}

template <typename T>
struct TagValue {
    using value_type = T;

    T value{};
    bool declared = false;

    constexpr TagValue() = default;
    constexpr TagValue(const T& input) : value(input), declared(true) {}
    constexpr TagValue(T&& input) : value(std::move(input)), declared(true) {}

    constexpr auto operator=(const T& input) -> TagValue& {
        value    = input;
        declared = true;
        return *this;
    }

    constexpr auto operator=(T&& input) -> TagValue& {
        value    = std::move(input);
        declared = true;
        return *this;
    }

    constexpr operator T() const { return value; }

    constexpr auto operator==(const TagValue&) const -> bool = default;
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
    constexpr static auto constexprCheck() -> bool {
        return (detail::performCheck<T, Tags>() && ...);
    }
};

template <auto... A, auto... B>
consteval auto operator+(TagList<A...>, TagList<B...>) {
    return TagList<A..., B...>{};
}

template <typename T>
struct IsTagList : std::false_type {};

template <auto... Tags>
struct IsTagList<TagList<Tags...>> : std::true_type {};

template <typename T>
inline constexpr bool is_tag_list_v = IsTagList<T>::value;

namespace detail {

template <std::size_t I, auto... Tags>
struct TagListElement {
    static_assert(I < sizeof...(Tags), "TagList index out of range");
    using tuple_type = std::tuple<decltype(Tags)...>;
    using type       = std::tuple_element_t<I, tuple_type>;
    constexpr static auto value = std::get<I>(std::tuple<decltype(Tags)...>{Tags...});
};

template <std::size_t I, auto... Tags>
using tag_list_element_t = typename TagListElement<I, Tags...>::type;

template <typename T, auto... Tags>
constexpr auto tagListHasTypeImpl() -> bool {
    if constexpr (sizeof...(Tags) == 0) {
        return false;
    } else {
        return (std::is_same_v<std::decay_t<T>, std::decay_t<decltype(Tags)>> || ...);
    }
}

template <typename T, auto... Tags>
constexpr auto tagListGetTypeImpl() -> T {
    if constexpr (sizeof...(Tags) == 0) {
        static_assert(std::is_default_constructible_v<T>,
                      "Tag not found and requested tag type is not default constructible");
        return T{};
    } else {
        T result{};
        (([&]() {
            if constexpr (std::is_same_v<std::decay_t<T>, std::decay_t<decltype(Tags)>>) {
                result = Tags;
            }
        }()), ...);
        return result;
    }
}

template <typename T, typename Tags>
constexpr auto tagHasType(const Tags& tags) -> bool;

template <typename T, typename Tags>
constexpr auto tagGetType(const Tags& tags) -> T;

template <auto... Left, auto... Right>
consteval auto concatTagLists(TagList<Left...>, TagList<Right...>) {
    return TagList<Left..., Right...>{};
}

template <auto Tag>
consteval auto flattenOneTag() {
    if constexpr (is_tag_list_v<std::remove_cvref_t<decltype(Tag)>>) {
        return []<auto... Inner>(TagList<Inner...>) consteval {
            if constexpr (sizeof...(Inner) == 0) {
                return TagList<>{};
            } else {
                return (flattenOneTag<Inner>() + ...);
            }
        }(Tag);
    } else {
        return TagList<Tag>{};
    }
}

template <auto... Tags>
consteval auto flattenTags() {
    if constexpr (sizeof...(Tags) == 0) {
        return TagList<>{};
    } else {
        return (flattenOneTag<Tags>() + ...);
    }
}

template <auto Tag, auto Target>
inline constexpr bool is_same_tag_v = []() consteval {
    if constexpr (!std::is_same_v<decltype(Tag), decltype(Target)>) {
        return false;
    } else {
        return Tag == Target;
    }
}();

template <auto Tag, auto... Accum>
consteval auto appendUniqueTag(TagList<Accum...>) {
    if constexpr (sizeof...(Accum) == 0) {
        return TagList<Tag>{};
    } else if constexpr ((is_same_tag_v<Tag, Accum> || ...)) {
        return TagList<Accum...>{};
    } else {
        return TagList<Accum..., Tag>{};
    }
}

template <typename Acc, auto... Rest>
struct DedupFold;

template <typename Acc>
struct DedupFold<Acc> {
    using type = Acc;
};

template <typename Acc, auto Head, auto... Tail>
struct DedupFold<Acc, Head, Tail...> {
    using next_acc = decltype(appendUniqueTag<Head>(Acc{}));
    using type     = typename DedupFold<next_acc, Tail...>::type;
};

template <auto... Tags>
consteval auto deduplicateTags(TagList<Tags...>) {
    return typename DedupFold<TagList<>, Tags...>::type{};
}

template <typename List>
struct NormalizeTagList;

template <>
struct NormalizeTagList<TagList<>> {
    constexpr static auto value = NoTags{};
};

template <auto... Tags>
struct NormalizeTagList<TagList<Tags...>> {
    static_assert(sizeof...(Tags) <= max_flattened_tag_count,
                  "too many reflection tags after flattening; split the metadata or reduce nested TagList depth");
    constexpr static auto value = TagList<Tags...>{};
};

template <auto... Tags>
struct NormalizeTags {
    using flattened             = decltype(flattenTags<Tags...>());
    using dedup                 = decltype(deduplicateTags(flattened{}));
    constexpr static auto value = NormalizeTagList<dedup>::value;
};

template <auto... Tags>
inline constexpr auto normalize_tags_v = NormalizeTags<Tags...>::value;

} // namespace detail

template <std::size_t I, auto... Tags>
constexpr auto get(TagList<Tags...> tags) noexcept -> detail::tag_list_element_t<I, Tags...> {
    static_cast<void>(tags);
    return detail::TagListElement<I, Tags...>::value;
}

template <auto... Tags>
template <std::size_t I>
constexpr auto TagList<Tags...>::get() const {
    return nekoproto::get<I>(*this);
}

template <auto... Tags, typename Accessor>
inline constexpr auto makeTags(Accessor&& accessor) { // NOLINT
    constexpr auto NormalizedTags = detail::normalize_tags_v<Tags...>;

    if constexpr (detail::is_resolvable_without_context_v<std::decay_t<Accessor>>) {
        using ResolvedType = detail::resolve_without_context_t<std::decay_t<Accessor>>;
        static_assert(detail::performCheck<ResolvedType, NormalizedTags>(),
                      "Tag check failed for a member of type, please check the tag definition");
    }
    return TaggedField<NormalizedTags, Accessor>{std::forward<Accessor>(accessor)};
}

template <typename T>
struct IsTaggedField : std::false_type {};

template <auto Tags, typename Accessor>
struct IsTaggedField<TaggedField<Tags, Accessor>> : std::true_type {};

template <auto Tags, typename Accessor>
struct IsTaggedField<TaggedField<Tags, Accessor>&> : std::true_type {};

template <auto Tags, typename Accessor>
struct IsTaggedField<const TaggedField<Tags, Accessor>> : std::true_type {};

template <auto Tags, typename Accessor>
struct IsTaggedField<const TaggedField<Tags, Accessor>&> : std::true_type {};

template <typename T>
inline constexpr bool is_tagged_field_v = IsTaggedField<std::decay_t<T>>::value; // NOLINT

template <typename T, class enable = void>
struct UnwrapTaggedField {
    using type                 = T;
    constexpr static auto tags = NoTags{}; // NOLINT
};

template <typename T>
struct UnwrapTaggedField<T, std::enable_if_t<is_tagged_field_v<T>>> {
    using type                 = typename std::decay_t<T>::accessor_type;
    constexpr static auto tags = std::decay_t<T>::tags; // NOLINT
};

template <typename T>
using unwrap_tagged_field_t = typename UnwrapTaggedField<T>::type;

template <typename T>
inline constexpr auto unwrap_tagged_field_tags_v = UnwrapTaggedField<T>::tags; // NOLINT

template <typename T>
constexpr auto fieldAccessor(T&& value) noexcept -> decltype(auto) {
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
struct ResolveMemberType {
    using type = field_accessor_t<Accessor>;
};

template <typename Accessor, typename HostType>
    requires(std::is_invocable_v<Accessor, HostType&>)
struct ResolveMemberType<Accessor, HostType> {
    using type = std::invoke_result_t<Accessor, HostType&>;
};

template <typename Accessor, typename HostType>
using resolve_member_type_t = typename std::decay_t<typename ResolveMemberType<std::decay_t<Accessor>, HostType>::type>;

namespace detail {

template <typename ValuesTuple, typename ContextType, std::size_t... Is>
constexpr auto performAllChecksImpl(std::index_sequence<Is...> /*unused*/) -> bool {
    return (performCheck<resolve_member_type_t<field_accessor_t<std::tuple_element_t<Is, ValuesTuple>>, ContextType>,
                         field_tags_v<std::tuple_element_t<Is, ValuesTuple>>>() &&
            ...);
}

template <typename ValuesTuple, typename ContextType>
constexpr auto performAllChecks() -> bool {
    if constexpr (std::tuple_size<ValuesTuple>::value > 0) {
        return performAllChecksImpl<ValuesTuple, ContextType>(
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
constexpr auto tagHas(const Tags& tags) -> bool;

template <typename Prop, typename Tags>
constexpr auto tagGet(const Tags& tags) -> typename Prop::type;

template <typename Prop, typename Tags>
constexpr auto tagGetExisting(const Tags& tags) -> decltype(auto);

template <typename T, auto... Tags>
constexpr auto tagHasType(const TagList<Tags...>& tags) -> bool {
    static_cast<void>(tags);

    if constexpr (sizeof...(Tags) == 0) {
        return false;
    } else {
        return tagListHasTypeImpl<T, Tags...>();
    }
}

template <typename T, auto... Tags>
constexpr auto tagGetType(const TagList<Tags...>& tags) -> T {
    static_cast<void>(tags);

    if constexpr (sizeof...(Tags) == 0) {
        static_assert(std::is_default_constructible_v<T>,
                      "Tag not found and requested tag type is not default constructible");
        return T{};
    } else {
        return tagListGetTypeImpl<T, Tags...>();
    }
}

template <typename T>
constexpr auto tagHasType(const NoTags& tags) -> bool {
    static_cast<void>(tags);
    return false;
}

template <typename T>
constexpr auto tagGetType(const NoTags& tags) -> T {
    static_cast<void>(tags);
    static_assert(std::is_default_constructible_v<T>,
                  "Tag not found and requested tag type is not default constructible");
    return T{};
}

template <typename T, typename Tag>
constexpr auto tagHasType(const Tag& tag) -> bool {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (std::is_same_v<std::decay_t<T>, RawTag>) {
        return true;
    } else if constexpr (requires { tag.base; }) {
        return tagHasType<T>(tag.base);
    } else {
        return false;
    }
}

template <typename T, typename Tag>
constexpr auto tagGetType(const Tag& tag) -> T {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (std::is_same_v<std::decay_t<T>, RawTag>) {
        return tag;
    } else if constexpr (requires { tag.base; }) {
        return tagGetType<T>(tag.base);
    } else {
        static_assert(std::is_default_constructible_v<T>,
                      "Tag not found and requested tag type is not default constructible");
        return T{};
    }
}

template <typename Prop, typename Tag>
constexpr auto tagHas(const Tag& tag) -> bool {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (tag_property_has_available_v<Prop, RawTag>) {
        if (Prop::has(tag)) {
            return true;
        }
    }

    if constexpr (requires { tag.base; }) {
        return tagHas<Prop>(tag.base);
    } else {
        return false;
    }
}

template <typename Prop, typename Tag>
constexpr auto tagGet(const Tag& tag) -> typename Prop::type {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (tag_property_get_available_v<Prop, RawTag>) {
        if (Prop::has(tag)) {
            return Prop::get(tag);
        }
    }

    if constexpr (requires { tag.base; }) {
        return tagGet<Prop>(tag.base);
    } else {
        return Prop::missing();
    }
}

template <typename Prop, typename Tag>
constexpr auto tagGetExisting(const Tag& tag) -> decltype(auto) {
    using RawTag = std::remove_cvref_t<Tag>;

    if constexpr (tag_property_get_available_v<Prop, RawTag>) {
        return Prop::get(tag);
    } else if constexpr (requires { tag.base; }) {
        return tagGetExisting<Prop>(tag.base);
    } else {
        static_assert(nekoproto::always_false_v<RawTag>, "requested tag property is missing");
    }
}

template <typename Prop, auto... Tags>
constexpr auto tagListHasImpl() -> bool {
    if constexpr (sizeof...(Tags) == 0) {
        return false;
    } else {
        return (tagHas<Prop>(Tags) || ...);
    }
}

template <typename Prop, auto... Tags>
constexpr auto tagListGetImpl() -> typename Prop::type {
    if constexpr (sizeof...(Tags) == 0) {
        return Prop::missing();
    } else {
        typename Prop::type result = Prop::missing();
        (([&]() {
            if constexpr (tagHas<Prop>(Tags)) {
                result = tagGet<Prop>(Tags);
            }
        }()), ...);
        return result;
    }
}

template <typename Prop, auto... Tags>
constexpr auto tagListGetExistingImpl() -> decltype(auto) {
    static_assert(sizeof...(Tags) > 0, "requested tag property is missing");
    constexpr auto matchingIndex = []() constexpr -> std::size_t {
        std::size_t found = sizeof...(Tags);
        std::size_t idx = 0;
        (([&]() {
            if constexpr (tagHas<Prop>(Tags)) {
                found = idx;
            }
            ++idx;
        }()), ...);
        return found;
    }();
    static_assert(matchingIndex < sizeof...(Tags), "requested tag property is missing");
    return tagGetExisting<Prop>(TagListElement<matchingIndex, Tags...>::value);
}

template <typename Prop, auto... Tags>
constexpr auto tagHas(const TagList<Tags...>& tags) -> bool {
    static_cast<void>(tags);
    return tagListHasImpl<Prop, Tags...>();
}

template <typename Prop, auto... Tags>
constexpr auto tagGet(const TagList<Tags...>& tags) -> typename Prop::type {
    static_cast<void>(tags);
    return tagListGetImpl<Prop, Tags...>();
}

template <typename Prop, auto... Tags>
constexpr auto tagGetExisting(const TagList<Tags...>& tags) -> decltype(auto) {
    static_cast<void>(tags);
    return tagListGetExistingImpl<Prop, Tags...>();
}

} // namespace detail

namespace tag_query {
template <typename Prop, typename Tags>
constexpr auto has(const Tags& tags) -> bool {
    return detail::tagHas<Prop>(tags);
}

template <typename Tag, typename Tags>
constexpr auto hasTag(const Tags& tags) -> bool {
    return detail::tagHasType<Tag>(tags);
}

template <typename Prop, typename Tags>
constexpr auto get(const Tags& tags) -> decltype(auto) {
    if constexpr (requires { typename Prop::type; }) {
        return detail::tagGet<Prop>(tags);
    } else {
        return detail::tagGetExisting<Prop>(tags);
    }
}

template <typename Tag, typename Tags>
constexpr auto getTag(const Tags& tags) -> Tag {
    return detail::tagGetType<Tag>(tags);
}
} // namespace tag_query

/**
 * @brief Unified visitor for reflection tags (supports TagList<...>, single Tag instance, or NoTags).
 */
template <typename Tags, typename Visitor>
constexpr void forEachTag(const Tags& tags, Visitor&& visitor) {
    if constexpr (is_tag_list_v<std::remove_cvref_t<Tags>>) {
        std::apply([&visitor](const auto&... tag) { (visitor(tag), ...); }, tags.tuple());
    } else if constexpr (!std::is_same_v<std::remove_cvref_t<Tags>, NoTags>) {
        visitor(tags);
    }
}
} // namespace nekoproto

namespace std {
template <auto... Tags>
struct tuple_size<nekoproto::TagList<Tags...>> : integral_constant<size_t, sizeof...(Tags)> {};

template <size_t I, auto... Tags>
struct tuple_element<I, nekoproto::TagList<Tags...>> {
    using type = nekoproto::detail::tag_list_element_t<I, Tags...>;
};

template <size_t I, auto... Tags>
constexpr auto get(const nekoproto::TagList<Tags...>& tags) noexcept
    -> nekoproto::detail::tag_list_element_t<I, Tags...> {
    return nekoproto::get<I>(tags);
}

template <size_t I, auto... Tags>
constexpr auto get(nekoproto::TagList<Tags...>& tags) noexcept -> nekoproto::detail::tag_list_element_t<I, Tags...> {
    return nekoproto::get<I>(tags);
}

template <size_t I, auto... Tags>
constexpr auto get(nekoproto::TagList<Tags...>&& tags) noexcept -> nekoproto::detail::tag_list_element_t<I, Tags...> {
    return nekoproto::get<I>(tags);
}
} // namespace std

#ifdef NEKO_PROTO_USE_FMT
#include <fmt/format.h>
namespace fmt {
template <typename T>
struct formatter<nekoproto::tag_detail::TagValue<T>> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const nekoproto::tag_detail::TagValue<T>& value, FormatContext& ctx) const -> decltype(ctx.out()) {
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
struct formatter<nekoproto::tag_detail::TagValue<T>> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const nekoproto::tag_detail::TagValue<T>& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        if (!value.declared) {
            return std::format_to(ctx.out(), "<undeclared>");
        }
        return std::format_to(ctx.out(), "{}", value.value);
    }
};
} // namespace std
#endif
#define NEKO_DETAIL_DEFINE_TAG_PROPERTY(Type, member, ClassName)                                                       \
    struct ClassName {                                                                                                 \
        using type = Type;                                                                                             \
        static constexpr auto missing() noexcept -> type { return {}; }                                                \
        template <typename Tag>                                                                                        \
        static constexpr auto has(const Tag& tag) -> bool {                                                            \
            if constexpr (requires { tag.member; }) {                                                                  \
                return nekoproto::tag_detail::tagValueDeclared(tag.member);                                            \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto get(const Tag& tag) -> type                                                              \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return nekoproto::tag_detail::makeTagValueCommon(tag.member);                                              \
        }                                                                                                              \
    };

#define NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(member, ClassName)                                                       \
    struct ClassName {                                                                                                 \
        template <typename Tag>                                                                                        \
        static constexpr auto has(const Tag& tag) -> bool {                                                            \
            if constexpr (requires { tag.member; }) {                                                                  \
                return nekoproto::tag_detail::tagValueDeclared(tag.member);                                            \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto get(const Tag& tag) -> decltype(auto)                                                    \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return nekoproto::tag_detail::makeTagValueCommon(tag.member);                                              \
        }                                                                                                              \
    };

#define NEKO_DETAIL_DEFINE_TYPE_TAG_PROPERTY(Type, member, ClassName, TraitName)                                       \
    template <typename Value>                                                                                          \
    struct ClassName {                                                                                                 \
        using type = Type;                                                                                             \
        static constexpr auto missing() noexcept -> type {                                                             \
            if constexpr (requires { TraitName<Value>::value; }) {                                                     \
                return TraitName<Value>::value;                                                                        \
            } else {                                                                                                   \
                return {};                                                                                             \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto has(const Tag& tag) -> bool {                                                            \
            if constexpr (requires { tag.member; }) {                                                                  \
                return nekoproto::tag_detail::tagValueDeclared(tag.member);                                            \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto get(const Tag& tag) -> type                                                              \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return static_cast<type>(tag.member);                                                                      \
        }                                                                                                              \
    };

#define NEKO_DEFINE_TAG_PROPERTY(Type, member, ClassName) NEKO_DETAIL_DEFINE_TAG_PROPERTY(Type, member, ClassName)
#define NEKO_DEFINE_TAG_VALUE_PROPERTY(member, ClassName) NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(member, ClassName)
#define NEKO_DEFINE_TYPE_TAG_PROPERTY(Type, member, ClassName, TraitName)                                              \
    NEKO_DETAIL_DEFINE_TYPE_TAG_PROPERTY(Type, member, ClassName, TraitName)

