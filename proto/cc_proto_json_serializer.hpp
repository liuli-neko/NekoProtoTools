#pragma once

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <vector>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

using JsonValue = rapidjson::Value;
using JsonDocument = rapidjson::Document;
using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

template <typename T, class enable = void>
struct JsonConvert;

class JsonSerializer {
 public:
  JsonSerializer() = default;
  JsonSerializer(const JsonSerializer &) {}
  JsonSerializer(JsonSerializer &&other) {}
  JsonSerializer &operator=(const JsonSerializer &) { return *this; }
  JsonSerializer &operator=(JsonSerializer &&other) { return *this; }
  ~JsonSerializer() {}
  inline void startSerialize() {
    if (!mWriter) {
      mBuffer.Clear();
      mWriter.reset(new JsonWriter(mBuffer));
    }
    mWriter->StartObject();
  }
  inline bool endSerialize(std::vector<char> *data) {
    mWriter->EndObject();
    if (!mWriter || !mWriter->IsComplete()) {
      return false;
    }
    (*data) = std::vector<char>(mBuffer.GetString(),
                                  mBuffer.GetString() + mBuffer.GetSize());
    mBuffer.Clear();
    mWriter.reset();
    return true;
  }

  template <typename T>
  void insert(const char *name, const T &value) {
    mWriter->Key(name);
    JsonConvert<T>::toJsonValue(*mWriter, value);
  }

  inline bool startDeserialize(const std::vector<char> &data) {
    mDocument.Parse(data.data(), data.size());
    if (mDocument.HasParseError()) {
      CS_LOG_ERROR("parse error %d", mDocument.GetParseError());
      return false;
    }
    return true;
  }

  inline bool startDeserialize(const JsonValue &data) {
    mDocument.CopyFrom(data, mDocument.GetAllocator());
    return true;
  }

  inline void endDeserialize() {
    mDocument.SetNull();
    mDocument.GetAllocator().Clear();
  }

  template <typename T>
  bool get(const char *name, T *value) {
    if (!mDocument.HasMember(name)) {
      return false;
    }
    return JsonConvert<T>::fromJsonValue(value, mDocument[name]);
  }

 private:
  JsonDocument mDocument;
  rapidjson::StringBuffer mBuffer;
  std::unique_ptr<JsonWriter> mWriter;
};

template <>
struct JsonConvert<int, void> {
  static void toJsonValue(JsonWriter &writer, const int value) {
    writer.Int(value);
  }
  static bool fromJsonValue(int *dst, const JsonValue &value) {
    if (!value.IsInt() || dst == nullptr) {
      return false;
    }
    (*dst) = value.GetInt();
    return true;
  }
};

template <>
struct JsonConvert<std::string, void> {
  static void toJsonValue(JsonWriter &writer, const std::string &value) {
    writer.String(value.c_str(), value.size(), true);
  }
  static bool fromJsonValue(std::string *dst, const JsonValue &value) {
    if (!value.IsString() || dst == nullptr) {
      return false;
    }
    (*dst) = std::string(value.GetString(), value.GetStringLength());
    return true;
  }
};

template <typename T>
struct JsonConvert<std::vector<T>, void> {
  static void toJsonValue(JsonWriter &writer, const std::vector<T> &value) {
    writer.StartArray();
    for (auto &v : value) {
      JsonConvert<T>::toJsonValue(writer, v);
    }
    writer.EndArray();
  }
  static bool fromJsonValue(std::vector<T> *dst, const JsonValue &value) {
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
struct JsonConvert<T,typename std::enable_if<std::is_same<
                          typename T::SerializerType, JsonSerializer>::value>::type> {
  static void toJsonValue(JsonWriter &writer, const T &value) {
    auto data = value.serialize();
    writer.RawValue(data.data(), data.size(), rapidjson::kObjectType);
  }
  static bool fromJsonValue(T *dst, const JsonValue &value) {
    if (!value.IsObject() || dst == nullptr) {
      return false;
    }
    dst->deserialize(value);
    return true;
  }
};

template <>
struct JsonConvert<bool, void> {
  static void toJsonValue(JsonWriter &writer, const bool value) {
    writer.Bool(value);
  }
  static bool fromJsonValue(bool *dst, const JsonValue &value) {
    if (!value.IsBool() || dst == nullptr) {
      return false;
    }
    (*dst) = value.GetBool();
    return true;
  }
};

template <>
struct JsonConvert<double, void> {
  static void toJsonValue(JsonWriter &writer, const double value) {
    writer.Double(value);
  }
  static bool fromJsonValue(double *dst, const JsonValue &value) {
    if (!value.IsDouble() || dst == nullptr) {
      return false;
    }
    (*dst) = value.GetDouble();
    return true;
  }
};

template <>
struct JsonConvert<float, void> {
  static void toJsonValue(JsonWriter &writer, const float value) {
    writer.Double(value);
  }
  static bool fromJsonValue(float *dst, const JsonValue &value) {
    if (!value.IsDouble() || dst == nullptr) {
      return false;
    }
    (*dst) = value.GetDouble();
    return true;
  }
};

template <>
struct JsonConvert<long long int, void> {
  static void toJsonValue(JsonWriter &writer, const long long int value) {
    writer.Int64(value);
  }
  static bool fromJsonValue(long long int *dst, const JsonValue &value) {
    if (!value.IsInt64() || dst == nullptr) {
      return false;
    }
    (*dst) = value.GetInt64();
    return true;
  }
};

CS_PROTO_END_NAMESPACE