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
#include <vector>

#include "nekoproto/global/global.hpp"
#include "reflection_serializer.hpp"

NEKO_BEGIN_NAMESPACE
class NEKO_PROTO_API ProtoFactory;
namespace detail {
class NEKO_PROTO_API AbstractProto;
NEKO_PROTO_API
auto static_init_funcs(const NEKO_STRING_VIEW&, std::function<void(ProtoFactory*)>)
    -> std::map<NEKO_STRING_VIEW, std::function<void(ProtoFactory*)>>&;
} // namespace detail

namespace detail {
class NEKO_PROTO_API AbstractProto {
public:
    virtual ~AbstractProto()                                                = default;
    virtual std::vector<char> toData() const NEKO_NOEXCEPT                  = 0;
    virtual bool toData(std::vector<char>& buffer) const NEKO_NOEXCEPT      = 0;
    virtual bool fromData(const char* data, std::size_t size) NEKO_NOEXCEPT = 0;
    virtual int type() const NEKO_NOEXCEPT                                  = 0;
    virtual NEKO_STRING_VIEW protoName() const NEKO_NOEXCEPT                = 0;
    virtual AbstractProto* clone() const                                    = 0;
    virtual detail::ReflectionObject* getReflectionObject() NEKO_NOEXCEPT   = 0;
    virtual void* data() NEKO_NOEXCEPT                                      = 0;
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
    ProtoBase& operator=(ProtoBase&& other) NEKO_NOEXCEPT;
    ProtoBase& operator=(const ProtoT& other) NEKO_NOEXCEPT;
    ProtoBase& operator=(ProtoT&& other) NEKO_NOEXCEPT;

    ProtoT& operator*() NEKO_NOEXCEPT { return *mData; }
    ProtoT* operator->() NEKO_NOEXCEPT { return mData; }
    const ProtoT& operator*() const NEKO_NOEXCEPT { return *mData; }
    const ProtoT* operator->() const NEKO_NOEXCEPT { return mData; }
    operator const ProtoT&() const NEKO_NOEXCEPT { return *mData; }
    operator ProtoT&() NEKO_NOEXCEPT { return *mData; }

    AbstractProto* clone() const override;
    bool toData(std::vector<char>& buffer) const NEKO_NOEXCEPT override;
    std::vector<char> toData() const NEKO_NOEXCEPT override;
    int type() const NEKO_NOEXCEPT override;
    bool fromData(const char* data, std::size_t size) NEKO_NOEXCEPT override;
    NEKO_STRING_VIEW protoName() const NEKO_NOEXCEPT override;
    static NEKO_STRING_VIEW name() NEKO_NOEXCEPT;
    static std::vector<char> Serialize(const ProtoT& proto);                    // NOLINT(readability-identifier-naming)
    static bool Serialize(const ProtoT& proto, std::vector<char>& buffer);      // NOLINT(readability-identifier-naming)
    static bool Deserialize(const char* data, std::size_t size, ProtoT& proto); // NOLINT(readability-identifier-naming)
    ReflectionObject* getReflectionObject() NEKO_NOEXCEPT override;
    virtual void* data() NEKO_NOEXCEPT override;

protected:
    ProtoBase(const ProtoBase& other)            = delete;
    ProtoBase& operator=(const ProtoBase& other) = delete;

private:
    std::unique_ptr<ReflectionSerializer> mReflectionSerializer = {};
    std::unique_ptr<ProtoT, void (*)(ProtoT*)> mData            = {};
    static NEKO_STRING_VIEW gProtoName;
};
class proto_method_access {
public:
    template <typename T>
    static auto static_method_specify_type() NEKO_NOEXCEPT -> decltype(T::specifyType()) { // NOLINT
        return T::specifyType();
    }
};
#define NEKO_MAKE_HAS_STATIC_METHOD_TEST1(name, test_name)                                                             \
    namespace detail {                                                                                                 \
    template <class T, class ResultT>                                                                                  \
    struct has_method_##test_name##_impl {                                                                             \
        template <class TT, class AA>                                                                                  \
        static auto test(int)                                                                                          \
            -> decltype(std::is_same<decltype(proto_method_access::static_method_##name<TT>()), AA>::value,            \
                        std::true_type());                                                                             \
        template <class, class>                                                                                        \
        static std::false_type test(...);                                                                              \
        static const bool value = std::is_same<decltype(test<T, ResultT>(0)), std::true_type>::value;                  \
    };                                                                                                                 \
    } /* end namespace detail */                                                                                       \
    template <class T, class A>                                                                                        \
    struct has_method_##test_name : std::integral_constant<bool, detail::has_method_##test_name##_impl<T, A>::value> { \
    };

NEKO_MAKE_HAS_STATIC_METHOD_TEST1(specify_type, specify_type)
#undef NEKO_MAKE_HAS_STATIC_METHOD_TEST1

template <typename ProtoT, typename SerializerT>
inline void* ProtoBase<ProtoT, SerializerT>::data() NEKO_NOEXCEPT {
    return mData.get();
}
template <typename ProtoT, typename SerializerT>
inline AbstractProto* ProtoBase<ProtoT, SerializerT>::clone() const {
    return new ProtoBase<ProtoT, SerializerT>(*mData);
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase() : mData(new ProtoT(), [](ProtoT* ptr) { delete ptr; }) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(const ProtoT& proto)
    : mData(new ProtoT(proto), [](ProtoT* ptr) { delete ptr; }) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(ProtoT&& proto)
    : mData(new ProtoT(std::move(proto)), [](ProtoT* ptr) { delete ptr; }) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(ProtoT* proto) : mData(proto, [](ProtoT*) {}) {}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>::ProtoBase(ProtoBase<T, SerializerT>&& other) {
    mReflectionSerializer = std::move(other.mReflectionSerializer);
    mData                 = std::move(other.mData);
    other.mData           = nullptr;
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::~ProtoBase() {
    mData.reset();
}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>& ProtoBase<T, SerializerT>::operator=(ProtoBase<T, SerializerT>&& other) NEKO_NOEXCEPT {
    mReflectionSerializer = std::move(other.mReflectionSerializer);
    mData                 = std::move(other.mData);
    other.mData           = nullptr;
    return *this;
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>& ProtoBase<ProtoT, SerializerT>::operator=(const ProtoT& other) NEKO_NOEXCEPT {
    (*mData) = other;
    return *this;
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>& ProtoBase<ProtoT, SerializerT>::operator=(ProtoT&& other) NEKO_NOEXCEPT {
    (*mData) = std::move(other);
    return *this;
}

template <typename T, typename SerializerT>
NEKO_STRING_VIEW ProtoBase<T, SerializerT>::protoName() const NEKO_NOEXCEPT {
    return gProtoName;
}

template <typename T, typename SerializerT>
NEKO_STRING_VIEW ProtoBase<T, SerializerT>::name() NEKO_NOEXCEPT {
    return gProtoName;
}

template <typename ProtoT, typename SerializerT>
inline ReflectionObject* ProtoBase<ProtoT, SerializerT>::getReflectionObject() NEKO_NOEXCEPT {
    NEKO_ASSERT(mData != nullptr, "ReflectionSerializer", "mData is nullptr");
    if (mReflectionSerializer != nullptr) {
        return mReflectionSerializer->getObject();
    }
    mReflectionSerializer.reset(new ReflectionSerializer());
    bool ret = mData->serialize(*mReflectionSerializer);
    NEKO_ASSERT(ret, "ReflectionSerializer", "{} get reflection object error", gProtoName);
    NEKO_ASSERT(mReflectionSerializer->getObject() != nullptr, "ReflectionSerializer",
                "mReflectionSerializer->getObject() is nullptr");
    return mReflectionSerializer->getObject();
}

template <typename ProtoT, typename SerializerT>
bool ProtoBase<ProtoT, SerializerT>::Serialize(const ProtoT& proto, std::vector<char>& buffer) {
    typename SerializerT::OutputSerializer serializer(buffer);
    auto ret = serializer(proto);
    if (!ret || !serializer.end()) {
        NEKO_LOG_ERROR("proto", "{} serialize error", gProtoName);
        return false;
    }
    return ret;
}
template <typename ProtoT, typename SerializerT>
std::vector<char> ProtoBase<ProtoT, SerializerT>::Serialize(const ProtoT& proto) {
    std::vector<char> data;
    if (!Serialize(proto, data)) {
        return std::vector<char>();
    }
    return data;
}

template <typename ProtoT, typename SerializerT>
bool ProtoBase<ProtoT, SerializerT>::Deserialize(const char* data, std::size_t size, ProtoT& proto) {
    typename SerializerT::InputSerializer serializer(data, size);
    if (!serializer) {
#if defined(NEKO_VERBOSE_LOGS)
        NEKO_LOG_INFO("proto", "{} data parser failed.", gProtoName);
#endif
        return false;
    }
    bool ret = serializer(proto);
    if (!ret) {
        NEKO_LOG_ERROR("proto", "{} deserialize error", gProtoName);
        return false;
    }
    return true;
}
template <typename T, typename SerializerT>
bool ProtoBase<T, SerializerT>::toData(std::vector<char>& buffer) const NEKO_NOEXCEPT {
    NEKO_ASSERT(mData != nullptr, "ReflectionSerializer", "mData is nullptr");
    return Serialize(*mData.get(), buffer);
}

template <typename T, typename SerializerT>
std::vector<char> ProtoBase<T, SerializerT>::toData() const NEKO_NOEXCEPT {
    NEKO_ASSERT(mData != nullptr, "ReflectionSerializer", "mData is nullptr");
    return Serialize(*mData.get());
}

template <typename T, typename SerializerT>
bool ProtoBase<T, SerializerT>::fromData(const char* data, std::size_t size) NEKO_NOEXCEPT {
    NEKO_ASSERT(mData != nullptr, "ReflectionSerializer", "mData is nullptr");
    return Deserialize(data, size, *mData.get());
}
} // namespace detail

NEKO_END_NAMESPACE