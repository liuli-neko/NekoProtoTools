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
#include "nekoproto/global/reflection_tags.hpp"
#include "nekoproto/global/string_literal.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
template <typename T, class = void>
struct is_flat_tag : std::false_type {}; // NOLINT

template <typename T, class = void>
struct is_unframed_tag : std::false_type {}; // NOLINT

namespace tag_query {

NEKO_DEFINE_NESTED_TAG(std::string_view, comment, comment)
NEKO_DEFINE_NESTED_TAG(std::string_view, name, name)

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
} // namespace tag_query

template <ConstexprString Comment, auto BaseTags = NoTags{}>
struct CommentTag {
    constexpr static auto comment = Comment.view(); // NOLINT
    constexpr static auto base    = BaseTags;       // NOLINT

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

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Name, auto BaseTags = NoTags{}>
inline constexpr auto rename_tag = NameTag<Name, BaseTags>{}; // NOLINT

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
NEKO_END_NAMESPACE
