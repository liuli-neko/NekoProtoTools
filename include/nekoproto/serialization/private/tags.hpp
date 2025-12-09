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

#include <type_traits> // std::decay_t, std::is_same_v, std::remove_reference_t
#include <utility>     // std::forward, std::move, std::remove_cvref_t

NEKO_BEGIN_NAMESPACE
struct NoTags {
    template <typename T>
        requires(std::is_class_v<T> && std::is_default_constructible_v<T> && std::is_aggregate_v<T> &&
                 (!std::is_same_v<std::remove_cvref_t<T>, std::string_view>) &&
                 (!std::is_same_v<std::remove_cvref_t<T>, std::string>))
    operator T() const {
        return T{};
    }
};

struct JsonTags {
    bool flat      = false;
    bool skipable  = false;
    bool rawString = false;
};

struct BinaryTags {
    bool fixedLength = false;
};

namespace tag_access {
template <typename T>
constexpr bool is_flat(const T& tags) {
    if constexpr (requires { tags.flat; }) {
        return tags.flat;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_skipable(const T& tags) {
    if constexpr (requires { tags.skipable; }) {
        return tags.skipable;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_fixed_length(const T& tags) {
    if constexpr (requires { tags.fixedLength; }) {
        return tags.fixedLength;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_raw_string(const T& tags) {
    if constexpr (requires { tags.rawString; }) {
        return tags.rawString;
    } else {
        return false;
    }
}
} // namespace tag_access

// --- The Core Wrapper Template ---
template <auto Tags, typename InnerValue>
struct TaggedValue {
    using raw_type             = InnerValue;
    using tag_type             = decltype(Tags); // 获取 Tag 的实际类型
    constexpr static auto tags = Tags;           // NOLINT
    InnerValue value;

    operator InnerValue() && noexcept { return std::move(value); }
    operator InnerValue() const& noexcept { return value; }
    operator InnerValue() & noexcept { return value; }
    auto operator*() const noexcept { return value; }
    auto operator->() const noexcept { return &value; }
    auto operator*() noexcept { return value; }
    auto operator->() noexcept { return &value; }
    auto& operator=(InnerValue&& value) noexcept {
        this->value = std::move(value);
        return *this;
    }
    auto& operator=(const InnerValue& value) noexcept {
        this->value = value;
        return *this;
    }
    operator raw_type&&() noexcept { return std::move(value); }
    operator const raw_type&() const noexcept { return value; }
    operator raw_type&() noexcept { return value; }
};

template <auto Tags, typename InnerValue>
inline constexpr auto make_tags(InnerValue&& value) { // NOLINT
    return TaggedValue<Tags, InnerValue>{std::forward<InnerValue>(value)};
}

// --- Helper Trait to Identify TaggedValue ---
template <typename T>
struct is_tagged_value : std::false_type {}; // NOLINT
template <auto Tags, typename Inner>
struct is_tagged_value<TaggedValue<Tags, Inner>> : std::true_type {};
template <auto Tags, typename Inner>
struct is_tagged_value<TaggedValue<Tags, Inner>&> : std::true_type {};
template <auto Tags, typename Inner>
struct is_tagged_value<const TaggedValue<Tags, Inner>> : std::true_type {};
template <auto Tags, typename Inner>
struct is_tagged_value<const TaggedValue<Tags, Inner>&> : std::true_type {};
template <typename T>
inline constexpr bool is_tagged_value_v = is_tagged_value<std::decay_t<T>>::value; // NOLINT

template <typename T, class enable = void>
struct unwrap_tags { // NOLINT
    using type                 = T;
    constexpr static auto tags = NoTags{}; // NOLINT
};

template <typename T>
struct unwrap_tags<T, std::enable_if_t<is_tagged_value_v<T>>> {
    using type                 = typename std::decay_t<T>::raw_type;
    constexpr static auto tags = std::decay_t<T>::tags; // NOLINT
};

template <typename T>
using unwrap_tags_t = typename unwrap_tags<T>::type;

template <typename T>
inline constexpr auto unwrap_tags_v = unwrap_tags<T>::tags; // NOLINT

NEKO_END_NAMESPACE