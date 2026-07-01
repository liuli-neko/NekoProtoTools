#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <ilias/task.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <ilias/io.hpp>

#include "nekoproto/global/config.h"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/rpc/error.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {
template <typename R, typename... Args>
struct function_traits<ilias::IoTask<R> (*)(Args...), void> {
    using return_type = R;
    using arg_tuple   = std::tuple<Args...>;
    template <typename Ret, template <typename...> class T>
    using args_in = T<Ret, Args...>;
};
} // namespace detail

namespace traits {

template <typename T, class enable = void>
struct TypeName {
    using RawType = std::remove_cvref_t<T>;

    static std::string name() {
        if constexpr (!std::is_same_v<T, RawType>) {
            return TypeName<RawType>::name();
        } else if constexpr (std::is_void_v<RawType>) {
            return "void";
        } else if constexpr (std::is_same_v<RawType, bool>) {
            return "bool";
        } else if constexpr (std::is_same_v<RawType, std::nullptr_t>) {
            return "null";
        } else if constexpr (std::is_same_v<RawType, std::byte>) {
            return "byte";
        } else if constexpr (std::is_same_v<RawType, std::string> || std::is_same_v<RawType, std::string_view>) {
            return "string";
        } else if constexpr (std::is_same_v<RawType, std::u8string>) {
            return "u8string";
        } else if constexpr (std::is_same_v<RawType, std::u16string>) {
            return "u16string";
        } else if constexpr (std::is_same_v<RawType, std::u32string>) {
            return "u32string";
        } else if constexpr (std::is_integral_v<RawType>) {
            std::string result;
            result += std::is_signed_v<RawType> ? "i" : "u";
            result += std::to_string(sizeof(RawType) * 8U);
            return result;
        } else if constexpr (std::is_floating_point_v<RawType>) {
            return "f" + std::to_string(sizeof(RawType) * 8U);
        } else if constexpr (std::is_pointer_v<RawType>) {
            std::string result = "ptr<";
            result += TypeName<std::remove_pointer_t<RawType>>::name();
            result += ">";
            return result;
        } else if constexpr (std::is_enum_v<RawType>) {
            std::string result = "enum<";
            result += NEKO_NAMESPACE::detail::class_nameof<RawType>;
            result += ">";
            return result;
        } else {
            std::string result = "object<";
            result += NEKO_NAMESPACE::detail::class_nameof<RawType>;
            result += ">";
            return result;
        }
    }
};

template <typename RawParamsType, std::size_t... Is>
constexpr auto parameter_to_string(std::index_sequence<Is...> /*unused*/,
                                   const std::vector<std::string>& names = {})
    -> std::string {
    if constexpr (sizeof...(Is) == 0) {
        return "";
    }
    std::string result;
    auto append_param = [&](auto idx) {
        constexpr size_t Idx = idx;
        using param_type     = std::tuple_element_t<Idx, RawParamsType>;

        if (names.size() == sizeof...(Is)) {
            result += TypeName<param_type>::name() + " " + std::string(names[Idx]);
        } else {
            result += TypeName<param_type>::name();
        }

        if constexpr (Idx < sizeof...(Is) - 1) {
            result += ", ";
        }
    };
    (append_param(std::integral_constant<size_t, Is>{}), ...);
    return result;
};

template <typename T>
struct TypeName<std::optional<T>, void> {
    static std::string name() {
        std::string result = "optional<";
        result += TypeName<T>::name();
        result += ">";
        return result;
    }
};
template <typename... Args>
struct TypeName<std::variant<Args...>, void> {
    static std::string name() {
        std::string result = "variant<";
        result += parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{});
        result += ">";
        return result;
    }
};
template <typename T, std::size_t N>
struct TypeName<std::array<T, N>, void> {
    static std::string name() {
        std::string result = "array<";
        result += TypeName<T>::name();
        result += ", ";
        result += std::to_string(N);
        result += ">";
        return result;
    }
};
template <typename... Args>
struct TypeName<std::tuple<Args...>, void> {
    static std::string name() {
        std::string result = "tuple<";
        result += parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{});
        result += ">";
        return result;
    }
};
template <typename T, typename T2>
struct TypeName<std::pair<T, T2>, void> {
    static std::string name() {
        std::string result = "pair<";
        result += TypeName<T>::name();
        result += ", ";
        result += TypeName<T2>::name();
        result += ">";
        return result;
    }
};
template <typename T>
struct TypeName<std::vector<T>, void> {
    static std::string name() {
        std::string result = "array<";
        result += TypeName<T>::name();
        result += ">";
        return result;
    }
};

#if __cpp_lib_move_only_function >= 202110L
template <typename... ArgsT>
using FunctionT = std::move_only_function<ArgsT...>;
#else
template <typename... ArgsT>
using FunctionT = std::function<ArgsT...>;
#endif

using NEKO_NAMESPACE::detail::function_traits;

template <typename RetT, typename... Args>
class RpcMethodExecutor {
public:
    using RawReturnType      = RetT;
    using CoroutinesFuncType = FunctionT<ilias::IoTask<RawReturnType>(Args...)>;

    auto operator()(Args... args) const -> ilias::IoTask<RawReturnType> {
        if (mCoFunction) {
            co_return co_await mCoFunction(std::forward<Args>(args)...);
        }
        co_return ilias::Err(RpcError::MethodNotBind);
    }

    auto notification(Args... args) const -> ilias::IoTask<void> {
        struct NotificationGuard {
            bool& flag;
            explicit NotificationGuard(bool& fg) : flag(fg) { flag = true; }
            ~NotificationGuard() { flag = false; }
        };
        NotificationGuard guard{mIsNotification};

        if (mCoFunction) {
            if (auto ret = co_await mCoFunction(std::forward<Args>(args)...); !ret) {
                co_return ilias::Err(ret.error());
            }
        }
        co_return {};
    }

    auto isNotification() const -> bool { return mIsNotification; }

protected:
    mutable CoroutinesFuncType mCoFunction;
    mutable bool mIsNotification = false;
};

template <typename RetT, typename... Args>
class RpcMethodMetadata : public RpcMethodExecutor<RetT, Args...> {
public:
    using RawReturnType = RetT;
    using RawParamsType = std::tuple<Args...>;
    using FunctionType  = FunctionT<RetT(Args...)>;

    constexpr static int NumParams = sizeof...(Args);
};

template <typename Callable>
using RpcMethodTraitsUnpacker =
    typename function_traits<Callable>::template args_in<typename function_traits<Callable>::return_type,
                                                         RpcMethodMetadata>;

} // namespace traits

NEKO_END_NAMESPACE
