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
#include "nekoproto/global/traits.hpp"

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

enum class YamlScalarStyle {
    Any,
    Plain,
    SingleQuoted,
    DoubleQuoted,
    Literal,
    Folded,
};

enum class YamlCollectionStyle {
    Any,
    Flow,
    Block,
};

/**
 * @brief Selects the wire shape used by union-like parsers such as std::variant.
 *
 * TaggedArray is the unambiguous default: [alternative_index, alternative_value].
 * Untagged writes only the active value and is intended for compatibility with
 * formats that historically inferred the alternative from the payload.
 */
enum class UnionEncoding {
    TaggedArray,
    Untagged,
};

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, leading_comment, leading_comment)   // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, trailing_comment, trailing_comment) // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, leading_comment, comment)           // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, name, name)                         // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, raw_string, raw_string)                         // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, raw_fixed_data, raw_fixed_data)                 // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, skippable, skippable)                           // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, yaml_tag, yaml_tag)                 // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, yaml_anchor, yaml_anchor)           // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(YamlScalarStyle, yaml_scalar_style, yaml_scalar_style) // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(YamlCollectionStyle, yaml_collection_style,            // NOLINT
                                yaml_collection_style)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, inline_table, inline_table)            // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(UnionEncoding, encoding, union_encoding)     // NOLINT

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
template <ConstexprString Text>
consteval bool yaml_tag_characters_valid() {
    for (const auto ch : Text.view()) {
        if (ch <= ' ') {
            return false;
        }
    }
    return true;
}

template <ConstexprString Text>
consteval bool yaml_tag_prefix_valid() {
    if constexpr (Text.size() == 0) {
        return false;
    }
    const auto view = Text.view();
    if (view[0] == '!') {
        return true;
    }
    if (!((view[0] >= 'a' && view[0] <= 'z') || (view[0] >= 'A' && view[0] <= 'Z'))) {
        return false;
    }
    for (std::size_t idx = 1; idx < view.size(); ++idx) {
        const auto ch = view[idx];
        if (ch == ':') {
            return true;
        }
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '+' ||
              ch == '-' || ch == '.')) {
            return false;
        }
    }
    return false;
}

template <ConstexprString Text>
consteval bool yaml_anchor_characters_valid() {
    for (const auto ch : Text.view()) {
        if (ch <= ' ' || ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == ',') {
            return false;
        }
    }
    return true;
}

struct serialization_ignore_tag_impl {
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

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

template <ConstexprString Tag>
struct yaml_tag_impl {
    static_assert(Tag.size() > 0, "YAML tag must not be empty");
    static_assert(yaml_tag_characters_valid<Tag>(), "YAML tag must not contain whitespace or control characters");
    static_assert(yaml_tag_prefix_valid<Tag>(), "YAML tag must be a local tag starting with '!' or a global URI tag");

    constexpr static auto yaml_tag = Tag.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Anchor>
struct yaml_anchor_tag_impl {
    static_assert(Anchor.size() > 0, "YAML anchor must not be empty");
    static_assert(yaml_anchor_characters_valid<Anchor>(),
                  "YAML anchor must not contain whitespace, control characters, or []{},");

    constexpr static auto yaml_anchor = Anchor.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <YamlScalarStyle Style>
struct yaml_scalar_style_tag_impl {
    constexpr static auto yaml_scalar_style = Style; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        if constexpr (Style != YamlScalarStyle::Any) {
            static_assert(traits::is_scalar_like_v<T>,
                          "YAML scalar style tags require a scalar field such as string, arithmetic, enum, or "
                          "optional scalar");
        }
        return true;
    }
};

template <YamlCollectionStyle Style>
struct yaml_collection_style_tag_impl {
    constexpr static auto yaml_collection_style = Style; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        if constexpr (Style != YamlCollectionStyle::Any) {
            static_assert(traits::is_collection_like_v<T>,
                          "YAML collection style tags require a sequence, map, tuple, pair, reflected/custom object, "
                          "or optional collection/object");
        }
        return true;
    }
};
} // namespace tag_detail

inline constexpr auto serialization_ignore_tag = tag_detail::serialization_ignore_tag_impl{}; // NOLINT

template <ConstexprString Comment>
inline constexpr auto leading_comment_tag = tag_detail::leading_comment_tag_impl<Comment>{}; // NOLINT

template <ConstexprString Comment>
inline constexpr auto trailing_comment_tag = tag_detail::trailing_comment_tag_impl<Comment>{}; // NOLINT

template <ConstexprString Comment>
inline constexpr auto comment_tag = leading_comment_tag<Comment>; // NOLINT

template <ConstexprString Name>
inline constexpr auto rename_tag = tag_detail::rename_tag_impl<Name>{}; // NOLINT

template <ConstexprString Tag>
inline constexpr auto yaml_tag = tag_detail::yaml_tag_impl<Tag>{}; // NOLINT

template <ConstexprString Anchor>
inline constexpr auto yaml_anchor_tag = tag_detail::yaml_anchor_tag_impl<Anchor>{}; // NOLINT

template <YamlScalarStyle Style>
inline constexpr auto yaml_scalar_style_tag = tag_detail::yaml_scalar_style_tag_impl<Style>{}; // NOLINT

template <YamlCollectionStyle Style>
inline constexpr auto yaml_collection_style_tag = tag_detail::yaml_collection_style_tag_impl<Style>{}; // NOLINT

struct TomlTag {
    tag_detail::tag_value<bool> inline_table{};

    template <typename T, auto /*Tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

struct ParserTag {
    tag_detail::tag_value<bool> flat{};
    tag_detail::tag_value<bool> skippable{};

    template <typename T, auto /*Tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

/**
 * @brief Parser-level policy for one union boundary.
 *
 * The variant parser consumes this tag instead of forwarding it to the active
 * alternative. Consequently an outer untagged union does not implicitly make
 * a nested union untagged as well; the nested field needs its own UnionTag.
 */
struct UnionTag {
    tag_detail::tag_value<UnionEncoding> encoding{};

    template <typename T, auto /*Tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

struct JsonTag {
    ParserTag base{};
    // Compatibility shim: flat/skippable are parser-level tags. Prefer ParserTag
    // for new metadata while existing JsonTag users keep working.
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
        return true;
    }
};

struct BinaryTag {
    tag_detail::tag_value<std::size_t> fixed_length{};
    tag_detail::tag_value<bool> raw_fixed_data{};
};

namespace tag_property {
struct ignore { // NOLINT
    using type = bool;

    static constexpr type missing() noexcept { return false; }

    template <typename Tag>
    static constexpr bool has(const Tag& tag) {
        using RawTag = std::remove_cvref_t<Tag>;
        static_cast<void>(tag);
        return std::is_same_v<RawTag, tag_detail::serialization_ignore_tag_impl>;
    }

    template <typename Tag>
    static constexpr type get(const Tag& tag) {
        using RawTag = std::remove_cvref_t<Tag>;
        static_cast<void>(tag);
        if constexpr (std::is_same_v<RawTag, tag_detail::serialization_ignore_tag_impl>) {
            return true;
        } else {
            return missing();
        }
    }
};
} // namespace tag_property
NEKO_END_NAMESPACE
