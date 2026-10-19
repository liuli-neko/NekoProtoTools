#pragma once
// Optional macro helpers for include and import consumers; no API redeclarations.
#define NEKO_DETAIL_DEFINE_TAG_PROPERTY(Type, member, ClassName)                                                       \
    struct ClassName {                                                                                                 \
        using type = Type;                                                                                             \
        static constexpr auto missing() noexcept -> type { return {}; }                                                \
        template <typename Tag>                                                                                        \
        static constexpr auto has(const Tag& tag) -> bool {                                                            \
            if constexpr (requires { tag.member; }) {                                                                  \
                return nekoproto::tag_detail::tagValueDeclared(tag.member);                                            \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto get(const Tag& tag) -> type                                                              \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return nekoproto::tag_detail::makeTagValueCommon(tag.member);                                              \
        }                                                                                                              \
    };

#define NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(member, ClassName)                                                       \
    struct ClassName {                                                                                                 \
        template <typename Tag>                                                                                        \
        static constexpr auto has(const Tag& tag) -> bool {                                                            \
            if constexpr (requires { tag.member; }) {                                                                  \
                return nekoproto::tag_detail::tagValueDeclared(tag.member);                                            \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto get(const Tag& tag) -> decltype(auto)                                                    \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return nekoproto::tag_detail::makeTagValueCommon(tag.member);                                              \
        }                                                                                                              \
    };

#define NEKO_DETAIL_DEFINE_TYPE_TAG_PROPERTY(Type, member, ClassName, TraitName)                                       \
    template <typename Value>                                                                                          \
    struct ClassName {                                                                                                 \
        using type = Type;                                                                                             \
        static constexpr auto missing() noexcept -> type {                                                             \
            if constexpr (requires { TraitName<Value>::value; }) {                                                     \
                return TraitName<Value>::value;                                                                        \
            } else {                                                                                                   \
                return {};                                                                                             \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto has(const Tag& tag) -> bool {                                                            \
            if constexpr (requires { tag.member; }) {                                                                  \
                return nekoproto::tag_detail::tagValueDeclared(tag.member);                                            \
            } else {                                                                                                   \
                return false;                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
        template <typename Tag>                                                                                        \
        static constexpr auto get(const Tag& tag) -> type                                                              \
            requires requires { tag.member; }                                                                          \
        {                                                                                                              \
            return static_cast<type>(tag.member);                                                                      \
        }                                                                                                              \
    };

#define NEKO_DEFINE_TAG_PROPERTY(Type, member, ClassName) NEKO_DETAIL_DEFINE_TAG_PROPERTY(Type, member, ClassName)
#define NEKO_DEFINE_TAG_VALUE_PROPERTY(member, ClassName) NEKO_DETAIL_DEFINE_TAG_VALUE_PROPERTY(member, ClassName)
#define NEKO_DEFINE_TYPE_TAG_PROPERTY(Type, member, ClassName, TraitName)                                              \
    NEKO_DETAIL_DEFINE_TYPE_TAG_PROPERTY(Type, member, ClassName, TraitName)
