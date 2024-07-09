#pragma once
#include "../global.hpp"

#include <type_traits>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#endif

NEKO_BEGIN_NAMESPACE
namespace traits {
namespace detail {
struct NameValuePairCore {}; // help to trait the type
struct SizeTagCore {};       // help to trait the type
struct SerializableTraitTest {
    template <typename T>
    bool operator()(T&&) const {
        return true;
    }
};
template <bool B, bool... Bs>
struct conditions_and : std::integral_constant<bool, B && conditions_and<Bs...>::value> {};
template <bool B>
struct conditions_and<B> : std::integral_constant<bool, B> {};
template <bool B, bool... Bs>
struct conditions_or : std::integral_constant<bool, B || conditions_or<Bs...>::value> {};
template <bool B>
struct conditions_or<B> : std::integral_constant<bool, B> {};
enum class default_type {};
template <bool... Conditions>
struct enable_if : std::enable_if<conditions_and<Conditions...>::value, default_type> {};
template <bool... Conditions>
struct disable_if : std::enable_if<!conditions_or<Conditions...>::value, default_type> {};
} // namespace detail

template <typename T>
T& dereference(T* ptr) {
    return *ptr;
}

template <typename T, class enable = void>
struct can_be_serializable : std::false_type {};
template <typename T>
struct can_be_serializable<
    T, typename std::enable_if<std::is_same<
           decltype(std::declval<T>().serialize(std::declval<detail::SerializableTraitTest&>())),
           decltype(std::declval<T>().deserialize(std::declval<detail::SerializableTraitTest&>()))>::value>::type>
    : std::true_type {};

static const detail::default_type default_value_for_enable = {};
template <bool... Conditions>
using enable_if_t = typename detail::enable_if<Conditions...>::type;
template <bool... Conditions>
using disable_if_t = typename detail::disable_if<Conditions...>::type;

class method_access {
public:
    template <typename SerializerT, typename T>
    static auto method_serialize(SerializerT& s, T& t) -> decltype(t.serialize(s)) {
        return t.serialize(s);
    }

    template <typename SerializerT, typename T>
    static auto method_deserialize(SerializerT& s, T& t) -> decltype(t.deserialize(s)) {
        return t.deserialize(s);
    }

    template <typename SerializerT, typename T>
    static auto method_const_serialize(SerializerT& s, const T& t) -> decltype(t.serialize(s)) {
        return t.serialize(s);
    }

    template <typename SerializerT, typename T>
    static auto method_load(SerializerT& s, T& t) -> decltype(t.load(s)) {
        return t.load(s);
    }

    template <typename SerializerT, typename T>
    static auto method_const_save(SerializerT& s, const T& t) -> decltype(t.save(s)) {
        return t.save(s);
    }

    template <typename SerializerT, typename T>
    static auto method_save(SerializerT& s, T& t) -> decltype(t.save(s)) {
        return t.save(s);
    }
};

#define NEKO_MAKE_HAS_METHOD_TEST(name, test_name)                                                                     \
    namespace detail {                                                                                                 \
    template <class T, class A>                                                                                        \
    struct has_method_##test_name##_impl {                                                                             \
        template <class TT, class AA>                                                                                  \
        static auto test(int)                                                                                          \
            -> decltype(method_access::method_##name(std::declval<AA&>(), std::declval<TT&>()), std::true_type());     \
        template <class, class>                                                                                        \
        static std::false_type test(...);                                                                              \
        static const bool value = std::is_same<decltype(test<T, A>(0)), std::true_type>::value;                        \
    };                                                                                                                 \
    } /* end namespace detail */                                                                                       \
    template <class T, class A>                                                                                        \
    struct has_method_##test_name : std::integral_constant<bool, detail::has_method_##test_name##_impl<T, A>::value> { \
    };

#define NEKO_MAKE_HAS_CONST_METHOD_TEST(name, test_name)                                                               \
    namespace detail {                                                                                                 \
    template <class T, class A>                                                                                        \
    struct has_method_##test_name##_impl {                                                                             \
        template <class TT, class AA>                                                                                  \
        static auto test(int)                                                                                          \
            -> decltype(method_access::method_##name(std::declval<AA&>(), std::declval<const TT&>()),                  \
                        std::true_type());                                                                             \
        template <class, class>                                                                                        \
        static std::false_type test(...);                                                                              \
        static const bool value = std::is_same<decltype(test<T, A>(0)), std::true_type>::value;                        \
    };                                                                                                                 \
    } /* end namespace detail */                                                                                       \
    template <class T, class A>                                                                                        \
    struct has_method_##test_name : std::integral_constant<bool, detail::has_method_##test_name##_impl<T, A>::value> { \
    };

namespace detail {
template <class T, class A>
struct has_function_save_impl {
    template <class TT, class AA>
    static auto test(int) -> decltype(save(std::declval<AA&>(), std::declval<const TT&>()), std::true_type());
    template <class, class>
    static std::false_type test(...);
    static const bool value = std::is_same<decltype(test<T, A>(0)), std::true_type>::value;

    template <class TT, class AA>
    static auto test2(int)
        -> decltype(save(std::declval<AA&>(), std::declval<typename std::remove_const<TT>::type&>()), std::true_type());
    template <class, class>
    static std::false_type test2(...);
    static const bool not_const_type = std::is_same<decltype(test2<T, A>(0)), std::true_type>::value;
};
template <class T, class A>
struct has_function_load_impl {
    template <class TT, class AA>
    static auto test(int) -> decltype(load(std::declval<AA&>(), std::declval<TT&>()), std::true_type());
    template <class, class>
    static std::false_type test(...);
    static const bool value = std::is_same<decltype(test<T, A>(0)), std::true_type>::value;
};
} /* end namespace detail */

template <class T, class A>
struct has_function_save : std::integral_constant<bool, detail::has_function_save_impl<T, A>::value> {
    using check = typename detail::has_function_save_impl<T, A>;
    static_assert(check::value || !check::not_const_type,
                  "detected a non-const type parameter in non-member function save. \n "
                  " non-member functions save must always pass their types as const");
};
template <class T, class A>
struct has_function_load : std::integral_constant<bool, detail::has_function_load_impl<T, A>::value> {};

NEKO_MAKE_HAS_METHOD_TEST(serialize, serialize)
NEKO_MAKE_HAS_METHOD_TEST(deserialize, deserialize)
NEKO_MAKE_HAS_METHOD_TEST(load, load)
NEKO_MAKE_HAS_METHOD_TEST(save, save)
NEKO_MAKE_HAS_CONST_METHOD_TEST(serialize, const_serialize)
NEKO_MAKE_HAS_CONST_METHOD_TEST(save, const_save)

#undef NEKO_MAKE_HAS_METHOD_TEST
#undef NEKO_MAKE_HAS_CONST_METHOD_TEST

#if NEKO_CPP_PLUS >= 17
template <typename T, class enable = void>
struct is_optional : std::false_type {};
template <typename T>
struct is_optional<std::optional<T>, void> : std::true_type {
    using value_type = T;
};
template <typename T>
struct is_optional<std::optional<T>&, void> : std::true_type {
    using value_type = T;
};
template <typename T>
struct is_optional<const std::optional<T>&, void> : std::true_type {
    using value_type = T;
};
template <typename T>
struct is_optional<const std::optional<T>, void> : std::true_type {
    using value_type = T;
};
#endif

} // namespace traits

template <typename T, class enable = void>
struct is_minimal_serializable : std::false_type {};

template <typename CharT, typename Traits, typename Alloc>
struct is_minimal_serializable<std::basic_string<CharT, Traits, Alloc>, void> : std::true_type {};

#if NEKO_CPP_PLUS >= 17
template <typename T>
struct is_minimal_serializable<std::optional<T>, void> : std::true_type {};

template <typename CharT, typename Traits>
struct is_minimal_serializable<std::basic_string_view<CharT, Traits>, void> : std::true_type {};
#endif
NEKO_END_NAMESPACE