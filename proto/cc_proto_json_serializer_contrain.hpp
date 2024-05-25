#pragma once

#include "cc_proto_json_serializer.hpp"

#include <array>
#include <list>
#include <map>
#include <set>

#ifdef GetObject
#undef GetObject
#endif

#if CS_CPP_PLUS >= 20
#include <string>
#endif

CS_PROTO_BEGIN_NAMESPACE

template <typename T>
struct JsonConvert<std::list<T>, void> {
  static bool toJsonValue(JsonWriter &writer, const std::list<T> &value) {
    auto ret = writer.StartArray();
    for (auto &v : value) {
      ret = JsonConvert<T>::toJsonValue(writer, v) && ret;
    }
    ret = writer.EndArray() && ret;
    return ret;
  }
  static bool fromJsonValue(std::list<T> *dst, const JsonValue &value) {
    if (!value.IsArray() || dst == nullptr) {
      return false;
    }
    dst->clear();
    for (auto &v : value.GetArray()) {
      T t;
      if (!JsonConvert<T>::fromJsonValue(&t, v)) {
        return false;
      }
      dst->push_back(t);
    }
    return true;
  }
};

template <typename T>
struct JsonConvert<std::set<T>, void> {
  static bool toJsonValue(JsonWriter &writer, const std::set<T> &value) {
    auto ret = writer.StartArray();
    for (auto &v : value) {
      ret = JsonConvert<T>::toJsonValue(writer, v) && ret;
    }
    ret = writer.EndArray() && ret;
    return ret;
  }
  static bool fromJsonValue(std::set<T> *dst, const JsonValue &value) {
    if (!value.IsArray() || dst == nullptr) {
      return false;
    }
    dst->clear();
    for (auto &v : value.GetArray()) {
      T t;
      if (!JsonConvert<T>::fromJsonValue(&t, v)) {
        return false;
      }
      dst->insert(t);
    }
    return true;
  }
};

#if CS_CPP_PLUS >= 20
template <typename T>
struct JsonConvert<std::map<std::u8string, T>, void> {
  static bool toJsonValue(JsonWriter &writer,
                          const std::map<std::u8string, T> &value) {
    auto ret = writer.StartObject();
    for (auto &v : value) {
      writer.Key(v.first.data(), v.first.size(), true);
      ret = JsonConvert<T>::toJsonValue(writer, v.second) && ret;
    }
    ret = writer.EndObject() && ret;
    return ret;
  }
  static bool fromJsonValue(std::map<std::u8string, T> *dst,
                            const JsonValue &value) {
    if (!value.IsObject()) {
      return false;
    }
    JsonDocument doc;
    doc.CopyFrom(value, doc.GetAllocator());
    if (!doc.IsObject()) {
      return false;
    }
    dst->clear();
    for (auto &v : doc.GetObject()) {
      T t;
      if (!JsonConvert<T>::fromJsonValue(&t, v.value)) {
        return false;
      }
      dst->insert(std::make_pair(v.name.GetString(), t));
    }
    return true;
  }
};
#endif
template <typename T, size_t t>
struct JsonConvert<std::array<T, t>, void> {
  static bool toJsonValue(JsonWriter &writer, const std::array<T, t> &value) {
    auto ret = writer.StartArray();
    for (int i = 0; i < t; ++i) {
      ret = JsonConvert<T>::toJsonValue(writer, value[i]) && ret;
    }
    ret = writer.EndArray() && ret;
    return ret;
  }
  static bool fromJsonValue(std::array<T, t> *dst, const JsonValue &value) {
    if (!value.IsArray() || dst == nullptr) {
      return false;
    }
    auto vars = value.GetArray();
    for (int i = 0; i < t; ++i) {
      if (!JsonConvert<T>::fromJsonValue(&((*dst)[i]), vars[i])) {
        return false;
      }
    }
    return true;
  }
};

template <typename T>
struct JsonConvert<std::map<std::string, T>, void> {
  static bool toJsonValue(JsonWriter &writer,
                          const std::map<std::string, T> &value) {
    auto ret = writer.StartObject();
    for (auto &v : value) {
      ret = writer.Key(v.first.c_str(), v.first.size(), true) && ret;
      ret = JsonConvert<T>::toJsonValue(writer, v.second) && ret;
    }
    ret = writer.EndObject() && ret;
    return ret;
  }
  static bool fromJsonValue(std::map<std::string, T> *dst,
                            const JsonValue &value) {
    if (!value.IsObject()) {
      return false;
    }
    JsonDocument doc;
    doc.CopyFrom(value, doc.GetAllocator());
    if (!doc.IsObject()) {
      return false;
    }
    dst->clear();
    for (auto &v : doc.GetObject()) {
      T t;
      if (!JsonConvert<T>::fromJsonValue(&t, v.value)) {
        return false;
      }
      dst->insert(std::make_pair(
          std::string(v.name.GetString(), v.name.GetStringLength()), t));
    }
    return true;
  }
};


template <typename ...Arg>
struct JsonConvert<std::tuple<Arg...>> {
  template <typename TupleT, size_t N>
  struct toJsonValueImp {
    using Type = typename std::tuple_element<N - 1, TupleT>::type;
    static bool toJsonValue(JsonWriter &writer,const TupleT &value) {
        auto ret = toJsonValueImp<TupleT, N-1>::toJsonValue(writer, value);
        ret = JsonConvert<Type>::toJsonValue(writer, std::get<N-1>(value)) && ret;
        return ret;
    }
  };
  template <typename TupleT>
  struct toJsonValueImp<TupleT, 1> {
    using Type = typename std::tuple_element<0, TupleT>::type;
    static bool toJsonValue(JsonWriter &writer,const TupleT &value) {
        return JsonConvert<Type>::toJsonValue(writer, std::get<0>(value));
    }
  };
  template <typename TupleT, size_t N>
  struct fromJsonValueImp {
    using Type = typename std::tuple_element<N - 1, TupleT>::type;
    static bool fromJsonValue(TupleT *dst, const JsonValue &value) {
        bool ret = fromJsonValueImp<TupleT, N-1>::fromJsonValue(dst, value);
        ret = JsonConvert<Type>::fromJsonValue(&(std::get<N-1>(*dst)), value[N-1]) && ret;
        return ret;
    }
  };
  template <typename TupleT>
  struct fromJsonValueImp<TupleT, 1> {
    using Type = typename std::tuple_element<0, TupleT>::type;
    static bool fromJsonValue(TupleT *dst, const JsonValue &value) {
        return JsonConvert<Type>::fromJsonValue(&(std::get<0>(*dst)), value[0]);
    }
  };
  static bool toJsonValue(JsonWriter &writer, const std::tuple<Arg...> &value) {
    auto ret = writer.StartArray();
    ret = toJsonValueImp<std::tuple<Arg...>, sizeof...(Arg)>::toJsonValue(writer, value) && ret;
    ret = writer.EndArray() && ret;
    return ret;
  }
  static bool fromJsonValue(std::tuple<Arg...> *dst, const JsonValue &value) {
    if (dst == nullptr || !value.IsArray()) {
        return false;
    }
    return fromJsonValueImp<std::tuple<Arg...>, sizeof...(Arg)>::fromJsonValue(dst, value);
  }
};

CS_PROTO_END_NAMESPACE