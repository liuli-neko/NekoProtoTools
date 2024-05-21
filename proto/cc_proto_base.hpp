#pragma once

#include <cstring>
#include <functional>
#include <map>
#include <vector>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE
class JsonSerializer;
class ProtoFactory;

auto static_init_funcs(const std::string &, std::function<void(ProtoFactory*)>)
    -> std::map<std::string, std::function<void(ProtoFactory*)>>;
int type_counter();

class CS_PROTO_API IProto {
public:
    IProto() = default;
    virtual ~IProto() = default;
    virtual std::vector<char> toData() const = 0;
    virtual bool formData(const std::vector<char>&) = 0;
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
    void regist(const std::string &name);
    template <typename T>
    static int proto_type();
    template <typename T>
    static std::string proto_name();
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
    std::map<std::string, int> mProtoNameMap;
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
    virtual ~ProtoBase() = default;
    ProtoBase& operator=(const ProtoBase& other);
    ProtoBase& operator=(ProtoBase&& other);

    std::vector<char> toData() const override;
    int type() const override;
    bool formData(const std::vector<char>& data) override;
    template <typename U>
    bool formData(const U& data);
    std::string protoName() const;

protected:
    mutable SerializerT mSerializer;
private:
    static std::string _init_class_name__;
};

template <typename T>
void ProtoFactory::regist(const std::string &name) {
    auto itemType = mProtoMap.find(proto_type<T>());
    auto itemName = mProtoNameMap.find(name);
    if (itemType != mProtoMap.end()) {
        std::string rname = "";
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
    mProtoNameMap.insert(std::make_pair(name, proto_type<T>()));
    mProtoMap.insert(std::make_pair(proto_type<T>(), creater<T>));
}

template <typename T>
int ProtoFactory::proto_type() {
    static int _proto_type = 0;
    if (_proto_type == 0) {
        _proto_type = type_counter();
    }
    return _proto_type;
}

template <typename T>
std::string ProtoFactory::proto_name() {
    return _cs_class_name<T>();
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
std::string ProtoBase<T, SerializerT>::_init_class_name__ = []() {
    std::string name = _cs_class_name<T>();
    static_init_funcs(name, [name](CS_PROTO_NAMESPACE::ProtoFactory* self) { self->regist<T>(name); });
    return name;
}();

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
std::string ProtoBase<T, SerializerT>::protoName() const {
    return _init_class_name__;
}

template <typename T, typename SerializerT>
std::vector<char> ProtoBase<T, SerializerT>::toData() const {
    mSerializer.startSerialize();
    auto self = dynamic_cast<const ProtoType*>(this);
    CS_ASSERT(self != nullptr, "Proto({}) is nullptr", protoName());
    auto ret = const_cast<ProtoType*>(self)->serialize(mSerializer);
    std::vector<char> data;
    if (!mSerializer.endSerialize(&data)) {
        CS_LOG_ERROR("Proto({}) serialize error", protoName());
    }
    return std::move(data);
}

template <typename T, typename SerializerT>
int ProtoBase<T, SerializerT>::type() const {
    return ProtoFactory::proto_type<T>();
}

template <typename T, typename SerializerT>
bool ProtoBase<T, SerializerT>::formData(const std::vector<char>& data) {
    if (!mSerializer.startDeserialize(data)) {
        return false;
    }
    auto self = dynamic_cast<const ProtoType*>(this);
    CS_ASSERT(self != nullptr, "Proto({}) is nullptr", protoName());
    bool ret = const_cast<ProtoType*>(self)->deserialize(mSerializer);
    if (!mSerializer.endDeserialize() || !ret) {
        return false;
    }
    return true;
}

template <typename T, typename SerializerT>
template <typename U>
bool ProtoBase<T, SerializerT>::formData(const U& data) {
    if (!mSerializer.startDeserialize(data)) {
        return false;
    }
    auto self = dynamic_cast<const ProtoType*>(this);
    CS_ASSERT(self != nullptr, "Proto({}) is nullptr", protoName());
    bool ret = const_cast<ProtoType*>(self)->deserialize(mSerializer);
    if (!mSerializer.endDeserialize() || !ret) {
        return false;
    }
    return true;
}

CS_PROTO_END_NAMESPACE
