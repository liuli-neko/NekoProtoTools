#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/reflection_tags.hpp"
#include "nekoproto/global/string_literal.hpp"
#include "nekoproto/global/traits.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace nekoproto {
namespace argparser {

template <auto Value>
struct ArgCommand {
    constexpr static auto value = tag_detail::tagValueDeclared(Value);
};

namespace detail {

// Type traits ----------------------------------------------------------------

template <typename T>
struct IsCommandType : std::false_type {};

template <auto Value>
struct IsCommandType<ArgCommand<Value>> : std::true_type {};

template <typename T>
struct IsArgOptional : std::false_type {};

template <typename T>
struct IsArgOptional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_arg_optional_v = IsArgOptional<std::remove_cvref_t<T>>::value; // NOLINT

template <typename T>
struct OptionalValue;

template <typename T>
struct OptionalValue<std::optional<T>> {
    using type = T;
};

template <typename T>
using optional_value_t = typename OptionalValue<std::remove_cvref_t<T>>::type;

template <typename T>
struct IsVector : std::false_type {};

template <typename T, typename Alloc>
struct IsVector<std::vector<T, Alloc>> : std::true_type {
    using value_type = T;
};

template <typename T>
inline constexpr bool is_vector_v = IsVector<std::remove_cvref_t<T>>::value; // NOLINT

template <typename T>
using vector_value_t = typename IsVector<std::remove_cvref_t<T>>::value_type;

template <typename T>
inline constexpr bool is_argparser_borrowed_text_v = []() consteval { // NOLINT
    using raw_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<raw_t, std::string_view>) {
        return true;
    } else if constexpr (is_arg_optional_v<raw_t>) {
        return is_argparser_borrowed_text_v<optional_value_t<raw_t>>;
    } else if constexpr (is_vector_v<raw_t>) {
        return is_argparser_borrowed_text_v<vector_value_t<raw_t>>;
    } else {
        return false;
    }
}();

template <typename T>
inline constexpr bool is_path_completion_supported_v = []() consteval { // NOLINT
    using raw_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<raw_t, std::string>) {
        return true;
    } else if constexpr (is_arg_optional_v<raw_t>) {
        return is_path_completion_supported_v<optional_value_t<raw_t>>;
    } else if constexpr (is_vector_v<raw_t>) {
        return is_path_completion_supported_v<vector_value_t<raw_t>>;
    } else {
        return false;
    }
}();

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
    std::is_class_v<std::remove_cvref_t<T>> && !traits::is_string_like_v<T> && !is_arg_optional_v<T> &&
    !is_vector_v<T> && nekoproto::detail::has_values_meta<std::remove_cvref_t<T>>;
} // namespace detail

struct ArgTags {
    tag_detail::TagValue<bool> required{};    // NOLINT
    tag_detail::TagValue<bool> positional{};  // NOLINT
    tag_detail::TagValue<bool> flag{};        // NOLINT
    tag_detail::TagValue<bool> repeatable{};  // NOLINT
    tag_detail::TagValue<bool> hidden{};      // NOLINT
    tag_detail::TagValue<bool> command{};     // NOLINT
    tag_detail::TagValue<double> range_min{}; // NOLINT
    tag_detail::TagValue<double> range_max{}; // NOLINT

    template <typename T, auto Tags>
    static constexpr auto constexprCheck() -> bool {
        using raw_t = std::remove_cvref_t<T>;

        constexpr bool is_command =
            (Tags.command.declared && static_cast<bool>(Tags.command)) || detail::IsCommandType<raw_t>::value;
        constexpr bool is_flag      = Tags.flag.declared && static_cast<bool>(Tags.flag);
        constexpr bool is_position  = Tags.positional.declared && static_cast<bool>(Tags.positional);
        constexpr bool has_range    = Tags.range_min.declared || Tags.range_max.declared;
        constexpr bool has_required = Tags.required.declared && static_cast<bool>(Tags.required);
        constexpr bool has_repeat   = Tags.repeatable.declared && static_cast<bool>(Tags.repeatable);

        if constexpr (has_range) {
            static_assert(detail::is_range_supported_v<raw_t>,
                          "argparser range tags require an arithmetic field, optional arithmetic field, or vector of "
                          "arithmetic values");
            static_assert(Tags.range_min.declared && Tags.range_max.declared,
                          "argparser ranges require both range_min and range_max; the upper bound is exclusive");
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
        if constexpr (has_repeat) {
            static_assert(detail::is_vector_v<raw_t>,
                          "argparser repeatable tags require a std::vector field; vectors are repeatable by default");
        }
        if constexpr (is_command) {
            static_assert(detail::IsCommandType<raw_t>::value || nekoproto::detail::has_values_meta<raw_t>,
                          "argparser command tags require a reflected command struct or ArgCommand placeholder");
            static_assert(!is_flag && !is_position && !has_range && !has_required && !has_repeat,
                          "argparser command tags cannot also be flag, positional, range, required, or repeatable");
        }
        return true;
    }
};
namespace detail {
inline constexpr auto isOptionNameCharacter(char ch) -> bool {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
}

inline constexpr auto isShortOptionCharacter(char ch) -> bool {
    return ch >= '!' && ch <= '~' && ch != '-' && ch != '=';
}

inline constexpr auto isAbsoluteNameCharacter(char ch) -> bool {
    return ch >= '!' && ch <= '~' && ch != '=' && ch != '/';
}

template <ConstexprString Long = "">
struct ArgLongNameImpl {
    static constexpr auto long_name = Long.view();
    template <typename T, auto Tags>
    constexpr static auto constexprCheck() -> bool {
        static_assert(Tags.long_name.size() >= 1 && Tags.long_name.size() <= 64 &&
                          std::all_of(Tags.long_name.begin(), Tags.long_name.end(), isOptionNameCharacter),
                      "argparser long names must contain 1-64 ASCII letters, digits, '-' or '_' characters");
        return true;
    }
};

template <char Short = '\0'>
struct ArgShortNameImpl {
    static constexpr auto short_name = Short;
    template <typename T, auto Tags>
    constexpr static auto constexprCheck() -> bool {
        static_assert(isShortOptionCharacter(Tags.short_name),
                      "argparser short names must be one visible ASCII character other than '-' or '='");
        return true;
    }
};

template <ConstexprString Long = "", char Short = '\0'>
struct ArgNameImpl {
    static constexpr auto long_name  = Long.view();
    static constexpr auto short_name = Short;

    template <typename T, auto Tags>
    constexpr static auto constexprCheck() -> bool {
        ArgLongNameImpl<Long>::template constexprCheck<T, Tags>();
        ArgShortNameImpl<Short>::template constexprCheck<T, Tags>();
        return true;
    }
};

template <ConstexprString Absolute = "">
struct ArgAbsoluteNameImpl {
    static constexpr auto absolute_long_name = Absolute.view();

    template <typename T, auto Tags>
    constexpr static auto constexprCheck() -> bool {
        static_assert(
            Tags.absolute_long_name.size() >= 1 && Tags.absolute_long_name.size() <= 64 &&
                std::all_of(Tags.absolute_long_name.begin(), Tags.absolute_long_name.end(), isAbsoluteNameCharacter),
            "argparser absolute names must contain 1-64 visible ASCII characters other than '=' or '/'");
        return true;
    }
};

template <auto Default>
struct ArgDefaultImpl {
    constexpr static auto getValue()
        requires IsConstexprString<decltype(Default)>::value
    {
        return Default.view();
    }
    constexpr static auto getValue() { return Default; }

    static constexpr auto default_value = getValue();

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString... Choices>
struct ArgChoicesImpl {
    static constexpr std::array choices = {Choices.view()...};

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString Help = "">
struct ArgHelpImpl {
    static constexpr auto help = Help.view();

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString ValueName = "">
struct ArgValueNameImpl {
    static constexpr auto value_name = ValueName.view();

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString EnvName = "">
struct ArgEnvImpl {
    static constexpr auto env_name = EnvName.view();

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <char Separator>
struct ArgSeparatorImpl {
    static constexpr auto separator = Separator;

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString... Aliases>
struct ArgAliasesImpl {
    static constexpr std::array aliases = {Aliases.view()...};

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <auto Implicit>
struct ArgImplicitImpl {
    constexpr static auto getValue()
        requires IsConstexprString<decltype(Implicit)>::value
    {
        return Implicit.view();
    }
    constexpr static auto getValue() { return Implicit; }

    static constexpr auto implicit_value = getValue();

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString Group = "">
struct ArgGroupImpl {
    static constexpr auto group = Group.view();

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString... Names>
struct ArgConfflictsImpl {
    static constexpr std::array conflicts = {Names.view()...};

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString... Names>
struct ArgRequiresImpl {
    static constexpr std::array requires_names = {Names.view()...};

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

template <ConstexprString Message = "">
struct ArgDeprecatedImpl {
    static constexpr auto deprecated_message = Message.view();

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

struct ArgCaseInsensitiveChoicesImpl {
    static constexpr bool case_insensitive_choices = true;

    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};

struct ArgCompleteDirectoryImpl;

struct ArgCompleteFileImpl {
    template <typename T, auto Tags>
    constexpr static auto constexprCheck() -> bool {
        constexpr auto arg_tags = tag_query::getTag<ArgTags>(Tags);
        static_assert(!tag_query::hasTag<ArgCompleteDirectoryImpl>(Tags),
                      "argparser file and directory completion tags are mutually exclusive");
        static_assert(!IsCommandType<std::remove_cvref_t<T>>::value &&
                          !(arg_tags.command.declared && static_cast<bool>(arg_tags.command)),
                      "argparser value completion tags cannot be used with commands");
        static_assert(!std::is_same_v<std::remove_cvref_t<T>, bool> &&
                          !(arg_tags.flag.declared && static_cast<bool>(arg_tags.flag)),
                      "argparser value completion tags cannot be used with flags");
        static_assert(is_path_completion_supported_v<T>,
                      "argparser file and directory completion tags require string storage");
        return true;
    }
};

struct ArgCompleteDirectoryImpl {
    template <typename T, auto Tags>
    constexpr static auto constexprCheck() -> bool {
        constexpr auto arg_tags = tag_query::getTag<ArgTags>(Tags);
        static_assert(!tag_query::hasTag<ArgCompleteFileImpl>(Tags),
                      "argparser file and directory completion tags are mutually exclusive");
        static_assert(!IsCommandType<std::remove_cvref_t<T>>::value &&
                          !(arg_tags.command.declared && static_cast<bool>(arg_tags.command)),
                      "argparser value completion tags cannot be used with commands");
        static_assert(!std::is_same_v<std::remove_cvref_t<T>, bool> &&
                          !(arg_tags.flag.declared && static_cast<bool>(arg_tags.flag)),
                      "argparser value completion tags cannot be used with flags");
        static_assert(is_path_completion_supported_v<T>,
                      "argparser file and directory completion tags require string storage");
        return true;
    }
};

struct ArgIgnoreTagImpl {
    template <typename T, auto /*tags*/>
    constexpr static auto constexprCheck() -> bool {
        return true;
    }
};
} // namespace detail

template <ConstexprString Long = "">
inline constexpr auto arg_long_name = detail::ArgLongNameImpl<Long>{};

template <char Short = '\0'>
inline constexpr auto arg_short_name = detail::ArgShortNameImpl<Short>{};

template <ConstexprString Long = "", char Short = '\0'>
inline constexpr auto arg_name = detail::ArgNameImpl<Long, Short>{};

/** Use a complete option path instead of the automatic enclosing-field prefix. */
template <ConstexprString Absolute = "">
inline constexpr auto arg_absolute_name = detail::ArgAbsoluteNameImpl<Absolute>{};

template <ConstexprString... Choices>
inline constexpr auto arg_choices = detail::ArgChoicesImpl<Choices...>{};

template <auto Default>
inline constexpr auto arg_default = detail::ArgDefaultImpl<Default>{};

template <ConstexprString Help = "">
inline constexpr auto arg_help = detail::ArgHelpImpl<Help>{};

template <ConstexprString ValueName = "">
inline constexpr auto arg_value_name = detail::ArgValueNameImpl<ValueName>{};

template <ConstexprString EnvName = "">
inline constexpr auto arg_env = detail::ArgEnvImpl<EnvName>{};

template <char Separator>
inline constexpr auto arg_separator = detail::ArgSeparatorImpl<Separator>{};

template <ConstexprString... Aliases>
inline constexpr auto arg_aliases = detail::ArgAliasesImpl<Aliases...>{};

template <auto Implicit>
inline constexpr auto arg_implicit = detail::ArgImplicitImpl<Implicit>{};

template <ConstexprString Group = "">
inline constexpr auto arg_group = detail::ArgGroupImpl<Group>{};

template <ConstexprString... Names>
inline constexpr auto arg_conflicts = detail::ArgConfflictsImpl<Names...>{};

template <ConstexprString... Names>
inline constexpr auto arg_requires = detail::ArgRequiresImpl<Names...>{};

template <ConstexprString Message = "">
inline constexpr auto arg_deprecated = detail::ArgDeprecatedImpl<Message>{};

inline constexpr auto arg_case_insensitive_choices = detail::ArgCaseInsensitiveChoicesImpl{};

inline constexpr auto arg_complete_file = detail::ArgCompleteFileImpl{};

inline constexpr auto arg_complete_directory = detail::ArgCompleteDirectoryImpl{};

inline constexpr auto arg_ignore_tag = detail::ArgIgnoreTagImpl{}; // NOLINT

// Tag access -----------------------------------------------------------------

namespace tag_property {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, long_name, LongName)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, absolute_long_name, AbsoluteLongName)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(char, short_name, ShortName)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, choices, Choices)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, help, Help)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, value_name, ValueName)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, env_name, EnvName)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(char, separator, Separator)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, aliases, Aliases)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, group, Group)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, conflicts, Conflicts)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::vector<std::string_view>, requires_names, RequiresNames)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, deprecated_message, DeprecatedMessage)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, case_insensitive_choices, CaseInsensitiveChoices)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, required, Required)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, positional, Positional)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, flag, Flag)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, repeatable, Repeatable)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, hidden, Hidden)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, command, Command)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(double, range_min, RangeMin)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(double, range_max, RangeMax)

NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(default_value, default_value)   // NOLINT
NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(implicit_value, implicit_value) // NOLINT

struct Ignore {
    using type = bool;

    static constexpr auto missing() noexcept -> type { return false; }

    template <typename Tag>
    static constexpr auto has(const Tag& tag) -> bool {
        using RawTag = std::remove_cvref_t<Tag>;
        static_cast<void>(tag);
        return std::is_same_v<RawTag, detail::ArgIgnoreTagImpl>;
    }

    template <typename Tag>
    static constexpr auto get(const Tag& tag) -> type {
        using RawTag = std::remove_cvref_t<Tag>;
        static_cast<void>(tag);
        if constexpr (std::is_same_v<RawTag, detail::ArgIgnoreTagImpl>) {
            return true;
        } else {
            return missing();
        }
    }
};
} // namespace tag_property

} // namespace argparser
} // namespace nekoproto
