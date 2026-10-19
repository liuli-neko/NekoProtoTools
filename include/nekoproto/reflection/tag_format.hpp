#pragma once
#include "tags.hpp"
#include "nekoproto/global/macros.hpp"
#ifdef NEKO_PROTO_USE_FMT
#include <fmt/format.h>
NEKO_BEGIN_EXPORT
namespace fmt {
template <typename T>
struct formatter<nekoproto::tag_detail::TagValue<T>> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const nekoproto::tag_detail::TagValue<T>& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        if (!value.declared) {
            return fmt::format_to(ctx.out(), "<undeclared>");
        }
        return fmt::format_to(ctx.out(), "{}", value.value);
    }
};
} // namespace fmt
NEKO_END_EXPORT
#elif defined(NEKO_PROTO_USE_STD_FORMAT)
#include <format>
NEKO_BEGIN_EXPORT
namespace std {
template <typename T>
struct formatter<nekoproto::tag_detail::TagValue<T>> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const nekoproto::tag_detail::TagValue<T>& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        if (!value.declared) {
            return std::format_to(ctx.out(), "<undeclared>");
        }
        return std::format_to(ctx.out(), "{}", value.value);
    }
};
} // namespace std
NEKO_END_EXPORT
#endif
