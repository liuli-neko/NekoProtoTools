#pragma once

#include "cc_proto_global.hpp"

#include <map>
#include <functional>
#include <string>
#include <fstream>

#if defined(__GNUC__) || defined(__MINGW__)
#include <cxxabi.h>
#elif defined(_WIN32)
#ifdef GetObject
#undef GetObject
#endif
#endif

#include <rapidjson/istreamwrapper.h>

#include "cc_proto_json_serializer.hpp"

CS_PROTO_BEGIN_NAMESPACE
#if defined(__GNUC__) || defined(__MINGW__)
namespace {
    template <class T>
    std::string _cs_class_name() {
        int status = -4; // some arbitrary value to eliminate the compiler warning
        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status),
            std::free
        };
        return (status==0) ? res.get() : typeid(T).name();
    }
}
#elif defined(_WIN32)
namespace {
    template <class T>
    std::string _cs_class_name() {
        std::string string(__FUNCSIG__);
        size_t start = string.find_last_of(' ');
        auto sstring = string.substr(start);
        size_t d =  sstring.find_last_of(':');
        if (d != std::string::npos)
            start = d;
        else
            start = 0;
        while (start < sstring.size() && (sstring[start] == ' ' || sstring[start] == '>' || sstring[start] == ':')) {
            ++start;
        }
        size_t end = sstring.find_last_of('>');
        return sstring.substr(start, end - start);
    }
}
#endif


class CS_PROTO_API IDumpableObject {
public:
    virtual std::string dumpToString() const = 0;
    virtual bool loadFromString(const std::string& str) = 0;
    static bool dumpToFile(const std::string& filePath, IDumpableObject *obj);
    static bool loadFromFile(const std::string& filePath, IDumpableObject **obj);
    virtual ~IDumpableObject() = default;
    virtual std::string className() const = 0;
    static IDumpableObject *create(const std::string& className);

protected:
    static std::map<std::string, std::function<IDumpableObject *()>> mObjectMap;
};

std::map<std::string, std::function<IDumpableObject *()>> IDumpableObject::mObjectMap; 

inline IDumpableObject *IDumpableObject::create(const std::string& className) {
    auto it = mObjectMap.find(className);
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

    virtual std::string dumpToString() const override;
    virtual bool loadFromString(const std::string& str) override;

    std::string className() const override;
private:
    CS_USED static struct _init_data {
        _init_data();
    } _init__flag;
};
template <typename T, typename SerializerT>
DumpableObject<T, SerializerT>::_init_data::_init_data() {
    static_assert(std::is_base_of<DumpableObject<T, SerializerT>, T>::value, "T must be derived from DumpableObject<T, SerializerT>");
    CS_LOG_INFO("DumpableObject<{}, {}> init", typeid(T).name(), typeid(SerializerT).name());
    mObjectMap.insert(std::make_pair(_cs_class_name<T>(), [](){
        return new T();
    }));
}

template <typename T, typename SerializerT>
CS_USED typename DumpableObject<T, SerializerT>::_init_data DumpableObject<T, SerializerT>::_init__flag = {};

template <typename T, typename SerializerT>
std::string DumpableObject<T, SerializerT>::className() const {
    return _cs_class_name<T>();
}

template <typename T, typename SerializerT>
std::string DumpableObject<T, SerializerT>::dumpToString() const {
    SerializerT serializer;
    serializer.startSerialize();
    auto self = const_cast<T *>(static_cast<const T *>(this));
    auto ret = self->serialize(serializer);
    std::vector<char> data;
    if (!serializer.endSerialize(&data) || !ret) {
        CS_LOG_ERROR("DumpableObject({}) dumpToString failed", className());
        return "";
    }
    return std::string(data.begin(), data.end());
}

template <typename T, typename SerializerT>
bool DumpableObject<T, SerializerT>::loadFromString(const std::string& str) {
    SerializerT serializer;
    if (!serializer.startDeserialize(std::vector<char>(str.begin(), str.end()))) {
        return false;
    }
    auto self = dynamic_cast<const T*>(this);
    bool ret = const_cast<T*>(self)->deserialize(serializer);
    if (!serializer.endDeserialize() || !ret) {
        return false;
    }
    return true;
}

template <>
struct JsonConvert<IDumpableObject *> {
    static bool toJsonValue(JsonWriter& writer, const IDumpableObject *value) {
        auto ret = writer.StartObject();
        ret = writer.Key("className") && ret;
        ret = writer.String(value->className().c_str()) && ret;
        ret = writer.Key("value") && ret;
        auto str = value->dumpToString();
        ret = (!str.empty() && ret);
        ret = writer.RawValue(str.c_str(), str.length(), rapidjson::kObjectType) && ret;
        ret = writer.EndObject() && ret;
        return ret;
    }
    static bool fromJsonValue(IDumpableObject ** dst, const JsonValue& value) {
        if (!value.IsObject() || dst == nullptr) {
            return false;
        }
        if (!value.HasMember("className") || !value["className"].IsString()) {
            return false;
        }
        std::string className = value["className"].GetString();
        auto d = IDumpableObject::create(className);
        CS_ASSERT(d != nullptr, "DumpableObject {} create failed", className);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        const auto& o = value["value"];
        auto ret = o.Accept(writer);
        ret = d->loadFromString(buffer.GetString()) && ret;
        (*dst) =  d;
        return ret;
    }
};

bool IDumpableObject::dumpToFile(const std::string& filePath, IDumpableObject *obj) {
    auto buffer = rapidjson::StringBuffer();
    auto writer = std::make_shared<JsonWriter>(buffer);
    auto ret = JsonConvert<IDumpableObject *>::toJsonValue(*writer, obj);
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    fprintf(stderr, "dump file: %s\n", filePath.c_str());
    file.write(buffer.GetString(), buffer.GetSize());
    file.close();
    return ret;

}

bool IDumpableObject::loadFromFile(const std::string& filePath, IDumpableObject **obj) {
    std::ifstream file(filePath, std::ios::in);
    if (!file.is_open()) {
        return false;
    }
    rapidjson::IStreamWrapper isw(file);
    rapidjson::Document document;
    document.ParseStream(isw);
    auto ret = JsonConvert<IDumpableObject *>::fromJsonValue(obj, document.GetObject());
    file.close();
    return ret;
}

CS_PROTO_END_NAMESPACE