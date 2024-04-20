#pragma once

#include "cc_proto_json_serializer.hpp"

#include <array>
#include <list>
#include <map>
#include <set>

CS_PROTO_BEGIN_NAMESPACE

template <typename T>
struct JsonConvert<std::list<T>, void> {
  static void toJsonValue(JsonWriter &writer, const std::list<T> &value) {
    writer.StartArray();
    for (auto &v : value) {
      JsonConvert<T>::toJsonValue(writer, v);
    }
    writer.EndArray();
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
  static void toJsonValue(JsonWriter &writer, const std::set<T> &value) {
    writer.StartArray();
    for (auto &v : value) {
      JsonConvert<T>::toJsonValue(writer, v);
    }
    writer.EndArray();
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

template <typename T>
struct JsonConvert<std::map<const char *, T>, void> {
  static void toJsonValue(JsonWriter &writer,
                          const std::map<const char *, T> &value) {
    rapidjson::StringBuffer mBuffer;
    JsonWriter mwriter(mBuffer);
    mwriter.StartObject();
    for (auto &v : value) {
      mwriter.Key(v.first);
      JsonConvert<T>::toJsonValue(mwriter, v.second);
    }
    mwriter.EndObject();
    writer.RawValue(mBuffer.GetString(), mBuffer.GetSize(),
                    rapidjson::kObjectType);
  }
  static bool fromJsonValue(std::map<const char *, T> *dst,
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

template <typename T, size_t t>
struct JsonConvert<std::array<T, t>, void> {
  static void toJsonValue(JsonWriter &writer, const std::array<T, t> &value) {
    writer.StartArray();
    for (int i = 0; i < t; ++i) {
      JsonConvert<T>::toJsonValue(writer, value[i]);
    }
    writer.EndArray();
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
  static void toJsonValue(JsonWriter &writer,
                          const std::map<std::string, T> &value) {
    rapidjson::StringBuffer mBuffer;
    JsonWriter mwriter(mBuffer);
    mwriter.StartObject();
    for (auto &v : value) {
      mwriter.Key(v.first.c_str(), v.first.size(), true);
      JsonConvert<T>::toJsonValue(mwriter, v.second);
    }
    mwriter.EndObject();
    writer.RawValue(mBuffer.GetString(), mBuffer.GetSize(),
                    rapidjson::kObjectType);
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
    static void toJsonValue(JsonWriter &writer,const TupleT &value) {
        toJsonValueImp<TupleT, N-1>::toJsonValue(writer, value);
        JsonConvert<Type>::toJsonValue(writer, std::get<N-1>(value));
    }
  };
  template <typename TupleT>
  struct toJsonValueImp<TupleT, 1> {
    using Type = typename std::tuple_element<0, TupleT>::type;
    static void toJsonValue(JsonWriter &writer,const TupleT &value) {
        JsonConvert<Type>::toJsonValue(writer, std::get<0>(value));
    }
  };
  template <typename TupleT, size_t N>
  struct fromJsonValueImp {
    using Type = typename std::tuple_element<N - 1, TupleT>::type;
    static void fromJsonValue(TupleT *dst, const JsonValue &value) {
        fromJsonValueImp<TupleT, N-1>::fromJsonValue(dst, value);
        JsonConvert<Type>::fromJsonValue(&(std::get<N-1>(*dst)), value[N-1]);
    }
  };
  template <typename TupleT>
  struct fromJsonValueImp<TupleT, 1> {
    using Type = typename std::tuple_element<0, TupleT>::type;
    static void fromJsonValue(TupleT *dst, const JsonValue &value) {
        JsonConvert<Type>::fromJsonValue(&(std::get<0>(*dst)), value[0]);
    }
  };
  static void toJsonValue(JsonWriter &writer, const std::tuple<Arg...> &value) {
    writer.StartArray();
    toJsonValueImp<std::tuple<Arg...>, sizeof...(Arg)>::toJsonValue(writer, value);
    writer.EndArray();
  }
  static bool fromJsonValue(std::tuple<Arg...> *dst, const JsonValue &value) {
    if (dst == nullptr || !value.IsArray()) {
        return false;
    }
    fromJsonValueImp<std::tuple<Arg...>, sizeof...(Arg)>::fromJsonValue(dst, value);
    return true;
  }
};

CS_PROTO_END_NAMESPACE