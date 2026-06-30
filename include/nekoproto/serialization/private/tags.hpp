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

namespace tag_prop {
NEKO_DEFINE_TAG_PROP(std::string_view, comment, comment) // NOLINT
NEKO_DEFINE_TAG_PROP(std::string_view, name, name)       // NOLINT
NEKO_DEFINE_TAG_PROP(bool, rawString, raw_string)        // NOLINT
NEKO_DEFINE_TAG_PROP(bool, skipable, skipable)           // NOLINT

NEKO_DEFINE_TYPE_TAG_PROP(bool, flat, flat)         // NOLINT
NEKO_DEFINE_TYPE_TAG_PROP(bool, unframed, unframed) // NOLINT

template <typename Value>
struct fixed_length { // NOLINT
    using type = std::size_t;

    static constexpr type missing() noexcept { return 0; }

    template <typename Tag>
    static constexpr bool has(const Tag& tag) {
        if constexpr (requires { tag.fixedLength; }) {
            return detail::tag_value_declared(tag.fixedLength);
        } else {
            return false;
        }
    }

    template <typename Tag>
    static constexpr type get(const Tag& tag)
        requires requires { tag.fixedLength; }
    {
        const auto size = static_cast<std::size_t>(tag.fixedLength);
        if constexpr (std::is_void_v<Value>) {
            return size;
        } else {
            return size == 1 ? sizeof(Value) : size;
        }
    }
};
} // namespace tag_prop

template <ConstexprString Comment>
struct CommentTag {
    constexpr static auto comment = Comment.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Comment>
inline constexpr auto comment_tag = CommentTag<Comment>{}; // NOLINT

template <ConstexprString Name>
struct NameTag {
    constexpr static auto name = Name.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Name>
inline constexpr auto rename_tag = NameTag<Name>{}; // NOLINT

struct JsonTags {
    TagValue<bool> flat{};
    TagValue<bool> skipable{};
    TagValue<bool> rawString{};

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
    TagValue<std::size_t> fixedLength{};
    TagValue<bool> unframed{};
};
NEKO_END_NAMESPACE
