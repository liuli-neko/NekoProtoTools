/**
 * @file jsonrpc_traits.hpp
 * @author llhsdmd(llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ilias/task.hpp>

#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/serializer_base.hpp"
#include "nekoproto/serialization/types/types.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"

NEKO_BEGIN_NAMESPACE
namespace traits {
template <typename T>
concept Serializable = requires(NekoProto::JsonSerializer::OutputSerializer serializer, T value) {
    { serializer(value) };
};
template <typename T, class enable = void>
struct IsSerializable : std::false_type {};
template <Serializable T>
struct IsSerializable<T, void> : std::true_type {};
template <>
struct IsSerializable<void> : std::true_type {};

// Helper to apply a function `func` to each element of a tuple `t`
// `func` should be a generic callable (like a template lambda)
template <typename Tuple, typename Func>
constexpr void for_each_tuple_element(Tuple&& tuple, Func&& func) {
    // Get tuple size at compile time
    constexpr std::size_t Ns = std::tuple_size_v<std::remove_cvref_t<Tuple>>;

    // Generate index sequence 0, 1, ..., Ns-1
    // Use an immediately-invoked lambda to capture the index pack Is...
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        // Use a fold expression over the comma operator
        // For each index I in Is..., call func(std::get<I>(tuple))
        ((void)std::forward<Func>(func)(std::get<Is>(std::forward<Tuple>(tuple))), ...);
        // The (void) cast suppresses potential warnings about unused result of comma operator
        // std::forward preserves value category of tuple and func
    }(std::make_index_sequence<Ns>{});
}

// --- ConstexprString Implementation (C++20 NTTP) ---
template <std::size_t N>
struct ConstexprString {
    std::array<char, N + 1> data{};

    // consteval 构造函数，确保编译时创建
    consteval ConstexprString(const char* str) noexcept {
        std::size_t actualLen = 0;
        // 复制直到 null 或达到 N
        while (str[actualLen] != '\0' && actualLen < N) {
            data[actualLen] = str[actualLen];
            actualLen++;
        }
        // 如果有空间，添加 null 终止符 (string_view 不需要，但 c_str 可能需要)
        if (actualLen <= N) {
            data[actualLen] = '\0';
        }
        // C++20 要求 NTTP 类型的所有基类和非静态数据成员都是 public 的
        // 并且类型是结构性相等的 (structural equality) - 默认即可
    }

    // 比较运算符对 NTTP 至关重要
    constexpr auto operator<=>(const ConstexprString&) const = default;
    constexpr bool operator==(const ConstexprString&) const  = default;

    // 访问器
    [[nodiscard]]
    constexpr std::size_t size() const noexcept {
        return N;
    }
    [[nodiscard]]
    constexpr std::string_view view() const noexcept {
        return std::string_view(data.data(), N);
    }
    [[nodiscard]]
    constexpr const char* c_str() const noexcept { // NOLINT
        return data.data();
    }
};

// CTAD 推导指引，方便从字面量创建 ConstexprString (去掉末尾 '\0')
template <std::size_t N>
ConstexprString(const char (&)[N]) -> ConstexprString<N - 1>;

template <typename T, class enable = void>
struct TypeName {
    constexpr static std::string name() { return std::string(::NekoProto::_class_name<T>()); }
};

template <ConstexprString... ArgNames>
struct ArgNamesHelper {};
template <typename RawParamsType, std::size_t... Is, ConstexprString... ArgNames>
constexpr auto parameter_to_string(std::index_sequence<Is...> /*unused*/, ArgNamesHelper<ArgNames...> /*unused*/ = {})
    -> std::string {
    if constexpr (sizeof...(Is) == 0) {
        return "";
    } else if constexpr (sizeof...(ArgNames) != sizeof...(Is)) {
        constexpr std::size_t Nt = sizeof...(Is) - 1;
        return ((traits::TypeName<std::tuple_element_t<Is, RawParamsType>>::name() + (Is == Nt ? "" : ", ")) + ...);
    } else {
        constexpr std::size_t Nt                                  = sizeof...(Is) - 1;
        std::array<std::string_view, sizeof...(Is)> argNamesArray = {ArgNames.view()...};
        return ((traits::TypeName<std::tuple_element_t<Is, RawParamsType>>::name() + " " +
                 std::string(argNamesArray[Is]) + (Is == Nt ? "" : ", ")) +
                ...);
    }
};
template <>
struct TypeName<void, void> {
    constexpr static std::string name() { return "void"; }
};
template <typename T>
struct TypeName<std::optional<T>, void> {
    constexpr static std::string name() { return "std::optional<" + TypeName<T>::name() + ">"; }
};
template <typename... Args>
struct TypeName<std::variant<Args...>, void> {
    constexpr static std::string name() {
        return "std::variant<" + parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}) +
               ">";
    }
};
template <typename T, std::size_t N>
struct TypeName<std::array<T, N>, void> {
    constexpr static std::string name() { return "std::array<" + TypeName<T>::name() + ", " + std::to_string(N) + ">"; }
};
template <typename... Args>
struct TypeName<std::tuple<Args...>, void> {
    ;
    constexpr static std::string name() {
        return "std::tuple<" + parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}) +
               ">";
    }
};
template <typename T, typename T2>
struct TypeName<std::pair<T, T2>, void> {
    constexpr static std::string name() {
        return "std::pair<" + TypeName<T>::name() + ", " + TypeName<T2>::name() + ">";
    }
};
template <typename T>
struct TypeName<std::vector<T>, void> {
    constexpr static std::string name() { return "std::vector<" + TypeName<T>::name() + ">"; }
};
template <>
struct TypeName<std::string, void> {
    constexpr static std::string name() { return "std::string"; }
};
template <>
struct TypeName<std::u8string, void> {
    constexpr static std::string name() { return "std::u8string"; }
};
template <>
struct TypeName<std::u16string, void> {
    constexpr static std::string name() { return "std::u16string"; }
};
template <>
struct TypeName<std::u32string, void> {
    constexpr static std::string name() { return "std::u32string"; }
};
template <>
struct TypeName<std::byte, void> {
    constexpr static std::string name() { return "std::byte"; }
};
template <>
struct TypeName<std::nullptr_t, void> {
    constexpr static std::string name() { return "std::nullptr_t"; }
};
template <>
struct TypeName<bool, void> {
    constexpr static std::string name() { return "bool"; }
};
template <>
struct TypeName<int, void> {
    constexpr static std::string name() { return "int"; }
};
template <>
struct TypeName<unsigned int, void> {
    constexpr static std::string name() { return "unsigned int"; }
};
template <>
struct TypeName<long, void> {
    constexpr static std::string name() { return "long"; }
};
template <>
struct TypeName<unsigned long, void> {
    constexpr static std::string name() { return "unsigned long"; }
};
template <>
struct TypeName<long long, void> {
    constexpr static std::string name() { return "long long"; }
};
template <>
struct TypeName<unsigned long long, void> {
    constexpr static std::string name() { return "unsigned long long"; }
};
template <>
struct TypeName<float, void> {
    constexpr static std::string name() { return "float"; }
};
template <>
struct TypeName<double, void> {
    constexpr static std::string name() { return "double"; }
};
template <typename T>
struct TypeName<T*, void> {
    constexpr static std::string name() { return TypeName<T>::name() + "*"; }
};
template <typename T>
struct TypeName<T&, void> {
    constexpr static std::string name() { return TypeName<T>::name() + "&"; }
};
template <typename T>
struct TypeName<const T&, void> {
    constexpr static std::string name() { return "const " + TypeName<T>::name() + "&"; }
};
template <typename T>
struct TypeName<T&&, void> {
    constexpr static std::string name() { return TypeName<T>::name() + "&&"; }
};
template <typename T>
struct TypeName<const T, void> {
    constexpr static std::string name() { return "const " + TypeName<T>::name(); }
};

template <typename T, ConstexprString... ArgNames>
struct SerializerHelperObject {
    T& mTuple;
};
} // namespace traits

template <typename Serializer, typename T, traits::ConstexprString... ArgNames>
inline bool save(Serializer& sa, const traits::SerializerHelperObject<T, ArgNames...>& value) {
    if constexpr (sizeof...(ArgNames) > 0) {
        bool ret                                                        = sa.startObject(sizeof...(ArgNames));
        std::array<std::string_view, sizeof...(ArgNames)> argNamesArray = {ArgNames.view()...};
        ret = [&sa, &value, &argNamesArray]<std::size_t... Is>(std::index_sequence<Is...>) {
            return ((sa(make_name_value_pair(argNamesArray[Is], std::get<Is>(value.mTuple)))) && ...);
        }(std::make_index_sequence<sizeof...(ArgNames)>{});
        return ret && sa.endObject();
    } else {
        return sa(value.mTuple);
    }
}

template <typename Serializer, typename T, traits::ConstexprString... ArgNames>
inline bool load(Serializer& sa, traits::SerializerHelperObject<T, ArgNames...>& value) {
    if (sa.isObject()) {
        if constexpr (sizeof...(ArgNames) > 0) {
            bool ret = sa.startNode();
            if (ret) {
                std::array<std::string_view, sizeof...(ArgNames)> argNamesArray = {ArgNames.view()...};
                ret = [&sa, &value, &argNamesArray]<std::size_t... Is>(std::index_sequence<Is...>) {
                    return ((sa(make_name_value_pair(argNamesArray[Is], std::get<Is>(value.mTuple)))) && ...);
                }(std::make_index_sequence<sizeof...(ArgNames)>{});
            }
            if (sa.finishNode() && ret) {
                return ret;
            }
            sa.rollbackItem();
        }
        return false;
    }
    return sa(value.mTuple);
}

template <typename T, traits::ConstexprString... ArgNames>
struct is_minimal_serializable<traits::SerializerHelperObject<T, ArgNames...>, void> : std::true_type {};
NEKO_END_NAMESPACE