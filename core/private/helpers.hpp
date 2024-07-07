#pragma once

#include "detail/traits.hpp"
#include "global.hpp"

#include <type_traits>

NEKO_BEGIN_NAMESPACE

template <class T>
class NameValuePair : traits::detail::NameValuePairCore {
private:
    // If we get passed an array, keep the type as is, otherwise store
    // a reference if we were passed an left value reference, else copy the value
    using Type = typename std::conditional<
        std::is_array<typename std::remove_reference<T>::type>::value, typename std::remove_cv<T>::type,
        typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type>::type;
    // prevent nested name value pair
    static_assert(!std::is_base_of<traits::detail::NameValuePairCore, T>::value,
                  "Cannot pair a name to a NameValuePair");
    NameValuePair& operator=(NameValuePair const&) = delete;

public:
    //! Constructs a new NameValuePair
    /*! @param name The name of the pair
        @param value The value to pair.  Ideally this should be an left-value reference so that
                the value can be both loaded and saved to.  If you pass an right-value reference,
                the NameValuePair will store a copy of it instead of a reference.  Thus you should
                only pass right-values in cases where this makes sense, such as the result of some
                size() call.
        @internal */
    NameValuePair(const char* name, const std::size_t nameLen, T&& value)
        : name(name), nameLen(nameLen), value(std::forward<T>(value)) {}
#if NEKO_CPP_PLUS >= 17
    NameValuePair(std::string_view name, T&& value)
        : name(name.data()), nameLen(name.size()), value(std::forward<T>(value)) {}
#endif
    const char const* name;
    std::size_t nameLen;
    Type value;
};

template <class T>
class SizeTag : public traits::detail::SizeTagCore {
private:
    // Store a reference if passed an lvalue reference, otherwise
    // make a copy of the data
    using Type = typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type;
    SizeTag& operator=(SizeTag const&) = delete;

public:
    SizeTag(T&& sz) : size(std::forward<T>(sz)) {}
    Type size;
};

template <class Archive, class T>
inline void prologue(Archive& /* archive */, T const& /* data */) {}

//! Called after a type is serialized to tear down any special archive state
//! for processing some type
/*! @ingroup Internal */
template <class Archive, class T>
inline void epilogue(Archive& /* archive */, T const& /* data */) {}

template <typename SelfT>
class OutputSerializer {
public:
    using SerializerType = SelfT;

public:
    OutputSerializer(SelfT* self) : mSelf(self) {}
    template <class... Types>
    inline bool operator()(Types&&... args) {
        return process(std::forward<Types>(args)...);
    }

private:
    // process a single value
    template <class T>
    inline bool process(T&& head) {
        prologue(*mSelf, head);
        auto ret = mSelf->processImpl(head);
        epilogue(*mSelf, head);
        return ret;
    }

    //! Unwinds to process all data
    template <class T, class... Other>
    inline bool process(T&& head, Other&&... tail) {
        auto ret = process(std::forward<T>(head));
        ret      = process(std::forward<Other>(tail)...) && ret;
        return ret;
    }
    template <class T, traits::enable_if_t<traits::has_method_const_serialize<T, typename SerializerType>::value,
                                           !traits::has_function_save<T, typename SerializerType>::value,
                                           !traits::has_method_const_save<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(const T& value) {
        return traits::method_access::method_serialize(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_function_save<T, typename SerializerType>::value,
                                           !traits::has_method_const_save<T, typename SerializerType>::value,
                                           !traits::has_method_const_serialize<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(const T& value) {
        return save(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_method_const_save<T, typename SerializerType>::value,
                                           !traits::has_function_save<T, typename SerializerType>::value,
                                           !traits::has_method_const_serialize<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(const T& value) {
        return traits::method_access::method_save(*mSelf, value);
    }
    template <class T, traits::enable_if_t<!traits::has_function_save<T, typename SerializerType>::value,
                                           !traits::has_method_const_save<T, typename SerializerType>::value,
                                           !traits::has_method_const_serialize<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(const T& value) {
        static_assert(traits::has_method_const_serialize<T, typename SerializerType>::value != 0,
                      "can not find any function to serialize this Type, must have a serialize method or save method "
                      "or save function.");
        return false;
    }

protected:
    SelfT* mSelf;
};

template <typename SelfT>
class InputSerializer {
public:
    using SerializerType = SelfT;

public:
    InputSerializer(SelfT* self) : mSelf(self) {}
    template <class... Types>
    inline bool operator()(Types&&... args) {
        return process(std::forward<Types>(args)...);
    }

private:
    // process a single value
    template <class T>
    inline bool process(T&& head) {
        prologue(*mSelf, head);
        auto ret = mSelf->processImpl(head);
        epilogue(*mSelf, head);
        return ret;
    }

    //! Unwinds to process all data
    template <class T, class... Other>
    inline bool process(T&& head, Other&&... tail) {
        auto ret = process(std::forward<T>(head));
        ret      = process(std::forward<Other>(tail)...) && ret;
        return ret;
    }
    template <class T, traits::enable_if_t<traits::has_method_deserialize<T, typename SerializerType>::value,
                                           !traits::has_method_load<T, typename SerializerType>::value,
                                           !traits::has_function_load<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(T& value) {
        return traits::method_access::method_deserialize(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_function_load<T, typename SerializerType>::value,
                                           !traits::has_method_load<T, typename SerializerType>::value,
                                           !traits::has_method_deserialize<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(T& value) {
        return load(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_method_load<T, typename SerializerType>::value,
                                           !traits::has_function_load<T, typename SerializerType>::value,
                                           !traits::has_method_deserialize<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(T& value) {
        return traits::method_access::method_load(*mSelf, value);
    }
    template <class T, traits::enable_if_t<!traits::has_function_load<T, typename SerializerType>::value,
                                           !traits::has_method_load<T, typename SerializerType>::value,
                                           !traits::has_method_deserialize<T, typename SerializerType>::value> =
                           traits::default_value_for_enable>
    inline bool processImpl(T& value) {
        static_assert(traits::has_method_const_serialize<T, typename SerializerType>::value != 0,
                      "can not find any function to serialize this Type, must have a serialize method or save method "
                      "or save function.");
        return false;
    }

protected:
    SelfT* mSelf;
};

namespace traits {
template <typename T, class enable = void>
struct is_input_serializer : std::false_type {};

template <typename T>
struct is_input_serializer<T, typename std::enable_if<std::is_base_of<InputSerializer<T>, T>::value>::type>
    : std::true_type {};

template <typename T, class enable = void>
struct is_output_serializer : std::false_type {};

template <typename T>
struct is_output_serializer<T, typename std::enable_if<std::is_base_of<OutputSerializer<T>, T>::value>::type>
    : std::true_type {};
} // namespace traits

NEKO_END_NAMESPACE