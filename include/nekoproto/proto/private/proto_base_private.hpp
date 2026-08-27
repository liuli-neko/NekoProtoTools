/**
 * @file proto_base_private.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-14
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <cstring>
#include <functional>
#include <map>
#include <type_traits>
#include <vector>

#include "nekoproto/global/global.hpp"
#include "reflection_serializer.hpp"

namespace nekoproto {
class NEKO_PROTO_API ProtoFactory;
namespace detail {
class NEKO_PROTO_API AbstractProto;
NEKO_PROTO_API
auto staticInitFuncs(const std::string_view&, std::function<void(ProtoFactory*)>)
    -> std::map<std::string_view, std::function<void(ProtoFactory*)>>&;
} // namespace detail

namespace detail {
class NEKO_PROTO_API AbstractProto {
public:
    virtual ~AbstractProto()                                                   = default;
    virtual auto toData() const noexcept -> std::vector<char>                  = 0;
    virtual auto toData(std::vector<char>& buffer) const noexcept -> bool      = 0;
    virtual auto fromData(const char* data, std::size_t size) noexcept -> bool = 0;
    virtual auto type() const noexcept -> int                                  = 0;
    virtual auto protoName() const noexcept -> std::string_view                = 0;
    virtual auto clone() const -> AbstractProto*                               = 0;
    virtual auto getReflectionObject() noexcept -> detail::ReflectionObject*   = 0;
    virtual auto data() noexcept -> void*                                      = 0;
};

template <typename ProtoT, typename SerializerT>
class ProtoBase final : public AbstractProto {
public:
    using ProtoType      = ProtoT;
    using SerializerType = SerializerT;
    using ProtoBaseType  = ProtoBase;

    ProtoBase();
    explicit ProtoBase(const ProtoT& /*proto*/);
    explicit ProtoBase(ProtoT&& /*proto*/);
    explicit ProtoBase(ProtoT* /*proto*/);
    ProtoBase(ProtoBase&& other);
    virtual ~ProtoBase();
    auto operator=(ProtoBase&& other) noexcept -> ProtoBase&;
    auto operator=(const ProtoT& other) noexcept -> ProtoBase&;
    auto operator=(ProtoT&& other) noexcept -> ProtoBase&;

    auto operator*() noexcept -> ProtoT& { return *data_; }
    auto operator->() noexcept -> ProtoT* { return data_.get(); }
    auto operator*() const noexcept -> const ProtoT& { return *data_; }
    auto operator->() const noexcept -> const ProtoT* { return data_.get(); }
    operator const ProtoT&() const noexcept { return *data_; }
    operator ProtoT&() noexcept { return *data_; }

    auto clone() const -> AbstractProto* override;
    auto toData(std::vector<char>& buffer) const noexcept -> bool override;
    auto toData() const noexcept -> std::vector<char> override;
    auto type() const noexcept -> int override;
    auto fromData(const char* data, std::size_t size) noexcept -> bool override;
    auto protoName() const noexcept -> std::string_view override;
    static auto name() noexcept -> std::string_view;
    static auto serialize(const ProtoT& proto) -> std::vector<char>;
    static auto serialize(const ProtoT& proto, std::vector<char>& buffer) -> bool;
    static auto deserialize(const char* data, std::size_t size, ProtoT& proto) -> bool;
    auto getReflectionObject() noexcept -> ReflectionObject* override;
    virtual auto data() noexcept -> void* override;

protected:
    ProtoBase(const ProtoBase& other)                    = delete;
    auto operator=(const ProtoBase& other) -> ProtoBase& = delete;

private:
    std::unique_ptr<ReflectionSerializer>      reflection_serializer_ = {};
    std::unique_ptr<ProtoT, void (*)(ProtoT*)> data_                  = {};
    static std::string_view                    s_proto_name;
};
class ProtoMethodAccess {
public:
    template <typename T>
    static auto staticMethodSpecifyType() noexcept -> decltype(T::specifyType()) {
        return T::specifyType();
    }
};

template <typename T, typename ResultT, typename = void>
struct HasSpecifyTypeMethod : std::false_type {};

template <typename T, typename ResultT>
struct HasSpecifyTypeMethod<T, ResultT, std::void_t<decltype(ProtoMethodAccess::staticMethodSpecifyType<T>())>>
    : std::bool_constant<std::is_same_v<decltype(ProtoMethodAccess::staticMethodSpecifyType<T>()), ResultT>> {};

template <typename ProtoT, typename SerializerT>
inline auto ProtoBase<ProtoT, SerializerT>::data() noexcept -> void* {
    return data_.get();
}
template <typename ProtoT, typename SerializerT>
inline auto ProtoBase<ProtoT, SerializerT>::clone() const -> AbstractProto* {
    return new ProtoBase<ProtoT, SerializerT>(*data_);
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase() : data_(new ProtoT(), [](ProtoT* ptr) { delete ptr; }) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(const ProtoT& proto)
    : data_(new ProtoT(proto), [](ProtoT* ptr) { delete ptr; }) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(ProtoT&& proto)
    : data_(new ProtoT(std::move(proto)), [](ProtoT* ptr) { delete ptr; }) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(ProtoT* proto) : data_(proto, [](ProtoT*) {}) {}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>::ProtoBase(ProtoBase<T, SerializerT>&& other) {
    reflection_serializer_ = std::move(other.reflection_serializer_);
    data_                  = std::move(other.data_);
    other.data_            = nullptr;
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::~ProtoBase() {
    data_.reset();
}

template <typename T, typename SerializerT>
auto ProtoBase<T, SerializerT>::operator=(ProtoBase<T, SerializerT>&& other) noexcept -> ProtoBase<T, SerializerT>& {
    reflection_serializer_ = std::move(other.reflection_serializer_);
    data_                  = std::move(other.data_);
    other.data_            = nullptr;
    return *this;
}

template <typename ProtoT, typename SerializerT>
inline auto ProtoBase<ProtoT, SerializerT>::operator=(const ProtoT& other) noexcept -> ProtoBase<ProtoT, SerializerT>& {
    (*data_) = other;
    return *this;
}

template <typename ProtoT, typename SerializerT>
inline auto ProtoBase<ProtoT, SerializerT>::operator=(ProtoT&& other) noexcept -> ProtoBase<ProtoT, SerializerT>& {
    (*data_) = std::move(other);
    return *this;
}

template <typename T, typename SerializerT>
auto ProtoBase<T, SerializerT>::protoName() const noexcept -> std::string_view {
    return s_proto_name;
}

template <typename T, typename SerializerT>
auto ProtoBase<T, SerializerT>::name() noexcept -> std::string_view {
    return s_proto_name;
}

template <typename ProtoT, typename SerializerT>
inline auto ProtoBase<ProtoT, SerializerT>::getReflectionObject() noexcept -> ReflectionObject* {
    NEKO_ASSERT(data_ != nullptr, "ReflectionSerializer", "data_ is nullptr");
    if (reflection_serializer_ != nullptr) {
        return reflection_serializer_->getObject();
    }
    reflection_serializer_ = std::make_unique<ReflectionSerializer>(ReflectionSerializer::reflection(*data_));
    return reflection_serializer_->getObject();
}

template <typename ProtoT, typename SerializerT>
auto ProtoBase<ProtoT, SerializerT>::serialize(const ProtoT& proto, std::vector<char>& buffer) -> bool {
    typename SerializerT::OutputSerializer serializer(buffer);
    auto ret = serializer(proto);
    if (!ret || !serializer.end()) {
        NEKO_LOG_ERROR("proto", "{} serialize error", s_proto_name);
        return false;
    }
    return ret;
}
template <typename ProtoT, typename SerializerT>
auto ProtoBase<ProtoT, SerializerT>::serialize(const ProtoT& proto) -> std::vector<char> {
    std::vector<char> data;
    if (!serialize(proto, data)) {
        return std::vector<char>();
    }
    return data;
}

template <typename ProtoT, typename SerializerT>
auto ProtoBase<ProtoT, SerializerT>::deserialize(const char* data, std::size_t size, ProtoT& proto) -> bool {
    typename SerializerT::InputSerializer serializer(data, size);
    if (!serializer) {
#if defined(NEKO_VERBOSE_LOGS)
        NEKO_LOG_INFO("proto", "{} data parser failed.", s_proto_name);
#endif
        return false;
    }
    bool ret = serializer(proto);
    if (!ret) {
        NEKO_LOG_ERROR("proto", "{} deserialize error", s_proto_name);
        return false;
    }
    return true;
}
template <typename T, typename SerializerT>
auto ProtoBase<T, SerializerT>::toData(std::vector<char>& buffer) const noexcept -> bool {
    NEKO_ASSERT(data_ != nullptr, "ReflectionSerializer", "data_ is nullptr");
    return serialize(*data_.get(), buffer);
}

template <typename T, typename SerializerT>
auto ProtoBase<T, SerializerT>::toData() const noexcept -> std::vector<char> {
    NEKO_ASSERT(data_ != nullptr, "ReflectionSerializer", "data_ is nullptr");
    return serialize(*data_.get());
}

template <typename T, typename SerializerT>
auto ProtoBase<T, SerializerT>::fromData(const char* data, std::size_t size) noexcept -> bool {
    NEKO_ASSERT(data_ != nullptr, "ReflectionSerializer", "data_ is nullptr");
    return deserialize(data, size, *data_.get());
}
} // namespace detail

} // namespace nekoproto
