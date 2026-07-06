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

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, leading_comment, leading_comment)   // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, trailing_comment, trailing_comment) // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, leading_comment, comment)           // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, name, name)                         // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, raw_string, raw_string)                         // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, skippable, skippable)                           // NOLINT

NEKO_DETAIL_DEFINE_TYPE_TAG_PROPERTY(bool, flat, flat)         // NOLINT
NEKO_DETAIL_DEFINE_TYPE_TAG_PROPERTY(bool, unframed, unframed) // NOLINT

template <typename Value>
struct fixed_length { // NOLINT
    using type = std::size_t;

    static constexpr type missing() noexcept { return 0; }

    template <typename Tag>
    static constexpr bool has(const Tag& tag) {
        if constexpr (requires { tag.fixed_length; }) {
            return tag_detail::tag_value_declared(tag.fixed_length);
        } else {
            return false;
        }
    }

    template <typename Tag>
    static constexpr type get(const Tag& tag)
        requires requires { tag.fixed_length; }
    {
        const auto size = static_cast<std::size_t>(tag.fixed_length);
        if constexpr (std::is_void_v<Value>) {
            return size;
        } else {
            return size == 1 ? sizeof(Value) : size;
        }
    }
};
} // namespace tag_property

namespace tag_detail {
template <ConstexprString Comment>
struct leading_comment_tag_impl {
    constexpr static auto leading_comment = Comment.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Comment>
struct trailing_comment_tag_impl {
    constexpr static auto trailing_comment = Comment.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Name>
struct rename_tag_impl {
    constexpr static auto name = Name.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};
} // namespace tag_detail

template <ConstexprString Comment>
inline constexpr auto leading_comment_tag = tag_detail::leading_comment_tag_impl<Comment>{}; // NOLINT

template <ConstexprString Comment>
inline constexpr auto trailing_comment_tag = tag_detail::trailing_comment_tag_impl<Comment>{}; // NOLINT

template <ConstexprString Comment>
inline constexpr auto comment_tag = leading_comment_tag<Comment>; // NOLINT

template <ConstexprString Name>
inline constexpr auto rename_tag = tag_detail::rename_tag_impl<Name>{}; // NOLINT

struct JsonTag {
    tag_detail::tag_value<bool> flat{};
    tag_detail::tag_value<bool> skippable{};
    tag_detail::tag_value<bool> raw_string{};

    /**
     * @brief 对 JsonTag 的类型约束进行编译期检查.
     *
     * @tparam T 被包裹的数据类型.
     * @tparam tags 具体的 tag 实例.
     * @return 如果类型 T 满足 tags 的约束，则返回 true.
     */
    template <typename T, auto Tags>
    constexpr static bool constexpr_check() { // NOLINT
        if constexpr (Tags.raw_string) {
            // 如果 raw_string 为 true，则包裹的类型必须是字符串类型
            if constexpr (detail::is_optional<T>::value) {
                constexpr bool is_string =
                    std::is_same_v<T, std::optional<std::string>> || std::is_same_v<T, std::optional<std::string_view>>;
                static_assert(is_string, "raw_string is true, but the type is not std::optional<std::string> or "
                                        "std::optional<std::string_view>");
                return is_string;
            } else {
                constexpr bool is_string = std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;
                static_assert(is_string, "raw_string is true, but the type is not std::string or std::string_view");
                return is_string;
            }
        }
        if constexpr (Tags.skippable) {
            // Missing fields are handled by the enclosing object parser. Any field
            // type can be skipped because deserialization targets an existing object.
            return true;
        }
        return true;
    }
};

struct BinaryTag {
    tag_detail::tag_value<std::size_t> fixed_length{};
    tag_detail::tag_value<bool> unframed{};
};
NEKO_END_NAMESPACE
