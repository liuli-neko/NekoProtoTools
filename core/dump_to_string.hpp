#pragma once

#include "serializer_base.hpp"

#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <string_view>
#include <variant>
#endif
#ifndef NEKO_SERIALIZABLE_TO_STRING_ENABLE
#define NEKO_SERIALIZABLE_TO_STRING_ENABLE 1
#endif

NEKO_BEGIN_NAMESPACE

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
};

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
struct FormatStringCovert<
    T, typename std::enable_if<has_to_string<T>::value && !std::is_enum<T>::value && !std::is_same<T, double>::value &&
                               !std::is_same<T, float>::value>::type> {
    static std::string toString(const char* name, const size_t len, const T& p) {
        if (len == 0)
            return std::to_string(p);
        return std::string(name, len) + std::string(" = ") + std::to_string(p);
    }
};

template <typename T>
struct FormatStringCovert<
    T, typename std::enable_if<std::is_same<T, double>::value || std::is_same<T, float>::value>::type> {
    static std::string toString(const char* name, const size_t len, const T& p) {
        std::ostringstream oss;
        if (len == 0) {
            oss << p;
            return oss.str();
        }
        oss << std::string(name, len) << " = " << p;
        return oss.str();
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