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
// --- The Core Wrapper Template ---
template <typename Tag, typename InnerValue>
struct TaggedValue {
    template <typename T, class enable = void>
    struct GetRawType {
        using type = T;
    };
    template <typename T>
        requires requires {
            typename T::TagType;
            typename T::InnerValue;
            typename T::RawType;
        }
    struct GetRawType<T, void> {
        using type = typename InnerValue::RawType;
    };
    using TagType   = Tag;
    using InnerType = InnerValue; // This is the type being wrapped (could be another TaggedValue or raw data)
    using RawType   = typename GetRawType<InnerValue>::type;

    InnerValue innerValue;

    // Constructor using perfect forwarding
    template <typename InnerArg>
    explicit constexpr TaggedValue(InnerArg&& arg) noexcept(std::is_nothrow_constructible_v<InnerValue, InnerArg&&>)
        : innerValue(std::forward<InnerArg>(arg)) {}

    // --- Accessor for the *innermost* raw data ---
    // Recursive helper: strips tags until it finds a non-TaggedValue type
    template <typename T>
    static constexpr decltype(auto) value(T& val) noexcept {
        if constexpr (requires(T val) {
                          typename T::TagType;
                          typename T::InnerType;
                          typename T::RawType;
                          { val.innerValue };
                      }) { // Check if T is a TaggedValue
            if constexpr (std::is_same_v<RawType, T>) {
                return (val);
            } else {
                return value(val.inner()); // Recurse
            }
        } else {
            return (val); // Base case: return the value/reference itself
        }
    }

    constexpr InnerType inner() & noexcept { return this->innerValue; }
    constexpr InnerType inner() const& noexcept { return this->innerValue; }
    constexpr InnerType inner() && noexcept { return std::move(this->innerValue); }
    constexpr InnerType inner() const&& noexcept { return std::move(this->innerValue); }

    // --- Accessors for the *immediately* wrapped value ---
    constexpr RawType& value() & noexcept { return value(*this); }
    constexpr const RawType& value() const& noexcept { return value(*this); }
    constexpr RawType&& value() && noexcept { return std::move(value(*this)); }
    constexpr const RawType&& value() const&& noexcept { return std::move(value(*this)); }

    // Convenience operators for immediate inner value access
    constexpr RawType& operator*() & noexcept { return value(); }
    constexpr const RawType& operator*() const& noexcept { value(); }
    constexpr RawType&& operator*() && noexcept { return std::move(value()); }
    constexpr const RawType&& operator*() const&& noexcept { return std::move(value()); }

    // Optional: pointer-like access if InnerValue supports it
    constexpr auto operator->() noexcept { return std::addressof(value()); }
    constexpr auto operator->() const noexcept { return std::addressof(value()); }
};

// --- Helper Trait to Identify TaggedValue ---
template <typename T>
struct is_tagged_value : std::false_type {}; // NOLINT
template <typename Tag, typename Inner>
struct is_tagged_value<TaggedValue<Tag, Inner>> : std::true_type {};
template <typename T>
inline constexpr bool is_tagged_value_v = is_tagged_value<std::remove_cvref_t<T>>::value; // NOLINT

// --- Tag Definitions and Factory Functions Namespace ---
#define NEKO_DEFINE_TAG(TagName)                                                                                       \
    namespace tag {                                                                                                    \
    struct TagName {                                                                                                   \
        static constexpr std::string_view name = #TagName;                                                             \
    };                                                                                                                 \
    }                                                                                                                  \
    template <typename Inner>                                                                                          \
    [[nodiscard]] constexpr decltype(auto) tag_##TagName(Inner&& inner) {                                              \
        if constexpr (tag_check<tag::TagName>(inner)) {                                                                \
            return std::forward<Inner>(inner);                                                                         \
        } else {                                                                                                       \
            using StoredType =                                                                                         \
                typename std::conditional<std::is_array<typename std::remove_reference<Inner>::type>::value,           \
                                          typename std::remove_cv<Inner>::type,                                        \
                                          typename std::conditional<std::is_lvalue_reference<Inner>::value, Inner,     \
                                                                    typename std::decay<Inner>::type>::type>::type;    \
                                                                                                                       \
            return TaggedValue<tag::TagName, StoredType>(std::forward<Inner>(inner));                                  \
        }                                                                                                              \
    }

// --- Tag Checking Function ---
template <typename TargetTag, typename T>
[[nodiscard]] constexpr bool tag_check(const T& value) noexcept {
    // Use remove_cvref to handle const/ref qualifiers on the value itself
    using CleanT = std::remove_cvref_t<T>;

    if constexpr (is_tagged_value_v<CleanT>) {
        // Check if the *current* tag matches
        if constexpr (std::is_same_v<TargetTag, typename CleanT::TagType>) {
            return true;
        } else {
            // If not, recursively check the inner value
            // Pass the inner value by const reference
            return tag_check<TargetTag>(value.value());
        }
    } else if constexpr (std::is_base_of_v<TargetTag, T>) {
        // If the value is a base class of the target tag, it is considered tagged
        return true;
    } else {
        // Base case: value is not tagged, so it can't have the target tag
        return false;
    }
}

NEKO_DEFINE_TAG(flat)     // NOLINT 在json中意味着直接将object展开到当前object中
NEKO_DEFINE_TAG(skipable) // NOLINT 在json中意味着可以跳过该字段,只能跳过命名字段

#undef NEKO_DEFINE_TAG

template <typename T>
[[nodiscard]] constexpr decltype(auto) tag_unpack(T&& value) noexcept {
    if constexpr (is_tagged_value_v<T>) {
        return value.value();
    } else {
        return std::forward<T>(value);
    }
}

// --- Deduction Guide (Optional but can be helpful) ---
template <typename Tag, typename InnerArg>
TaggedValue(Tag, InnerArg&&) -> TaggedValue<Tag, std::decay_t<InnerArg>>; // Basic deduction

// More specific deduction if needed, especially around references:
template <typename Tag, typename T>
TaggedValue(Tag, T&) -> TaggedValue<Tag, std::reference_wrapper<T>>;
template <typename Tag, typename T>
TaggedValue(Tag, const T&) -> TaggedValue<Tag, std::reference_wrapper<const T>>;

NEKO_END_NAMESPACE