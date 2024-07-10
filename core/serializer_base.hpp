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

#include <string>
#include <vector>

#include "private/global.hpp"
#include "private/helpers.hpp"

NEKO_BEGIN_NAMESPACE
namespace {
#if NEKO_CPP_PLUS >= 17
template <size_t N, typename SerializerT, typename TupleT, std::size_t... Indices>
inline bool unfold_function_imp1(SerializerT& serializer, const std::array<std::string_view, N>& names, TupleT&& value,
                                 std::index_sequence<Indices...>) NEKO_NOEXCEPT {
    return ((serializer(makeNameValuePair(names[Indices], traits::dereference(std::get<Indices>(value)))) && true) +
            ...) == N;
}

template <size_t N, typename SerializerT, typename TupleT, std::size_t... Indices>
inline bool unfold_function_imp2(SerializerT& serializer, const std::array<std::string_view, N>& names,
                                 const TupleT& value, std::index_sequence<Indices...>) NEKO_NOEXCEPT {
    return ((serializer(makeNameValuePair(names[Indices], std::get<Indices>(value))) && true) + ...) == N;
}

inline constexpr int _members_size(std::string_view names) NEKO_NOEXCEPT {
    int count  = 0;
    auto begin = 0;
    auto end   = names.find_first_of(',', begin + 1);
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
inline constexpr std::array<std::string_view, N> _parse_names(std::string_view names) NEKO_NOEXCEPT {
    std::array<std::string_view, N> namesVec;
    auto begin = 0;
    auto end   = names.find_first_of(',', begin + 1);
    int i      = 0;
    while (end != std::string_view::npos) {
        auto bbegin            = names.find_first_not_of(' ', begin);
        auto eend              = names.find_last_not_of(' ', end);
        std::string_view token = names.substr(bbegin, eend - bbegin);
        namesVec[i++]          = token;
        begin                  = end + 1;
        end                    = names.find_first_of(',', begin + 1);
    }
    if (begin != names.size()) {
        auto bbegin            = names.find_first_not_of(' ', begin);
        auto eend              = names.back() == ' ' ? names.find_last_not_of(' ') : names.size();
        std::string_view token = names.substr(bbegin, eend - bbegin);
        namesVec[N - 1]        = token;
    }
    return std::move(namesVec);
}

template <int N, typename SerializerT, typename... Args>
inline bool _unfold_function1(SerializerT& serializer, const std::array<std::string_view, N>& namesVec,
                              Args&... args) NEKO_NOEXCEPT {
    return unfold_function_imp1<N, SerializerT>(serializer, namesVec, std::make_tuple((&args)...),
                                                std::index_sequence_for<Args...>());
}

template <int N, typename SerializerT, typename... Args>
inline bool _unfold_function2(SerializerT& serializer, const std::array<std::string_view, N>& namesVec,
                              const Args&... args) NEKO_NOEXCEPT {
    return unfold_function_imp2<N, SerializerT>(serializer, namesVec, std::make_tuple(args...),
                                                std::index_sequence_for<Args...>());
}

#else

inline std::vector<std::pair<size_t, size_t>> _parse_names(const char* names) NEKO_NOEXCEPT {
    std::string namesStr(names);
    std::vector<std::pair<size_t, size_t>> namesVec;
    auto begin = 0;
    auto end   = namesStr.find_first_of(',', begin + 1);
    while (end != std::string::npos) {
        auto bbegin = namesStr.find_first_not_of(' ', begin);
        auto eend   = namesStr.find_last_not_of(' ', end);
        namesVec.push_back(std::make_pair(bbegin, eend - bbegin));
        begin = end + 1;
        end   = namesStr.find_first_of(',', begin + 1);
    }
    if (begin != namesStr.size()) {
        auto bbegin = namesStr.find_first_not_of(' ', begin);
        auto eend   = namesStr.back() == ' ' ? namesStr.find_last_not_of(' ') : namesStr.size();
        namesVec.push_back(std::make_pair(bbegin, eend - bbegin));
    }
    return std::move(namesVec);
}

template <typename SerializerT, typename T>
inline bool unfold_function_imp1(SerializerT& serializer, const char* names,
                                 const std::vector<std::pair<size_t, size_t>>& namesVec, int i,
                                 const T& value) NEKO_NOEXCEPT {
    NEKO_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    return serializer(makeNameValuePair(names + namesVec[i].first, namesVec[i].second, value));
}

template <typename SerializerT, typename T, typename... Args>
inline bool unfold_function_imp1(SerializerT& serializer, const char* names,
                                 const std::vector<std::pair<size_t, size_t>>& namesVec, int i, const T& value,
                                 const Args&... args) NEKO_NOEXCEPT {
    bool ret = 0;
    NEKO_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    ret = serializer(makeNameValuePair(names + namesVec[i].first, namesVec[i].second, value));

    return unfold_function_imp1<SerializerT>(serializer, names, namesVec, i + 1, args...) && ret;
}
template <typename SerializerT, typename... Args>
inline bool _unfold_function1(SerializerT& serializer, const char* names,
                              const std::vector<std::pair<size_t, size_t>>& namesVec, Args&... args) NEKO_NOEXCEPT {
    int i = 0;
    return unfold_function_imp1<SerializerT>(serializer, names, namesVec, i, args...);
}

template <typename SerializerT, typename T>
inline bool unfold_function_imp2(SerializerT& serializer, const char* names,
                                 const std::vector<std::pair<size_t, size_t>>& namesVec, int i,
                                 T& value) NEKO_NOEXCEPT {
    NEKO_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    return serializer(makeNameValuePair(names + namesVec[i].first, namesVec[i].second, value));
}

template <typename SerializerT, typename T, typename... Args>
inline bool unfold_function_imp2(SerializerT& serializer, const char* names,
                                 const std::vector<std::pair<size_t, size_t>>& namesVec, int i, T& value,
                                 Args&... args) NEKO_NOEXCEPT {
    bool ret = 0;
    NEKO_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    ret = serializer(makeNameValuePair(names + namesVec[i].first, namesVec[i].second, value));

    return unfold_function_imp2<SerializerT>(serializer, names, namesVec, i + 1, args...) && ret;
}
template <typename SerializerT, typename... Args>
inline bool _unfold_function2(SerializerT& serializer, const char* names,
                              const std::vector<std::pair<size_t, size_t>>& namesVec, Args&... args) NEKO_NOEXCEPT {
    int i = 0;
    return unfold_function_imp2<SerializerT>(serializer, names, namesVec, i, args...);
}
#endif
} // namespace

NEKO_END_NAMESPACE

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
        static auto _kNames_ = NEKO_NAMESPACE::_parse_names(#__VA_ARGS__);                                             \
        return NEKO_NAMESPACE::_unfold_function1<SerializerT>(serializer, #__VA_ARGS__, _kNames_, __VA_ARGS__);        \
    }                                                                                                                  \
    template <typename SerializerT>                                                                                    \
    bool deserialize(SerializerT& serializer) NEKO_NOEXCEPT {                                                          \
        static auto _kNames_ = NEKO_NAMESPACE::_parse_names(#__VA_ARGS__);                                             \
        return NEKO_NAMESPACE::_unfold_function2<SerializerT>(serializer, #__VA_ARGS__, _kNames_, __VA_ARGS__);        \
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
#define NEKO_SERIALIZER(...)                                                                                           \
private:                                                                                                               \
public:                                                                                                                \
    template <typename SerializerT>                                                                                    \
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {                                                      \
        constexpr uint32_t _kSize_ = NEKO_NAMESPACE::_members_size(#__VA_ARGS__);                                      \
        constexpr std::array<std::string_view, _kSize_> _kNames_ =                                                     \
            NEKO_NAMESPACE::_parse_names<_kSize_>(#__VA_ARGS__);                                                       \
        return NEKO_NAMESPACE::_unfold_function2<_kSize_, SerializerT>(serializer, _kNames_, __VA_ARGS__);             \
    }                                                                                                                  \
    template <typename SerializerT>                                                                                    \
    bool deserialize(SerializerT& serializer) NEKO_NOEXCEPT {                                                          \
        constexpr uint32_t _kSize_ = NEKO_NAMESPACE::_members_size(#__VA_ARGS__);                                      \
        constexpr std::array<std::string_view, _kSize_> _kNames_ =                                                     \
            NEKO_NAMESPACE::_parse_names<_kSize_>(#__VA_ARGS__);                                                       \
        return NEKO_NAMESPACE::_unfold_function1<_kSize_, SerializerT>(serializer, _kNames_, __VA_ARGS__);             \
    }

#endif