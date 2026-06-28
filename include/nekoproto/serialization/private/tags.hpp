/**
 * @file tags.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-28
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/global/string_literal.hpp"

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

NEKO_BEGIN_NAMESPACE
namespace detail {
// 默认情况：无法在没有上下文的情况下解析
template <typename T>
struct is_resolvable_without_context : std::false_type {}; // NOLINT

// 特化：成员对象指针是可以在没有上下文的情况下解析的
template <typename MemberType, typename ClassType>
struct is_resolvable_without_context<MemberType ClassType::*> : std::true_type {};

template <typename Fn>
struct is_resolvable_without_context<std::is_invocable<Fn>> : std::true_type {};

template <typename T>
inline constexpr bool is_resolvable_without_context_v = is_resolvable_without_context<T>::value; // NOLINT

// --- 相应的解析器 ---

// 默认情况：返回原类型
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

// 特化：为成员对象指针提取其类型
template <typename MemberType, typename ClassType>
struct resolve_without_context<MemberType ClassType::*> {
    using type = MemberType;
};

template <typename T>
using resolve_without_context_t = typename resolve_without_context<T>::type;
/**
 * @brief 检查一个 Tag 类型是否提供了 constexpr_check 函数，如果提供了就执行它。
 *
 * @tparam T 被包裹的数据类型.
 * @tparam tags 具体的 tag 实例.
 * @return 如果 TagType 中定义了 constexpr_check 且返回 true，或者未定义，则返回 true。
 *         如果定义了 constexpr_check 但返回 false，则返回 false。
 */
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
        // make to std::vector
        return std::vector<std::decay_t<T>>{std::begin(value), std::end(value)};
    } else if constexpr (NEKO_NAMESPACE::detail::is_std_array<std::decay_t<T>>::value) {
        // make to std::vector
        return std::vector<typename std::decay_t<T>::value_type>{std::begin(value), std::end(value)};
    } else if constexpr (is_constexpr_string<T>::value) {
        return value.view();
    } else {
        return std::forward<T>(value);
    }
}

} // namespace detail

template <typename T, class = void>
struct is_flat_tag : std::false_type {}; // NOLINT

template <typename T, class = void>
struct is_unframed_tag : std::false_type {}; // NOLINT

namespace tag_access {
#define NEKO_DEFINE_NESTED_TAG(Type, name, fname)                                                                      \
    template <typename T>                                                                                              \
    constexpr Type fname(const T& tags) {                                                                              \
        static_cast<void>(tags);                                                                                       \
        using Tag = std::remove_cvref_t<T>;                                                                            \
        if constexpr (requires { tags.name; }) {                                                                       \
            return NEKO_NAMESPACE::detail::make_tag_value_common(tags.name);                                           \
        } else {                                                                                                       \
            return {};                                                                                                 \
        }                                                                                                              \
    }                                                                                                                  \
    template <typename T>                                                                                              \
    constexpr Type recursive_##fname(const T& tags) {                                                                  \
        static_cast<void>(tags);                                                                                       \
        using Tag = std::remove_cvref_t<T>;                                                                            \
        if constexpr (requires { tags.name; }) {                                                                       \
            return NEKO_NAMESPACE::detail::make_tag_value_common(tags.name);                                           \
        } else if constexpr (requires { tags.base; }) {                                                                \
            return recursive_##fname(tags.base);                                                                       \
        } else {                                                                                                       \
            return {};                                                                                                 \
        }                                                                                                              \
    }                                                                                                                  \
    template <typename T>                                                                                              \
    constexpr bool has_##fname(const T& tags) {                                                                        \
        static_cast<void>(tags);                                                                                       \
        using Tag = std::remove_cvref_t<T>;                                                                            \
        return requires { tags.name; };                                                                                \
    }                                                                                                                  \
    template <typename T>                                                                                              \
    constexpr bool has_recursive_##fname(const T& tags) {                                                              \
        using Tag = std::remove_cvref_t<T>;                                                                            \
        if constexpr (requires { tags.name; }) {                                                                       \
            return true;                                                                                               \
        } else if constexpr (requires { tags.base; }) {                                                                \
            return has_recursive_##fname(tags.base);                                                                   \
        } else {                                                                                                       \
            return false;                                                                                              \
        }                                                                                                              \
    }                                                                                                                  \
    template <typename T>                                                                                              \
    constexpr auto consume_##fname(const T& tags) {                                                                    \
        using Tag = std::remove_cvref_t<T>;                                                                            \
        if constexpr (requires {                                                                                       \
                          tags.name;                                                                                   \
                          tags.base;                                                                                   \
                      }) {                                                                                             \
            return tags.base;                                                                                          \
        } else {                                                                                                       \
            return tags;                                                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    template <typename T>                                                                                              \
    constexpr auto consume_recursive_##fname(const T& tags) {                                                          \
        using Tag = std::remove_cvref_t<T>;                                                                            \
        if constexpr (requires {                                                                                       \
                          tags.name;                                                                                   \
                          tags.base;                                                                                   \
                      }) {                                                                                             \
            return tags.base;                                                                                          \
        } else if constexpr (requires { tags.base; }) {                                                                \
            if constexpr (has_recursive_##fname(tags.base)) {                                                          \
                constexpr auto ConsumedBase = consume_recursive_##fname(tags.base);                                    \
                return NEKO_NAMESPACE::detail::rebind_tag_base<Tag, ConsumedBase>();                                   \
            } else {                                                                                                   \
                return tags;                                                                                           \
            }                                                                                                          \
        } else {                                                                                                       \
            return tags;                                                                                               \
        }                                                                                                              \
    }

template <typename Value, typename T>
constexpr bool is_flat(const T& tags) {
    if constexpr (requires { tags.flat; }) {
        return tags.flat;
    } else if constexpr (requires { tags.base; }) {
        return is_flat<Value>(tags.base);
    } else if constexpr (is_flat_tag<Value>::value) {
        return true;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_skipable(const T& tags) {
    if constexpr (requires { tags.skipable; }) {
        return tags.skipable;
    } else if constexpr (requires { tags.base; }) {
        return is_skipable(tags.base);
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_fixed_length(const T& tags) {
    if constexpr (requires { tags.fixedLength; }) {
        return tags.fixedLength > 0;
    } else if constexpr (requires { tags.base; }) {
        return is_fixed_length(tags.base);
    } else {
        return false;
    }
}

template <typename Value, typename T>
constexpr std::size_t fixed_length(const T& tags) {
    if constexpr (requires { tags.fixedLength; }) {
        const auto size = static_cast<std::size_t>(tags.fixedLength);
        return size == 1 ? sizeof(Value) : size;
    } else if constexpr (requires { tags.base; }) {
        return fixed_length<Value>(tags.base);
    } else {
        return 0;
    }
}

template <typename Value, typename T>
constexpr bool is_unframed(const T& tags) {
    if constexpr (requires { tags.unframed; }) {
        return tags.unframed;
    } else if constexpr (requires { tags.base; }) {
        return is_unframed<Value>(tags.base);
    } else if constexpr (is_unframed_tag<Value>::value) {
        return true;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_raw_string(const T& tags) {
    if constexpr (requires { tags.rawString; }) {
        return tags.rawString;
    } else if constexpr (requires { tags.base; }) {
        return is_raw_string(tags.base);
    } else {
        return false;
    }
}
} // namespace tag_access

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
        // 如果可以，立即用解析出的类型进行检查
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

template <ConstexprString Comment, auto BaseTags = NoTags{}>
struct CommentTag {
    constexpr static auto comment = Comment.view(); // NOLINT
    constexpr static auto base    = BaseTags;       // NOLINT

    template <auto NewBase>
    using rebind_base = CommentTag<Comment, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Comment, auto BaseTags = NoTags{}>
inline constexpr auto comment_tag = CommentTag<Comment, BaseTags>{}; // NOLINT

template <ConstexprString Name, auto BaseTags = NoTags{}>
struct NameTag {
    constexpr static auto name = Name.view(); // NOLINT
    constexpr static auto base = BaseTags;    // NOLINT

    template <auto NewBase>
    using rebind_base = NameTag<Name, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Name, auto BaseTags = NoTags{}>
inline constexpr auto rename_tag = NameTag<Name, BaseTags>{}; // NOLINT

namespace detail {
template <typename Tag, auto NewBase>
constexpr auto rebind_tag_base() {
    using CleanTag = std::remove_cvref_t<Tag>;
    if constexpr (requires { typename CleanTag::template rebind_base<NewBase>; }) {
        return typename CleanTag::template rebind_base<NewBase>{};
    } else {
        static_assert(always_false_v<CleanTag>,
                      "Tag wrappers with recursive consumers must provide rebind_base<NewBase>");
    }
}
} // namespace detail

namespace tag_access {
NEKO_DEFINE_NESTED_TAG(std::string_view, comment, comment)
NEKO_DEFINE_NESTED_TAG(std::string_view, name, name)
} // namespace tag_access

// --- Helper Trait to Identify FieldSpec ---
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

/**
 * @brief 上下文感知的成员类型解析器
 *
 * @tparam Accessor 访问器类型 (成员指针或可调用对象)
 * @tparam HostType 宿主类类型 (例如 Test2)
 */
template <typename Accessor, typename HostType>
struct resolve_member_type { // NOLINT
    using type = field_accessor_t<Accessor>;
};

// 为可调用对象 (如 Lambda) 提供特化版本
template <typename Accessor, typename HostType>
    requires(std::is_invocable_v<Accessor, HostType&>)
struct resolve_member_type<Accessor, HostType> {
    using type = std::invoke_result_t<Accessor, HostType&>;
};

template <typename Accessor, typename HostType>
using resolve_member_type_t =
    typename std::decay_t<typename resolve_member_type<std::decay_t<Accessor>, HostType>::type>;

struct JsonTags {
    bool flat      = false;
    bool skipable  = false;
    bool rawString = false;

    /**
     * @brief 对 JsonTags 的类型约束进行编译期检查.
     *
     * @tparam T 被包裹的数据类型.
     * @tparam tags 具体的 tag 实例.
     * @return 如果类型 T 满足 tags 的约束，则返回 true.
     */
    template <typename T, auto tags>
    constexpr static bool constexpr_check() { // NOLINT
        if constexpr (tags.rawString) {
            // 如果 rawString 为 true，则包裹的类型必须是字符串类型
            if constexpr (detail::is_optional<T>::value) {
                constexpr bool IsString =
                    std::is_same_v<T, std::optional<std::string>> || std::is_same_v<T, std::optional<std::string_view>>;
                static_assert(IsString, "rawString is true, but the type is not std::optional<std::string> or "
                                        "std::optional<std::string_view>");
                return IsString;
            } else {
                constexpr bool IsString = std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;
                static_assert(IsString, "rawString is true, but the type is not std::string or std::string_view");
                return IsString;
            }
        }
        if constexpr (tags.skipable) {
            // Missing fields are handled by the enclosing object parser. Any field
            // type can be skipped because deserialization targets an existing object.
            return true;
        }
        return true;
    }
};

struct BinaryTags {
    std::size_t fixedLength = 0;
    bool unframed           = false;
};
namespace detail {
template <typename ValuesTuple, typename ContextType, std::size_t... Is>
constexpr bool perform_all_checks_impl(std::index_sequence<Is...> /*unused*/) {
    // 对元组中的每一个元素，都调用 perform_check
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
