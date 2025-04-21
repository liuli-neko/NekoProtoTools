#pragma once

#include "global.hpp"
#include "traits.hpp"

#include <cstring>
#include <type_traits>
#include <vector>

NEKO_BEGIN_NAMESPACE

template <class T>
struct NameValuePair : traits::detail::NameValuePairCore {
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
    NameValuePair(const char* name, const std::size_t nameLen, T&& value) NEKO_NOEXCEPT
        : name(name),
          nameLen(nameLen),
          value(std::forward<T>(value)) {}
#if NEKO_CPP_PLUS >= 17
    NameValuePair(std::string_view name, T&& value) NEKO_NOEXCEPT : name(name.data()),
                                                                    nameLen(name.size()),
                                                                    value(std::forward<T>(value)) {}
#endif
    const char* name;
    std::size_t nameLen;
    Type value;
};
template <typename T>
struct is_minimal_serializable<NameValuePair<T>, void> : std::true_type {};

template <class T>
inline NameValuePair<T> make_name_value_pair(const char* name, T&& value) NEKO_NOEXCEPT {
    return {name, std::strlen(name), std::forward<T>(value)};
}

template <class T>
inline NameValuePair<T> make_name_value_pair(const char* name, std::size_t len, T&& value) NEKO_NOEXCEPT {
    return {name, len, std::forward<T>(value)};
}

template <class T>
inline NameValuePair<T> make_name_value_pair(const std::string& name, T&& value) NEKO_NOEXCEPT {
    return {name.c_str(), name.size(), std::forward<T>(value)};
}

#if NEKO_CPP_PLUS >= 17
template <class T>
inline NameValuePair<T> make_name_value_pair(const std::string_view& name, T&& value) NEKO_NOEXCEPT {
    return {name, std::forward<T>(value)};
}
#endif

#define NEKO_PROTO_NAME_VALUE_PAIR(value) make_name_value_pair(#value, value)

template <class T>
class SizeTag : public traits::detail::SizeTagCore {
private:
    // Store a reference if passed an lvalue reference, otherwise
    // make a copy of the data
    using Type = typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type;
    SizeTag& operator=(SizeTag const&) = delete;

public:
    SizeTag(T&& sz) NEKO_NOEXCEPT : size(std::forward<T>(sz)) {}
    Type size;
};

template <typename T>
struct is_minimal_serializable<SizeTag<T>, void> : std::true_type {};

template <class T>
inline SizeTag<T> make_size_tag(T&& sz) NEKO_NOEXCEPT {
    return {std::forward<T>(sz)};
}

template <class Archive, class T>
inline bool prologue(Archive& /* archive */, T const& /* data */) NEKO_NOEXCEPT {
    return true;
}

//! Called after a type is serialized to tear down any special archive state
//! for processing some type
/*! @ingroup Internal */
template <class Archive, class T>
inline bool epilogue(Archive& /* archive */, T const& /* data */) NEKO_NOEXCEPT {
    return true;
}
namespace detail {
template <typename SelfT>
class OutputSerializer {
public:
    using SerializerType = SelfT;

public:
    OutputSerializer(SelfT* self) NEKO_NOEXCEPT : mSelf(self) {}
    template <class... Types>
    bool operator()(Types&&... args) NEKO_NOEXCEPT {
        return _process(std::forward<Types>(args)...);
    }

private:
    // process a single value
    template <class T>
    bool _process(T&& head) NEKO_NOEXCEPT {
        auto ret = prologue(*mSelf, head);
        ret = ret && mSelf->_processImpl(head);
        epilogue(*mSelf, head);
        return ret;
    }

    //! Unwinds to process all data
    template <class T, class... Other>
    bool _process(T&& head, Other&&... tail) NEKO_NOEXCEPT {
        auto ret = _process(std::forward<T>(head));
        ret      = ret && _process(std::forward<Other>(tail)...);
        return ret;
    }
    template <class T, traits::enable_if_t<traits::has_method_const_serialize<T, SerializerType>::value,
                                           !traits::has_function_save<T, SerializerType>::value,
                                           !traits::has_method_const_save<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(const T& value) NEKO_NOEXCEPT {
        return traits::method_access::method_const_serialize(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_function_save<T, SerializerType>::value,
                                           !traits::has_method_const_save<T, SerializerType>::value,
                                           !traits::has_method_const_serialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(const T& value) NEKO_NOEXCEPT {
        return save(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_method_const_save<T, SerializerType>::value,
                                           !traits::has_function_save<T, SerializerType>::value,
                                           !traits::has_method_const_serialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(const T& value) NEKO_NOEXCEPT {
        return traits::method_access::method_const_save(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_method_serialize<T, SerializerType>::value,
                                           !traits::has_method_const_save<T, SerializerType>::value,
                                           !traits::has_function_save<T, SerializerType>::value,
                                           !traits::has_method_const_serialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(const T& value) NEKO_NOEXCEPT {
        // will copy the value if it is not const, I'm not sure if this is the best way to do it. but there is no way to
        // convert a const value to a reference safely. maybe we should have a const serializer method.
        auto tmpValue = value;
        return traits::method_access::method_serialize(*mSelf, tmpValue);
    }
    template <class T, traits::enable_if_t<traits::has_method_serialize<T, SerializerType>::value,
                                           !traits::has_method_const_save<T, SerializerType>::value,
                                           !traits::has_function_save<T, SerializerType>::value,
                                           !traits::has_method_const_serialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(T& value) NEKO_NOEXCEPT {
        return traits::method_access::method_serialize(*mSelf, value);
    }
    template <class T, traits::enable_if_t<!traits::has_function_save<T, SerializerType>::value,
                                           !traits::has_method_const_save<T, SerializerType>::value,
                                           !traits::has_method_const_serialize<T, SerializerType>::value,
                                           !traits::has_method_serialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(const T& value) NEKO_NOEXCEPT {
        static_assert(traits::has_method_const_serialize<T, SerializerType>::value != 0,
                      "can not find any function to serialize this Type, must have a serialize method or save method"
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
    InputSerializer(SelfT* self) NEKO_NOEXCEPT : mSelf(self) {}
    template <class... Types>
    bool operator()(Types&&... args) NEKO_NOEXCEPT {
        return _process(args...);
    }

private:
    // process a single value
    template <class T>
    bool _process(T&& head) NEKO_NOEXCEPT {
        auto ret = prologue(*mSelf, head);
        ret      = ret && mSelf->_processImpl(head);
        ret      = ret && epilogue(*mSelf, head);
        return ret;
    }

    //! Unwinds to process all data
    template <class T, class... Other>
    bool _process(T&& head, Other&&... tail) NEKO_NOEXCEPT {
        auto ret = _process(head);
        ret      = _process(tail...) && ret;
        return ret;
    }
    template <class T, traits::enable_if_t<traits::has_method_deserialize<T, SerializerType>::value,
                                           !traits::has_method_load<T, SerializerType>::value,
                                           !traits::has_function_load<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(T& value) NEKO_NOEXCEPT {
        return traits::method_access::method_deserialize(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_function_load<T, SerializerType>::value,
                                           !traits::has_method_load<T, SerializerType>::value,
                                           !traits::has_method_deserialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(T& value) NEKO_NOEXCEPT {
        return load(*mSelf, value);
    }
    template <class T, traits::enable_if_t<traits::has_method_load<T, SerializerType>::value,
                                           !traits::has_function_load<T, SerializerType>::value,
                                           !traits::has_method_deserialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(T& value) NEKO_NOEXCEPT {
        return traits::method_access::method_load(*mSelf, value);
    }
    template <class T, traits::enable_if_t<!traits::has_method_load<T, SerializerType>::value,
                                           !traits::has_function_load<T, SerializerType>::value,
                                           !traits::has_method_deserialize<T, SerializerType>::value,
                                           traits::has_method_serialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(T& value) NEKO_NOEXCEPT {
        return traits::method_access::method_serialize(*mSelf, value);
    }
    template <class T, traits::enable_if_t<!traits::has_function_load<T, SerializerType>::value,
                                           !traits::has_method_load<T, SerializerType>::value,
                                           !traits::has_method_deserialize<T, SerializerType>::value,
                                           !traits::has_method_serialize<T, SerializerType>::value> =
                           traits::default_value_for_enable>
    bool _processImpl(T& value) NEKO_NOEXCEPT {
        static_assert(traits::has_method_deserialize<T, SerializerType>::value != 0,
                      "can not find any function to serialize this Type, must have a serialize method or save method"
                      "or save function.");
        return false;
    }

protected:
    SelfT* mSelf;
};
} // namespace detail
namespace traits {
template <typename T, class enable = void>
struct is_input_serializer : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T>
struct is_input_serializer<
    T, typename std::enable_if<std::is_base_of<NEKO_NAMESPACE::detail::InputSerializer<T>, T>::value>::type>
    : std::true_type {};

template <typename T, class enable = void>
struct is_output_serializer : std::false_type {}; // NOLINT(readability-identifier-naming)

template <typename T>
struct is_output_serializer<
    T, typename std::enable_if<std::is_base_of<NEKO_NAMESPACE::detail::OutputSerializer<T>, T>::value>::type>
    : std::true_type {};
} // namespace traits
namespace detail {
class OutBufferWrapper {
public:
    using Ch = char;

    explicit OutBufferWrapper() NEKO_NOEXCEPT;
    explicit OutBufferWrapper(std::vector<Ch>& vec) NEKO_NOEXCEPT;
    void setVector(std::vector<Ch>* vec) NEKO_NOEXCEPT;
    void Put(Ch ch) NEKO_NOEXCEPT;             // NOLINT(readability-identifier-naming)
    void Flush() NEKO_NOEXCEPT;                // NOLINT(readability-identifier-naming)
    const Ch* GetString() const NEKO_NOEXCEPT; // NOLINT(readability-identifier-naming)
    std::size_t GetSize() const NEKO_NOEXCEPT; // NOLINT(readability-identifier-naming)
    void Clear() NEKO_NOEXCEPT;                // NOLINT(readability-identifier-naming)

private:
    std::vector<Ch>* mVec;
    std::vector<Ch> mVecUnique; // maybe make it a static.
};

inline OutBufferWrapper::OutBufferWrapper() NEKO_NOEXCEPT : mVec(&mVecUnique) {}
inline OutBufferWrapper::OutBufferWrapper(std::vector<Ch>& vec) NEKO_NOEXCEPT : mVec(&vec) {}
inline void OutBufferWrapper::setVector(std::vector<Ch>* vec) NEKO_NOEXCEPT {
    if (vec != nullptr) {
        mVec = vec;
    } else {
        mVec = &mVecUnique;
        mVec->clear();
    }
}
inline void OutBufferWrapper::Put(Ch ch) NEKO_NOEXCEPT { mVec->push_back(ch); }
inline void OutBufferWrapper::Flush() NEKO_NOEXCEPT {}
inline const OutBufferWrapper::Ch* OutBufferWrapper::GetString() const NEKO_NOEXCEPT { return mVec->data(); }
inline std::size_t OutBufferWrapper::GetSize() const NEKO_NOEXCEPT { return mVec->size(); }
inline void OutBufferWrapper::Clear() NEKO_NOEXCEPT { mVec->clear(); }
} // namespace detail

template <typename SerializerT, typename T>
inline bool save(SerializerT& serializer, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT, typename T>
inline bool save(SerializerT& serializer, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const int8_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const uint8_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const int16_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const uint16_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const int32_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const uint32_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const int64_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const uint64_t value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const float value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const double value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const bool value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT, typename CharT, typename Traits, typename Alloc>
inline bool save(SerializerT& serializer, const std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, const char* value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}

template <typename SerializerT>
inline bool save(SerializerT& serializer, std::nullptr_t) NEKO_NOEXCEPT {
    return serializer.saveValue(nullptr);
}

#if NEKO_CPP_PLUS >= 17
template <typename SerializerT, typename CharT, typename Traits>
inline bool save(SerializerT& serializer, const std::basic_string_view<CharT, Traits>& value) NEKO_NOEXCEPT {
    return serializer.saveValue(value);
}
#endif

template <typename SerializerT, typename CharT, typename Traits, typename Alloc>
inline bool load(SerializerT& serializer, std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, int8_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, int16_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, int32_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, int64_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, uint8_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, uint16_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, uint32_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, uint64_t& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, float& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, double& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, bool& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT>
inline bool load(SerializerT& serializer, std::nullptr_t) NEKO_NOEXCEPT {
    return serializer.loadValue(nullptr);
}

template <typename SerializerT, typename T>
inline bool load(SerializerT& serializer, const SizeTag<T>& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

template <typename SerializerT, typename T>
inline bool load(SerializerT& serializer, const NameValuePair<T>& value) NEKO_NOEXCEPT {
    return serializer.loadValue(value);
}

NEKO_END_NAMESPACE