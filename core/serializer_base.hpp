/**
 * @file cc_serializer_base.hpp
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
 *
 *     JsonSerializer serializer;
 *     std::vector<char> data;
 *     serializer.startSerialize(&data);
 *     obj.serialize(serializer);
 *     serializer.endSerialize();
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

#include <bitset>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#include <string_view>
#include <variant>
#endif

#include "private/global.hpp"

NEKO_BEGIN_NAMESPACE

namespace {
#if NEKO_CPP_PLUS >= 17
template <size_t N, typename SerializerT, typename TupleT, std::size_t... Indices>
inline bool unfold_function_imp1(SerializerT& serializer, const std::array<std::string_view, N>& names,
                                 const TupleT& value, std::index_sequence<Indices...>) NEKO_NOEXCEPT {
    return ((serializer.get(names[Indices].data(), names[Indices].size(), std::get<Indices>(value)) && true) + ...) ==
           N;
}
template <size_t N, typename SerializerT, typename TupleT, std::size_t... Indices>
inline bool unfold_function_imp2(SerializerT& serializer, const std::array<std::string_view, N>& names,
                                 const TupleT& value, std::index_sequence<Indices...>) NEKO_NOEXCEPT {
    return ((serializer.insert(names[Indices].data(), names[Indices].size(), std::get<Indices>(value)) && true) +
            ...) == N;
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
    return serializer.insert(names + namesVec[i].first, namesVec[i].second, value);
}

template <typename SerializerT, typename T, typename... Args>
inline bool unfold_function_imp1(SerializerT& serializer, const char* names,
                                 const std::vector<std::pair<size_t, size_t>>& namesVec, int i, const T& value,
                                 const Args&... args) NEKO_NOEXCEPT {
    bool ret = 0;
    NEKO_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    ret = serializer.insert(names + namesVec[i].first, namesVec[i].second, value);

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
    return serializer.get(names + namesVec[i].first, namesVec[i].second, &value);
}

template <typename SerializerT, typename T, typename... Args>
inline bool unfold_function_imp2(SerializerT& serializer, const char* names,
                                 const std::vector<std::pair<size_t, size_t>>& namesVec, int i, T& value,
                                 Args&... args) NEKO_NOEXCEPT {
    bool ret = 0;
    NEKO_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    ret = serializer.get(names + namesVec[i].first, namesVec[i].second, &value);

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

template <typename T, class enable = void>
struct FormatStringCovert;

template <typename T, class enable>
struct FormatStringCovert {
    static std::string toString(const char* name, const size_t len, const T& p) {
        if (len == 0)
            return std::string(_class_name<T>()) + "(" + std::to_string((size_t)&p) + ")";
        return std::string(name, len) + std::string(" = ") + std::string(_class_name<T>()) + "(" +
               std::to_string((size_t)&p) + ")";
    }
};

struct SerializableFormater {
    std::string str;
    template <typename T>
    bool insert(const char* name, const size_t len, const T& value) NEKO_NOEXCEPT {
        auto ret = FormatStringCovert<T>::toString(name, len, value) + ", ";
        str += ret;
        return true;
    }
    template <typename T>
    bool get(const char* name, const size_t len, T* value) NEKO_NOEXCEPT {
        return true;
    }
};

template <typename T, class enable = void>
struct can_serialize : std::false_type {};

template <typename T>
struct can_serialize<
    T, typename std::enable_if<
           std::is_same<decltype(std::declval<T>().serialize(std::declval<SerializableFormater&>())),
                        decltype(std::declval<T>().deserialize(std::declval<SerializableFormater&>()))>::value>::type>
    : std::true_type {};

template <typename T>
std::string SerializableToString(const T& p) {
    SerializableFormater formater;
    formater.str = std::string(_class_name<T>()) + std::string("{");
    p.serialize(formater);
    formater.str.pop_back();
    formater.str.back() = '}';
    return formater.str;
}

template <typename T>
struct has_to_string {
    template <typename U>
    static auto test(int) -> decltype(std::to_string(std::declval<U>()), std::true_type());

    template <typename>
    static auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
struct FormatStringCovert<T, typename std::enable_if<has_to_string<T>::value && !std::is_enum<T>::value>::type> {
    static std::string toString(const char* name, const size_t len, const T& p) {
        if (len == 0)
            return std::to_string(p);
        return std::string(name, len) + std::string(" = ") + std::to_string(p);
    }
};

template <typename T>
struct FormatStringCovert<T, typename std::enable_if<can_serialize<T>::value>::type> {
    static std::string toString(const char* name, const size_t len, const T& p) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += SerializableToString(p);
        return ret;
    }
};

template <>
struct FormatStringCovert<std::string, void> {
    static std::string toString(const char* name, const size_t len, const std::string& p) {
        if (len == 0)
            return std::string("\"") + p + std::string("\"");
        return std::string(name, len) + std::string(" = \"") + p + "\"";
    }
};

template <>
struct FormatStringCovert<const char*, void> {
    static std::string toString(const char* name, const size_t len, const char* p) {
        if (len == 0)
            return std::string("\"") + p + std::string("\"");
        return std::string(name, len) + std::string(" = \"") + std::string(p) + "\"";
    }
    static std::string toString(const char* name, const size_t len, const char* p, const size_t len1) {
        if (len == 0)
            return std::string("\"") + std::string(p, len) + std::string("\"");
        return std::string(name, len) + std::string(" = \"") + std::string(p, len1) + "\"";
    }
};

template <>
struct FormatStringCovert<char*, void> {
    static std::string toString(const char* name, const size_t len, const char* p) {
        if (len == 0)
            return std::string("\"") + p + std::string("\"");
        return std::string(name, len) + std::string(" = \"") + std::string(p) + "\"";
    }
    static std::string toString(const char* name, const size_t len, const char* p, const size_t len1) {
        if (len == 0)
            return std::string("\"") + std::string(p, len) + std::string("\"");
        return std::string(name, len) + std::string(" = \"") + std::string(p, len1) + "\"";
    }
};

#if NEKO_CPP_PLUS >= 17
template <>
struct FormatStringCovert<std::string_view, void> {
    static std::string toString(const char* name, const size_t len, std::string_view p) {
        if (len == 0)
            return std::string("\"") + std::string(p) + std::string("\"");
        return std::string(name, len) + std::string(" = \"") + std::string(p) + "\"";
    }
};

template <typename T>
struct FormatStringCovert<std::optional<T>, void> {
    static std::string toString(const char* name, const size_t len, const std::optional<T>& p) {
        if (p.has_value()) {
            return FormatStringCovert<T>::toString(name, len, *p);
        } else {
            std::string ret;
            if (len > 0)
                ret = std::string(name, len) + " = ";
            return ret + "null";
        }
    }
};

template <typename... Ts>
struct FormatStringCovert<std::variant<Ts...>, void> {
    template <typename T, size_t N>
    static std::string toStringImp(const char* name, const size_t len, const std::variant<Ts...>& value) {
        if (value.index() != N)
            return "";
        return FormatStringCovert<T>::toString(name, len, std::get<N>(value));
    }

    template <size_t... Ns>
    static std::string unfoldToString(const char* name, const size_t len, const std::variant<Ts...>& value,
                                      std::index_sequence<Ns...>) {
        return (toStringImp<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(name, len, value) + ...);
    }

    static std::string toString(const char* name, const size_t len, const std::variant<Ts...>& p) {
        return unfoldToString(name, len, p, std::make_index_sequence<sizeof...(Ts)>());
    }
};
#endif

template <typename T>
struct FormatStringCovert<const T*, typename std::enable_if<std::is_void<T>::value>::type> {
    static std::string toString(const char* name, const size_t len, const T* p) {
        if (len == 0)
            return std::string("p(") + std::string("0x") + std::to_string((size_t)p) + ")";
        return std::string(name, len) + std::string(" = p(") + std::string("0x") + std::to_string((size_t)p) + ")";
    }
};

template <typename T>
struct FormatStringCovert<const T*, typename std::enable_if<can_serialize<T>::vaule>::type> {
    static std::string toString(const char* name, const size_t len, const T& p) {
        std::string ret;
        if (len == 0)
            ret = std::string(name, len) + std::string(" = ");
        return ret += SerializableToString(p);
    }
};

template <typename T>
struct FormatStringCovert<const T*,
                          typename std::enable_if<!std::is_void<T>::value && !std::is_same<T, char>::value>::type> {
    static std::string toString(const char* name, const size_t len, const T* p) {
        if (len == 0)
            return FormatStringCovert<T>::toString(name, len, *p);
        return std::string("*") + FormatStringCovert<T>::toString(name, len, *p);
    }
};

template <typename T>
struct FormatStringCovert<std::vector<T>, void> {
    static std::string toString(const char* name, const size_t len, const std::vector<T>& p) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += "vector[";
        for (auto& v : p) {
            ret += FormatStringCovert<T>::toString(nullptr, 0, v) + std::string(", ");
        }
        if (ret.back() == ' ') {
            ret.pop_back();
            ret.back() = ']';
        } else {
            ret.push_back(']');
        }
        return ret;
    }
};

template <typename T, typename U>
struct FormatStringCovert<std::map<T, U>, void> {
    static std::string toString(const char* name, const size_t len, const std::map<T, U>& p) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += "map{";
        for (auto& v : p) {
            ret += FormatStringCovert<T>::toString(nullptr, 0, v.first) + std::string(":");
            ret += FormatStringCovert<U>::toString(nullptr, 0, v.second) + std::string(", ");
        }
        if (ret.back() == ' ') {
            ret.pop_back();
            ret.back() = '}';
        } else {
            ret.push_back('}');
        }
        return ret;
    }
};

template <typename T>
struct FormatStringCovert<std::list<T>, void> {
    static std::string toString(const char* name, const size_t len, const std::list<T>& p) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += "list[";
        for (auto& v : p) {
            ret += FormatStringCovert<T>::toString(nullptr, 0, v) + std::string(", ");
        }
        if (ret.back() == ' ') {
            ret.pop_back();
            ret.back() = ']';
        } else {
            ret.push_back(']');
        }
        return ret;
    }
};

template <typename T>
struct FormatStringCovert<std::set<T>, void> {
    static std::string toString(const char* name, const size_t len, const std::set<T>& p) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += "set{";
        for (auto& v : p) {
            ret += FormatStringCovert<T>::toString(nullptr, 0, v) + std::string(", ");
        }
        if (ret.back() == ' ') {
            ret.pop_back();
            ret.back() = '}';
        } else {
            ret.push_back('}');
        }
        return ret;
    }
};

template <typename T, size_t N>
struct FormatStringCovert<std::array<T, N>, void> {
    static std::string toString(const char* name, const size_t len, const std::array<T, N>& p) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += "array{";
        for (auto& v : p) {
            ret += FormatStringCovert<T>::toString(nullptr, 0, v) + std::string(", ");
        }
        if (ret.back() == ' ') {
            ret.pop_back();
            ret.back() = '}';
        } else {
            ret.push_back('}');
        }
        return ret;
    }
};

template <typename... Args>
struct FormatStringCovert<std::tuple<Args...>, void> {
    template <size_t N, typename T>
    struct UnfoldTuple {
        using Type = typename std::tuple_element<N - 1, std::tuple<Args...>>::type;
        static std::string unfoldTuple(const T& value) {
            return UnfoldTuple<N - 1, T>::unfoldTuple(value) + std::string(", ") +
                   FormatStringCovert<Type>::toString(nullptr, 0, std::get<N - 1>(value));
        }
    };
    template <typename T>
    struct UnfoldTuple<1, T> {
        using Type = typename std::tuple_element<0, std::tuple<Args...>>::type;
        static std::string unfoldTuple(const T& value) {
            return FormatStringCovert<Type>::toString(nullptr, 0, std::get<0>(value));
        }
    };
    static std::string toString(const char* name, const size_t len, const std::tuple<Args...>& p) {
        std::string ret;
        if (len > 0)
            ret = std::string(name, len) + std::string(" = ");
        ret += "tuple(";
        return ret + UnfoldTuple<sizeof...(Args), std::tuple<Args...>>::unfoldTuple(p) + ")";
    }
};

NEKO_END_NAMESPACE

#if NEKO_CPP_PLUS < 17
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