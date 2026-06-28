#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/string_literal.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <concepts>

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

    template <auto NewBase>
    using rebind_base = ArgNameTag<Long, Short, NewBase>;

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

    template <auto NewBase>
    using rebind_base = ArgDefaultTag<Default, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}, ConstexprString... Choices>
struct ArgChoicesTag {
    static constexpr std::array choices = {Choices.view()...}; // NOLINT
    static constexpr auto base          = BaseTags;            // NOLINT

    template <auto NewBase>
    using rebind_base = ArgChoicesTag<NewBase, Choices...>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Help = "", auto BaseTags = NoTags{}>
struct ArgHelpTag {
    static constexpr auto help = Help.view(); // NOLINT
    static constexpr auto base = BaseTags;    // NOLINT

    template <auto NewBase>
    using rebind_base = ArgHelpTag<Help, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString ValueName = "", auto BaseTags = NoTags{}>
struct ArgValueNameTag {
    static constexpr auto valueName = ValueName.view(); // NOLINT
    static constexpr auto base      = BaseTags;         // NOLINT

    template <auto NewBase>
    using rebind_base = ArgValueNameTag<ValueName, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString EnvName = "", auto BaseTags = NoTags{}>
struct ArgEnvTag {
    static constexpr auto envName = EnvName.view(); // NOLINT
    static constexpr auto base    = BaseTags;       // NOLINT

    template <auto NewBase>
    using rebind_base = ArgEnvTag<EnvName, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <char Separator, auto BaseTags = NoTags{}>
struct ArgSeparatorTag {
    static constexpr auto separator = Separator; // NOLINT
    static constexpr auto base      = BaseTags;  // NOLINT

    template <auto NewBase>
    using rebind_base = ArgSeparatorTag<Separator, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <auto BaseTags = NoTags{}, ConstexprString... Aliases>
struct ArgAliasesTag {
    static constexpr std::array aliases = {Aliases.view()...}; // NOLINT
    static constexpr auto base          = BaseTags;            // NOLINT

    template <auto NewBase>
    using rebind_base = ArgAliasesTag<NewBase, Aliases...>;

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

    template <auto NewBase>
    using rebind_base = ArgImplicitTag<Implicit, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Group = "", auto BaseTags = NoTags{}>
struct ArgGroupTag {
    static constexpr auto group = Group.view(); // NOLINT
    static constexpr auto base  = BaseTags;     // NOLINT

    template <auto NewBase>
    using rebind_base = ArgGroupTag<Group, NewBase>;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return NEKO_NAMESPACE::detail::perform_check<T, BaseTags>();
    }
};

template <ConstexprString Long = "", ConstexprString Short = "", auto BaseTags = ArgTags{}>
inline constexpr auto arg_name = ArgNameTag<Long, Short, BaseTags>{}; // NOLINT

template <auto BaseTags = ArgTags{}, ConstexprString... Choices>
inline constexpr auto arg_choices = ArgChoicesTag<BaseTags, Choices...>{}; // NOLINT

template <auto Default, auto BaseTags = ArgTags{}>
inline constexpr auto arg_default = ArgDefaultTag<Default, BaseTags>{}; // NOLINT

template <ConstexprString Help = "", auto BaseTags = ArgTags{}>
inline constexpr auto arg_help = ArgHelpTag<Help, BaseTags>{}; // NOLINT

template <ConstexprString ValueName = "", auto BaseTags = ArgTags{}>
inline constexpr auto arg_value_name = ArgValueNameTag<ValueName, BaseTags>{}; // NOLINT

template <ConstexprString EnvName = "", auto BaseTags = ArgTags{}>
inline constexpr auto arg_env = ArgEnvTag<EnvName, BaseTags>{}; // NOLINT

template <char Separator, auto BaseTags = ArgTags{}>
inline constexpr auto arg_separator = ArgSeparatorTag<Separator, BaseTags>{}; // NOLINT

template <auto BaseTags = ArgTags{}, ConstexprString... Aliases>
inline constexpr auto arg_aliases = ArgAliasesTag<BaseTags, Aliases...>{}; // NOLINT

template <auto Implicit, auto BaseTags = ArgTags{}>
inline constexpr auto arg_implicit = ArgImplicitTag<Implicit, BaseTags>{}; // NOLINT

template <ConstexprString Group = "", auto BaseTags = ArgTags{}>
inline constexpr auto arg_group = ArgGroupTag<Group, BaseTags>{}; // NOLINT

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

namespace tag_access {
NEKO_DEFINE_NESTED_TAG(std::string_view, longName, long_name)
NEKO_DEFINE_NESTED_TAG(std::string_view, shortName, short_name)
NEKO_DEFINE_NESTED_TAG(std::vector<std::string_view>, choices, choices)
NEKO_DEFINE_NESTED_TAG(std::string_view, help, help)
NEKO_DEFINE_NESTED_TAG(std::string_view, valueName, value_name)
NEKO_DEFINE_NESTED_TAG(std::string_view, envName, env_name)
NEKO_DEFINE_NESTED_TAG(char, separator, separator)
NEKO_DEFINE_NESTED_TAG(std::vector<std::string_view>, aliases, aliases)
NEKO_DEFINE_NESTED_TAG(std::string_view, group, group)

template <typename Tags>
constexpr bool has_default_value(const Tags& tags) {
    using Tag = std::remove_cvref_t<Tags>;
    if constexpr (requires { Tag::defaultValue; }) {
        static_cast<void>(tags);
        return true;
    } else if constexpr (requires { tags.base; }) {
        return has_default_value(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr decltype(auto) recursive_default_value(const Tags& tags) {
    using Tag = std::remove_cvref_t<Tags>;
    if constexpr (requires { Tag::defaultValue; }) {
        static_cast<void>(tags);
        return NEKO_NAMESPACE::detail::make_tag_value_common(Tag::defaultValue);
    } else if constexpr (requires { tags.base; }) {
        return recursive_default_value(tags.base);
    } else {
        static_assert(!std::is_same_v<Tag, Tag>, "arg_default value was requested from tags without a default");
    }
}

template <typename Tags>
constexpr bool has_implicit_value(const Tags& tags) {
    using Tag = std::remove_cvref_t<Tags>;
    if constexpr (requires { Tag::implicitValue; }) {
        static_cast<void>(tags);
        return true;
    } else if constexpr (requires { tags.base; }) {
        return has_implicit_value(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr decltype(auto) recursive_implicit_value(const Tags& tags) {
    using Tag = std::remove_cvref_t<Tags>;
    if constexpr (requires { Tag::implicitValue; }) {
        static_cast<void>(tags);
        return NEKO_NAMESPACE::detail::make_tag_value_common(Tag::implicitValue);
    } else if constexpr (requires { tags.base; }) {
        return recursive_implicit_value(tags.base);
    } else {
        static_assert(!std::is_same_v<Tag, Tag>, "arg_implicit value was requested from tags without an implicit value");
    }
}

template <typename Tags>
constexpr bool is_required(const Tags& tags) {
    if constexpr (requires { tags.required; }) {
        return tags.required;
    } else if constexpr (requires { tags.base; }) {
        return is_required(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool is_positional(const Tags& tags) {
    if constexpr (requires { tags.positional; }) {
        return tags.positional;
    } else if constexpr (requires { tags.base; }) {
        return is_positional(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool is_flag(const Tags& tags) {
    if constexpr (requires { tags.flag; }) {
        return tags.flag;
    } else if constexpr (requires { tags.base; }) {
        return is_flag(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool is_repeatable(const Tags& tags) {
    if constexpr (requires { tags.repeatable; }) {
        return tags.repeatable;
    } else if constexpr (requires { tags.base; }) {
        return is_repeatable(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool is_hidden(const Tags& tags) {
    if constexpr (requires { tags.hidden; }) {
        return tags.hidden;
    } else if constexpr (requires { tags.base; }) {
        return is_hidden(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr bool is_command(const Tags& tags) {
    if constexpr (requires { tags.command; }) {
        return tags.command;
    } else if constexpr (requires { tags.base; }) {
        return is_command(tags.base);
    } else {
        return false;
    }
}

template <typename Tags>
constexpr double range_min(const Tags& tags) {
    if constexpr (requires { tags.rangeMin; }) {
        return tags.rangeMin;
    } else if constexpr (requires { tags.base; }) {
        return range_min(tags.base);
    } else {
        return 0.0;
    }
}

template <typename Tags>
constexpr double range_max(const Tags& tags) {
    if constexpr (requires { tags.rangeMax; }) {
        return tags.rangeMax;
    } else if constexpr (requires { tags.base; }) {
        return range_max(tags.base);
    } else {
        return 0.0;
    }
}

template <typename Tags>
constexpr bool has_range(const Tags& tags) {
    return range_min(tags) != range_max(tags);
}
} // namespace tag_access
} // namespace argparser
NEKO_END_NAMESPACE
