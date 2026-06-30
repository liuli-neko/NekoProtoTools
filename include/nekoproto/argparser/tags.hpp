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
namespace detail {

// Type traits ----------------------------------------------------------------

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
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_bool_value_v<RawT>) {
        return true;
    } else if constexpr (is_arg_optional_v<RawT>) {
        return is_bool_value_v<optional_value_t<RawT>>;
    } else if constexpr (is_vector_v<RawT>) {
        return is_bool_value_v<vector_value_t<RawT>>;
    } else {
        return false;
    }
}();

template <typename T>
inline constexpr bool is_range_value_v = // NOLINT
    std::is_arithmetic_v<std::remove_cvref_t<T>> && !std::is_same_v<std::remove_cvref_t<T>, bool>;

template <typename T>
inline constexpr bool is_range_supported_v = []() consteval { // NOLINT
    using RawT = std::remove_cvref_t<T>;
    if constexpr (is_range_value_v<RawT>) {
        return true;
    } else if constexpr (is_arg_optional_v<RawT>) {
        return is_range_value_v<optional_value_t<RawT>>;
    } else if constexpr (is_vector_v<RawT>) {
        return is_range_value_v<vector_value_t<RawT>>;
    } else {
        return false;
    }
}();

template <typename T>
inline constexpr bool is_nested_option_v = // NOLINT
    std::is_class_v<std::remove_cvref_t<T>> && !is_string_like_v<T> && !is_arg_optional_v<T> && !is_vector_v<T> &&
    NEKO_NAMESPACE::detail::has_values_meta<std::remove_cvref_t<T>>;
} // namespace detail

struct ArgCommand {
    bool value;
};

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

        constexpr bool is_command   = Tags.command.declared && static_cast<bool>(Tags.command);
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
            static_assert(std::is_same_v<raw_t, ArgCommand> || NEKO_NAMESPACE::detail::has_values_meta<raw_t>,
                          "argparser command tags require a reflected command struct or ArgCommand placeholder");
            static_assert(!is_flag && !is_position && !has_range && !has_required && !has_repeat,
                          "argparser command tags cannot also be flag, positional, range, required, or repeatable");
        }
        return true;
    }
};

template <ConstexprString Long = "", ConstexprString Short = "">
struct ArgNameTag {
    static constexpr auto long_name  = Long.view();  // NOLINT
    static constexpr auto short_name = Short.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <auto Default>
struct ArgDefaultTag {
    constexpr static auto get_value()
        requires is_constexpr_string<decltype(Default)>::value
    {
        return Default.view();
    }
    constexpr static auto get_value() { return Default; }

    static constexpr auto default_value = get_value(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString... Choices>
struct ArgChoicesTag {
    static constexpr std::array choices = {Choices.view()...}; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Help = "">
struct ArgHelpTag {
    static constexpr auto help = Help.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString ValueName = "">
struct ArgValueNameTag {
    static constexpr auto value_name = ValueName.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString EnvName = "">
struct ArgEnvTag {
    static constexpr auto env_name = EnvName.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <char Separator>
struct ArgSeparatorTag {
    static constexpr auto separator = Separator; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString... Aliases>
struct ArgAliasesTag {
    static constexpr std::array aliases = {Aliases.view()...}; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <auto Implicit>
struct ArgImplicitTag {
    constexpr static auto get_value()
        requires is_constexpr_string<decltype(Implicit)>::value
    {
        return Implicit.view();
    }
    constexpr static auto get_value() { return Implicit; }

    static constexpr auto implicit_value = get_value(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Group = "">
struct ArgGroupTag {
    static constexpr auto group = Group.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString... Names>
struct ArgConflictsTag {
    static constexpr std::array conflicts = {Names.view()...}; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString... Names>
struct ArgRequiresTag {
    static constexpr std::array requires_names = {Names.view()...}; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Message = "">
struct ArgDeprecatedTag {
    static constexpr auto deprecated_message = Message.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

struct ArgCaseInsensitiveChoicesTag {
    static constexpr bool case_insensitive_choices = true; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Long = "", ConstexprString Short = "">
inline constexpr auto arg_name = ArgNameTag<Long, Short>{}; // NOLINT

template <ConstexprString... Choices>
inline constexpr auto arg_choices = ArgChoicesTag<Choices...>{}; // NOLINT

template <auto Default>
inline constexpr auto arg_default = ArgDefaultTag<Default>{}; // NOLINT

template <ConstexprString Help = "">
inline constexpr auto arg_help = ArgHelpTag<Help>{}; // NOLINT

template <ConstexprString ValueName = "">
inline constexpr auto arg_value_name = ArgValueNameTag<ValueName>{}; // NOLINT

template <ConstexprString EnvName = "">
inline constexpr auto arg_env = ArgEnvTag<EnvName>{}; // NOLINT

template <char Separator>
inline constexpr auto arg_separator = ArgSeparatorTag<Separator>{}; // NOLINT

template <ConstexprString... Aliases>
inline constexpr auto arg_aliases = ArgAliasesTag<Aliases...>{}; // NOLINT

template <auto Implicit>
inline constexpr auto arg_implicit = ArgImplicitTag<Implicit>{}; // NOLINT

template <ConstexprString Group = "">
inline constexpr auto arg_group = ArgGroupTag<Group>{}; // NOLINT

template <ConstexprString... Names>
inline constexpr auto arg_conflicts = ArgConflictsTag<Names...>{}; // NOLINT

template <ConstexprString... Names>
inline constexpr auto arg_requires = ArgRequiresTag<Names...>{}; // NOLINT

template <ConstexprString Message = "">
inline constexpr auto arg_deprecated = ArgDeprecatedTag<Message>{}; // NOLINT

inline constexpr auto arg_case_insensitive_choices = ArgCaseInsensitiveChoicesTag{}; // NOLINT

// Tag access -----------------------------------------------------------------

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, long_name, long_name)                        // NOLINT
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, short_name, short_name)                      // NOLINT
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
} // namespace tag_property

} // namespace argparser
NEKO_END_NAMESPACE
