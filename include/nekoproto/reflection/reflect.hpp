#pragma once
#include "nekoproto/global/macros.hpp"

#include "model.hpp"
#include "detail/enum_provider.hpp"
#include "nekoproto/global/log.hpp"
#include <any>
#include <map>

namespace nekoproto {
namespace detail {
struct RefAny {
    RefAny()              = default;
    RefAny(const RefAny&) = default;
    RefAny(RefAny&&)      = default;
    template <typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, RefAny> && std::is_lvalue_reference_v<T &&>)
    RefAny(T&& obj) : any_(std::ref(obj)) {}
    template <typename T>
    auto as() -> decltype(auto) {
        return std::any_cast<std::reference_wrapper<T>>(any_).get();
    }
    auto operator=(const RefAny&) -> RefAny& = default;
    auto operator=(RefAny&&) -> RefAny&      = default;

    template <typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, RefAny>)
    auto operator=(T&& obj) -> RefAny& {
        as<std::remove_cvref_t<T>>() = std::forward<T>(obj);
        return *this;
    }

    auto type() const -> const auto& { return any_.type(); }
    auto valid() const -> bool { return any_.has_value(); }

    template <typename T>
    operator T() {
        return as<T>();
    }
    template <typename T>
    operator std::reference_wrapper<T>() {
        return std::any_cast<std::reference_wrapper<T>>(any_);
    }
    template <typename T>
    auto operator==(const T& obj) const -> bool {
        return std::any_cast<std::reference_wrapper<T>>(any_).get() == obj;
    }

private:
    std::any any_;
};

} // namespace detail

// =============================================================================
// Public Concepts & Type Traits
// =============================================================================

NEKO_BEGIN_EXPORT
template <typename T>
inline constexpr bool is_reflectable_v =
    detail::HasValuesMeta<std::remove_cvref_t<T>> || std::is_enum_v<std::remove_cvref_t<T>>;

template <typename T>
concept Reflectable = is_reflectable_v<T>;

template <typename T>
inline constexpr bool has_named_reflection_v = detail::HasNamesMeta<std::remove_cvref_t<T>>;

template <typename T>
concept NamedReflectable = has_named_reflection_v<T>;

template <typename T>
inline constexpr bool is_tuple_v = detail::is_std_tuple_v<std::remove_cvref_t<T>>;

template <typename T>
concept TupleLike = is_tuple_v<T>;

template <typename T>
inline constexpr bool is_optional_v = traits::OptionalLikeType<std::remove_cvref_t<T>>::value;

template <typename T>
using OptionalValueT = traits::UnwrappedOptionalLikeTypeT<std::remove_cvref_t<T>>;

template <typename T>
concept OptionalLike = traits::OptionalLike<std::remove_cvref_t<T>>;

template <class... Ts>
struct Overloads : public Ts... {
    using Ts::operator()...;
    Overloads(Ts... args) : Ts(args)... {}
};

template <std::size_t Index, typename FieldRef, typename TagsType>
struct FieldView {
    static constexpr std::size_t index = Index;
    using field_type                   = std::remove_cvref_t<FieldRef>;
    using tags_type                    = TagsType;

    std::string_view name;
    FieldRef         value;
    TagsType         tags;

    constexpr auto get_name() const noexcept -> std::string_view { return name; }
    constexpr auto& get_value() const noexcept { return value; }
    constexpr auto get_tags() const noexcept -> const TagsType& { return tags; }
};

template <typename T, class enable = void>
struct Reflect {
private:
    using Provider = detail::ReflectProvider<T>;
    using Model    = detail::ReflectModel<T>;

    template <typename U>
    using ObjectReference = std::remove_reference_t<U>&;

    template <typename U>
    using BoundAccessors = decltype(Provider::bind(std::declval<ObjectReference<U>>()));

    template <std::size_t I, typename U>
    using FieldReference = decltype(Provider::template getBound<I>(std::declval<BoundAccessors<U>&>()));

    template <typename U, typename CallAbleT>
    static constexpr auto forEachAdaptive(U&& obj, CallAbleT&& func) {
        decltype(auto) accessors  = Provider::bind(obj);
        constexpr auto fieldNames = Provider::names();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                auto&& val  = Provider::template getBound<I>(accessors);
                auto&& tags = std::get<I>(field_tags);
                if constexpr (std::is_invocable_v<CallAbleT&, decltype(val), std::string_view, decltype(tags)>) {
                    static_assert(Provider::has_names, "type has no names meta or names size mismatch");
                    return detail::removeVoidToMonostate(func, val, fieldNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, decltype(val), decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, val, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, decltype(val), std::string_view>) {
                    static_assert(Provider::has_names, "type has no names meta or names size mismatch");
                    return detail::removeVoidToMonostate(func, val, fieldNames[I]);
                } else if constexpr (std::is_invocable_v<CallAbleT&, decltype(val)>) {
                    return detail::removeVoidToMonostate(func, val);
                } else {
                    static_assert(!Provider::has_values, "Callback function signature not supported. "
                                                         "Supported: (val, name, tags), (val, tags), (val, "
                                                         "name), (val)");
                }
            };
            return std::tuple{invoke(std::integral_constant<std::size_t, Is>{})...};
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename CallAbleT>
    static constexpr auto forEachMetaAdaptive(CallAbleT&& func) {
        constexpr auto fieldNames = names();
        using names_type          = std::decay_t<decltype(fieldNames)>;
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                using Field    = std::tuple_element_t<I, value_types>;
                using FieldTag = std::type_identity<Field>;
                auto&& tags    = std::get<I>(field_tags);
                if constexpr (std::is_invocable_v<CallAbleT&, FieldTag, std::string_view, decltype(tags)>) {
                    static_assert(detail::IsStdArray<names_type>::size == value_count,
                                  "type has no names meta or names size mismatch");
                    return detail::removeVoidToMonostate(func, FieldTag{}, fieldNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, std::string_view, decltype(tags)>) {
                    static_assert(detail::IsStdArray<names_type>::size == value_count,
                                  "type has no names meta or names size mismatch");
                    return detail::removeVoidToMonostate(func, fieldNames[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, FieldTag, decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, FieldTag{}, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, tags);
                } else {
                    static_assert(!Provider::has_values,
                                  "Callback function signature not supported. Supported: (type, name, tags), "
                                  "(name, tags), (type, tags), (tags)");
                }
            };
            return std::tuple{invoke(std::integral_constant<std::size_t, Is>{})...};
        }(std::make_index_sequence<value_count>{});
    }

public:
    // Parser-oriented traversal: no FieldView or result tuple, and stop at the
    // first field whose callback reports failure.
    template <typename U, typename CallAbleT>
    static constexpr bool forEachWhile(U&& obj, CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        decltype(auto) accessors = Provider::bind(obj);
        constexpr auto fieldNames = Provider::names();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
#if NEKO_USE_EXPANSION_STATEMENTS
            template for (constexpr auto i : std::array<std::size_t, sizeof...(Is)>{Is...}) {
                if (!static_cast<bool>(func(std::integral_constant<std::size_t, i>{},
                    Provider::template getBound<i>(accessors),
                    (i < fieldNames.size()) ? fieldNames[i] : std::string_view{},
                    std::get<i>(field_tags)))) return false;
            }
            return true;
#else
            return (static_cast<bool>(func(std::integral_constant<std::size_t, Is>{},
                                           Provider::template getBound<Is>(accessors),
                                           (Is < fieldNames.size()) ? fieldNames[Is] : std::string_view{},
                                           std::get<Is>(field_tags))) && ...);
#endif
        }(std::make_index_sequence<Provider::value_count>{});
    }

    // STL-style field view iterator
    template <typename U, typename CallAbleT>
    static constexpr auto forEachField(U&& obj, CallAbleT&& func) {
        if constexpr (!Provider::has_values) {
            static_assert(Provider::has_values, "type has no values meta");
        }
        decltype(auto) accessors  = Provider::bind(obj);
        constexpr auto fieldNames = Provider::names();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                auto&& val  = Provider::template getBound<I>(accessors);
                auto&& tags = std::get<I>(field_tags);
                std::string_view name = (I < fieldNames.size()) ? fieldNames[I] : std::string_view{};
                return detail::removeVoidToMonostate(
                    func, FieldView<I, decltype(val), decltype(tags)>{name, val, tags});
            };
            return std::tuple{invoke(std::integral_constant<std::size_t, Is>{})...};
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr auto forEach(U&& obj, CallAbleT&& func) {
        if constexpr (!Provider::has_values) {
            static_assert(Provider::has_values, "type has no values meta");
        }
        return forEachAdaptive(std::forward<U>(obj), std::forward<CallAbleT>(func));
    }

    template <typename U, typename CallAbleT>
    static constexpr auto forEachFull(U&& obj, CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        decltype(auto) accessors  = Provider::bind(obj);
        constexpr auto fieldNames = Provider::names();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(Provider::template getBound<Is>(accessors),
                                                        fieldNames[Is], std::get<Is>(field_tags)))> &&
                           ...)) {
                (func(Provider::template getBound<Is>(accessors), fieldNames[Is], std::get<Is>(field_tags)), ...);
            } else {
                return std::tuple{detail::removeVoidToMonostate(func, Provider::template getBound<Is>(accessors),
                                                                 fieldNames[Is], std::get<Is>(field_tags))...};
            }
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr auto forEachTagged(U&& obj, CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        decltype(auto) accessors = Provider::bind(obj);
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(Provider::template getBound<Is>(accessors),
                                                        std::get<Is>(field_tags)))> &&
                           ...)) {
                (func(Provider::template getBound<Is>(accessors), std::get<Is>(field_tags)), ...);
            } else {
                return std::tuple{detail::removeVoidToMonostate(func, Provider::template getBound<Is>(accessors),
                                                                 std::get<Is>(field_tags))...};
            }
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr auto forEachNamed(U&& obj, CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        decltype(auto) accessors  = Provider::bind(obj);
        constexpr auto fieldNames = Provider::names();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(Provider::template getBound<Is>(accessors),
                                                        fieldNames[Is]))> &&
                           ...)) {
                (func(Provider::template getBound<Is>(accessors), fieldNames[Is]), ...);
            } else {
                return std::tuple{detail::removeVoidToMonostate(func, Provider::template getBound<Is>(accessors),
                                                                 fieldNames[Is])...};
            }
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr auto forEachValue(U&& obj, CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        decltype(auto) accessors = Provider::bind(obj);
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(Provider::template getBound<Is>(accessors)))> && ...)) {
                (func(Provider::template getBound<Is>(accessors)), ...);
            } else {
                return std::tuple{
                    detail::removeVoidToMonostate(func, Provider::template getBound<Is>(accessors))...};
            }
        }(std::make_index_sequence<Provider::value_count>{});
    }

    static constexpr auto size() noexcept {
        if constexpr (Provider::has_names) {
            return Provider::names().size();
        } else if constexpr (Provider::has_values) {
            return Provider::value_count;
        } else {
            return 0;
        }
    }

    static constexpr auto names() noexcept -> decltype(auto) {
        if constexpr (Provider::has_names) {
            return Provider::names();
        } else {
            return std::array<std::string_view, 0>{};
        }
    }

    static constexpr auto name(int index) noexcept {
        if constexpr (Provider::has_names) {
            auto names = Provider::names();
            if (index < 0 || index >= static_cast<int>(names.size())) {
                NEKO_LOG_ERROR("reflection", "index out of range");
                return std::string_view{};
            }
            return names[index];
        } else {
            return std::string_view{};
        }
    }

    static constexpr auto className() noexcept -> decltype(auto) { return detail::class_nameof<T>; }

    template <std::size_t N, typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>>
    static auto value(U&& obj) noexcept -> decltype(auto) {
        if constexpr (Provider::has_values) {
            static_assert(Provider::value_count > N, "out of range");
            return Provider::template get<N>(std::forward<U>(obj));
        } else {
            static_assert(Provider::has_values, "no values meta");
        }
    }

    template <typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>> && std::is_lvalue_reference_v<U&&>
    static auto value(U&& obj, int idx) -> detail::RefAny {
        if constexpr (Provider::has_values) {
            detail::RefAny ret;
            [&obj, &ret, idx]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                ((idx == Is ? (ret = detail::RefAny(Provider::template get<Is>(obj))) : detail::RefAny{}), ...);
            }(std::make_index_sequence<Provider::value_count>{});
            return ret;
        } else {
            static_assert(Provider::has_values, "no values meta");
        }
    }

    template <typename U>
        requires std::is_same_v<std::remove_cvref_t<U>, std::remove_cvref_t<T>> && std::is_lvalue_reference_v<U&&>
    static auto value(U&& obj, std::string_view name) -> detail::RefAny {
        if constexpr (Provider::has_values && Provider::has_names) {
            detail::RefAny ret;
            [&obj, &ret, name]<std::size_t... Is>(std::index_sequence<Is...>) mutable {
                ((name == Provider::template name<Is>() ? (ret = detail::RefAny(Provider::template get<Is>(obj)))
                                                        : detail::RefAny{}),
                 ...);
            }(std::make_index_sequence<Provider::value_count>{});
            return ret;
        } else {
            static_assert(Provider::has_values && Provider::has_names, "no values or names meta");
        }
    }

    using value_types                = typename Model::value_types;
    static constexpr auto field_tags = Model::field_tags;
    static constexpr int  value_count = Model::value_count;

    template <typename CallAbleT>
    static constexpr auto forEachMeta(CallAbleT&& func) {
        if constexpr (!Provider::has_values) {
            static_assert(Provider::has_values, "type has no values meta");
        }
        return forEachMetaAdaptive(std::forward<CallAbleT>(func));
    }

    template <typename CallAbleT>
    static constexpr auto forEachMetaFull(CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        constexpr auto fieldNames = names();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(std::type_identity<std::tuple_element_t<Is, value_types>>{},
                                                        fieldNames[Is], std::get<Is>(field_tags)))> &&
                           ...)) {
                (func(std::type_identity<std::tuple_element_t<Is, value_types>>{}, fieldNames[Is],
                      std::get<Is>(field_tags)),
                 ...);
            } else {
                return std::tuple{
                    detail::removeVoidToMonostate(func, std::type_identity<std::tuple_element_t<Is, value_types>>{},
                                                     fieldNames[Is], std::get<Is>(field_tags))...};
            }
        }(std::make_index_sequence<value_count>{});
    }

    template <typename CallAbleT>
    static constexpr auto forEachMetaNamed(CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        constexpr auto fieldNames = names();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(fieldNames[Is], std::get<Is>(field_tags)))> && ...)) {
                (func(fieldNames[Is], std::get<Is>(field_tags)), ...);
            } else {
                return std::tuple{detail::removeVoidToMonostate(func, fieldNames[Is], std::get<Is>(field_tags))...};
            }
        }(std::make_index_sequence<value_count>{});
    }

    template <typename CallAbleT>
    static constexpr auto forEachMetaTyped(CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(std::type_identity<std::tuple_element_t<Is, value_types>>{},
                                                        std::get<Is>(field_tags)))> &&
                           ...)) {
                (func(std::type_identity<std::tuple_element_t<Is, value_types>>{}, std::get<Is>(field_tags)), ...);
            } else {
                return std::tuple{detail::removeVoidToMonostate(
                    func, std::type_identity<std::tuple_element_t<Is, value_types>>{}, std::get<Is>(field_tags))...};
            }
        }(std::make_index_sequence<value_count>{});
    }

    template <typename CallAbleT>
    static constexpr auto forEachMetaTagged(CallAbleT&& func) -> decltype(auto) {
        static_assert(Provider::has_values, "type has no values meta");
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
            if constexpr ((std::is_void_v<decltype(func(std::get<Is>(field_tags)))> && ...)) {
                (func(std::get<Is>(field_tags)), ...);
            } else {
                return std::tuple{detail::removeVoidToMonostate(func, std::get<Is>(field_tags))...};
            }
        }(std::make_index_sequence<value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr void visitFull(U&& obj, CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        decltype(auto) accessors  = Provider::bind(obj);
        constexpr auto fieldNames = Provider::names();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(Provider::template getBound<Is>(accessors), fieldNames[Is], std::get<Is>(field_tags)), ...);
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr void visitTagged(U&& obj, CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        decltype(auto) accessors = Provider::bind(obj);
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(Provider::template getBound<Is>(accessors), std::get<Is>(field_tags)), ...);
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr void visitNamed(U&& obj, CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        decltype(auto) accessors  = Provider::bind(obj);
        constexpr auto fieldNames = Provider::names();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(Provider::template getBound<Is>(accessors), fieldNames[Is]), ...);
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename U, typename CallAbleT>
    static constexpr void visitValue(U&& obj, CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        decltype(auto) accessors = Provider::bind(obj);
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(Provider::template getBound<Is>(accessors)), ...);
        }(std::make_index_sequence<Provider::value_count>{});
    }

    template <typename CallAbleT>
    static constexpr void visitMetaFull(CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        constexpr auto fieldNames = names();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(std::type_identity<std::tuple_element_t<Is, value_types>>{}, fieldNames[Is], std::get<Is>(field_tags)),
             ...);
        }(std::make_index_sequence<value_count>{});
    }

    template <typename CallAbleT>
    static constexpr void visitMetaNamed(CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        static_assert(Provider::has_names, "type has no names meta or names size mismatch");
        constexpr auto fieldNames = names();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(fieldNames[Is], std::get<Is>(field_tags)), ...);
        }(std::make_index_sequence<value_count>{});
    }

    template <typename CallAbleT>
    static constexpr void visitMetaTyped(CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(std::type_identity<std::tuple_element_t<Is, value_types>>{}, std::get<Is>(field_tags)), ...);
        }(std::make_index_sequence<value_count>{});
    }

    template <typename CallAbleT>
    static constexpr void visitMetaTagged(CallAbleT&& func) {
        static_assert(Provider::has_values, "type has no values meta");
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(std::get<Is>(field_tags)), ...);
        }(std::make_index_sequence<value_count>{});
    }

private:
    template <auto MemberPtr>
    static consteval auto _indexOfImpl() noexcept -> std::size_t {
        return Provider::template memberIndex<MemberPtr>();
    }

public:
    template <auto MemberPtr>
    static consteval auto indexOf() noexcept -> std::size_t {
        constexpr std::size_t idx = _indexOfImpl<MemberPtr>();
        static_assert(idx != static_cast<std::size_t>(-1),
                      "Member pointer not found in reflection metadata of type T");
        return idx;
    }

    template <auto MemberPtr>
    static consteval auto hasMember() noexcept -> bool {
        return _indexOfImpl<MemberPtr>() != static_cast<std::size_t>(-1);
    }

    template <auto MemberPtr>
    static consteval auto nameOf() noexcept -> std::string_view {
        return names()[indexOf<MemberPtr>()];
    }

    template <auto MemberPtr>
    static consteval auto rawNameOf() noexcept -> std::string_view {
        return names()[indexOf<MemberPtr>()];
    }

    template <auto MemberPtr>
    static consteval auto tagOf() noexcept -> decltype(auto) {
        return std::get<indexOf<MemberPtr>()>(field_tags);
    }

    template <auto MemberPtr>
    using FieldType = std::tuple_element_t<indexOf<MemberPtr>(), typename Model::value_types>;
};

NEKO_END_EXPORT
namespace detail {
template <typename Key, typename Value, std::size_t N>
struct FlatEnumMap {
    std::array<std::pair<Key, Value>, N> entries{};

    struct Iterator {
        const std::pair<Key, Value>* ptr = nullptr;
        constexpr const auto& operator*() const noexcept { return *ptr; }
        constexpr const auto* operator->() const noexcept { return ptr; }
        constexpr Iterator& operator++() noexcept { ++ptr; return *this; }
        constexpr bool operator==(const Iterator& other) const noexcept { return ptr == other.ptr; }
        constexpr bool operator!=(const Iterator& other) const noexcept { return ptr != other.ptr; }
    };

    constexpr auto begin() const noexcept { return Iterator{entries.data()}; }
    constexpr auto end() const noexcept { return Iterator{entries.data() + N}; }

    constexpr auto find(std::string_view key) const noexcept -> Iterator {
        for (std::size_t i = 0; i < N; ++i) {
            if (entries[i].first == key) return Iterator{&entries[i]};
        }
        return end();
    }

    constexpr auto find(Value val) const noexcept -> Iterator {
        for (std::size_t i = 0; i < N; ++i) {
            if (entries[i].first == val) return Iterator{&entries[i]};
        }
        return end();
    }
};
} // namespace detail

NEKO_BEGIN_EXPORT
template <typename T>
struct Reflect<T, std::enable_if_t<std::is_enum_v<T>>> {
    static_assert(!requires { sizeof(Meta<T>); } || detail::IsMetaEnumerate<T>,
                  "invalid custom enum metadata; lower-priority sources are not used");
private:
    using AutoTable = detail::SelectedEnumReflectionTable<T>;

    static constexpr auto tags() noexcept {
        if constexpr (detail::IsMetaEnumerate<T>) {
            return Meta<T>::value.tags;
        } else if constexpr (requires { AutoTable::tags; }) {
            return AutoTable::tags;
        } else {
            return detail::no_tags_tuple_v<size()>;
        }
    }

public:
    static constexpr auto names() noexcept {
        if constexpr (detail::IsMetaEnumerate<T>) {
            return Meta<T>::value.names;
        } else {
            return AutoTable::names;
        }
    }
    static constexpr auto values() noexcept {
        if constexpr (detail::IsMetaEnumerate<T>) {
            return Meta<T>::value.values;
        } else {
            return AutoTable::values;
        }
    }
    static const auto& nameMap() noexcept {
        static constexpr auto N = size();
        static constexpr auto s_name_map = []() consteval {
            detail::FlatEnumMap<std::string_view, T, N> map{};
            auto ns = names();
            auto vs = values();
            for (std::size_t i = 0; i < N; ++i) {
                map.entries[i] = {ns[i], vs[i]};
            }
            return map;
        }();
        return s_name_map;
    }
    static const auto& valueMap() noexcept {
        static constexpr auto N = size();
        static constexpr auto s_value_map = []() consteval {
            detail::FlatEnumMap<T, std::string_view, N> map{};
            auto ns = names();
            auto vs = values();
            for (std::size_t i = 0; i < N; ++i) {
                map.entries[i] = {vs[i], ns[i]};
            }
            return map;
        }();
        return s_value_map;
    }
    static constexpr auto className() noexcept { return detail::class_nameof<T>; }
    static constexpr auto size() noexcept {
        if constexpr (detail::IsMetaEnumerate<T>) {
            return Meta<T>::value.names.size();
        } else {
            return AutoTable::size;
        }
    }
    static constexpr auto field_tags  = tags();
    static constexpr int  value_count = static_cast<int>(size());

    static constexpr auto value(std::string_view name) {
        auto enum_values = values();
        auto enum_names  = names();
        for (int i = 0; i < (int)size(); ++i) {
            if (enum_names[i] == name) {
                return enum_values[i];
            }
        }
        NEKO_LOG_ERROR("reflection", "name not found");
        return T{}; // FIXME: this is not a good way to handle this
    }

    static constexpr auto name(T value) {
        const auto& enum_names  = names();
        const auto& enum_values = values();
        for (int i = 0; i < (int)size(); ++i) {
            if (enum_values[i] == value) {
                return enum_names[i];
            }
        }
        NEKO_LOG_ERROR("reflection", "value not found");
        return std::string_view{}; // FIXME: this is not a good way to handle this
    }

    template <typename U>
        requires(std::is_same_v<T, U> || std::is_integral_v<U>)
    static constexpr auto flagsToString(U value, std::string delimiter = "|") {
        const auto& enum_names  = names();
        const auto& enum_values = values();
        std::string result;
        for (int i = 0; i < (int)size(); ++i) {
            if (static_cast<std::underlying_type_t<T>>(enum_values[i]) & static_cast<std::underlying_type_t<T>>(value)) {
                result += std::string(enum_names[i]) + delimiter;
            }
        }
        if (result.size() > delimiter.size()) {
            result.erase(result.size() - delimiter.size());
        }
        return result;
    }

    template <typename CallAbleT>
    static constexpr auto forEachMeta(CallAbleT&& func) {
        constexpr auto enum_names  = names();
        constexpr auto enum_values = values();
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            auto invoke = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                constexpr auto value = enum_values[I];
                auto&&         tags  = std::get<I>(field_tags);
                if constexpr (std::is_invocable_v<CallAbleT&, std::integral_constant<T, value>, std::string_view,
                                                  decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, std::integral_constant<T, value>{}, enum_names[I],
                                                            tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, T, std::string_view, decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, value, enum_names[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, std::string_view, decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, enum_names[I], tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, std::integral_constant<T, value>,
                                                         decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, std::integral_constant<T, value>{}, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, T, decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, value, tags);
                } else if constexpr (std::is_invocable_v<CallAbleT&, decltype(tags)>) {
                    return detail::removeVoidToMonostate(func, tags);
                } else {
                    static_assert(!std::is_enum_v<T>,
                                  "Callback function signature not supported. Supported: (value, name, tags), "
                                  "(name, tags), (value, tags), (tags)");
                }
            };
            return std::tuple{invoke(std::integral_constant<std::size_t, Is>{})...};
        }(std::make_index_sequence<size()>{});
    }

    template <typename CallAbleT>
    static constexpr void visitMeta(CallAbleT&& func) {
        constexpr auto enum_names  = names();
        constexpr auto enum_values = values();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (func(enum_values[Is], enum_names[Is], std::get<Is>(field_tags)), ...);
        }(std::make_index_sequence<size()>{});
    }

    template <typename CallAbleT>
    static constexpr void visitMetaFull(CallAbleT&& func) {
        visitMeta(std::forward<CallAbleT>(func));
    }
};

NEKO_END_EXPORT
} // namespace nekoproto
