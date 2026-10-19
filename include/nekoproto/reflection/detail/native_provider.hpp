#pragma once
#include "nekoproto/global/macros.hpp"
#include "legacy_metadata.hpp"
#include "native_annotations.hpp"

#if NEKO_USE_NATIVE_REFLECTION
#include <meta>

NEKO_BEGIN_EXPORT
namespace nekoproto::detail {
template <typename T>
consteval bool nativeObjectEligible() {
    if constexpr (!std::is_class_v<T> || std::is_union_v<T> || IsStdArray<T>::value || is_std_tuple_v<T>) {
        return false;
    } else if constexpr (!requires { sizeof(T); }) {
        return false;
    } else {
        constexpr auto all = std::meta::access_context::unchecked();
        constexpr auto public_context = std::meta::access_context::unprivileged();
        if (!std::meta::bases_of(^^T, all).empty()) return false;
        for (auto member : std::meta::nonstatic_data_members_of(^^T, all)) {
            if (!std::meta::is_accessible(member, public_context) || std::meta::is_bit_field(member) ||
                !std::meta::has_identifier(member)) return false;
        }
        return true;
    }
}

template <typename T>
struct NativeReflectionProvider<T, std::enable_if_t<nativeObjectEligible<T>()>> {
    using context_type = T;
    static constexpr bool available = true;
    static constexpr bool has_values = true;
    static constexpr bool has_names = true;
    static constexpr auto provider_kind = ReflectProviderKind::NativeReflection;
    static constexpr std::size_t value_count = std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::unprivileged()).size();

    template <std::size_t I>
    static consteval auto member() {
        static_assert(I < value_count, "native field index out of range");
        return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged())[I];
    }
    template <std::size_t I>
    using raw_field_type = [:std::meta::type_of(member<I>()):];
    template <std::size_t I>
    // Match the existing public value_types/FieldType decay contract. Access
    // through getBound still preserves the actual array/reference/cv category.
    using field_type = std::decay_t<raw_field_type<I>>;

    template <std::size_t I>
    static consteval auto name() { return std::meta::identifier_of(member<I>()); }

    static consteval auto names() {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return std::array<std::string_view, value_count>{name<I>()...};
        }(std::make_index_sequence<value_count>{});
    }
    template <std::size_t I>
    static consteval auto tag() { return nativeTagsOf<member<I>()>(); }

    static consteval auto tags() {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return std::tuple{tag<I>()...};
        }(std::make_index_sequence<value_count>{});
    }

    template <typename U> struct Bound { U* object; };
    template <typename U>
    static constexpr auto bind(U& object) { return Bound<U>{&object}; }

    template <std::size_t I, typename U>
    static constexpr auto getBound(Bound<U>& bound) -> decltype(auto) {
        constexpr auto field = member<I>();
        return (bound.object->[:field:]);
    }
    template <std::size_t I, typename U>
    static constexpr auto get(U&& object) -> decltype(auto) {
        auto bound = bind(object);
        return getBound<I>(bound);
    }

    template <auto Pointer>
    static consteval std::size_t memberIndex() {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            std::size_t found = static_cast<std::size_t>(-1);
            auto check = [&]<std::size_t N>() {
                if constexpr (std::is_member_object_pointer_v<decltype(Pointer)>) {
                    using Field = [:std::meta::type_of(member<N>()):];
                    if constexpr (!std::is_reference_v<Field> && std::is_same_v<decltype(Pointer), Field T::*>) {
                        if (std::meta::extract<Field T::*>(member<N>()) == Pointer) found = N;
                    }
                }
            };
            (check.template operator()<I>(), ...);
            return found;
        }(std::make_index_sequence<value_count>{});
    }
};

template <typename T, std::size_t I>
struct NativeReflectionFieldTags<T, I, std::enable_if_t<nativeObjectEligible<T>()>> {
    static constexpr auto value = NativeReflectionProvider<T>::template tag<I>();
};
}
NEKO_END_EXPORT
#endif
