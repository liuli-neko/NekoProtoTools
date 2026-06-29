#pragma once

#include "nekoproto/argparser/config.hpp"
#include "nekoproto/argparser/error.hpp"
#include "nekoproto/argparser/tags.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/reflection.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser::detail {

struct ArgSpec {
    std::string longName;
    std::string shortName;
    std::vector<std::string_view> aliases;
    std::string help;
    std::string valueName;
    std::string envName;
    std::string group;
    std::string deprecatedMessage;
    bool required               = false;
    bool positional             = false;
    bool flag                   = false;
    bool repeatable             = false;
    bool hidden                 = false;
    bool hasRange               = false;
    bool hasDefault             = false;
    bool hasImplicit            = false;
    bool caseInsensitiveChoices = false;
    bool deprecated             = false;
    double rangeMin             = 0.0;
    double rangeMax             = 0.0;
    char separator              = '\0';
    std::string defaultValue;
    std::string implicitValue;
    std::vector<std::string_view> choices;
    std::vector<std::string_view> conflicts;
    std::vector<std::string_view> requiresNames;
};

struct ArgSchema {
    std::vector<ArgSpec> specs;
    std::vector<std::size_t> positionalSpecs;

    [[nodiscard]] std::optional<std::size_t> find_long_index(std::string_view name) const {
        for (std::size_t index = 0; index < specs.size(); ++index) {
            const auto& spec = specs[index];
            if (!spec.positional && spec.longName == name) {
                return index;
            }
            for (const auto alias : spec.aliases) {
                if (alias.size() > 1 && alias == name) {
                    return index;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> find_short_index(std::string_view name) const {
        for (std::size_t index = 0; index < specs.size(); ++index) {
            const auto& spec = specs[index];
            if (!spec.positional && !spec.shortName.empty() && spec.shortName == name) {
                return index;
            }
            for (const auto alias : spec.aliases) {
                if (alias.size() == 1 && alias == name) {
                    return index;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> find_reference_index(std::string_view name) const {
        const auto normalized = trim_option_prefix(name);
        if (normalized.empty()) {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < specs.size(); ++index) {
            if (spec_matches_reference(specs[index], normalized)) {
                return index;
            }
        }
        return std::nullopt;
    }

private:
    static std::string_view trim_option_prefix(std::string_view name) {
        if (name.starts_with("--")) {
            return name.substr(2);
        }
        if (name.starts_with("-")) {
            return name.substr(1);
        }
        return name;
    }

    static bool spec_matches_reference(const ArgSpec& spec, std::string_view normalized) {
        if (spec.longName == normalized) {
            return true;
        }
        if (!spec.shortName.empty() && spec.shortName == normalized) {
            return true;
        }
        return std::any_of(spec.aliases.begin(), spec.aliases.end(),
                           [&](const auto alias) { return alias == normalized; });
    }
};

inline std::string join_arg_name(std::string_view prefix, std::string_view name, char separator) {
    if (prefix.empty()) {
        return std::string(name);
    }
    std::string result(prefix);
    result.push_back(separator);
    result.append(name);
    return result;
}

template <typename T>
constexpr bool field_type_is_flag(bool taggedFlag) {
    return taggedFlag || std::is_same_v<std::remove_cvref_t<T>, bool>;
}

template <typename T>
std::string default_value_to_string(const T& value) {
    using RawT = std::remove_cvref_t<T>;
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string(std::string_view(value));
    } else if constexpr (std::is_same_v<RawT, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum_v<RawT>) {
        constexpr auto EnumNames  = Reflect<RawT>::names();
        constexpr auto EnumValues = Reflect<RawT>::values();
        for (std::size_t idx = 0; idx < EnumNames.size(); ++idx) {
            if (EnumValues[idx] == value) {
                return std::string(EnumNames[idx]);
            }
        }
        return std::to_string(static_cast<std::underlying_type_t<RawT>>(value));
    } else if constexpr (requires(std::ostringstream& stream, const T& item) { stream << item; }) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else {
        return {};
    }
}

template <typename FieldT, typename Tags>
ArgSpec make_arg_spec(std::string_view prefix, std::string_view reflectedName, const Tags& tags,
                      const ArgParserConfig& config) {
    const auto explicitLongName = tag_query::get<tag_prop::long_name>(tags);
    const auto name             = explicitLongName.empty() ? reflectedName : explicitLongName;

    ArgSpec spec;
    spec.longName  = join_arg_name(prefix, name, config.nestedSeparator);
    spec.shortName = std::string(tag_query::get<tag_prop::short_name>(tags));
    auto aliases   = tag_query::get<tag_prop::aliases>(tags);
    spec.aliases.assign(aliases.begin(), aliases.end());
    spec.help                   = std::string(tag_query::get<tag_prop::help>(tags));
    spec.valueName              = std::string(tag_query::get<tag_prop::value_name>(tags));
    spec.envName                = std::string(tag_query::get<tag_prop::env_name>(tags));
    spec.group                  = std::string(tag_query::get<tag_prop::group>(tags));
    spec.deprecatedMessage      = std::string(tag_query::get<tag_prop::deprecated_message>(tags));
    spec.required               = tag_query::get<tag_prop::required>(tags);
    spec.positional             = tag_query::get<tag_prop::positional>(tags);
    spec.flag                   = field_type_is_flag<FieldT>(tag_query::get<tag_prop::flag>(tags));
    spec.repeatable             = tag_query::get<tag_prop::repeatable>(tags) || is_vector_v<FieldT>;
    spec.hidden                 = tag_query::get<tag_prop::hidden>(tags);
    spec.hasRange =
        tag_query::has<tag_prop::range_min>(tags) || tag_query::has<tag_prop::range_max>(tags);
    spec.hasDefault             = tag_query::has<tag_prop::default_value>(tags);
    spec.hasImplicit            = tag_query::has<tag_prop::implicit_value>(tags);
    spec.caseInsensitiveChoices = tag_query::get<tag_prop::case_insensitive_choices>(tags);
    spec.deprecated             = tag_query::has<tag_prop::deprecated_message>(tags);
    spec.rangeMin               = tag_query::get<tag_prop::range_min>(tags);
    spec.rangeMax               = tag_query::get<tag_prop::range_max>(tags);
    spec.separator              = tag_query::get<tag_prop::separator>(tags);

    if constexpr (tag_query::has<tag_prop::default_value>(decltype(tags){})) {
        spec.defaultValue = default_value_to_string(tag_query::get<tag_prop::default_value>(tags));
    }
    if constexpr (tag_query::has<tag_prop::implicit_value>(decltype(tags){})) {
        spec.implicitValue = default_value_to_string(tag_query::get<tag_prop::implicit_value>(tags));
    }

    auto choices = tag_query::get<tag_prop::choices>(tags);
    spec.choices.assign(choices.begin(), choices.end());
    auto conflicts = tag_query::get<tag_prop::conflicts>(tags);
    spec.conflicts.assign(conflicts.begin(), conflicts.end());
    auto requiresNames = tag_query::get<tag_prop::requires_names>(tags);
    spec.requiresNames.assign(requiresNames.begin(), requiresNames.end());
    return spec;
}

template <typename T>
void collect_schema_into(std::string_view prefix, const ArgParserConfig& config, ArgSchema& schema) {
    static_assert(NEKO_NAMESPACE::detail::has_values_meta<std::remove_cvref_t<T>>,
                  "argparser requires a reflected options type");

    Reflect<std::remove_cvref_t<T>>::forEachMeta(
        [&]<typename FieldT>(std::type_identity<FieldT>, std::string_view reflectedName, const auto& tags) {
            const auto explicitLongName = tag_query::get<tag_prop::long_name>(tags);
            const auto name             = explicitLongName.empty() ? reflectedName : explicitLongName;

            if constexpr (is_nested_option_v<FieldT>) {
                collect_schema_into<FieldT>(join_arg_name(prefix, name, config.nestedSeparator), config, schema);
            } else {
                schema.specs.push_back(make_arg_spec<FieldT>(prefix, reflectedName, tags, config));
                if (schema.specs.back().positional) {
                    schema.positionalSpecs.push_back(schema.specs.size() - 1);
                }
            }
        });
}

template <typename T>
ArgSchema collect_schema(const ArgParserConfig& config) {
    ArgSchema schema;
    collect_schema_into<T>({}, config, schema);
    return schema;
}

} // namespace argparser::detail
NEKO_END_NAMESPACE
