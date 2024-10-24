/**
 * @file proto_base.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief provide base interface for proto message and proto factory
 *
 * @mainpage NekoProtoTools
 *
 * @section intro_sec Introduction
 *
 * NekoProtoTools is a c++ library for serialize and deserialize proto message.
 * no external code generator required, more easy to use. but support only c++.
 * If you don't need cross language protocols, this would be a good choice
 *
 * @section usage_sec Usage
 * you need only include this header file, and inherit the ProtoBase, and
 * use CS_SERIALIZE specify the members you want to serialize.
 *
 * @section example_sec Example
 * @code {.c++}
 * #include "proto_base.hpp"
 * #include "json_serializer.hpp"
 * #include "serializer_base.hpp"
 *
 * struct ProtoMessage {
 *     int a;
 *     std::string b;
 *
 *     NEKO_SERIALIZE(a, b)
 *     NEKO_DECLARE_PROTOCOL(ProtoMessage, JsonSerializer)
 * }
 *
 * int main() {
 *     ProtoFactory factory(1, 0, 0);
 *     IProto* msg = factory.create("ProtoMessage"); // or auto msg = makeProtocol(ProtoMessage{});
 *     auto raw = msg->cast<ProtoMessage>();
 *     raw->a = 1;
 *     raw->b = "hello";
 *     std::vector<char> data;
 *     data = msg->toData();
 *     // do something
 *     delete msg;
 *     return 0;
 * }
 * @endcode
 *
 * @section note_sec Note
 * The protocol registered in this protocol library will be assigned
 * a typeId through the dictionary order of the protocol name. You
 * can obtain this ID through the type() method. Since the ID is
 * automatically generated, it is important to pay attention to this
 * when using an ID to interact between different protocol libraries.
 *
 * @par license
 *  GPL-2.0 license
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024 by llhsdmd
 *
 */
#pragma once

#include <cstring>
#include <functional>
#include <map>
#include <vector>

#include "private/global.hpp"
#include "private/reflection_serializer.hpp"

#define NEKO_RESERVED_PROTO_TYPE_SIZE 64

NEKO_BEGIN_NAMESPACE
class ProtoFactory;

NEKO_PROTO_API
auto static_init_funcs(const NEKO_STRING_VIEW&, std::function<void(ProtoFactory*)>)
    -> std::map<NEKO_STRING_VIEW, std::function<void(ProtoFactory*)>>&;

class NEKO_PROTO_API IProto {
public:
    IProto()          = default;
    virtual ~IProto() = default;
    /**
     * @brief serializer self
     *
     * @return std::vector<char> the data
     */
    virtual std::vector<char> toData() const NEKO_NOEXCEPT             = 0;
    virtual bool toData(std::vector<char>& buffer) const NEKO_NOEXCEPT = 0;
    /**
     * @brief deserializer self
     * The input data must be serialized using the same serializer as the current one
     * @param[in] data
     * @return true deserialized successfully
     * @return false maybe missing some data or data is invalid
     */
    virtual bool fromData(const char* data, size_t size) NEKO_NOEXCEPT = 0;
    /**
     * @brief type
     *  This type value is generated by sorting the names of all types
     * registered in the factory in lexicographic order
     * @return int
     */
    virtual int type() const NEKO_NOEXCEPT = 0;
    /**
     * @brief proto message name
     *
     * @return NEKO_STRING_VIEW is std::string_view after c++17 and std::string before c++14
     */
    virtual NEKO_STRING_VIEW protoName() const NEKO_NOEXCEPT = 0;

    template <typename T>
    bool getField(const NEKO_STRING_VIEW& name, T* result) NEKO_NOEXCEPT {
        auto* reflectionObject = _getReflectionObject();
        if (reflectionObject == nullptr) {
            return false;
        }
        return reflectionObject->getField(name, result);
    }

    template <typename T>
    T getField(const NEKO_STRING_VIEW& name, const T& defaultValue) NEKO_NOEXCEPT {
        auto* reflectionObject = _getReflectionObject();
        NEKO_ASSERT(reflectionObject != nullptr, "ReflectionSerializer", "reflectionObject is nullptr");
        return reflectionObject->getField(name, defaultValue);
    }

    template <typename T>
    bool setField(const NEKO_STRING_VIEW& name, const T& value) NEKO_NOEXCEPT {
        auto* reflectionObject = _getReflectionObject();
        NEKO_ASSERT(reflectionObject != nullptr, "ReflectionSerializer", "reflectionObject is nullptr");
        return reflectionObject->setField(name, value);
    }

    template <typename T>
    T* cast() NEKO_NOEXCEPT;

protected:
    virtual detail::ReflectionObject* _getReflectionObject() NEKO_NOEXCEPT = 0;
    virtual void* _data() NEKO_NOEXCEPT                                    = 0;
};

template <typename ProtoT, typename SerializerT>
class ProtoBase final : public IProto {
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

    bool toData(std::vector<char>& buffer) const NEKO_NOEXCEPT override;
    std::vector<char> toData() const NEKO_NOEXCEPT override;
    int type() const NEKO_NOEXCEPT override;
    bool fromData(const char* data, size_t size) NEKO_NOEXCEPT override;
    NEKO_STRING_VIEW protoName() const NEKO_NOEXCEPT override;
    static NEKO_STRING_VIEW name() NEKO_NOEXCEPT;
    static std::vector<char> Serialize(const ProtoT& proto);               // NOLINT(readability-identifier-naming)
    static bool Serialize(const ProtoT& proto, std::vector<char>& buffer); // NOLINT(readability-identifier-naming)
    static bool Deserialize(const char* data, size_t size, ProtoT& proto); // NOLINT(readability-identifier-naming)

protected:
    inline detail::ReflectionObject* _getReflectionObject() NEKO_NOEXCEPT override;
    ProtoBase(const ProtoBase& other)            = delete;
    ProtoBase& operator=(const ProtoBase& other) = delete;
    virtual void* _data() NEKO_NOEXCEPT override;

private:
    std::unique_ptr<detail::ReflectionSerializer> mReflectionSerializer = {};
    ProtoT* mData                                                       = {};
    bool mIsNew                                                         = false;
    static NEKO_STRING_VIEW gProtoName;
};

class NEKO_PROTO_API ProtoFactory {
public:
    explicit ProtoFactory(int major = 0, int minor = 0, int patch = 1);
    ~ProtoFactory();

    template <typename T>
    void regist(const NEKO_STRING_VIEW& name) NEKO_NOEXCEPT;
    void regist(const NEKO_STRING_VIEW& name, std::function<IProto*()> creator) NEKO_NOEXCEPT;
    static const std::map<NEKO_STRING_VIEW, int>& protoTypeMap() NEKO_NOEXCEPT;
    template <typename T>
    static int protoType() NEKO_NOEXCEPT;
    template <typename T>
    static int specifyProtoType(int type) NEKO_NOEXCEPT;
    template <typename T>
    static NEKO_STRING_VIEW protoName() NEKO_NOEXCEPT;
    /**
     * @brief create a proto object by type
     *  this object is a pointer, you need to delete it by yourself
     * @param type
     * @return std::unique_ptr<IProto>
     */
    std::unique_ptr<IProto> create(int type) const NEKO_NOEXCEPT;
    /**
     * @brief create a proto object by name
     *  this object is a pointer, you need to delete it by yourself
     * @param name
     * @return IProto*
     */
    std::unique_ptr<IProto> create(const char* name) const NEKO_NOEXCEPT;
    uint32_t version() const NEKO_NOEXCEPT;

private:
    ProtoFactory& operator=(const ProtoFactory&) = delete;
    ProtoFactory& operator=(ProtoFactory&&)      = delete;
    void _init() NEKO_NOEXCEPT;
    template <typename T>
    static IProto* _creater() NEKO_NOEXCEPT;
    void _setVersion(int major, int minor, int patch) NEKO_NOEXCEPT;
    static int _protoType(const NEKO_STRING_VIEW& name, bool isDeclared = false, int specifyType = -1) NEKO_NOEXCEPT;
    static std::map<NEKO_STRING_VIEW, int>& _staticProtoTypeMap();

private:
    std::vector<std::function<IProto*()>> mCreaterList;
    std::unordered_map<int, std::function<IProto*()>> mDynamicCreaterMap;
    uint32_t mVersion;
};

template <typename T>
void ProtoFactory::regist(const NEKO_STRING_VIEW& name) NEKO_NOEXCEPT {
    regist(name, _creater<T>);
}
template <typename T>
int ProtoFactory::specifyProtoType(const int type) NEKO_NOEXCEPT {
    return _protoType(protoName<T>(), true, type);
}

template <typename T>
int ProtoFactory::protoType() NEKO_NOEXCEPT {
    return _protoType(protoName<T>(), false);
}

template <typename T>
NEKO_STRING_VIEW ProtoFactory::protoName() NEKO_NOEXCEPT {
    const auto& name = decltype(T::makeProto(std::declval<T>()))::name();
    if (name.empty()) NEKO_IF_UNLIKELY {
            return _class_name<T>();
        }
    return name;
}

template <typename T>
IProto* ProtoFactory::_creater() NEKO_NOEXCEPT {
    return new T();
}

namespace detail {
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
        static auto                                                                                                    \
        test(int) -> decltype(std::is_same<decltype(proto_method_access::static_method_##name<TT>()), AA>::value,      \
                              std::true_type());                                                                       \
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

template <typename ProtoT, class enable = void>
struct declared_specify_type { // NOLINT
    static void declared() {}
};
template <typename ProtoT>
struct declared_specify_type<ProtoT, typename std::enable_if<has_method_specify_type<ProtoT, int>::value>::type> {
    static void declared() NEKO_NOEXCEPT {
        auto ret = ProtoFactory::specifyProtoType<ProtoT>(proto_method_access::static_method_specify_type<ProtoT>());
        NEKO_ASSERT(ret != -1, "proto", "type declaration failed");
    }
};
} // namespace detail

template <typename ProtoT, typename SerializerT>
NEKO_STRING_VIEW ProtoBase<ProtoT, SerializerT>::gProtoName = []() NEKO_NOEXCEPT {
    NEKO_STRING_VIEW name = _class_name<ProtoT>();
    detail::declared_specify_type<ProtoT>::declared();
    static_init_funcs(name, [name](NEKO_NAMESPACE::ProtoFactory* self) { self->regist<ProtoBaseType>(name); });
    return name;
}();

template <typename T>
inline T* IProto::cast() NEKO_NOEXCEPT {
    if (type() == ProtoFactory::protoType<T>()) {
        return reinterpret_cast<T*>(_data());
    }
    return nullptr;
}
template <typename ProtoT, typename SerializerT>
inline void* ProtoBase<ProtoT, SerializerT>::_data() NEKO_NOEXCEPT {
    return (void*)(mData);
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase() : mData(new ProtoT()), mIsNew(true) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(const ProtoT& proto) : mData(new ProtoT(proto)), mIsNew(true) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(ProtoT&& proto) : mData(new ProtoT(std::move(proto))), mIsNew(true) {}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::ProtoBase(ProtoT* proto) : mData(proto), mIsNew(false) {}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>::ProtoBase(ProtoBase<T, SerializerT>&& other) {
    mReflectionSerializer = std::move(other.mReflectionSerializer);
    mData                 = std::move(other.mData);
    mIsNew                = other.mIsNew;
    other.mIsNew          = false;
    other.mData           = nullptr;
}

template <typename ProtoT, typename SerializerT>
inline ProtoBase<ProtoT, SerializerT>::~ProtoBase() {
    if (mIsNew) {
        delete mData;
    }
}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>& ProtoBase<T, SerializerT>::operator=(ProtoBase<T, SerializerT>&& other) NEKO_NOEXCEPT {
    mReflectionSerializer = std::move(other.mReflectionSerializer);
    mData                 = std::move(other.mData);
    mIsNew                = other.mIsNew;
    other.mIsNew          = false;
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
inline detail::ReflectionObject* ProtoBase<ProtoT, SerializerT>::_getReflectionObject() NEKO_NOEXCEPT {
    NEKO_ASSERT(mData != nullptr, "ReflectionSerializer", "mData is nullptr");
    if (mReflectionSerializer != nullptr) {
        return mReflectionSerializer->getObject();
    }
    mReflectionSerializer.reset(new detail::ReflectionSerializer());
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
bool ProtoBase<ProtoT, SerializerT>::Deserialize(const char* data, size_t size, ProtoT& proto) {
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
    return Serialize(*mData, buffer);
}

template <typename T, typename SerializerT>
std::vector<char> ProtoBase<T, SerializerT>::toData() const NEKO_NOEXCEPT {
    NEKO_ASSERT(mData != nullptr, "ReflectionSerializer", "mData is nullptr");
    return Serialize(*mData);
}

template <typename T, typename SerializerT>
int ProtoBase<T, SerializerT>::type() const NEKO_NOEXCEPT {
    return ProtoFactory::protoType<T>();
}

template <typename T, typename SerializerT>
bool ProtoBase<T, SerializerT>::fromData(const char* data, size_t size) NEKO_NOEXCEPT {
    NEKO_ASSERT(mData != nullptr, "ReflectionSerializer", "mData is nullptr");
    return Deserialize(data, size, *mData);
}

#define NEKO_DECLARE_PROTOCOL(className, Serializer)                                                                   \
public:                                                                                                                \
    using ProtoType = typename NEKO_NAMESPACE::ProtoBase<className, Serializer>;                                       \
    /** @brief make proto from self pointer */                                                                         \
    inline ProtoType makeProto() NEKO_NOEXCEPT { return ProtoType(this); }                                             \
    /** @brief make proto with structure */                                                                            \
    template <typename... Args>                                                                                        \
    inline static ProtoType emplaceProto(Args&&... args) NEKO_NOEXCEPT {                                               \
        static_assert(std::is_move_constructible<className>::value, "class " #className " must be copable");           \
        return ProtoType(className{std::forward<Args>(args)...});                                                      \
    }                                                                                                                  \
    /** @brief make proto by copying other */                                                                          \
    inline static ProtoType makeProto(const className& other) NEKO_NOEXCEPT {                                          \
        static_assert(std::is_copy_constructible<className>::value, "class " #className " must be copable");           \
        return ProtoType(other);                                                                                       \
    }

NEKO_END_NAMESPACE
