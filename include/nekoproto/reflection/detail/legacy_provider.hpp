#pragma once
#include "nekoproto/global/macros.hpp"
#include "legacy_metadata.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
template <typename T>
struct LegacyReflectionProvider {
    using context_type = std::decay_t<T>;

    static constexpr ReflectProviderKind provider_kind = (is_ref_by_local_v<T> || is_ref_by_meta_v<T>) ? ReflectProviderKind::ExplicitMetadata :
        (can_unwrap_v<T> ? ReflectProviderKind::LegacyAutoUnwrap : ReflectProviderKind::Error);
    static constexpr MetaKind            legacy_kind   = meta_kind_v<T>;
    static constexpr MetaKind            kind          = legacy_kind;
    static constexpr bool                has_names     = HasLegacyNamesMeta<T>;
    static constexpr bool                has_values    = provider_kind != ReflectProviderKind::Error;

    static constexpr auto names() {
        if constexpr (HasNameFunction<T>) {
            return MetaPrivate<T>::names();
        } else {
            return std::array<std::string_view, 0>{};
        }
    }

    template <typename U>
    static constexpr auto accessors(U&& obj) -> decltype(auto) {
        if constexpr (requires { std::forward<U>(obj).nekoMemberTuple(); }) {
            // NEKO_SERIALIZER can bind all fields once. Traversals consume this
            // tuple directly instead of invoking N accessors that each rebuild
            // the complete argument tuple.
            return std::forward<U>(obj).nekoMemberTuple();
        } else if constexpr (HasValueFunctionOne<T>) {
            return MetaPrivate<T>::value(std::forward<U>(obj));
        } else {
            return accessors();
        }
    }

    static constexpr auto accessors() -> decltype(auto) {
        if constexpr (HasValueFunction<T>) {
            return MetaPrivate<T>::value();
        } else {
            return std::forward_as_tuple();
        }
    }

    using accessors_type = std::decay_t<decltype(accessors(std::declval<context_type&>()))>;

    static constexpr std::size_t value_count = has_values ? reflect_accessor_count_v<accessors_type> : 0;

    template <std::size_t I>
    using accessor_type = typename ReflectAccessorAt<I, accessors_type>::type;

    template <std::size_t I>
    using field_type = ResolveMemberTypeT<FieldAccessorT<accessor_type<I>>, context_type>;

    template <std::size_t I>
    static constexpr auto name() {
        static_assert(has_names, "provider has no reflected names");
        constexpr auto field_names = names();
        static_assert(I < field_names.size(), "provider name index out of range");
        return field_names[I];
    }

    template <std::size_t I>
    static constexpr auto tag() {
        return std::get<I>(tags());
    }

    template <std::size_t I, typename U>
    static constexpr auto get(U&& obj) -> decltype(auto) {
        decltype(auto) values = accessors(std::forward<U>(obj));
        return getFrom<I>(values, obj);
    }

    template <std::size_t I, typename Values, typename U>
    static constexpr auto getFrom(Values&& values, U&& obj) -> decltype(auto) {
        if constexpr (is_std_tuple_v<std::decay_t<Values>>) {
            decltype(auto) acc = std::get<I>(std::forward<Values>(values));
            using AccType      = std::decay_t<decltype(acc)>;
            if constexpr (std::is_reference_v<decltype(acc)> && !std::is_member_object_pointer_v<AccType> &&
                          !IsMemberRefFunction<AccType, std::remove_reference_t<U>>) {
                return std::forward<decltype(acc)>(acc);
            } else {
                return valueRef(acc, obj);
            }
        } else {
            return valueRef(std::forward<Values>(values), obj);
        }
    }

    static constexpr auto tags() {
        if constexpr (has_values) {
            using values_type = std::decay_t<decltype(accessors(std::declval<context_type&>()))>;
            if constexpr (IsLocalRefObject<T> || IsLocalRefArray<T>) {
                return T::Neko::value.tags;
            } else if constexpr (!is_ref_by_local_v<T> && (IsMetaRefObject<T> || IsMetaRefArray<T>)) {
                return Meta<T>::value.tags;
            } else if constexpr (requires { T::Neko::field_tags; }) {
                return T::Neko::field_tags;
            } else if constexpr (!is_ref_by_local_v<T> && requires { Meta<T>::field_tags; }) {
                return Meta<T>::field_tags;
            } else if constexpr (is_std_tuple_v<values_type>) {
                return tuple_tags_unwrap_v<values_type, context_type>;
            } else {
                return std::make_tuple(field_tags_v<values_type>);
            }
        } else {
            return std::make_tuple();
        }
    }

    template <typename U>
    struct Bound {
        decltype(accessors(std::declval<U&>())) values;
        U* object;
    };
    template <typename U>
    static constexpr auto bind(U& object) {
        return Bound<U>{accessors(object), &object};
    }
    template <std::size_t I, typename U>
    static constexpr auto getBound(Bound<U>& bound) -> decltype(auto) {
        return getFrom<I>(bound.values, *bound.object);
    }
    template <auto MemberPtr>
    static consteval auto memberIndex() noexcept -> std::size_t {
        if constexpr (!LegacyReflectionProvider::has_values) {
            return static_cast<std::size_t>(-1);
        } else if constexpr (std::is_member_object_pointer_v<decltype(MemberPtr)> &&
                             !std::is_base_of_v<traits::MemberClassOfT<decltype(MemberPtr)>, T>) {
            return static_cast<std::size_t>(-1);
        } else if constexpr (LegacyReflectionProvider::provider_kind == detail::ReflectProviderKind::ExplicitMetadata) {
            constexpr auto accs = LegacyReflectionProvider::accessors();
            return []<std::size_t... Is>(std::index_sequence<Is...>, const auto& tuple_accs) consteval -> std::size_t {
                std::size_t found = static_cast<std::size_t>(-1);
                auto check_one = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) consteval {
                    auto raw_acc = fieldAccessor(detail::ReflectAccessorAt<I, std::decay_t<decltype(tuple_accs)>>::get(tuple_accs));
                    using AccType = std::decay_t<decltype(raw_acc)>;
                    using PtrType = std::decay_t<decltype(MemberPtr)>;
                    if constexpr (std::is_same_v<AccType, PtrType>) {
                        if (raw_acc == MemberPtr) {
                            found = I;
                        }
                    }
                };
                (check_one(std::integral_constant<std::size_t, Is>{}), ...);
                return found;
            }(std::make_index_sequence<LegacyReflectionProvider::value_count>{}, accs);
        } else if constexpr (LegacyReflectionProvider::provider_kind == detail::ReflectProviderKind::LegacyAutoUnwrap) {
            if constexpr (std::is_member_object_pointer_v<decltype(MemberPtr)>) {
                constexpr std::string_view member_name = detail::memberPointerName<MemberPtr>();
                if constexpr (!member_name.empty()) {
                    constexpr auto field_names = LegacyReflectionProvider::names();
                    for (std::size_t i = 0; i < field_names.size(); ++i) {
                        if (field_names[i] == member_name) {
                            return i;
                        }
                    }
                }
            }
            return static_cast<std::size_t>(-1);
        } else {
            return static_cast<std::size_t>(-1);
        }
    }

};

}
NEKO_END_EXPORT
