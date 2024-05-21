#pragma once

#include "cc_proto_global.hpp"

#include <map>
#include <functional>
#include <string>

CS_PROTO_BEGIN_NAMESPACE

class IDumpableObject {
public:
    virtual std::string dumpToString() = 0;
    virtual bool loadFromString(const std::string& str) = 0;
    virtual bool dumpToFile(const std::string& filePath) = 0;
    virtual bool loadFromFile(const std::string& filePath) = 0;
    virtual ~IDumpableObject() = default;
    virtual std::string objectName() const = 0;
    static IDumpableObject *create(const std::string& objectName);
protected:
    static std::map<std::string, std::function<IDumpableObject *()>> mObjectMap;
};

std::map<std::string, std::function<IDumpableObject *()>> IDumpableObject::mObjectMap; 

inline IDumpableObject *IDumpableObject::create(const std::string& objectName) {
    auto it = mObjectMap.find(objectName);
    if (it != mObjectMap.end()) {
        return (it->second)();
    }
    return nullptr;
}

template <typename T, typename SerializerT>
class DumpableObject : public IDumpableObject {
public:
    using ObjectType = T;
    using SerializerType = SerializerT;

    virtual std::string dumpToString() override;
    virtual bool loadFromString(const std::string& str) override;
    virtual bool dumpToFile(const std::string& filePath) override { CS_ASSERT(false, "not implemented"); return false; };
    virtual bool loadFromFile(const std::string& filePath) override { CS_ASSERT(false, "not implemented"); return false; };

private:
    struct _init_data {
        _init_data();
    };
    static _init_data _init__flag; // FIXME: can't call function?
};
template <typename T, typename SerializerT>
DumpableObject<T, SerializerT>::_init_data::_init_data() {
    static_assert(std::is_base_of<DumpableObject<T, SerializerT>, T>::value, "T must be derived from DumpableObject<T, SerializerT>");
    CS_LOG_INFO("DumpableObject<{}, {}> init", typeid(T).name(), typeid(SerializerT).name());
    mObjectMap.insert(std::make_pair(T().objectName(), [](){
        return new T();
    }));
}

template <typename T, typename SerializerT>
DumpableObject<T, SerializerT>::_init_data DumpableObject<T, SerializerT>::_init__flag = {};

template <typename T, typename SerializerT>
std::string DumpableObject<T, SerializerT>::dumpToString() {
    SerializerT serializer;
    serializer.startSerialize();
    auto self = const_cast<T *>(static_cast<const T *>(this));
    auto ret = self->serialize(serializer);
    std::vector<char> data;
    if (!serializer.endSerialize(&data) || !ret) {
        CS_LOG_ERROR("DumpableObject({}) dumpToString failed", objectName());
        return "";
    }
    return std::string(data.begin(), data.end());
}

template <typename T, typename SerializerT>
bool DumpableObject<T, SerializerT>::loadFromString(const std::string& str) {
    SerializerT serializer;
    if (!serializer.startDeserialize(std::vector(str.begin(), str.end()))) {
        return false;
    }
    auto self = dynamic_cast<const T*>(this);
    bool ret = const_cast<T*>(self)->deserialize(serializer);
    if (!serializer.endDeserialize() || !ret) {
        return false;
    }
    return true;
}

CS_PROTO_END_NAMESPACE