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

struct ArgTags {
    TagValue<bool> required{};   // NOLINT
    TagValue<bool> positional{}; // NOLINT
    TagValue<bool> flag{};       // NOLINT
    TagValue<bool> repeatable{}; // NOLINT
    TagValue<bool> hidden{};     // NOLINT
    TagValue<bool> command{};    // NOLINT
    TagValue<double> rangeMin{}; // NOLINT
    TagValue<double> rangeMax{}; // NOLINT

    template <typename T, auto tags>
    constexpr static bool constexpr_check() { // NOLINT
        using RawT = std::remove_cvref_t<T>;

        constexpr bool IsCommand   = tags.command.declared && static_cast<bool>(tags.command);
        constexpr bool IsFlag      = tags.flag.declared && static_cast<bool>(tags.flag);
        constexpr bool IsPosition  = tags.positional.declared && static_cast<bool>(tags.positional);
        constexpr bool HasRange    = tags.rangeMin.declared || tags.rangeMax.declared;
        constexpr bool HasRequired = tags.required.declared && static_cast<bool>(tags.required);
        constexpr bool HasRepeat   = tags.repeatable.declared && static_cast<bool>(tags.repeatable);

        if constexpr (HasRange) {
            static_assert(detail::is_range_supported_v<RawT>,
                          "argparser range tags require an arithmetic field, optional arithmetic field, or vector of "
                          "arithmetic values");
            static_assert(static_cast<double>(tags.rangeMin) <= static_cast<double>(tags.rangeMax),
                          "argparser rangeMin must be less than or equal to rangeMax");
        }
        if constexpr (IsFlag) {
            static_assert(detail::is_bool_supported_v<RawT>,
                          "argparser flag tags require a bool field, optional bool field, or vector<bool> field");
        }
        if constexpr (IsFlag && IsPosition) {
            static_assert(!IsFlag, "argparser positional fields cannot also be flags");
        }
        if constexpr (IsCommand) {
            static_assert(std::is_same_v<RawT, ArgCommand> || NEKO_NAMESPACE::detail::has_values_meta<RawT>,
                          "argparser command tags require a reflected command struct or ArgCommand placeholder");
            static_assert(!IsFlag && !IsPosition && !HasRange && !HasRequired && !HasRepeat,
                          "argparser command tags cannot also be flag, positional, range, required, or repeatable");
        }
        return true;
    }
};

template <ConstexprString Long = "", ConstexprString Short = "">
struct ArgNameTag {
    static constexpr auto longName  = Long.view();  // NOLINT
    static constexpr auto shortName = Short.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <auto Default>
struct ArgDefaultTag {
    constexpr static auto getValue()
        requires is_constexpr_string<decltype(Default)>::value
    {
        return Default.view();
    }
    constexpr static auto getValue() { return Default; }

    static constexpr auto defaultValue = getValue(); // NOLINT

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
    static constexpr auto valueName = ValueName.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString EnvName = "">
struct ArgEnvTag {
    static constexpr auto envName = EnvName.view(); // NOLINT

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
    constexpr static auto getValue()
        requires is_constexpr_string<decltype(Implicit)>::value
    {
        return Implicit.view();
    }
    constexpr static auto getValue() { return Implicit; }

    static constexpr auto implicitValue = getValue(); // NOLINT

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
    static constexpr std::array requiresNames = {Names.view()...}; // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

template <ConstexprString Message = "">
struct ArgDeprecatedTag {
    static constexpr auto deprecatedMessage = Message.view(); // NOLINT

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() { // NOLINT
        return true;
    }
};

struct ArgCaseInsensitiveChoicesTag {
    static constexpr bool caseInsensitiveChoices = true; // NOLINT

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

namespace tag_prop {
NEKO_DEFINE_TAG_PROP(std::string_view, longName, long_name)                        // NOLINT
NEKO_DEFINE_TAG_PROP(std::string_view, shortName, short_name)                      // NOLINT
NEKO_DEFINE_TAG_PROP(std::vector<std::string_view>, choices, choices)              // NOLINT
NEKO_DEFINE_TAG_PROP(std::string_view, help, help)                                 // NOLINT
NEKO_DEFINE_TAG_PROP(std::string_view, valueName, value_name)                      // NOLINT
NEKO_DEFINE_TAG_PROP(std::string_view, envName, env_name)                          // NOLINT
NEKO_DEFINE_TAG_PROP(char, separator, separator)                                   // NOLINT
NEKO_DEFINE_TAG_PROP(std::vector<std::string_view>, aliases, aliases)              // NOLINT
NEKO_DEFINE_TAG_PROP(std::string_view, group, group)                               // NOLINT
NEKO_DEFINE_TAG_PROP(std::vector<std::string_view>, conflicts, conflicts)          // NOLINT
NEKO_DEFINE_TAG_PROP(std::vector<std::string_view>, requiresNames, requires_names) // NOLINT
NEKO_DEFINE_TAG_PROP(std::string_view, deprecatedMessage, deprecated_message)      // NOLINT
NEKO_DEFINE_TAG_PROP(bool, caseInsensitiveChoices, case_insensitive_choices)       // NOLINT
NEKO_DEFINE_TAG_PROP(bool, required, required)                                     // NOLINT
NEKO_DEFINE_TAG_PROP(bool, positional, positional)                                 // NOLINT
NEKO_DEFINE_TAG_PROP(bool, flag, flag)                                             // NOLINT
NEKO_DEFINE_TAG_PROP(bool, repeatable, repeatable)                                 // NOLINT
NEKO_DEFINE_TAG_PROP(bool, hidden, hidden)                                         // NOLINT
NEKO_DEFINE_TAG_PROP(bool, command, command)                                       // NOLINT
NEKO_DEFINE_TAG_PROP(double, rangeMin, range_min)                                  // NOLINT
NEKO_DEFINE_TAG_PROP(double, rangeMax, range_max)                                  // NOLINT

NEKO_DEFINE_TAG_VALUE_PROP(defaultValue, default_value)   // NOLINT
NEKO_DEFINE_TAG_VALUE_PROP(implicitValue, implicit_value) // NOLINT
} // namespace tag_prop

} // namespace argparser
NEKO_END_NAMESPACE
