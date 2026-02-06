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

#include <type_traits>
#include <utility>
#include <variant>

NEKO_BEGIN_NAMESPACE
namespace detail {
// 默认情况：无法在没有上下文的情况下解析
template <typename T>
struct is_resolvable_without_context : std::false_type {};

// 特化：成员对象指针是可以在没有上下文的情况下解析的
template <typename MemberType, typename ClassType>
struct is_resolvable_without_context<MemberType ClassType::*> : std::true_type {};

template <typename Fn>
struct is_resolvable_without_context<std::is_invocable<Fn>> : std::true_type {};

template <typename T>
inline constexpr bool is_resolvable_without_context_v = is_resolvable_without_context<T>::value;

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
} // namespace detail

namespace tag_access {
template <typename T>
constexpr bool is_flat(const T& tags) {
    if constexpr (requires { tags.flat; }) {
        return tags.flat;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_skipable(const T& tags) {
    if constexpr (requires { tags.skipable; }) {
        return tags.skipable;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_fixed_length(const T& tags) {
    if constexpr (requires { tags.fixedLength; }) {
        return tags.fixedLength;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_raw_string(const T& tags) {
    if constexpr (requires { tags.rawString; }) {
        return tags.rawString;
    } else {
        return false;
    }
}
} // namespace tag_access
template <auto Tags, typename InnerValue, char mode = 0>
struct TaggedValue;

// --- The Core Wrapper Template ---
template <auto Tags, typename InnerValue>
struct TaggedValue<Tags, InnerValue, 1> {
    using raw_type             = InnerValue;
    using tag_type             = decltype(Tags); // 获取 Tag 的实际类型
    constexpr static auto tags = Tags;           // NOLINT
    InnerValue value;

    operator InnerValue() && noexcept { return std::move(value); }
    operator InnerValue() const& noexcept { return value; }
    operator InnerValue() & noexcept { return value; }
    auto operator*() const noexcept { return value; }
    auto operator->() const noexcept { return &value; }
    auto operator*() noexcept { return value; }
    auto operator->() noexcept { return &value; }
    auto& operator=(InnerValue&& value) noexcept {
        this->value = std::move(value);
        return *this;
    }
    auto& operator=(const InnerValue& value) noexcept {
        this->value = value;
        return *this;
    }
    operator raw_type&&() noexcept { return std::move(value); }
    operator const raw_type&() const noexcept { return value; }
    operator raw_type&() noexcept { return value; }
};

template <auto Tags, typename InnerValue>
struct TaggedValue<Tags, InnerValue, 0> : TaggedValue<Tags, InnerValue, 1> {
    static_assert(detail::perform_check<InnerValue, Tags>(), "TaggedValue is not valid for this type");
};

template <auto Tags, typename InnerValue>
inline constexpr auto make_tags(InnerValue&& value) { // NOLINT
    if constexpr (detail::is_resolvable_without_context_v<std::decay_t<InnerValue>>) {
        // 如果可以，立即用解析出的类型进行检查
        using ResolvedType = detail::resolve_without_context_t<std::decay_t<InnerValue>>;
        static_assert(detail::perform_check<ResolvedType, Tags>(),
                      "Tag check failed for a member of type, please check the tag definition");
    }
    return TaggedValue<Tags, InnerValue, 1>{std::forward<InnerValue>(value)};
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

// --- Helper Trait to Identify TaggedValue ---
template <typename T>
struct is_tagged_value : std::false_type {}; // NOLINT
template <auto Tags, typename Inner, char mode>
struct is_tagged_value<TaggedValue<Tags, Inner, mode>> : std::true_type {};
template <auto Tags, typename Inner, char mode>
struct is_tagged_value<TaggedValue<Tags, Inner, mode>&> : std::true_type {};
template <auto Tags, typename Inner, char mode>
struct is_tagged_value<const TaggedValue<Tags, Inner, mode>> : std::true_type {};
template <auto Tags, typename Inner, char mode>
struct is_tagged_value<const TaggedValue<Tags, Inner, mode>&> : std::true_type {};
template <typename T>
inline constexpr bool is_tagged_value_v = is_tagged_value<std::decay_t<T>>::value; // NOLINT

template <typename T, class enable = void>
struct unwrap_tags { // NOLINT
    using type                 = T;
    constexpr static auto tags = NoTags{}; // NOLINT
};

template <typename T>
struct unwrap_tags<T, std::enable_if_t<is_tagged_value_v<T>>> {
    using type                 = typename std::decay_t<T>::raw_type;
    constexpr static auto tags = std::decay_t<T>::tags; // NOLINT
};

template <typename T>
using unwrap_tags_t = typename unwrap_tags<T>::type;

template <typename T>
inline constexpr auto unwrap_tags_v = unwrap_tags<T>::tags; // NOLINT

/**
 * @brief 上下文感知的成员类型解析器
 *
 * @tparam Accessor 访问器类型 (成员指针或可调用对象)
 * @tparam HostType 宿主类类型 (例如 Test2)
 */
template <typename Accessor, typename HostType>
struct resolve_member_type { // NOLINT
    using type = unwrap_tags_t<Accessor>;
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
            // 如果 skipable 为 true，则包裹的类型必须是可空的类型, 如 std::optional
            // 或者包含 std::monostate 的 std::variant
            constexpr bool IsEmptyAble = requires(T value) {
                { value.reset() }; // 需要支持 reset 操作
                { *value };        // 需要支持解引用操作
            } || requires(T value) {
                { value.index() };                  // 需要支持 index 操作
                { value.valueless_by_exception() }; // 需要支持 valueless_by_exception 操作
                { value.template emplace<std::monostate>() };
            };
            static_assert(IsEmptyAble, "skipable is true, but the type is not emptyable");
            return IsEmptyAble;
        }
        return true;
    }
};

struct BinaryTags {
    bool fixedLength = false;
};
namespace detail {
template <typename ValuesTuple, typename ContextType, std::size_t... Is>
constexpr bool perform_all_checks_impl(std::index_sequence<Is...> /*unused*/) {
    // 对元组中的每一个元素，都调用 perform_check
    return (perform_check<resolve_member_type_t<unwrap_tags_t<std::tuple_element_t<Is, ValuesTuple>>, ContextType>,
                          unwrap_tags_v<std::tuple_element_t<Is, ValuesTuple>>>() &&
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