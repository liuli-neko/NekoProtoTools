/**
 * @file proto_json_serializer_container.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "json_serializer.hpp"

#include <array>
#include <list>
#include <map>
#include <set>

#ifdef GetObject
#undef GetObject
#endif

#if NEKO_CPP_PLUS >= 17
#include <variant>
#endif

#if NEKO_CPP_PLUS >= 20
#include <string>
#endif

NEKO_BEGIN_NAMESPACE

template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, std::list<T>, void> {
    static bool toJsonValue(WriterT& writer, const std::list<T>& value) {
        auto ret = writer.StartArray();
        for (auto& v : value) {
            ret = JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, v) && ret;
        }
        ret = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(std::list<T>* dst, const ValueT& value) {
        if (!value.IsArray() || dst == nullptr) {
            return false;
        }
        dst->clear();
        for (auto& v : value.GetArray()) {
            T t;
            if (!JsonConvert<WriterT, ValueT, T>::fromJsonValue(&t, v)) {
                return false;
            }
            dst->push_back(t);
        }
        return true;
    }
};

template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, std::set<T>, void> {
    static bool toJsonValue(WriterT& writer, const std::set<T>& value) {
        auto ret = writer.StartArray();
        for (auto& v : value) {
            ret = JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, v) && ret;
        }
        ret = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(std::set<T>* dst, const ValueT& value) {
        if (!value.IsArray() || dst == nullptr) {
            return false;
        }
        dst->clear();
        for (auto& v : value.GetArray()) {
            T t;
            if (!JsonConvert<WriterT, ValueT, T>::fromJsonValue(&t, v)) {
                return false;
            }
            dst->insert(t);
        }
        return true;
    }
};

#if NEKO_CPP_PLUS >= 20
template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, std::map<std::u8string, T>, void> {
    static bool toJsonValue(WriterT& writer, const std::map<std::u8string, T>& value) {
        auto ret = writer.StartObject();
        for (auto& v : value) {
            writer.Key(v.first.data(), v.first.size(), true);
            ret = JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, v.second) && ret;
        }
        ret = writer.EndObject() && ret;
        return ret;
    }
    static bool fromJsonValue(std::map<std::u8string, T>* dst, const ValueT& value) {
        if (!value.IsObject()) {
            return false;
        }
        JsonDocument doc;
        doc.CopyFrom(value, doc.GetAllocator());
        if (!doc.IsObject()) {
            return false;
        }
        dst->clear();
        for (auto& v : doc.GetObject()) {
            T t;
            if (!JsonConvert<WriterT, ValueT, T>::fromJsonValue(&t, v.value)) {
                return false;
            }
            dst->insert(std::make_pair(v.name.GetString(), t));
        }
        return true;
    }
};
#endif
template <typename WriterT, typename ValueT, typename T, size_t t>
struct JsonConvert<WriterT, ValueT, std::array<T, t>, void> {
    static bool toJsonValue(WriterT& writer, const std::array<T, t>& value) {
        auto ret = writer.StartArray();
        for (int i = 0; i < t; ++i) {
            ret = JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, value[i]) && ret;
        }
        ret = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(std::array<T, t>* dst, const ValueT& value) {
        if (!value.IsArray() || dst == nullptr) {
            return false;
        }
        auto vars = value.GetArray();
        for (int i = 0; i < t; ++i) {
            if (!JsonConvert<WriterT, ValueT, T>::fromJsonValue(&((*dst)[i]), vars[i])) {
                return false;
            }
        }
        return true;
    }
};

template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, std::map<std::string, T>, void> {
    static bool toJsonValue(WriterT& writer, const std::map<std::string, T>& value) {
        auto ret = writer.StartObject();
        for (auto& v : value) {
            ret = writer.Key(v.first.c_str(), v.first.size(), true) && ret;
            ret = JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, v.second) && ret;
        }
        ret = writer.EndObject() && ret;
        return ret;
    }
    static bool fromJsonValue(std::map<std::string, T>* dst, const ValueT& value) {
        if (!value.IsObject()) {
            return false;
        }
        JsonDocument doc;
        doc.CopyFrom(value, doc.GetAllocator());
        if (!doc.IsObject()) {
            return false;
        }
        dst->clear();
        for (auto& v : doc.GetObject()) {
            T t;
            if (!JsonConvert<WriterT, ValueT, T>::fromJsonValue(&t, v.value)) {
                return false;
            }
            dst->insert(std::make_pair(std::string(v.name.GetString(), v.name.GetStringLength()), t));
        }
        return true;
    }
};

template <typename WriterT, typename ValueT, typename... Arg>
struct JsonConvert<WriterT, ValueT, std::tuple<Arg...>> {
    template <typename TupleT, size_t N>
    struct toJsonValueImp {
        using Type = typename std::tuple_element<N - 1, TupleT>::type;
        static bool toJsonValue(WriterT& writer, const TupleT& value) {
            auto ret = toJsonValueImp<TupleT, N - 1>::toJsonValue(writer, value);
            ret      = JsonConvert<WriterT, ValueT, Type>::toJsonValue(writer, std::get<N - 1>(value)) && ret;
            return ret;
        }
    };
    template <typename TupleT>
    struct toJsonValueImp<TupleT, 1> {
        using Type = typename std::tuple_element<0, TupleT>::type;
        static bool toJsonValue(WriterT& writer, const TupleT& value) {
            return JsonConvert<WriterT, ValueT, Type>::toJsonValue(writer, std::get<0>(value));
        }
    };
    template <typename TupleT, size_t N>
    struct fromJsonValueImp {
        using Type = typename std::tuple_element<N - 1, TupleT>::type;
        static bool fromJsonValue(TupleT* dst, const ValueT& value) {
            bool ret = fromJsonValueImp<TupleT, N - 1>::fromJsonValue(dst, value);
            ret      = JsonConvert<WriterT, ValueT, Type>::fromJsonValue(&(std::get<N - 1>(*dst)), value[N - 1]) && ret;
            return ret;
        }
    };
    template <typename TupleT>
    struct fromJsonValueImp<TupleT, 1> {
        using Type = typename std::tuple_element<0, TupleT>::type;
        static bool fromJsonValue(TupleT* dst, const ValueT& value) {
            return JsonConvert<WriterT, ValueT, Type>::fromJsonValue(&(std::get<0>(*dst)), value[0]);
        }
    };
    static bool toJsonValue(WriterT& writer, const std::tuple<Arg...>& value) {
        auto ret = writer.StartArray();
        ret      = toJsonValueImp<std::tuple<Arg...>, sizeof...(Arg)>::toJsonValue(writer, value) && ret;
        ret      = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(std::tuple<Arg...>* dst, const ValueT& value) {
        if (dst == nullptr || !value.IsArray()) {
            return false;
        }
        return fromJsonValueImp<std::tuple<Arg...>, sizeof...(Arg)>::fromJsonValue(dst, value);
    }
};

NEKO_END_NAMESPACE