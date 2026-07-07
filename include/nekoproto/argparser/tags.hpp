#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflection_tags.hpp"
#include "nekoproto/global/string_literal.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser {

template <auto Value>
struct ArgCommand {
    constexpr static auto value = tag_detail::tag_value_declared(Value);
};

namespace detail {

// Type traits ----------------------------------------------------------------

template <typename T>
struct is_command_type : std::false_type {};

template <auto Value>
struct is_command_type<ArgCommand<Value>> : std::true_type {};

template <typename T>
struct is_arg_optional : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T>
struct is_arg_optional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_arg_optional_v = is_arg_optional<std::remove_cvref_t<T>>::value; // NOLINT

template <typename T>
struct optional_value;

template <typename T>
struct optional_value<std::optional<T>> {
    using type = T;
};

template <typename T>
using optional_value_t = typename optional_value<std::remove_cvref_t<T>>::type;

template <typename T>
struct is_vector : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {
    using value_type = T;
};

template <typename T>
inline constexpr bool is_vector_v = is_vector<std::remove_cvref_t<T>>::value; // NOLINT

template <typename T>
using vector_value_t = typename is_vector<std::remove_cvref_t<T>>::value_type;

template <typename T>
inline constexpr bool is_string_like_v = // NOLINT
    std::is_same_v<std::remove_cvref_t<T>, std::string> || std::is_same_v<std::remove_cvref_t<T>, std::string_view>;

template <typename T>
inline constexpr bool is_bool_value_v = std::is_same_v<std::remove_cvref_t<T>, bool>; // NOLINT

template <typename T>
inline constexpr bool is_bool_supported_v = []() consteval { // NOLINT
    using raw_t = std::remove_cvref_t<T>;
    if constexpr (is_bool_value_v<raw_t>) {
        return true;
    } else if constexpr (is_arg_optional_v<raw_t>) {
        return is_bool_value_v<optional_value_t<raw_t>>;
    } else if constexpr (is_vector_v<raw_t>) {
        return is_bool_value_v<vector_value_t<raw_t>>;
    } else {
        return false;
    }
}();

template <typename T>
inline constexpr bool is_range_value_v = // NOLINT
    std::is_arithmetic_v<std::remove_cvref_t<T>> && !std::is_same_v<std::remove_cvref_t<T>, bool>;

template <typename T>
inline constexpr bool is_range_supported_v = []() consteval { // NOLINT
    using raw_t = std::remove_cvref_t<T>;
    if constexpr (is_range_value_v<raw_t>) {
        return true;
    } else if constexpr (is_arg_optional_v<raw_t>) {
        return is_range_value_v<optional_value_t<raw_t>>;
    } else if constexpr (is_vector_v<raw_t>) {
        return is_range_value_v<vector_value_t<raw_t>>;
    } else {
        return false;
    }
}();

template <typename T>
inline constexpr bool is_nested_option_v = // NOLINT
    std::is_class_v<std::remove_cvref_t<T>> && !is_string_like_v<T> && !is_arg_optional_v<T> && !is_vector_v<T> &&
    NEKO_NAMESPACE::detail::has_values_meta<std::remove_cvref_t<T>>;
} // namespace detail

struct ArgTags {
    tag_detail::tag_value<bool> required{};    // NOLINT
    tag_detail::tag_value<bool> positional{};  // NOLINT
    tag_detail::tag_value<bool> flag{};        // NOLINT
    tag_detail::tag_value<bool> repeatable{};  // NOLINT
    tag_detail::tag_value<bool> hidden{};      // NOLINT
    tag_detail::tag_value<bool> command{};     // NOLINT
    tag_detail::tag_value<double> range_min{}; // NOLINT
    tag_detail::tag_value<double> range_max{}; // NOLINT

    template <typename T, auto Tags>
    constexpr static bool constexpr_check() { // NOLINT
        using raw_t = std::remove_cvref_t<T>;

        constexpr bool is_command =
            (Tags.command.declared && static_cast<bool>(Tags.command)) || detail::is_command_type<raw_t>::value;
        constexpr bool is_flag      = Tags.flag.declared && static_cast<bool>(Tags.flag);
        constexpr bool is_position  = Tags.positional.declared && static_cast<bool>(Tags.positional);
        constexpr bool has_range    = Tags.range_min.declared || Tags.range_max.declared;
        constexpr bool has_required = Tags.required.declared && static_cast<bool>(Tags.required);
        constexpr bool has_repeat   = Tags.repeatable.declared && static_cast<bool>(Tags.repeatable);

        if constexpr (has_range) {
            static_assert(detail::is_range_supported_v<raw_t>,
                          "argparser range tags require an arithmetic field, optional arithmetic field, or vector of "
                          "arithmetic values");
            static_assert(static_cast<double>(Tags.range_min) <= static_cast<double>(Tags.range_max),
                          "argparser range_min must be less than or equal to range_max");
        }
        if constexpr (is_flag) {
            static_assert(detail::is_bool_supported_v<raw_t>,
                          "argparser flag tags require a bool field, optional bool field, or vector<bool> field");
        }
        if constexpr (is_flag && is_position) {
            static_assert(!is_flag, "argparser positional fields cannot also be flags");
        }
        if constexpr (is_command) {
            static_assert(detail::is_command_type<raw_t>::value || NEKO_NAMESPACE::detail::has_values_meta<raw_t>,
                          "argparser command tags require a reflected command struct or ArgCommand placeholder");
            static_assert(!is_flag && !is_position && !has_range && !has_required && !has_repeat,
                          "argparser command tags cannot also be flag, positional, range, required, or repeatable");
        }
        return true;
    }
};
namespace detail {
inline constexpr bool is_character(char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); }
inline constexpr bool is_visible(char ch) { return (ch >= ' ' && ch <= '~'); }

template <ConstexprString Long = "">
struct arg_long_name_impl {
    static constexpr auto long_name = Long.view();
    template <typename T, auto Tags>
    constexpr static bool constexpr_check() {
        static_assert(Tags.long_name.size() > 1 && Tags.long_name.size() <= 64 &&
                          std::all_of(Tags.long_name.begin(), Tags.long_name.end(), is_visible),
                      "argparser long names must be visible ASCII characters and 1-64 characters long");
        return true;
    }
};

template <char Short = '\0'>
struct arg_short_name_impl {
    static constexpr auto short_name = Short;
    template <typename T, auto Tags>
    constexpr static bool constexpr_check() {
        static_assert(is_character(Tags.short_name), "argparser short names must be a single visible ASCII character");
        return true;
    }
};

template <ConstexprString Long = "", char Short = '\0'>
struct arg_name_impl {
    static constexpr auto long_name  = Long.view();
    static constexpr auto short_name = Short;

    template <typename T, auto Tags>
    constexpr static bool constexpr_check() {
        arg_long_name_impl<Long>::template constexpr_check<T, Tags>();
        arg_short_name_impl<Short>::template constexpr_check<T, Tags>();
        return true;
    }
};

template <auto Default>
struct arg_default_impl {
    constexpr static auto get_value()
        requires is_constexpr_string<decltype(Default)>::value
    {
        return Default.view();
    }
    constexpr static auto get_value() { return Default; }

    static constexpr auto default_value = get_value();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString... Choices>
struct arg_choices_impl {
    static constexpr std::array choices = {Choices.view()...};

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString Help = "">
struct arg_help_impl {
    static constexpr auto help = Help.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString ValueName = "">
struct arg_value_name_impl {
    static constexpr auto value_name = ValueName.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString EnvName = "">
struct arg_env_impl {
    static constexpr auto env_name = EnvName.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <char Separator>
struct arg_separator_impl {
    static constexpr auto separator = Separator;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString... Aliases>
struct arg_aliases_impl {
    static constexpr std::array aliases = {Aliases.view()...};

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <auto Implicit>
struct arg_implicit_impl {
    constexpr static auto get_value()
        requires is_constexpr_string<decltype(Implicit)>::value
    {
        return Implicit.view();
    }
    constexpr static auto get_value() { return Implicit; }

    static constexpr auto implicit_value = get_value();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString Group = "">
struct arg_group_impl {
    static constexpr auto group = Group.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString... Names>
struct arg_confflicts_impl {
    static constexpr std::array conflicts = {Names.view()...};

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString... Names>
struct arg_requires_impl {
    static constexpr std::array requires_names = {Names.view()...};

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <ConstexprString Message = "">
struct arg_deprecated_impl {
    static constexpr auto deprecated_message = Message.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

struct arg_case_insensitive_choices_impl {
    static constexpr bool case_insensitive_choices = true;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

struct arg_ignore_tag_impl {
    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};
} // namespace detail

template <ConstexprString Long = "">
inline constexpr auto arg_long_name = detail::arg_long_name_impl<Long>{};

template <char Short = '\0'>
inline constexpr auto arg_short_name = detail::arg_short_name_impl<Short>{};

template <ConstexprString Long = "", char Short = '\0'>
inline constexpr auto arg_name = detail::arg_name_impl<Long, Short>{};

template <ConstexprString... Choices>
inline constexpr auto arg_choices = detail::arg_choices_impl<Choices...>{};

template <auto Default>
inline constexpr auto arg_default = detail::arg_default_impl<Default>{};

template <ConstexprString Help = "">
inline constexpr auto arg_help = detail::arg_help_impl<Help>{};

template <ConstexprString ValueName = "">
inline constexpr auto arg_value_name = detail::arg_value_name_impl<ValueName>{};

template <ConstexprString EnvName = "">
inline constexpr auto arg_env = detail::arg_env_impl<EnvName>{};

template <char Separator>
inline constexpr auto arg_separator = detail::arg_separator_impl<Separator>{};

template <ConstexprString... Aliases>
inline constexpr auto arg_aliases = detail::arg_aliases_impl<Aliases...>{};

template <auto Implicit>
inline constexpr auto arg_implicit = detail::arg_implicit_impl<Implicit>{};

template <ConstexprString Group = "">
inline constexpr auto arg_group = detail::arg_group_impl<Group>{};

template <ConstexprString... Names>
inline constexpr auto arg_conflicts = detail::arg_confflicts_impl<Names...>{};

template <ConstexprString... Names>
inline constexpr auto arg_requires = detail::arg_requires_impl<Names...>{};

template <ConstexprString Message = "">
inline constexpr auto arg_deprecated = detail::arg_deprecated_impl<Message>{};

inline constexpr auto arg_case_insensitive_choices = detail::arg_case_insensitive_choices_impl{};

inline constexpr auto arg_ignore_tag = detail::arg_ignore_tag_impl{}; // NOLINT

// Tag access -----------------------------------------------------------------

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, long_name, long_name)                        // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(char, short_name, short_name)                                  // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, choices, choices)               // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, help, help)                                  // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, value_name, value_name)                      // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, env_name, env_name)                          // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(char, separator, separator)                                    // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, aliases, aliases)               // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, group, group)                                // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, conflicts, conflicts)           // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, requires_names, requires_names) // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, deprecated_message, deprecated_message)      // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, case_insensitive_choices, case_insensitive_choices)      // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, required, required)                                      // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, positional, positional)                                  // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, flag, flag)                                              // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, repeatable, repeatable)                                  // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, hidden, hidden)                                          // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, command, command)                                        // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(double, range_min, range_min)                                  // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(double, range_max, range_max)                                  // NOLINT

NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(default_value, default_value)   // NOLINT
NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(implicit_value, implicit_value) // NOLINT

struct ignore { // NOLINT
    using type = bool;

    static constexpr type missing() noexcept { return false; }

    template <typename Tag>
    static constexpr bool has(const Tag& tag) {
        using RawTag = std::remove_cvref_t<Tag>;
        static_cast<void>(tag);
        return std::is_same_v<RawTag, detail::arg_ignore_tag_impl>;
    }

    template <typename Tag>
    static constexpr type get(const Tag& tag) {
        using RawTag = std::remove_cvref_t<Tag>;
        static_cast<void>(tag);
        if constexpr (std::is_same_v<RawTag, detail::arg_ignore_tag_impl>) {
            return true;
        } else {
            return missing();
        }
    }
};
} // namespace tag_property

} // namespace argparser
NEKO_END_NAMESPACE
