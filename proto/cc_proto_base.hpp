#pragma once

#include <cstring>
#include <functional>
#include <map>
#include <vector>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE
class JsonSerializer;
class ProtoFactory;

template <typename T>
int _proto_type();
template <typename T>
const char* _proto_name();
auto static_init_funcs(const char*, std::function<void(ProtoFactory*)>)
    -> std::map<const char*, std::function<void(ProtoFactory*)>>;
int type_counter();

class CS_PROTO_API IProto {
public:
    IProto() = default;
    virtual ~IProto() = default;
    virtual std::vector<char> serialize() const = 0;
    virtual bool deserialize(const std::vector<char>&) = 0;
    virtual int type() const = 0;
};

class CS_PROTO_API ProtoFactory {
public:
    ProtoFactory(int major = 0, int minor = 0, int patch = 1);
    ProtoFactory(const ProtoFactory&);
    ProtoFactory(ProtoFactory&&);
    ProtoFactory& operator=(const ProtoFactory&);
    ProtoFactory& operator=(ProtoFactory&&);
    ~ProtoFactory();

    void init();
    template <typename T>
    void regist(const char* name);
    template <typename T>
    static int proto_type();
    template <typename T>
    static const char* proto_name();
    /**
     * @brief create a proto object by type
     *  this object is a pointer, you need to delete it by yourself
     * @param type
     * @return IProto*
     */
    IProto* create(int type) const;
    /**
     * @brief create a proto object by name
     *  this object is a pointer, you need to delete it by yourself
     * @param name
     * @return IProto*
     */
    inline IProto* create(const char* name) const;
    /**
     * @brief create a object
     * this object is a pointer, you need to delete it by yourself
     * @return T*
     */
    template <typename T>
    inline T* create() const;
    uint32_t version() const;

private:
    std::map<int, std::function<IProto*()>> mProtoMap;
    std::map<const char*, int> mProtoNameMap;
    uint32_t mVersion = 0;

    template <typename T>
    static IProto* creater();
    void setVersion(int major, int minor, int patch);
};

template <typename T, typename SerializerT = JsonSerializer>
class ProtoBase : public IProto {
public:
    using ProtoType = T;
    using SerializerType = SerializerT;
    using ProtoBaseType = ProtoBase;

    ProtoBase() = default;
    ProtoBase(const ProtoBase& other);
    ProtoBase(ProtoBase&& other);
    ProtoBase& operator=(const ProtoBase& other);
    ProtoBase& operator=(ProtoBase&& other);

    std::vector<char> serialize() const override;
    int type() const override;
    bool deserialize(const std::vector<char>& data) override;
    template <typename U>
    bool deserialize(const U& data);
    virtual ~ProtoBase() = default;
    SerializerT* serializer();

protected:
    mutable SerializerT mSerializer;
    struct less {
        bool operator()(const char* a, const char* b) const;
    };
    static std::map<const char*, std::function<bool(ProtoBase*)>, less> mSerializeVistor;
    static std::map<const char*, std::function<bool(ProtoBase*)>, less> mDeserializeVistor;
};

template <typename T>
void ProtoFactory::regist(const char* name) {
    auto itemType = mProtoMap.find(proto_type<T>());
    auto itemName = mProtoNameMap.find(name);
    if (itemType != mProtoMap.end()) {
        auto rname = "";
        for (auto item : mProtoNameMap) {
            if (item.second == proto_type<T>()) {
                rname = item.first;
                break;
            }
        }
        CS_LOG_WARN("type {} is regist by proto({}), proto({}) can't regist again", proto_type<T>(), rname, name);
    }
    if (itemName != mProtoNameMap.end()) {
        CS_LOG_WARN("proto({}) is regist type {}, can't regist type {} again", name, itemName->second, proto_type<T>());
    }
    CS_LOG_INFO("Init proto {}:{} for factory({})", name, proto_type<T>(), (void*)this);
    mProtoNameMap.insert(std::make_pair(_proto_name<T>(), proto_type<T>()));
    mProtoMap.insert(std::make_pair(proto_type<T>(), creater<T>));
}

template <typename T>
int ProtoFactory::proto_type() {
    return _proto_type<T>();
}

template <typename T>
const char* ProtoFactory::proto_name() {
    return _proto_name<T>();
}

template <typename T>
T* ProtoFactory::create() const {
    return new T();
}

template <typename T>
IProto* ProtoFactory::creater() {
    return new T();
}

template <typename T, typename SerializerT>
std::map<const char*, std::function<bool(ProtoBase<T, SerializerT>*)>, typename ProtoBase<T, SerializerT>::less>
    ProtoBase<T, SerializerT>::mSerializeVistor;
template <typename T, typename SerializerT>
std::map<const char*, std::function<bool(ProtoBase<T, SerializerT>*)>, typename ProtoBase<T, SerializerT>::less>
    ProtoBase<T, SerializerT>::mDeserializeVistor;

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>::ProtoBase(const ProtoBase<T, SerializerT>& other) {
    mSerializer = other.mSerializer;
}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>::ProtoBase(ProtoBase<T, SerializerT>&& other) {
    mSerializer = std::move(other.mSerializer);
}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>& ProtoBase<T, SerializerT>::operator=(const ProtoBase<T, SerializerT>& other) {
    mSerializer = other.mSerializer;
    return *this;
}

template <typename T, typename SerializerT>
ProtoBase<T, SerializerT>& ProtoBase<T, SerializerT>::operator=(ProtoBase<T, SerializerT>&& other) {
    mSerializer = std::move(other.mSerializer);
    return *this;
}

template <typename T, typename SerializerT>
std::vector<char> ProtoBase<T, SerializerT>::serialize() const {
    mSerializer.startSerialize();
    for (auto item : mSerializeVistor) {
        if (!(item.second)(const_cast<ProtoBase*>(this))) {
            CS_LOG_WARN("field {} serialize error", item.first);
        }
    }
    std::vector<char> ret;
    if (!mSerializer.endSerialize(&ret)) {
        CS_LOG_ERROR("Proto({}) serialize error", _proto_name<T>());
    }
    return std::move(ret);
}

template <typename T, typename SerializerT>
int ProtoBase<T, SerializerT>::type() const {
    return _proto_type<T>();
}

template <typename T, typename SerializerT>
bool ProtoBase<T, SerializerT>::deserialize(const std::vector<char>& data) {
    if (!mSerializer.startDeserialize(data)) {
        return false;
    }
    bool ret = true;
    for (auto item : mDeserializeVistor) {
        if (!(item.second)(const_cast<ProtoBase*>(this))) {
            ret = false;
            CS_LOG_WARN("field {} deserialize error", item.first);
        }
    }
    if (!mSerializer.endDeserialize() || !ret) {
        return false;
    }
    return true;
}

template <typename T, typename SerializerT>
template <typename U>
bool ProtoBase<T, SerializerT>::deserialize(const U& data) {
    if (!mSerializer.startDeserialize(data)) {
        return false;
    }
    bool ret = true;
    for (auto item : mDeserializeVistor) {
        if (!(item.second)(const_cast<ProtoBase*>(this))) {
            ret = false;
            CS_LOG_WARN("field {} deserialize error", item.first);
        }
    }
    if (!mSerializer.endDeserialize() || !ret) {
        return false;
    }
    return true;
}

template <typename T, typename SerializerT>
bool ProtoBase<T, SerializerT>::less::operator()(const char* a, const char* b) const {
    return ::strcmp(a, b) < 0;
}

template <typename T, typename SerializerT>
SerializerT* ProtoBase<T, SerializerT>::serializer() {
    return &mSerializer;
}

template <typename T, class enable = void>
struct ContainerTraitsHelper;

template <typename T>
struct ContainerTraitsHelper<std::vector<T>> {
    using key_type = int;
    using value_type = T;
};

template <typename T, size_t N>
struct ContainerTraitsHelper<std::array<T, N>> {
    using key_type = int;
    using value_type = T;
};

template <typename T, typename U>
struct ContainerTraitsHelper<std::map<T, U>> {
    using key_type = T;
    using value_type = U;
};

CS_PROTO_END_NAMESPACE

#define CS_PROPERTY_FIELD(type, name)                                     \
    __CS_DECLARE_PROTO_FIELD1(name)                                       \
public:                                                                   \
    inline const type& name() const { return m_##name; }                  \
    inline type& mutable_##name() { return m_##name; }                    \
    inline void set_##name(const type& value) { m_##name = value; }       \
    inline void set_##name(type&& value) { m_##name = std::move(value); } \
                                                                          \
private:                                                                  \
    type m_##name

#define CS_PROPERTY_CONTAINER_FIELD(type, name)                                               \
    __CS_DECLARE_PROTO_FIELD1(name)                                                           \
public:                                                                                       \
    inline const type& name() const { return m_##name; }                                      \
    inline const CS_PROTO_NAMESPACE::ContainerTraitsHelper<type>::value_type& name(           \
        const CS_PROTO_NAMESPACE::ContainerTraitsHelper<type>::key_type& index) {             \
        return m_##name[index];                                                               \
    }                                                                                         \
    inline type& mutable_##name() { return m_##name; }                                        \
    inline const CS_PROTO_NAMESPACE::ContainerTraitsHelper<type>::value_type& mutable_##name( \
        const CS_PROTO_NAMESPACE::ContainerTraitsHelper<type>::key_type& index) {             \
        return m_##name[index];                                                               \
    }                                                                                         \
    inline void set_##name(const type& value) { m_##name = value; }                           \
    inline void set_##name(type&& value) { m_##name = std::move(value); }                     \
                                                                                              \
private:                                                                                      \
    type m_##name

#define __CS_DECLARE_PROTO_FIELD1(name)                                                          \
private:                                                                                         \
    template <typename T>                                                                        \
    inline static void _regist_field_##name(T*) {                                                \
        static bool _init_##name = false;                                                        \
        if (_init_##name) {                                                                      \
            return;                                                                              \
        }                                                                                        \
        _init_##name = true;                                                                     \
        T::mSerializeVistor.insert(std::make_pair(#name, [](typename T::ProtoBaseType* self) {   \
            auto ptr = dynamic_cast<const T*>(self);                                             \
            if (ptr == nullptr) {                                                                \
                CS_LOG_ERROR("please make sure T is self whien public ProtoBase<T>");            \
                return false;                                                                    \
            }                                                                                    \
            return self->serializer()->insert(#name, ptr->name());                               \
        }));                                                                                     \
        T::mDeserializeVistor.insert(std::make_pair(#name, [](typename T::ProtoBaseType* self) { \
            auto ptr = dynamic_cast<T*>(self);                                                   \
            if (ptr == nullptr) {                                                                \
                CS_LOG_ERROR("please make sure T is self whien public ProtoBase<T>");            \
                return false;                                                                    \
            }                                                                                    \
            return self->serializer()->get(#name, &(ptr->mutable_##name()));                     \
        }));                                                                                     \
    }                                                                                            \
    bool _init_##name = [this]() {                                                               \
        _regist_field_##name(this);                                                              \
        return true;                                                                             \
    }();

#define CS_DECLARE_PROTO_FIELD(name)                                                             \
private:                                                                                         \
    template <typename T>                                                                        \
    inline static void _regist_field_##name(T*) {                                                \
        static bool _init_##name = false;                                                        \
        if (_init_##name) {                                                                      \
            return;                                                                              \
        }                                                                                        \
        _init_##name = true;                                                                     \
        T::mSerializeVistor.insert(std::make_pair(#name, [](typename T::ProtoBaseType* self) {   \
            auto ptr = dynamic_cast<const T*>(self);                                             \
            if (ptr == nullptr) {                                                                \
                CS_LOG_ERROR("please make sure T is self whien public ProtoBase<T>");            \
                return false;                                                                    \
            }                                                                                    \
            return self->serializer()->insert(#name, ptr->name);                                 \
        }));                                                                                     \
        T::mDeserializeVistor.insert(std::make_pair(#name, [](typename T::ProtoBaseType* self) { \
            auto ptr = dynamic_cast<T*>(self);                                                   \
            if (ptr == nullptr) {                                                                \
                CS_LOG_ERROR("please make sure T is self whien public ProtoBase<T>");            \
                return false;                                                                    \
            }                                                                                    \
            return self->serializer()->get(#name, &(ptr->name));                                 \
        }));                                                                                     \
    }                                                                                            \
    bool _init_##name = [this]() {                                                               \
        _regist_field_##name(this);                                                              \
        return true;                                                                             \
    }();

#ifdef __GNU__
#define CS_DECLARE_PROTO_FIELDS(...) FOR_EACH(CS_DECLARE_PROTO_FIELD, __VA_ARGS__)
#endif

#define CS_DECLARE_PROTO(type, name)                                                                             \
    CS_PROTO_BEGIN_NAMESPACE                                                                                     \
    template <>                                                                                                  \
    int _proto_type<type>() {                                                                                    \
        static int _proto_type = 0;                                                                              \
        if (_proto_type == 0) {                                                                                  \
            _proto_type = type_counter();                                                                        \
        }                                                                                                        \
        return _proto_type;                                                                                      \
    }                                                                                                            \
    template <>                                                                                                  \
    constexpr const char* _proto_name<type>() {                                                                  \
        return #name;                                                                                            \
    }                                                                                                            \
    namespace {                                                                                                  \
    struct _regist_##name {                                                                                      \
        _regist_##name() {                                                                                       \
            static_init_funcs(#name, [](CS_PROTO_NAMESPACE::ProtoFactory* self) { self->regist<type>(#name); }); \
        }                                                                                                        \
    } _regist_##name##__tmp;                                                                                     \
    }                                                                                                            \
    CS_PROTO_END_NAMESPACE
