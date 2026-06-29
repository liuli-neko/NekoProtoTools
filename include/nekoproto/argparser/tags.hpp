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
struct ArgTags {
    bool required   = false;
    bool positional = false;
    bool flag       = false;
    bool repeatable = false;
    bool hidden     = false;
    bool command    = false;
    double rangeMin = 0.0;
    double rangeMax = 0.0;

    template <typename T, auto tags>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Long = "", ConstexprString Short = "", auto BaseTags = NoTags{}>
struct ArgNameTag {
    static constexpr auto longName  = Long.view();  // NOLINT
    static constexpr auto shortName = Short.view(); // NOLINT
    static constexpr auto base      = BaseTags;     // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto Default, auto BaseTags = NoTags{}>
struct ArgDefaultTag {
    constexpr static auto getValue()
        requires is_constexpr_string<decltype(Default)>::value
    {
        return Default.view();
    }
    constexpr static auto getValue() { return Default; }

    static constexpr auto defaultValue = getValue(); // NOLINT
    static constexpr auto base         = BaseTags;   // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}, ConstexprString... Choices>
struct ArgChoicesTag {
    static constexpr std::array choices = {Choices.view()...}; // NOLINT
    static constexpr auto base          = BaseTags;            // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Help = "", auto BaseTags = NoTags{}>
struct ArgHelpTag {
    static constexpr auto help = Help.view(); // NOLINT
    static constexpr auto base = BaseTags;    // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString ValueName = "", auto BaseTags = NoTags{}>
struct ArgValueNameTag {
    static constexpr auto valueName = ValueName.view(); // NOLINT
    static constexpr auto base      = BaseTags;         // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString EnvName = "", auto BaseTags = NoTags{}>
struct ArgEnvTag {
    static constexpr auto envName = EnvName.view(); // NOLINT
    static constexpr auto base    = BaseTags;       // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <char Separator, auto BaseTags = NoTags{}>
struct ArgSeparatorTag {
    static constexpr auto separator = Separator; // NOLINT
    static constexpr auto base      = BaseTags;  // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}, ConstexprString... Aliases>
struct ArgAliasesTag {
    static constexpr std::array aliases = {Aliases.view()...}; // NOLINT
    static constexpr auto base          = BaseTags;            // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto Implicit, auto BaseTags = NoTags{}>
struct ArgImplicitTag {
    constexpr static auto getValue()
        requires is_constexpr_string<decltype(Implicit)>::value
    {
        return Implicit.view();
    }
    constexpr static auto getValue() { return Implicit; }

    static constexpr auto implicitValue = getValue(); // NOLINT
    static constexpr auto base          = BaseTags;   // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Group = "", auto BaseTags = NoTags{}>
struct ArgGroupTag {
    static constexpr auto group = Group.view(); // NOLINT
    static constexpr auto base  = BaseTags;     // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}, ConstexprString... Names>
struct ArgConflictsTag {
    static constexpr std::array conflicts = {Names.view()...}; // NOLINT
    static constexpr auto base            = BaseTags;          // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}, ConstexprString... Names>
struct ArgRequiresTag {
    static constexpr std::array requiresNames = {Names.view()...}; // NOLINT
    static constexpr auto base                = BaseTags;          // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Message = "", auto BaseTags = NoTags{}>
struct ArgDeprecatedTag {
    static constexpr auto deprecatedMessage = Message.view(); // NOLINT
    static constexpr auto base              = BaseTags;       // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}>
struct ArgCaseInsensitiveChoicesTag {
    static constexpr bool caseInsensitiveChoices = true;     // NOLINT
    static constexpr auto base                   = BaseTags; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Long = "", ConstexprString Short = "", auto BaseTags = NoTags{}>
inline constexpr auto arg_name = ArgNameTag<Long, Short, BaseTags>{}; // NOLINT

template <auto BaseTags = NoTags{}, ConstexprString... Choices>
inline constexpr auto arg_choices = ArgChoicesTag<BaseTags, Choices...>{}; // NOLINT

template <auto Default, auto BaseTags = NoTags{}>
inline constexpr auto arg_default = ArgDefaultTag<Default, BaseTags>{}; // NOLINT

template <ConstexprString Help = "", auto BaseTags = NoTags{}>
inline constexpr auto arg_help = ArgHelpTag<Help, BaseTags>{}; // NOLINT

template <ConstexprString ValueName = "", auto BaseTags = NoTags{}>
inline constexpr auto arg_value_name = ArgValueNameTag<ValueName, BaseTags>{}; // NOLINT

template <ConstexprString EnvName = "", auto BaseTags = NoTags{}>
inline constexpr auto arg_env = ArgEnvTag<EnvName, BaseTags>{}; // NOLINT

template <char Separator, auto BaseTags = NoTags{}>
inline constexpr auto arg_separator = ArgSeparatorTag<Separator, BaseTags>{}; // NOLINT

template <auto BaseTags = NoTags{}, ConstexprString... Aliases>
inline constexpr auto arg_aliases = ArgAliasesTag<BaseTags, Aliases...>{}; // NOLINT

template <auto Implicit, auto BaseTags = NoTags{}>
inline constexpr auto arg_implicit = ArgImplicitTag<Implicit, BaseTags>{}; // NOLINT

template <ConstexprString Group = "", auto BaseTags = NoTags{}>
inline constexpr auto arg_group = ArgGroupTag<Group, BaseTags>{}; // NOLINT

template <auto BaseTags = NoTags{}, ConstexprString... Names>
inline constexpr auto arg_conflicts = ArgConflictsTag<BaseTags, Names...>{}; // NOLINT

template <auto BaseTags = NoTags{}, ConstexprString... Names>
inline constexpr auto arg_requires = ArgRequiresTag<BaseTags, Names...>{}; // NOLINT

template <ConstexprString Message = "", auto BaseTags = NoTags{}>
inline constexpr auto arg_deprecated = ArgDeprecatedTag<Message, BaseTags>{}; // NOLINT

template <auto BaseTags = NoTags{}>
inline constexpr auto arg_case_insensitive_choices = ArgCaseInsensitiveChoicesTag<BaseTags>{}; // NOLINT

struct ArgCommand {
    bool value;
};

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
inline constexpr bool is_nested_option_v = // NOLINT
    std::is_class_v<std::remove_cvref_t<T>> && !is_string_like_v<T> && !is_arg_optional_v<T> && !is_vector_v<T> &&
    NEKO_NAMESPACE::detail::has_values_meta<std::remove_cvref_t<T>>;
} // namespace detail

// Tag access -----------------------------------------------------------------

namespace tag_query {
NEKO_DEFINE_TAG_QUERY(std::string_view, longName, long_name)
NEKO_DEFINE_TAG_QUERY(std::string_view, shortName, short_name)
NEKO_DEFINE_TAG_QUERY(std::vector<std::string_view>, choices, choices)
NEKO_DEFINE_TAG_QUERY(std::string_view, help, help)
NEKO_DEFINE_TAG_QUERY(std::string_view, valueName, value_name)
NEKO_DEFINE_TAG_QUERY(std::string_view, envName, env_name)
NEKO_DEFINE_TAG_QUERY(char, separator, separator)
NEKO_DEFINE_TAG_QUERY(std::vector<std::string_view>, aliases, aliases)
NEKO_DEFINE_TAG_QUERY(std::string_view, group, group)
NEKO_DEFINE_TAG_QUERY(std::vector<std::string_view>, conflicts, conflicts)
NEKO_DEFINE_TAG_QUERY(std::vector<std::string_view>, requiresNames, requires_names)
NEKO_DEFINE_TAG_QUERY(std::string_view, deprecatedMessage, deprecated_message)
NEKO_DEFINE_TAG_QUERY(bool, caseInsensitiveChoices, case_insensitive_choices)

NEKO_DEFINE_TAG_VALUE_QUERY(defaultValue, default_value,
                            "arg_default value was requested from tags without a default")
NEKO_DEFINE_TAG_VALUE_QUERY(implicitValue, implicit_value,
                            "arg_implicit value was requested from tags without an implicit value")

NEKO_DEFINE_TAG_QUERY(bool, required, required)
NEKO_DEFINE_TAG_QUERY(bool, positional, positional)
NEKO_DEFINE_TAG_QUERY(bool, flag, flag)
NEKO_DEFINE_TAG_QUERY(bool, repeatable, repeatable)
NEKO_DEFINE_TAG_QUERY(bool, hidden, hidden)
NEKO_DEFINE_TAG_QUERY(bool, command, command)
NEKO_DEFINE_TAG_QUERY(double, rangeMin, range_min)
NEKO_DEFINE_TAG_QUERY(double, rangeMax, range_max)

template <typename Tags>
constexpr bool has_range(const Tags& tags) {
    return range_min(tags) != range_max(tags);
}
} // namespace tag_query
} // namespace argparser
NEKO_END_NAMESPACE
