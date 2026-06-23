#pragma once

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "nekoproto/rpc/traits.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename T, class enable = void>
class RpcMethodTraits;

template <typename Callable>
class RpcMethodTraits<Callable,
                      std::void_t<typename traits::function_traits<std::remove_cvref_t<Callable>>::return_type>>
    : public traits::RpcMethodTraitsUnpacker<std::remove_cvref_t<Callable>> {
private:
    using Traits   = traits::function_traits<std::remove_cvref_t<Callable>>;
    using Base     = traits::RpcMethodTraitsUnpacker<std::remove_cvref_t<Callable>>;

public:
    using RawReturnType = typename Base::RawReturnType;
    using RawParamsType = typename Base::RawParamsType;
    using FunctionType  = typename Base::FunctionType;

    constexpr static int NumParams = Base::NumParams;
};

template <typename T>
concept RpcMethodT = requires() { std::is_constructible_v<typename RpcMethodTraits<T>::FunctionType, T>; };

template <auto Ptr>
concept RpcMethodFuncT = requires() { typename RpcMethodTraits<decltype(Ptr)>::FunctionType; };

template <RpcMethodT T, ConstexprString... ArgNames>
class RpcMethodDynamic : public RpcMethodTraits<T> {
    static_assert(sizeof...(ArgNames) == 0 || RpcMethodTraits<T>::NumParams == sizeof...(ArgNames),
                  "RpcMethodDynamic: The number of parameters and names do not match.");

public:
    using RpcMethodMarker = void;
    using MethodType      = T;
    using MethodTraits    = RpcMethodTraits<T>;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;
    using ArgNamesHelper = traits::ArgNamesHelper<ArgNames...>;

    template <template <typename, ConstexprString...> class Template>
    using ApplyArgNames = Template<MethodTraits, ArgNames...>;

    constexpr static std::array<std::string_view, sizeof...(ArgNames)> argNames = {ArgNames.view()...};

    RpcMethodDynamic(std::string_view name, bool isNotification = false) noexcept
        : mDeclaredName(name), mName(name) {
        this->mIsNotification = isNotification;
        _updateDescription();
    }

    RpcMethodDynamic(std::string_view name, FunctionType func, bool isNotification = false) noexcept
        : RpcMethodDynamic(name, isNotification) {
        set(std::move(func));
    }

    RpcMethodDynamic(std::string_view name, CoroutinesFuncType func, bool isNotification = false) noexcept
        : RpcMethodDynamic(name, isNotification) {
        set(std::move(func));
    }

    template <typename U>
        requires std::is_convertible_v<U, FunctionType> && (!std::is_convertible_v<U, CoroutinesFuncType>)
    RpcMethodDynamic& operator=(U&& func) noexcept {
        if constexpr (std::is_bind_expression_v<U>) {
            return set(std::move(func));
        } else {
            return set(FunctionType(std::move(func)));
        }
    }

    template <typename U>
        requires std::is_convertible_v<U, CoroutinesFuncType>
    RpcMethodDynamic& operator=(U&& func) noexcept {
        if constexpr (std::is_bind_expression_v<U>) {
            return set(std::move(func));
        } else {
            return set(CoroutinesFuncType(std::move(func)));
        }
    }

    RpcMethodDynamic& set(FunctionType&& func) noexcept {
        this->mCoFunction = [func = std::move(func)](auto... args) mutable -> ilias::IoTask<RawReturnType> {
            if constexpr (std::is_void_v<RawReturnType>) {
                func(args...);
                co_return {};
            } else {
                co_return func(args...);
            }
        };
        return *this;
    }

    RpcMethodDynamic& set(CoroutinesFuncType&& func) noexcept {
        this->mCoFunction = std::move(func);
        return *this;
    }

    operator bool() const noexcept { return this->mCoFunction != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return this->mCoFunction == nullptr; }
    void clear() noexcept { this->mCoFunction = nullptr; }

    const std::string& name() const noexcept { return mName; }
    std::string_view name() noexcept { return mName; }
    std::string_view declaredName() const noexcept { return mDeclaredName; }

    void setRemoteName(std::string_view name) {
        mName = std::string(name);
        _updateDescription();
    }

    std::string description;

private:
    void _updateDescription() {
        description =
            std::string(traits::TypeName<RawReturnType>::name()) + " " + mName + "(" +
            std::string(traits::parameter_to_string<RawParamsType>(
                std::make_index_sequence<std::tuple_size_v<RawParamsType>>{}, traits::ArgNamesHelper<ArgNames...>{})) +
            ")";
    }

private:
    std::string mDeclaredName;
    std::string mName;
};

template <RpcMethodT T, ConstexprString MethodName, ConstexprString... ArgNames>
class RpcMethod : public RpcMethodDynamic<T, ArgNames...> {
public:
    RpcMethod() : RpcMethodDynamic<T, ArgNames...>(MethodName.view()) {}
    explicit RpcMethod(bool isNotification) : RpcMethodDynamic<T, ArgNames...>(MethodName.view(), isNotification) {}
    using RpcMethodDynamic<T, ArgNames...>::operator=;
    operator bool() const noexcept { return this->mCoFunction != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return this->mCoFunction == nullptr; }
};

template <auto Ptr, ConstexprString... ArgNames>
class RpcMethodF : public RpcMethodDynamic<decltype(Ptr), ArgNames...> {
public:
    constexpr static std::string_view MethodName = detail::func_nameof<Ptr>;
    RpcMethodF() : RpcMethodDynamic<decltype(Ptr), ArgNames...>(MethodName) { this->operator=(Ptr); }
    explicit RpcMethodF(bool isNotification) : RpcMethodDynamic<decltype(Ptr), ArgNames...>(MethodName, isNotification) {
        this->operator=(Ptr);
    }
    using RpcMethodDynamic<decltype(Ptr), ArgNames...>::operator=;
};

template <auto Ptr, typename T, ConstexprString... ArgNames>
class RpcMethodFN : public RpcMethodDynamic<decltype(Ptr), ArgNames...> {
    constexpr static std::string_view seq = ".";

public:
    constexpr static std::string_view MethodName = detail::join<detail::class_nameof<T>, seq, detail::func_nameof<Ptr>>;
    RpcMethodFN() : RpcMethodDynamic<decltype(Ptr), ArgNames...>(MethodName) { this->operator=(Ptr); }
    explicit RpcMethodFN(bool isNotification)
        : RpcMethodDynamic<decltype(Ptr), ArgNames...>(MethodName, isNotification) {
        this->operator=(Ptr);
    }
    using RpcMethodDynamic<decltype(Ptr), ArgNames...>::operator=;
};

template <typename T, class enable = void>
struct is_rpc_method : std::false_type {};

template <typename T>
struct is_rpc_method<T, std::void_t<typename std::remove_cvref_t<T>::RpcMethodMarker>> : std::true_type {};

template <typename T>
concept RpcMethodObject = is_rpc_method<T>::value;

} // namespace detail

using detail::RpcMethod;
using detail::RpcMethodF;
using detail::RpcMethodFN;

NEKO_END_NAMESPACE
