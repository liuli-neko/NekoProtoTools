/**
 * @file serializer_base.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief serializer base
 *
 * @mainpage NekoProtoTools
 *
 * @section intro_sec Introduction
 * A simple c++ header to generate serializer and deserializer function for class.
 * This file provides the macro NEKO_SERIALIZER to generation, and this function is
 * a template function, can't be used directly. you need provide compliant requirements
 * Serializer. a default JsonSerializer is provided in proto_json_serializer.hpp.
 *
 * @section usage_sec Usage
 * you only need to use NEKO_SERIALIZER in your class, and specify the members you want to
 * serialize.
 *
 * @section example_sec Example
 * @code {.c++}
 *
 * class MyClass {
 *     std::string name;
 *     int age;
 *     std::string address;
 *
 *     NEKO_SERIALIZER(name, age, address)
 * };
 *
 * int main()
 *     MyClass obj;
 *     obj.name = "Alice";
 *     obj.age = 18;
 *     obj.address = "Zh";
 *     std::vector<char> data;
 *     {
 *          JsonSerializer::OutputSerializer out(data);
 *          out(obj);
 *     }
 *     data.push_back(0);
 *     std::cout << data.data() << std::endl;
 *
 *     return 0;
 * }
 * @endcode
 *
 *
 * @par license
 *  GPL-2.0 license
 *
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024 by llhsdmd
 *
 */
#pragma once

#include <array>
#include <string>
#include <vector>

#include "private/global.hpp"
#include "private/helpers.hpp"
#include "private/log.hpp"

NEKO_BEGIN_NAMESPACE
// NOLINTBEGIN
namespace detail {
#if NEKO_CPP_PLUS >= 17
inline constexpr int _members_size(std::string_view names) NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
    int count         = 0;
    std::size_t begin = 0;
    std::size_t end   = names.find_first_of(',', begin + 1);
    while (end != std::string::npos) {
        begin = end + 1;
        end   = names.find_first_of(',', begin + 1);
        count++;
    }
    if (begin != names.size()) {
        count++;
    }
    return count;
}
template <int N>
inline constexpr std::array<std::string_view, N>
_parse_names(std::string_view names) NEKO_NOEXCEPT { // NOLINT(readability-identifier-naming)
    std::array<std::string_view, N> namesVec;
    std::size_t begin = 0;
    std::size_t end   = names.find_first_of(',', begin + 1);
    int icount        = 0;
    while (end != std::string_view::npos) {
        std::size_t bbegin     = names.find_first_not_of(' ', begin);
        std::size_t eend       = names.find_last_not_of(' ', end);
        std::string_view token = names.substr(bbegin, eend - bbegin);
        namesVec[icount++]     = token;
        begin                  = end + 1;
        end                    = names.find_first_of(',', begin + 1);
    }
    if (begin != names.size()) {
        std::size_t bbegin     = names.find_first_not_of(' ', begin);
        std::size_t eend       = names.back() == ' ' ? names.find_last_not_of(' ') : names.size();
        std::string_view token = names.substr(bbegin, eend - bbegin);
        namesVec[N - 1]        = token;
    }
    return namesVec;
}
#else
struct neko_string_view {
    const char* mData;
    std::size_t mSize;
    std::size_t size() const { return mSize; }
    const char* data() const { return mData; }
};
inline std::vector<neko_string_view> _parse_names(const char* names) NEKO_NOEXCEPT {
    std::string namesStr(names);
    std::vector<neko_string_view> namesVec;
    std::size_t begin = 0;
    std::size_t end   = namesStr.find_first_of(',', begin + 1);
    while (end != std::string::npos) {
        std::size_t bbegin = namesStr.find_first_not_of(' ', begin);
        std::size_t eend   = namesStr.find_last_not_of(' ', end);
        namesVec.push_back({names + bbegin, eend - bbegin});
        begin = end + 1;
        end   = namesStr.find_first_of(',', begin + 1);
    }
    if (begin != namesStr.size()) {
        std::size_t bbegin = namesStr.find_first_not_of(' ', begin);
        std::size_t eend   = namesStr.back() == ' ' ? namesStr.find_last_not_of(' ') : namesStr.size();
        namesVec.push_back({names + bbegin, eend - bbegin});
    }
    return namesVec;
}
#endif
template <typename SerializerT, typename NamesT, typename T>
inline bool unfold_function_imp(SerializerT& serializer, const NamesT& namesVec, int nameIdx, T&& value) NEKO_NOEXCEPT {
    NEKO_ASSERT(nameIdx < (int)namesVec.size(), "serializer", "unfoldFunctionImp: index out of range");
    return serializer(make_name_value_pair(namesVec[nameIdx].data(), namesVec[nameIdx].size(), std::forward<T>(value)));
}

template <typename SerializerT, typename NamesT, typename T, typename... Args>
inline bool unfold_function_imp(SerializerT& serializer, const NamesT& namesVec, int nameIdx, T&& value,
                                Args&&... args) NEKO_NOEXCEPT {
    bool ret = 0;
    NEKO_ASSERT(nameIdx < (int)namesVec.size(), "serializer", "unfoldFunctionImp: index out of range");
    ret = serializer(make_name_value_pair(namesVec[nameIdx].data(), namesVec[nameIdx].size(), std::forward<T>(value)));

    return unfold_function_imp<SerializerT>(serializer, namesVec, nameIdx + 1, args...) && ret;
}
template <typename SerializerT, typename NamesT, typename... Args>
inline bool _unfold_function(SerializerT& serializer, const NamesT& namesVec, // NOLINT(readability-identifier-naming)
                             Args&&... args) NEKO_NOEXCEPT {
    int index = 0;
    return unfold_function_imp<SerializerT>(serializer, namesVec, index, args...);
}
#if NEKO_CPP_PLUS >= 17
NEKO_CONSTEXPR_FUNC int count_parameter_size(std::string_view view) { return view.empty() ? 0 : 1; }
NEKO_CONSTEXPR_FUNC int count_parameter_size() { return 0; }
#endif
} // namespace detail

NEKO_END_NAMESPACE
// NOLINTEND

#if NEKO_CPP_PLUS < 17
/**
 * @brief generate serialize and deserialize functions for a class.
 *
 * Give all member variables that require serialization and deserialization support as parameters to the macro,
 * this macro will generate the serialize and deserialize functions for the class to process all given members.
 *
 * @param ...Args
 * member variables that require serialization and deserialization support.
 *
 * @note
 * Don't use this macro duplicate times. because it will generate same functions for class.
 * This function cannot directly serialize members and needs to be used in conjunction with supported serializers.
 * Please refer to the default JSON serializer implementation for the implementation specifications of the serializer.
 *
 * @example
 * class MyClass {
 *  ...
 *  NEKO_SERIALIZER(a, b, c)
 * private:
 *  int a;
 *  std::string b;
 *  std::vector<int> c;
 * };
 */
#define NEKO_SERIALIZER(...)                                                                                           \
public:                                                                                                                \
    template <typename SerializerT>                                                                                    \
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {                                                      \
        static auto _kNames_ = NEKO_NAMESPACE::detail::_parse_names(#__VA_ARGS__);                                     \
        return NEKO_NAMESPACE::detail::_unfold_function<SerializerT>(serializer, _kNames_, __VA_ARGS__);               \
    }                                                                                                                  \
    template <typename SerializerT>                                                                                    \
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {                                                            \
        static auto _kNames_ = NEKO_NAMESPACE::detail::_parse_names(#__VA_ARGS__);                                     \
        return NEKO_NAMESPACE::detail::_unfold_function<SerializerT>(serializer, _kNames_, __VA_ARGS__);               \
    }
#else
/**
 * @brief generate serialize and deserialize functions for a class.
 *
 * Give all member variables that require serialization and deserialization support as parameters to the macro,
 * this macro will generate the serialize and deserialize functions for the class to process all given members.
 *
 * @param ...Args
 * member variables that require serialization and deserialization support.
 *
 * @note
 * Don't use this macro duplicate times. because it will generate same functions for class.
 * This function cannot directly serialize members and needs to be used in conjunction with supported serializers.
 * Please refer to the default JSON serializer implementation for the implementation specifications of the serializer.
 *
 * @example
 * class MyClass {
 *  ...
 *  NEKO_SERIALIZER(a, b, c)
 * private:
 *  int a;
 *  std::string b;
 *  std::vector<int> c;
 * };
 */
#ifdef __GNUC__
#define NEKO_SERIALIZER(...)                                                                                           \
private:                                                                                                               \
public:                                                                                                                \
    template <typename SerializerT>                                                                                    \
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {                                                      \
        if constexpr (NEKO_NAMESPACE::detail::count_parameter_size(#__VA_ARGS__)) {                                    \
            constexpr uint32_t _kSize_ = NEKO_NAMESPACE::detail::_members_size(#__VA_ARGS__);                          \
            constexpr std::array<std::string_view, _kSize_> _kNames_ =                                                 \
                NEKO_NAMESPACE::detail::_parse_names<_kSize_>(#__VA_ARGS__);                                           \
            return NEKO_NAMESPACE::detail::_unfold_function<SerializerT>(serializer,                                   \
                                                                         _kNames_ __VA_OPT__(, )##__VA_ARGS__);        \
        } else {                                                                                                       \
            return true;                                                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    template <typename SerializerT>                                                                                    \
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {                                                            \
        if constexpr (NEKO_NAMESPACE::detail::count_parameter_size(#__VA_ARGS__)) {                                    \
            constexpr uint32_t _kSize_ = NEKO_NAMESPACE::detail::_members_size(#__VA_ARGS__);                          \
            constexpr std::array<std::string_view, _kSize_> _kNames_ =                                                 \
                NEKO_NAMESPACE::detail::_parse_names<_kSize_>(#__VA_ARGS__);                                           \
            return NEKO_NAMESPACE::detail::_unfold_function<SerializerT>(serializer,                                   \
                                                                         _kNames_ __VA_OPT__(, )##__VA_ARGS__);        \
        } else {                                                                                                       \
            return true;                                                                                               \
        }                                                                                                              \
    }
#else
#define NEKO_SERIALIZER(...)                                                                                           \
private:                                                                                                               \
public:                                                                                                                \
    template <typename SerializerT>                                                                                    \
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {                                                      \
        if constexpr (NEKO_NAMESPACE::detail::count_parameter_size(#__VA_ARGS__)) {                                    \
            constexpr uint32_t _kSize_ = NEKO_NAMESPACE::detail::_members_size(#__VA_ARGS__);                          \
            constexpr std::array<std::string_view, _kSize_> _kNames_ =                                                 \
                NEKO_NAMESPACE::detail::_parse_names<_kSize_>(#__VA_ARGS__);                                           \
            return NEKO_NAMESPACE::detail::_unfold_function<SerializerT>(serializer, _kNames_, ##__VA_ARGS__);         \
        } else {                                                                                                       \
            return true;                                                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    template <typename SerializerT>                                                                                    \
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {                                                            \
        if constexpr (NEKO_NAMESPACE::detail::count_parameter_size(#__VA_ARGS__)) {                                    \
            constexpr uint32_t _kSize_ = NEKO_NAMESPACE::detail::_members_size(#__VA_ARGS__);                          \
            constexpr std::array<std::string_view, _kSize_> _kNames_ =                                                 \
                NEKO_NAMESPACE::detail::_parse_names<_kSize_>(#__VA_ARGS__);                                           \
            return NEKO_NAMESPACE::detail::_unfold_function<SerializerT>(serializer, _kNames_, ##__VA_ARGS__);         \
        } else {                                                                                                       \
            return true;                                                                                               \
        }                                                                                                              \
    }
#endif
#endif