#pragma once

#include "nekoproto/global/global.hpp"

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nekoproto {

template <class T>
struct NameValuePair;

namespace detail {
template <typename T>
struct IsNameValuePair : std::false_type {};

template <typename T>
struct IsNameValuePair<NameValuePair<T>> : std::true_type {};
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
    static_assert(!detail::IsNameValuePair<std::remove_cvref_t<T>>::value,
                  "Cannot pair a name to a NameValuePair");
    auto operator=(NameValuePair const&) -> NameValuePair& = delete;

public:
    //! Constructs a new NameValuePair
    /*! @param name The name of the pair
        @param value The value to pair.  Ideally this should be an left-value reference so that
                the value can be both read and written.  If you pass an right-value reference,
                the NameValuePair will store a copy of it instead of a reference.  Thus you should
                only pass right-values in cases where this makes sense, such as the result of some
                size() call.
        @internal */
    NameValuePair(const char* name, const std::size_t nameLen, T&& value) noexcept
        : name(name),
          nameLen(nameLen),
          value(std::forward<T>(value)) {}
    NameValuePair(std::string_view name, T&& value) noexcept : name(name.data()),
                                                                    nameLen(name.size()),
                                                                    value(std::forward<T>(value)) {}
    const char* name;
    std::size_t nameLen;
    Type value;
};

template <class T>
inline auto makeNameValuePair(const char* name, T&& value) noexcept -> NameValuePair<T> {
    return {name, std::strlen(name), std::forward<T>(value)};
}

template <class T>
inline auto makeNameValuePair(const char* name, std::size_t len, T&& value) noexcept -> NameValuePair<T> {
    return {name, len, std::forward<T>(value)};
}

template <class T>
inline auto makeNameValuePair(const std::string& name, T&& value) noexcept -> NameValuePair<T> {
    return {name.c_str(), name.size(), std::forward<T>(value)};
}

template <class T>
inline auto makeNameValuePair(const std::string_view& name, T&& value) noexcept -> NameValuePair<T> {
    return {name, std::forward<T>(value)};
}

#define NEKO_PROTO_NAME_VALUE_PAIR(value) makeNameValuePair(#value, value)

namespace detail {
class OutBufferWrapper {
public:
    using Ch = char;

    explicit OutBufferWrapper() noexcept;
    explicit OutBufferWrapper(std::vector<Ch>& vec) noexcept;
    void setVector(std::vector<Ch>* vec) noexcept;
    void Put(Ch ch) noexcept;             // NOLINT(readability-identifier-naming)
    void Flush() noexcept;                // NOLINT(readability-identifier-naming)
    auto GetString() const noexcept -> const Ch*; // NOLINT(readability-identifier-naming)
    auto GetSize() const noexcept -> std::size_t; // NOLINT(readability-identifier-naming)
    void Clear() noexcept;                // NOLINT(readability-identifier-naming)

private:
    std::vector<Ch>* mVec;
    std::vector<Ch> mVecUnique; // maybe make it a static.
};

class ByteOutBufferWrapper {
public:
    using Ch = char;

    explicit ByteOutBufferWrapper(std::vector<std::byte>& vec) noexcept : mVec(&vec) {}
    void Put(Ch ch) noexcept { // NOLINT(readability-identifier-naming)
        mVec->push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    void Flush() noexcept {} // NOLINT(readability-identifier-naming)
    auto GetString() const noexcept -> const Ch* { // NOLINT(readability-identifier-naming)
        return reinterpret_cast<const Ch*>(mVec->data());
    }
    auto GetSize() const noexcept -> std::size_t { return mVec->size(); } // NOLINT(readability-identifier-naming)
    void Clear() noexcept { mVec->clear(); }                      // NOLINT(readability-identifier-naming)

private:
    std::vector<std::byte>* mVec;
};

inline OutBufferWrapper::OutBufferWrapper() noexcept : mVec(&mVecUnique) {}
inline OutBufferWrapper::OutBufferWrapper(std::vector<Ch>& vec) noexcept : mVec(&vec) {}
inline void OutBufferWrapper::setVector(std::vector<Ch>* vec) noexcept {
    if (vec != nullptr) {
        mVec = vec;
    } else {
        mVec = &mVecUnique;
        mVec->clear();
    }
}
inline void OutBufferWrapper::Put(Ch ch) noexcept { mVec->push_back(ch); }
inline void OutBufferWrapper::Flush() noexcept {}
inline auto OutBufferWrapper::GetString() const noexcept -> const OutBufferWrapper::Ch* { return mVec->data(); }
inline auto OutBufferWrapper::GetSize() const noexcept -> std::size_t { return mVec->size(); }
inline void OutBufferWrapper::Clear() noexcept { mVec->clear(); }
} // namespace detail

} // namespace nekoproto
