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

#include <string>
#include <type_traits> // std::decay_t, std::is_same_v, std::remove_reference_t
#include <utility>     // std::forward, std::move, std::remove_cvref_t

NEKO_BEGIN_NAMESPACE
struct Tag {
    bool flat        = false;
    bool skipable    = false;
    bool fixedLength = false;
    bool rawString   = false;
};

// --- The Core Wrapper Template ---
template <auto Tag, typename InnerValue>
struct TaggedValue {
    using raw_type            = InnerValue;
    constexpr static auto tag = Tag;
    InnerValue value;
};

template <auto Tag, typename InnerValue>
inline constexpr auto make_tag(InnerValue&& value) {
    return TaggedValue<Tag, InnerValue>{std::forward<InnerValue>(value)};
}

// --- Helper Trait to Identify TaggedValue ---
template <typename T>
struct is_tagged_value : std::false_type {}; // NOLINT
template <auto Tag, typename Inner>
struct is_tagged_value<TaggedValue<Tag, Inner>> : std::true_type {};
template <auto Tag, typename Inner>
struct is_tagged_value<TaggedValue<Tag, Inner>&> : std::true_type {};
template <auto Tag, typename Inner>
struct is_tagged_value<const TaggedValue<Tag, Inner>> : std::true_type {};
template <auto Tag, typename Inner>
struct is_tagged_value<const TaggedValue<Tag, Inner>&> : std::true_type {};
template <typename T>
inline constexpr bool is_tagged_value_v = is_tagged_value<std::remove_cvref_t<T>>::value; // NOLINT

NEKO_END_NAMESPACE