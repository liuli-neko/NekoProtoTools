#pragma once

#include <vector>
#include <functional>
#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

template <bool isSerlizer, typename SerializerT, typename T>
bool unfoldFunctionImp(SerializerT &serializer, const std::vector<std::string> &names, int i, T& value) {
    CS_ASSERT(i < names.size(), "unfoldFunctionImp: index out of range");
#if CS_CPP_PULS >= 17
    if constexpr (isSerlizer)
#else
    if (isSerlizer)
#endif
        return serializer.insert(names[i].c_str(), value);
    else
        return serializer.get(names[i].c_str(), &value);
    
}

template <bool isSerlizer, typename SerializerT, typename T, typename ...Args>
bool unfoldFunctionImp(SerializerT &serializer, const std::vector<std::string> &names, int i, T& value, Args& ...args) {
    bool ret = 0;
    CS_ASSERT(i < names.size(), "unfoldFunctionImp: index out of range");
#if CS_CPP_PULS >= 17
    if constexpr (isSerlizer)
#else
    if (isSerlizer)
#endif
        ret = serializer.insert(names[i].c_str(), value);
    else
        ret = serializer.get(names[i].c_str(), &value);

    return unfoldFunctionImp<isSerlizer, SerializerT, std::decay_t<Args>...>(serializer, names, i + 1, args...) && ret;
}

template <bool isSerlizer, typename SerializerT, typename ...Args>
bool unfoldFunction(SerializerT &serializer, const char *names, Args& ...args) {
    std::string namesStr(names);
    std::vector<std::string> namesVec;
    auto begin = 0;
    auto end = namesStr.find_first_of(',', begin + 1);
    while (end != std::string::npos) {
        auto bbegin = namesStr.find_first_not_of(' ', begin);
        auto eend = namesStr.find_last_not_of(' ', end);
        std::string token = namesStr.substr(bbegin, eend - bbegin);
        namesVec.push_back(token);
        begin = end + 1;
        end = namesStr.find_first_of(',', begin + 1);
    }
    if (begin != namesStr.size()) {
        auto bbegin = namesStr.find_first_not_of(' ', begin);
        auto eend = namesStr.back() == ' ' ? namesStr.find_last_not_of(' ') : namesStr.size();
        std::string token = namesStr.substr(bbegin, eend - bbegin);
        namesVec.push_back(token);
    }
    int i = 0;
    return unfoldFunctionImp<isSerlizer, SerializerT, std::decay_t<Args>...>(serializer, namesVec, i, args...);
}

CS_PROTO_END_NAMESPACE

#define CS_SERIALIZER(...)                                                                                                  \
public:                                                                                                                     \
    template <typename SerializerT>                                                                                         \
    bool serialize(SerializerT &serializer) {                                                                               \
        return CS_PROTO_NAMESPACE::unfoldFunction<true, SerializerT>(serializer, #__VA_ARGS__, __VA_ARGS__);                \
    }                                                                                                                       \
    template <typename SerializerT>                                                                                         \
    bool deserialize(SerializerT &serializer) {                                                                             \
        return CS_PROTO_NAMESPACE::unfoldFunction<false, SerializerT>(serializer, #__VA_ARGS__, __VA_ARGS__);               \
    }
