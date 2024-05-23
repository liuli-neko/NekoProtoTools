#pragma once

#include <vector>
#include <functional>
#include <string>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

#if CS_CPP_PLUS >= 17
template <int N, typename SerializerT, typename T>
inline bool unfoldFunctionImp1(SerializerT &serializer, const std::array<std::string_view, N> &names, int& i, T& value) {
    CS_ASSERT(i < names.size(), "unfoldFunctionImp: index out of range");
    ++i;
    return serializer.get(names[i - 1].data(), names[i - 1].size(), &value);
    
}
template <int N, typename SerializerT, typename T>
inline bool unfoldFunctionImp2(SerializerT &serializer, const std::array<std::string_view, N> &names, int& i,const T& value) {
    CS_ASSERT(i < names.size(), "unfoldFunctionImp: index out of range");
    ++i;
    return serializer.insert(names[i - 1].data(), names[i - 1].size(), value);
}

inline constexpr int membersSize(std::string_view names) {
    int count = 0;
    auto begin = 0;
    auto end = names.find_first_of(',', begin + 1);
    while (end != std::string::npos) {
        begin = end + 1;
        end = names.find_first_of(',', begin + 1);
        count ++;
    }
    if (begin != names.size()) {
        count ++;
    }
    return count;
}
template <int N>
inline constexpr std::array<std::string_view, N> parseNames(std::string_view names) {
    std::array<std::string_view, N> namesVec;
    auto begin = 0;
    auto end = names.find_first_of(',', begin + 1);
    int i = 0;
    while (end != std::string_view::npos) {
        auto bbegin = names.find_first_not_of(' ', begin);
        auto eend = names.find_last_not_of(' ', end);
        std::string_view token = names.substr(bbegin, eend - bbegin);
        namesVec[i++] = token;
        begin = end + 1;
        end = names.find_first_of(',', begin + 1);
    }
    if (begin != names.size()) {
        auto bbegin = names.find_first_not_of(' ', begin);
        auto eend = names.back() == ' ' ? names.find_last_not_of(' ') : names.size();
        std::string_view token = names.substr(bbegin, eend - bbegin);
        namesVec[N - 1] = token;
    }
    return std::move(namesVec);
}
template <int N, typename SerializerT, typename ...Args>
inline bool unfoldFunction1(SerializerT &serializer,const std::array<std::string_view, N> &namesVec, Args& ...args) {
    int i = 0;
    return (unfoldFunctionImp1<N, SerializerT, std::decay_t<Args>>(serializer, namesVec, i, args) + ...) == N;
}
template <int N, typename SerializerT, typename ...Args>
inline bool unfoldFunction2(SerializerT &serializer,const std::array<std::string_view, N> &namesVec, const Args& ...args) {
    int i = 0;
    return (unfoldFunctionImp2<N, SerializerT, std::decay_t<Args>>(serializer, namesVec, i, args) + ...) == N;
}
#else

inline std::vector<std::pair<size_t, size_t>> parseNames(const char *names) {
    std::string namesStr(names);
    std::vector<std::pair<size_t, size_t>> namesVec;
    auto begin = 0;
    auto end = namesStr.find_first_of(',', begin + 1);
    while (end != std::string::npos) {
        auto bbegin = namesStr.find_first_not_of(' ', begin);
        auto eend = namesStr.find_last_not_of(' ', end);
        namesVec.push_back(std::make_pair(bbegin, eend - bbegin));
        begin = end + 1;
        end = namesStr.find_first_of(',', begin + 1);
    }
    if (begin != namesStr.size()) {
        auto bbegin = namesStr.find_first_not_of(' ', begin);
        auto eend = namesStr.back() == ' ' ? namesStr.find_last_not_of(' ') : namesStr.size();
        namesVec.push_back(std::make_pair(bbegin, eend - bbegin));
    }
    return std::move(namesVec);
}

template <typename SerializerT, typename T>
inline bool unfoldFunctionImp1(SerializerT &serializer, const char *names, const std::vector<std::pair<size_t, size_t>> &namesVec, int i, const T& value) {
    CS_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    return serializer.insert(names + namesVec[i].first, namesVec[i].second, value);
    
}

template <typename SerializerT, typename T, typename ...Args>
inline bool unfoldFunctionImp1(SerializerT &serializer, const char *names, const std::vector<std::pair<size_t, size_t>> &namesVec, int i, const T& value, const Args& ...args) {
    bool ret = 0;
    CS_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    ret = serializer.insert(names + namesVec[i].first, namesVec[i].second, value);

    return unfoldFunctionImp1<SerializerT>(serializer, names, namesVec, i + 1, args...) && ret;
}
template <typename SerializerT, typename ...Args>
inline bool unfoldFunction1(SerializerT &serializer, const char *names, const std::vector<std::pair<size_t, size_t>> &namesVec, Args& ...args) {
    int i = 0;
    return unfoldFunctionImp1<SerializerT>(serializer, names, namesVec, i, args...);
}

template <typename SerializerT, typename T>
inline bool unfoldFunctionImp2(SerializerT &serializer, const char *names, const std::vector<std::pair<size_t, size_t>> &namesVec, int i, T& value) {
    CS_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    return serializer.get(names + namesVec[i].first, namesVec[i].second, &value);
    
}

template <typename SerializerT, typename T, typename ...Args>
inline bool unfoldFunctionImp2(SerializerT &serializer, const char *names, const std::vector<std::pair<size_t, size_t>> &namesVec, int i, T& value, Args& ...args) {
    bool ret = 0;
    CS_ASSERT(i < namesVec.size(), "unfoldFunctionImp: index out of range");
    ret = serializer.get(names + namesVec[i].first, namesVec[i].second, &value);

    return unfoldFunctionImp2<SerializerT>(serializer, names, namesVec, i + 1, args...) && ret;
}
template <typename SerializerT, typename ...Args>
inline bool unfoldFunction2(SerializerT &serializer, const char *names, const std::vector<std::pair<size_t, size_t>> &namesVec, Args& ...args) {
    int i = 0;
    return unfoldFunctionImp2<SerializerT>(serializer, names, namesVec, i, args...);
}
#endif
CS_PROTO_END_NAMESPACE

#if CS_CPP_PLUS < 17
#define CS_SERIALIZER(...)                                                                                                  \
public:                                                                                                                     \
    template <typename SerializerT>                                                                                         \
    bool serialize(SerializerT &serializer) const {                                                                         \
        static auto names = CS_PROTO_NAMESPACE::parseNames(#__VA_ARGS__);                                                   \
        return CS_PROTO_NAMESPACE::unfoldFunction1<SerializerT>(serializer, #__VA_ARGS__, names, __VA_ARGS__);              \
    }                                                                                                                       \
    template <typename SerializerT>                                                                                         \
    bool deserialize(SerializerT &serializer) {                                                                             \
        static auto names = CS_PROTO_NAMESPACE::parseNames(#__VA_ARGS__);                                                   \
        return CS_PROTO_NAMESPACE::unfoldFunction2<SerializerT>(serializer, #__VA_ARGS__, names, __VA_ARGS__);              \
    }
#else
#define CS_SERIALIZER(...)                                                                                                  \
private:                                                                                                                    \
        static inline constexpr std::array<std::string_view, CS_PROTO_NAMESPACE::membersSize(#__VA_ARGS__)> _names__ =      \
            CS_PROTO_NAMESPACE::parseNames<CS_PROTO_NAMESPACE::membersSize(#__VA_ARGS__)>(#__VA_ARGS__);                    \
public:                                                                                                                     \
    template <typename SerializerT>                                                                                         \
    bool serialize(SerializerT &serializer) const {                                                                         \
        return CS_PROTO_NAMESPACE::unfoldFunction2<CS_PROTO_NAMESPACE::membersSize(#__VA_ARGS__), SerializerT>(             \
                serializer, _names__, __VA_ARGS__);                                                                         \
    }                                                                                                                       \
    template <typename SerializerT>                                                                                         \
    bool deserialize(SerializerT &serializer) {                                                                             \
        return CS_PROTO_NAMESPACE::unfoldFunction1<CS_PROTO_NAMESPACE::membersSize(#__VA_ARGS__), SerializerT>(             \
                serializer, _names__, __VA_ARGS__);                                                                         \
    }
#endif