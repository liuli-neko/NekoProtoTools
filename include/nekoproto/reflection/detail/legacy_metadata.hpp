#pragma once
#include "nekoproto/global/macros.hpp"

#include "nekoproto/reflection/metadata.hpp"

NEKO_BEGIN_EXPORT
namespace nekoproto {
namespace detail {
template <typename T, class = void>
struct IsRefObject : std::false_type {};
template <typename T, typename TagsT>
struct IsRefObject<Object<T, TagsT>, void> : std::true_type {};
template <typename T, class = void>
struct IsRefArray : std::false_type {};
template <typename T, typename TagsT>
struct IsRefArray<Array<T, TagsT>, void> : std::true_type {};

template <typename T, class = void>
struct IsRefEnumerate : std::false_type {};

template <typename T, std::size_t N, typename TagsT>
struct IsRefEnumerate<Enumerate<T, N, TagsT>, void> : std::true_type {};

template <typename T, typename U, class = void>
struct MemberMetadata {
    using raw_type                          = FieldAccessorT<T>;
    using parent_type                       = U;
    constexpr static bool is_ref            = std::is_reference_v<raw_type>;
    constexpr static bool is_l_ref          = std::is_lvalue_reference_v<raw_type>;
    constexpr static bool is_lambda         = IsMemberRefFunction<raw_type, U>;
    constexpr static bool is_member_pointer = std::is_member_object_pointer_v<raw_type>;
    constexpr static bool is_ok             = is_ref || is_lambda || is_member_pointer;

    constexpr static auto tags = field_tags_v<T>;
    static constexpr auto get(auto&& accessor, U& obj) noexcept -> decltype(auto) {
        using AccessorType = std::decay_t<decltype(accessor)>;
        if constexpr (std::is_member_object_pointer_v<AccessorType>) {
            return obj.*accessor;
        } else if constexpr (IsMemberRefFunction<AccessorType, U>) {
            return accessor(obj); // lambda 或函数
        } else if constexpr (is_tagged_field_v<AccessorType>) {
            return get(accessor.accessor, obj);
        } else {
            return accessor; // 直接引用
        }
    }
};

template <typename T>
concept IsLocalRefValue = requires {
    { T::Neko::value };
    requires MemberMetadata<decltype(T::Neko::value), T>::is_ok;
};

template <typename T>
concept IsMetaRefValue = requires {
    { Meta<T>::value };
    requires MemberMetadata<decltype(Meta<T>::value), T>::is_ok;
};

template <typename A, typename T, class = void>
struct IsAllMetaRefValue : std::false_type {};

template <typename A, typename... Args>
struct IsAllMetaRefValue<A, std::tuple<Args...>, void> {
    constexpr static bool value = (MemberMetadata<Args, A>::is_ok && ...);
};

template <typename T>
concept IsLocalNames = requires {
    { T::Neko::names };
};

template <typename T>
concept IsMetaNames = requires {
    { Meta<T>::names };
};

template <typename T>
concept IsGlobalRefValues = requires(T& test) {
    { unwrapStruct(test) };
};

template <typename T>
concept IsMetaRefValues = requires {
    { Meta<T>::values };
    requires is_std_tuple_v<std::decay_t<decltype(Meta<T>::values)>>;
    requires IsAllMetaRefValue<T, std::decay_t<decltype(Meta<T>::values)>>::value;
};

template <typename T>
concept IsLocalRefValues = requires {
    { T::Neko::values };
    requires is_std_tuple_v<std::decay_t<decltype(T::Neko::values)>>;
    requires IsAllMetaRefValue<T, std::decay_t<decltype(T::Neko::values)>>::value;
};

template <typename T>
concept IsLocalRefObject = requires {
    { T::Neko::value };
    requires IsRefObject<std::decay_t<decltype(T::Neko::value)>>::value;
};

template <typename T>
concept IsMetaRefObject = requires {
    { Meta<T>::value };
    requires IsRefObject<std::decay_t<decltype(Meta<T>::value)>>::value;
};

template <typename T>
concept IsLocalRefArray = requires {
    { T::Neko::value };
    requires IsRefArray<std::decay_t<decltype(T::Neko::value)>>::value;
};

template <typename T>
concept IsMetaRefArray = requires {
    { Meta<T>::value };
    requires IsRefArray<std::decay_t<decltype(Meta<T>::value)>>::value;
};

template <typename T>
concept IsMetaEnumerate = std::is_enum_v<T> && requires {
    { Meta<T>::value };
    requires IsRefEnumerate<std::decay_t<decltype(Meta<T>::value)>>::value;
};

template <typename T, typename ObjT>
constexpr auto valueRef(T&& accessor, ObjT& obj) noexcept -> decltype(auto) {
    return MemberMetadata<std::decay_t<T>, ObjT>::get(std::forward<T>(accessor), obj);
}

template <typename T>
inline constexpr bool is_ref_by_local_v =
    (IsLocalRefValues<T> && IsLocalNames<T>) || IsLocalRefObject<T> || IsLocalRefArray<T> ||
    IsLocalRefValue<T>;

template <typename T>
inline constexpr bool is_ref_by_meta_v =
    (IsMetaRefValues<T> && IsMetaNames<T>) || IsMetaRefObject<T> || IsMetaRefArray<T> ||
    IsMetaRefValue<T>;

enum class MetaKind {
    ErrorKind,        // error
    AutoUnwrap,       // can_unwrap_v<T> && !is_ref_by_local_v<T> && !is_ref_by_meta_v<T>
    LocalValuesNames, // is_local_ref_values<T> && is_local_names<T>
    LocalObject,      // is_local_ref_object<T>
    LocalArray,       // is_local_ref_array<T>
    LocalValue,       // is_local_ref_value<T> && !is_local_names<T>
    LocalValueNames,  // is_local_names<T> && is_local_ref_value<T>
    MetaValuesNames,  // !is_ref_by_local_v<T> && is_meta_ref_values<T> && is_meta_names<T>
    MetaObject,       // !is_ref_by_local_v<T> && is_meta_ref_object<T>
    MetaArray,        // !is_ref_by_local_v<T> && is_meta_ref_array<T>
    MetaValue,        // !is_ref_by_local_v<T> && is_meta_ref_value<T> && !is_meta_names<T>
    MetaValueNames    // !is_ref_by_local_v<T> && is_meta_names<T> && is_meta_ref_value<T>
};

// High-level backend selection.
//
// MetaKind still describes the exact legacy metadata shape needed by
// MetaPrivateBase. ReflectProviderKind describes which backend family owns the
// type, so a future native reflection provider can replace only automatic
// discovery without disturbing explicit T::Neko or Meta<T> metadata.
enum class ReflectProviderKind { Error, ExplicitMetadata, NativeReflection, LegacyAutoUnwrap };

template <typename T, class Enable = void>
struct NativeReflectionProvider {
    static constexpr bool available = false;
};

template <typename T>
inline constexpr bool native_reflection_provider_available_v = NativeReflectionProvider<T>::available;

// Native reflection annotation bridge.
//
// Future C++26 annotations should be translated here into the same tag objects
// used by makeTags today. The default field hook returns NoTags so enabling the
// placeholder provider cannot silently change serializer or argparser behavior.
template <auto... Annotations>
struct NativeAnnotationTags {
    static constexpr auto value = normalize_tags_v<Annotations...>;
};

template <auto... Annotations>
inline constexpr auto native_annotation_tags_v = NativeAnnotationTags<Annotations...>::value;

template <typename T, std::size_t I, class Enable = void>
struct NativeReflectionFieldTags {
    static constexpr auto value = NoTags{};
};

template <typename T, std::size_t I>
inline constexpr auto native_reflection_field_tags_v = NativeReflectionFieldTags<T, I>::value;

template <typename T>
constexpr auto getReflectProviderKind() noexcept -> ReflectProviderKind {
    if constexpr (std::is_enum_v<T>) {
        return ReflectProviderKind::Error; // enum concepts retain their existing semantics
    } else if constexpr (is_ref_by_local_v<T> || is_ref_by_meta_v<T>) {
        return ReflectProviderKind::ExplicitMetadata;
    } else if constexpr (requires { typename T::Neko; } || requires { sizeof(Meta<T>); }) {
        static_assert(nekoproto::always_false_v<T>, "invalid custom reflection metadata; lower-priority sources are not used");
        return ReflectProviderKind::Error;
    } else if constexpr (native_reflection_provider_available_v<T>) {
        return ReflectProviderKind::NativeReflection;
    } else if constexpr (can_unwrap_v<T>) {
        return ReflectProviderKind::LegacyAutoUnwrap;
    } else {
        return ReflectProviderKind::Error;
    }
}

template <typename T>
inline constexpr auto reflect_provider_kind_v = getReflectProviderKind<T>();

// 辅助函数，确定类型使用哪种 tag
template <typename T>
constexpr auto getMetaKind() noexcept -> MetaKind {
    if constexpr (IsLocalRefValues<T> && IsLocalNames<T>) {
        return MetaKind::LocalValuesNames;
    } else if constexpr (IsLocalRefObject<T>) {
        return MetaKind::LocalObject;
    } else if constexpr (IsLocalRefArray<T>) {
        return MetaKind::LocalArray;
    } else if constexpr (IsLocalRefValue<T> && IsLocalNames<T>) {
        return MetaKind::LocalValueNames;
    } else if constexpr (IsLocalRefValue<T>) {
        return MetaKind::LocalValue;
    } else if constexpr (!is_ref_by_local_v<T> && IsMetaRefValues<T> && IsMetaNames<T>) {
        return MetaKind::MetaValuesNames;
    } else if constexpr (!is_ref_by_local_v<T> && IsMetaRefObject<T>) {
        return MetaKind::MetaObject;
    } else if constexpr (!is_ref_by_local_v<T> && IsMetaRefArray<T>) {
        return MetaKind::MetaArray;
    } else if constexpr (!is_ref_by_local_v<T> && IsMetaRefValue<T> && IsMetaNames<T>) {
        return MetaKind::MetaValueNames;
    } else if constexpr (!is_ref_by_local_v<T> && IsMetaRefValue<T>) {
        return MetaKind::MetaValue;
    } else if constexpr (can_unwrap_v<T>) {
        return MetaKind::AutoUnwrap;
    } else {
        return MetaKind::ErrorKind;
    }
}

template <typename T>
inline constexpr auto meta_kind_v = getMetaKind<T>();

template <typename T, class = void>
struct MetaPrivate {
    static constexpr auto names() noexcept {
        if constexpr (is_ref_by_local_v<T>) {
            if constexpr (IsLocalRefObject<T>) return T::Neko::value.names;
            else if constexpr (IsLocalNames<T>) return T::Neko::names;
            else return std::array<std::string_view, 0>{};
        } else if constexpr (is_ref_by_meta_v<T>) {
            if constexpr (IsMetaRefObject<T>) return Meta<T>::value.names;
            else if constexpr (IsMetaNames<T>) return Meta<T>::names;
            else return std::array<std::string_view, 0>{};
        } else if constexpr (can_unwrap_v<T>) {
            return memberNamesImpl<T>(std::make_index_sequence<member_count_v<T>>{});
        } else return std::array<std::string_view, 0>{};
    }

    template <typename U>
        requires std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>
    static constexpr auto value(U&& obj) -> decltype(auto) {
        if constexpr (IsLocalRefObject<T> || IsLocalRefArray<T>) {
            return T::Neko::value.values;
        } else if constexpr (IsLocalRefValues<T>) {
            return T::Neko::values;
        } else if constexpr (IsLocalRefValue<T>) {
            return T::Neko::value;
        } else if constexpr (IsMetaRefObject<T> || IsMetaRefArray<T>) {
            return Meta<T>::value.values;
        } else if constexpr (IsMetaRefValues<T>) {
            return Meta<T>::values;
        } else if constexpr (IsMetaRefValue<T>) {
            return Meta<T>::value;
        } else if constexpr (can_unwrap_v<T>) {
            return unwrapStruct(std::forward<U>(obj));
        } else {
            return std::forward_as_tuple();
        }
    }

    static constexpr auto value() noexcept {
        if constexpr (IsLocalRefObject<T> || IsLocalRefArray<T>) {
            return T::Neko::value.values;
        } else if constexpr (IsLocalRefValues<T>) {
            return T::Neko::values;
        } else if constexpr (IsLocalRefValue<T>) {
            return T::Neko::value;
        } else if constexpr (IsMetaRefObject<T> || IsMetaRefArray<T>) {
            return Meta<T>::value.values;
        } else if constexpr (IsMetaRefValues<T>) {
            return Meta<T>::values;
        } else if constexpr (IsMetaRefValue<T>) {
            return Meta<T>::value;
        } else {
            return std::forward_as_tuple();
        }
    }
};

template <typename T>
concept HasLegacyNamesMeta = meta_kind_v<T> != MetaKind::ErrorKind && meta_kind_v<T> != MetaKind::MetaValue &&
                         meta_kind_v<T> != MetaKind::LocalValue && meta_kind_v<T> != MetaKind::MetaArray &&
                         meta_kind_v<T> != MetaKind::LocalArray;

template <typename T>
concept HasLegacyValuesMeta = meta_kind_v<T> != MetaKind::ErrorKind;

template <typename T>
concept HasValueFunctionOne = requires(T& obj) {
    { MetaPrivate<T>::value(obj) };
};
template <typename T>
concept HasValueFunction = requires {
    { MetaPrivate<T>::value() };
};
template <typename T>
concept HasNameFunction = requires {
    { MetaPrivate<T>::names() };
};

template <typename Values, bool IsTuple = is_std_tuple_v<Values>>
struct ReflectAccessorCount {
    static constexpr std::size_t value = 1;
};

template <typename Values>
struct ReflectAccessorCount<Values, true> {
    static constexpr std::size_t value = std::tuple_size_v<Values>;
};

template <typename Values>
inline constexpr auto reflect_accessor_count_v = ReflectAccessorCount<Values>::value;

// Bridges the legacy "tuple of accessors or one accessor" representation into
// the index-based provider API. Reflect<T> should not need to know this storage
// detail anymore.
template <std::size_t I, typename Values, bool IsTuple = is_std_tuple_v<Values>>
struct ReflectAccessorAt {
    static_assert(I == 0, "single accessor index out of range");
    using type = Values;

    template <typename U>
    static constexpr auto get(U&& values) -> decltype(auto) {
        static_assert(I == 0, "single accessor index out of range");
        return std::forward<U>(values);
    }
};

template <std::size_t I, typename Values>
struct ReflectAccessorAt<I, Values, true> {
    using type = std::tuple_element_t<I, Values>;

    template <typename U>
    static constexpr auto get(U&& values) -> decltype(auto) {
        return std::get<I>(std::forward<U>(values));
    }
};

// Transitional metadata adapter.
//
// This is the single place that should know how current metadata is spelled:
// T::Neko, Meta<T>, Object/Array/value forms, macro-generated field_tags, and
// aggregate AutoUnwrap. Reflect<T> consumes this normalized provider surface so
// future native reflection can replace only the automatic discovery branch.

} // namespace detail
} // namespace nekoproto
NEKO_END_EXPORT
