#pragma once

#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <ilias/io.hpp>

#include "nekoproto/rpc/properties.hpp"
#include "nekoproto/rpc/tags.hpp"
#include "nekoproto/rpc/traits.hpp"

namespace nekoproto {
namespace detail {

template <typename T, class enable = void>
class RpcMethodTraits;

template <typename Callable>
class RpcMethodTraits<Callable,
                      std::void_t<typename traits::FunctionTraits<std::remove_cvref_t<Callable>>::return_type>>
    : public traits::RpcMethodTraitsUnpacker<std::remove_cvref_t<Callable>> {
private:
    using Traits = traits::FunctionTraits<std::remove_cvref_t<Callable>>;
    using Base   = traits::RpcMethodTraitsUnpacker<std::remove_cvref_t<Callable>>;

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

struct RpcMethodData {
    template <size_t N>
    explicit RpcMethodData(std::array<std::string_view, N> arg_names, std::string_view name)
        : declared_name(name), remote_name(name) {
        this->arg_names.assign(arg_names.begin(), arg_names.end());
    }

    std::string declared_name;
    std::string remote_name;
    std::string rpc_prefix;
    bool rpc_no_prefix = false;
    std::string description;
    std::string rpc_version;
    std::vector<std::string> arg_names;
};

// Dynamic registration surface used by RpcServer::bindMethod(name, func) and
// by RpcClient::callRemote(name, ...). The method name is supplied at runtime,
// while optional argument names are stored as runtime metadata.
template <RpcMethodT T>
class RpcMethodDynamic : public RpcMethodTraits<T> {
public:
    using RpcMethodMarker = void;
    using MethodType      = T;
    using MethodTraits    = RpcMethodTraits<T>;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;

    template <size_t N>
        requires(N == 0 || N == MethodTraits::NumParams)
    RpcMethodDynamic(const std::array<std::string_view, N>& argNames, std::string_view name,
                     bool isNotification = false)
        : mMetadata(argNames, name) {
        this->mIsNotification = isNotification;
        updateSignature();
    }

    template <size_t N>
        requires(N == 0 || N == MethodTraits::NumParams)
    RpcMethodDynamic(const std::array<std::string_view, N>& argNames, std::string_view name, FunctionType func,
                     bool isNotification = false)
        : RpcMethodDynamic(argNames, name, isNotification) {
        set(std::move(func));
    }

    template <size_t N>
        requires(N == 0 || N == MethodTraits::NumParams)
    RpcMethodDynamic(const std::array<std::string_view, N>& argNames, std::string_view name, CoroutinesFuncType func,
                     bool isNotification = false)
        : RpcMethodDynamic(argNames, name, isNotification) {
        set(std::move(func));
    }

    template <typename U>
        requires std::is_convertible_v<U, FunctionType> && (!std::is_convertible_v<U, CoroutinesFuncType>)
    auto operator=(U&& func) -> RpcMethodDynamic& {
        if constexpr (std::is_bind_expression_v<U>) {
            return set(std::move(func));
        } else {
            return set(FunctionType(std::move(func)));
        }
    }

    template <typename U>
        requires std::is_convertible_v<U, CoroutinesFuncType>
    auto operator=(U&& func) -> RpcMethodDynamic& {
        if constexpr (std::is_bind_expression_v<U>) {
            return set(std::move(func));
        } else {
            return set(CoroutinesFuncType(std::move(func)));
        }
    }

    auto set(FunctionType&& func) -> RpcMethodDynamic& {
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

    auto set(CoroutinesFuncType&& func) -> RpcMethodDynamic& {
        this->mCoFunction = std::move(func);
        return *this;
    }

    operator bool() const noexcept { return this->mCoFunction != nullptr; }
    auto operator==(std::nullptr_t) const noexcept -> bool { return this->mCoFunction == nullptr; }
    void clear() noexcept { this->mCoFunction = nullptr; }

    auto name() const noexcept -> const std::string& { return mMetadata.remote_name; }
    auto name() noexcept -> std::string_view { return mMetadata.remote_name; }
    auto declaredName() const noexcept -> std::string_view { return mMetadata.declared_name; }
    auto rpcPrefix() const noexcept -> std::string_view { return mMetadata.rpc_prefix; }
    auto rpcNoPrefix() const noexcept -> bool { return mMetadata.rpc_no_prefix; }
    auto description() const noexcept -> std::string_view { return mMetadata.description; }
    auto rpcVersion() const noexcept -> std::string_view { return mMetadata.rpc_version; }

    void setDeclaredName(std::string_view name) {
        mMetadata.declared_name = name;
        updateSignature();
    }

    void setRemoteName(std::string_view name) {
        mMetadata.remote_name = name;
        updateSignature();
    }

    void setDescription(std::string_view description) { mMetadata.description = description; }
    void setRpcVersion(std::string_view version) { mMetadata.rpc_version = version; }
    void setRpcPrefix(std::string_view prefix) { mMetadata.rpc_prefix = prefix; }
    void setRpcNoPrefix(bool noPrefix = true) { mMetadata.rpc_no_prefix = noPrefix; }

    void setRpcArgNames(const std::vector<std::string_view>& names) {
        if (!std::equal(names.begin(), names.end(), mMetadata.arg_names.begin(), mMetadata.arg_names.end(),
                        [](const std::string_view& aa, const std::string_view& bb) { return aa == bb; })) {
            mMetadata.arg_names.assign(names.begin(), names.end());
            updateSignature();
        }
    }

    void applyRpcProperties(const RpcPropertyPatch& properties) {
        if (properties.name) {
            setDeclaredName(*properties.name);
        }
        if (properties.noPrefix) {
            setRpcNoPrefix(*properties.noPrefix);
        } else if (properties.prefix) {
            setRpcPrefix(*properties.prefix);
        }
        if (properties.description) {
            setDescription(*properties.description);
        }
        if (properties.version) {
            setRpcVersion(*properties.version);
        }
        if (properties.hasArgNames) {
            setRpcArgNames(properties.argNames);
        }
        if (properties.notification) {
            setRpcNotification(*properties.notification);
        }
    }

    auto rpcArgNames() const noexcept -> const std::vector<std::string>& { return mMetadata.arg_names; }

    std::string signature;

private:
    void setRpcNotification(bool isNotification = true) noexcept { this->mIsNotification = isNotification; }
    static auto build(std::string_view name, const RpcMethodData& metadata) -> std::string {
        auto parameter_string = traits::parameterToString<RawParamsType>(
            std::make_index_sequence<std::tuple_size_v<RawParamsType>>{}, metadata.arg_names);
        return std::string(traits::TypeName<RawReturnType>::name()) + " " + std::string(name) + "(" + parameter_string +
               ")";
    }
    void updateSignature() { signature = build(name(), mMetadata); }

private:
    RpcMethodData mMetadata;
};

// Static protocol-field declaration:
//   RpcMethod<int(int), "calc.add", "value"> add;
// This is the common shape inside reflected protocol structs. The name is
// declared in the type and may later be prefixed by registry tags.
template <RpcMethodT T, ConstexprString MethodName, ConstexprString... ArgNames>
class RpcMethod : public RpcMethodDynamic<T> {
public:
    RpcMethod()
        : RpcMethodDynamic<T>(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                              MethodName.view()) {}
    explicit RpcMethod(bool is_notification)
        : RpcMethodDynamic<T>(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...}, MethodName.view(),
                              is_notification) {}
    using RpcMethodDynamic<T>::operator=;
    operator bool() const noexcept { return this->mCoFunction != nullptr; }
    auto operator==(std::nullptr_t) const noexcept -> bool { return this->mCoFunction == nullptr; }
};

// Verbose metadata-first declaration. This keeps the compact RpcMethod API
// intact while allowing method attributes to live next to the signature:
//   RpcMethodSpec<int(int), rpc_name<"calc.add">, rpc_args<"value">> add;
template <RpcMethodT T, auto... Specs>
class RpcMethodSpecImpl : public RpcMethodDynamic<T> {
    constexpr static auto SpecTags = normalize_tags_v<Specs...>;
    static_assert(!(tag_query::has<tag_property::RpcPrefix>(SpecTags) &&
                    tag_query::has<tag_property::RpcNoPrefix>(SpecTags)),
                  "RpcMethodSpec cannot use rpc_prefix<...> and rpc_no_prefix together.");

    using Base = RpcMethodDynamic<T>;

public:
    RpcMethodSpecImpl() : Base(std::array<std::string_view, 0>{}, methodName(), defaultNotification()) {
        applySpecMetadata();
    }
    explicit RpcMethodSpecImpl(bool isNotification)
        : Base(std::array<std::string_view, 0>{}, methodName(), isNotification) {
        applySpecMetadata();
    }
    using Base::operator=;
    operator bool() const noexcept { return this->mCoFunction != nullptr; }
    auto operator==(std::nullptr_t) const noexcept -> bool { return this->mCoFunction == nullptr; }

private:
    static constexpr auto methodName() -> std::string_view {
        if constexpr (tag_query::has<tag_property::RpcName>(SpecTags)) {
            return tag_query::get<tag_property::RpcName>(SpecTags);
        } else {
            return {};
        }
    }

    static constexpr auto defaultNotification() -> bool {
        if constexpr (tag_query::has<tag_property::RpcNotificationFlag>(SpecTags)) {
            return tag_query::get<tag_property::RpcNotificationFlag>(SpecTags);
        } else {
            return false;
        }
    }

    void applySpecMetadata() { this->applyRpcProperties(collectRpcProperties(SpecTags)); }
};

template <RpcMethodT T, auto... Specs>
class RpcMethodSpec : public RpcMethodSpecImpl<T, Specs...> {
public:
    using Base = RpcMethodSpecImpl<T, Specs...>;
    using Base::Base;
    using Base::operator=;
};

// Function-pointer declaration that derives the method name from Ptr:
//   RpcMethodF<&free_function, "lhs", "rhs"> method;
// RpcServer::bindMethod<Ptr>() uses the same naming rule for ad-hoc binding.
template <auto Ptr, ConstexprString... ArgNames>
class RpcMethodF : public RpcMethodDynamic<decltype(Ptr)> {
public:
    constexpr static std::string_view method_name = detail::func_nameof<Ptr>;
    constexpr static std::string_view MethodName  = method_name;

    RpcMethodF()
        : RpcMethodDynamic<decltype(Ptr)>(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                                          method_name) {
        this->operator=(Ptr);
    }

    explicit RpcMethodF(bool is_notification)
        : RpcMethodDynamic<decltype(Ptr)>(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                                          method_name, is_notification) {
        this->operator=(Ptr);
    }

    using RpcMethodDynamic<decltype(Ptr)>::operator=;
};

// Function-pointer declaration with an explicit class namespace prefix:
//   RpcMethodFN<&Class::method, Class, "arg"> method;
template <auto Ptr, typename T, ConstexprString... ArgNames>
class RpcMethodFN : public RpcMethodDynamic<decltype(Ptr)> {
    constexpr static std::string_view Seq = ".";

public:
    constexpr static std::string_view MethodName =
        detail::join<detail::class_nameof<T>, Seq, detail::func_nameof<Ptr>>;

    RpcMethodFN()
        : RpcMethodDynamic<decltype(Ptr)>(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                                          MethodName) {
        this->operator=(Ptr);
    }

    explicit RpcMethodFN(bool is_notification)
        : RpcMethodDynamic<decltype(Ptr)>(std::array<std::string_view, sizeof...(ArgNames)>{ArgNames.view()...},
                                          MethodName, is_notification) {
        this->operator=(Ptr);
    }

    using RpcMethodDynamic<decltype(Ptr)>::operator=;
};

template <typename T, class Enable = void>
struct IsRpcMethod : std::false_type {};

template <typename T>
struct IsRpcMethod<T, std::void_t<typename std::remove_cvref_t<T>::RpcMethodMarker>> : std::true_type {};

template <typename T>
concept RpcMethodObject = IsRpcMethod<T>::value;

} // namespace detail

using detail::RpcMethod;
using detail::RpcMethodF;
using detail::RpcMethodFN;
using detail::RpcMethodSpec;

} // namespace nekoproto
