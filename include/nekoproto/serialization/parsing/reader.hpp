#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/private/tags.hpp"

#include <cstddef>
#include <utility>

namespace nekoproto {
namespace parsing {

/**
 * @brief Reader-side tag dispatch matching Parent<W>'s writer dispatch.
 *
 * Every helper prefers a tag-aware backend overload and falls back to the
 * legacy overload. Parsers call these helpers only for the node currently
 * being decoded; container elements do not inherit their container's tags.
 */
template <typename R, typename Tags>
auto readerToArray(typename R::InputValueType input, const Tags& tags) {
    if constexpr (requires { R::toArray(input, tags); }) {
        return R::toArray(input, tags);
    } else {
        return R::toArray(input);
    }
}

template <typename R, typename Tags>
auto readerToObject(typename R::InputValueType input, const Tags& tags) {
    if constexpr (requires { R::toObject(input, tags); }) {
        return R::toObject(input, tags);
    } else {
        return R::toObject(input);
    }
}

template <typename R, typename T, typename Tags>
auto readerToBasic(typename R::InputValueType input, const Tags& tags) {
    if constexpr (requires { R::template toBasicType<T>(input, tags); }) {
        return R::template toBasicType<T>(input, tags);
    } else {
        return R::template toBasicType<T>(input);
    }
}

template <typename R, typename T, typename Tags>
auto readerToFixedBasic(typename R::InputValueType input, std::size_t size, const Tags& tags) {
    if constexpr (requires { R::template toFixedBasicType<T>(input, size, tags); }) {
        return R::template toFixedBasicType<T>(input, size, tags);
    } else {
        return R::template toFixedBasicType<T>(input, size);
    }
}

template <typename R, typename Tags>
auto readerIsEmpty(typename R::InputValueType input, const Tags& tags) -> bool {
    if constexpr (requires { R::isEmpty(input, tags); }) {
        return R::isEmpty(input, tags);
    } else {
        return R::isEmpty(input);
    }
}

template <typename R, typename Tags>
    requires requires(typename R::InputValueType input, const Tags& tags) { R::toRawString(input, tags); } ||
             requires(typename R::InputValueType input) { R::toRawString(input); }
auto readerToRawString(typename R::InputValueType input, const Tags& tags) {
    if constexpr (requires { R::toRawString(input, tags); }) {
        return R::toRawString(input, tags);
    } else {
        return R::toRawString(input);
    }
}

template <typename R, typename CharT, typename Traits, typename Tags>
    requires requires(typename R::InputValueType input, const Tags& tags) {
        R::template toStringView<CharT, Traits>(input, tags);
    } || requires(typename R::InputValueType input) { R::template toStringView<CharT, Traits>(input); }
auto readerToStringView(typename R::InputValueType input, const Tags& tags) {
    if constexpr (requires { R::template toStringView<CharT, Traits>(input, tags); }) {
        return R::template toStringView<CharT, Traits>(input, tags);
    } else {
        return R::template toStringView<CharT, Traits>(input);
    }
}

template <typename R, typename Name, typename Tags>
auto readerObjectField(const typename R::InputObjectType& object, Name&& name, const Tags& tags) {
    if constexpr (requires { R::objectField(object, std::forward<Name>(name), tags); }) {
        return R::objectField(object, std::forward<Name>(name), tags);
    } else {
        return R::objectField(object, std::forward<Name>(name));
    }
}

template <typename R, typename Fn, typename Tags>
auto readerForEachObjectMember(const typename R::InputObjectType& object, Fn&& fn, const Tags& tags) -> bool {
    if constexpr (requires { R::forEachObjectMember(object, std::forward<Fn>(fn), tags); }) {
        return R::forEachObjectMember(object, std::forward<Fn>(fn), tags);
    } else {
        return R::forEachObjectMember(object, std::forward<Fn>(fn));
    }
}

} // namespace parsing
} // namespace nekoproto
