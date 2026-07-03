#pragma once

#include "nekoproto/global/global.hpp"

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE

template <class T>
struct NameValuePair;

namespace detail {
template <typename T>
struct is_name_value_pair : std::false_type {};

template <typename T>
struct is_name_value_pair<NameValuePair<T>> : std::true_type {};
} // namespace detail

template <class T>
struct NameValuePair {
private:
    // If we get passed an array, keep the type as is, otherwise store
    // a reference if we were passed an left value reference, else copy the value
    using Type = typename std::conditional<
        std::is_array<typename std::remove_reference<T>::type>::value, typename std::remove_cv<T>::type,
        typename std::conditional<std::is_lvalue_reference<T>::value, T, typename std::decay<T>::type>::type>::type;
    // prevent nested name value pair
    static_assert(!detail::is_name_value_pair<std::remove_cvref_t<T>>::value,
                  "Cannot pair a name to a NameValuePair");
    NameValuePair& operator=(NameValuePair const&) = delete;

public:
    //! Constructs a new NameValuePair
    /*! @param name The name of the pair
        @param value The value to pair.  Ideally this should be an left-value reference so that
                the value can be both read and written.  If you pass an right-value reference,
                the NameValuePair will store a copy of it instead of a reference.  Thus you should
                only pass right-values in cases where this makes sense, such as the result of some
                size() call.
        @internal */
    NameValuePair(const char* name, const std::size_t nameLen, T&& value) NEKO_NOEXCEPT
        : name(name),
          nameLen(nameLen),
          value(std::forward<T>(value)) {}
    NameValuePair(std::string_view name, T&& value) NEKO_NOEXCEPT : name(name.data()),
                                                                    nameLen(name.size()),
                                                                    value(std::forward<T>(value)) {}
    const char* name;
    std::size_t nameLen;
    Type value;
};

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

template <class T>
inline NameValuePair<T> make_name_value_pair(const std::string_view& name, T&& value) NEKO_NOEXCEPT {
    return {name, std::forward<T>(value)};
}

#define NEKO_PROTO_NAME_VALUE_PAIR(value) make_name_value_pair(#value, value)

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

class ByteOutBufferWrapper {
public:
    using Ch = char;

    explicit ByteOutBufferWrapper(std::vector<std::byte>& vec) NEKO_NOEXCEPT : mVec(&vec) {}
    void Put(Ch ch) NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
        mVec->push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    void Flush() NEKO_NOEXCEPT {} // NOLINT(readability-identifier-naming)
    const Ch* GetString() const NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
        return reinterpret_cast<const Ch*>(mVec->data());
    }
    std::size_t GetSize() const NEKO_NOEXCEPT { return mVec->size(); } // NOLINT(readability-identifier-naming)
    void Clear() NEKO_NOEXCEPT { mVec->clear(); }                      // NOLINT(readability-identifier-naming)

private:
    std::vector<std::byte>* mVec;
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

NEKO_END_NAMESPACE
