#pragma once

#include <signal.h>

#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstring>

#include "cc_proto_global.hpp"

CS_PROTO_BEGIN_NAMESPACE

class ProtoFactory;
template <typename T>
constexpr int _proto_type();
template <typename T>
constexpr const char *_proto_name();

class IProto {
 public:
  IProto() = default;
  virtual ~IProto() {}
  virtual std::vector<char> serialize() const = 0;
  virtual void deserialize(const std::vector<char> &) = 0;
  virtual int type() const = 0;
};

#if __cplusplus >= 202100L
template <typename T>
concept Proto = requires(T x) {
  T();
  { x.serialize() } -> std::convertible_to<std::vector<char>>;
  { x.deserialize(std::vector<char>()) } -> std::convertible_to<void>;
};
#endif

class JsonSerializer;
template <typename T, typename SerializerT = JsonSerializer>
class ProtoBase : public IProto {
 public:
  using ProtoType = T;
  using SerializerType = SerializerT;

  inline ProtoBase() = default;

  inline ProtoBase(const ProtoBase &other) { mSerializer = other.mSerializer; }

  inline ProtoBase(ProtoBase &&other) {
    mSerializer = std::move(other.mSerializer);
  }

  inline ProtoBase &operator=(const ProtoBase &other) {
    mSerializer = other.mSerializer;
    return *this;
  }

  inline ProtoBase operator==(ProtoBase &&other) {
    mSerializer = std::move(other.mSerializer);
    return *this;
  }

  inline std::vector<char> serialize() const override {
    mSerializer.startSerialize();
    for (auto item : mSerializeVistor) {
      (item.second)(const_cast<ProtoBase *>(this));
    }
    std::vector<char> ret;
    if (!mSerializer.endSerialize(&ret)) {
      LOG("serialize error");
    }
    return std::move(ret);
  }

  inline void deserialize(const std::vector<char> &data) override {
    mSerializer.startDeserialize(data);
    for (auto item : mDeserializeVistor) {
      (item.second)(const_cast<ProtoBase *>(this));
    }
    mSerializer.endDeserialize();
  }

  template <typename U>
  void deserialize(const U &data) {
    mSerializer.startDeserialize(data);
    for (auto item : mDeserializeVistor) {
      (item.second)(const_cast<ProtoBase *>(this));
    }
    mSerializer.endDeserialize();
  }

  inline int type() const override { return _proto_type<T>(); }

  virtual ~ProtoBase() {}

  inline SerializerT *serializer() { return &mSerializer; }

 protected:
  mutable SerializerT mSerializer;
  struct less {
    bool operator()(const char *a, const char *b) const {
      return ::strcmp(a, b) < 0;
    }
  };
  static std::map<const char *, std::function<void(ProtoBase *)>, less>
      mSerializeVistor;

  static std::map<const char *, std::function<void(ProtoBase *)>, less>
      mDeserializeVistor;
};

template <typename T, typename SerializerT>
std::map<const char *, std::function<void(ProtoBase<T, SerializerT> *)>, typename ProtoBase<T, SerializerT>::less>
    ProtoBase<T, SerializerT>::mSerializeVistor;
template <typename T, typename SerializerT>
std::map<const char *, std::function<void(ProtoBase<T, SerializerT> *)>, typename ProtoBase<T, SerializerT>::less>
    ProtoBase<T, SerializerT>::mDeserializeVistor;


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


#define CS_PROPERTY_FIELD(type, name)                                         \
    __CS_DECLARE_PROTO_FIELD1(name)                                           \
public:                                                                       \
    inline const type &name() const { return m_##name; }                      \
    inline type & mutable_##name() { return m_##name; }                       \
    inline void set_##name(const type &value) { m_##name = value; }           \
    inline void set_##name(type &&value) { m_##name = std::move(value); }     \
private:                                                                      \
    type m_##name                                                             \

#define CS_PROPERTY_CONTAINER_FIELD(type, name)                               \
    __CS_DECLARE_PROTO_FIELD1(name)                                           \
public:                                                                       \
    inline const type &name() const { return m_##name; }                      \
    inline const ContainerTraitsHelper<type>::value_type                      \
        &name(const ContainerTraitsHelper<type>::key_type &index)             \
        { return m_##name[index]; }                                           \
    inline type & mutable_##name() { return m_##name; }                       \
    inline const ContainerTraitsHelper<type>::value_type                      \
        &mutable_##name(const ContainerTraitsHelper<type>::key_type &index)   \
        { return m_##name[index]; }                                           \
    inline void set_##name(const type &value) { m_##name = value; }           \
    inline void set_##name(type &&value) { m_##name = std::move(value); }     \
private:                                                                      \
    type m_##name                                                             \

#define __CS_DECLARE_PROTO_FIELD1(name)                                       \
 private:                                                                     \
  template <typename T>                                                       \
  inline static void _regist_field_##name(T *) {                              \
    static bool _init_##name = false;                                         \
    if (_init_##name) { return; }                                             \
    _init_##name = true;                                                      \
    T::mSerializeVistor.insert(std::make_pair(#name, [](ProtoBase<T> *self) { \
      self->serializer()->insert(#name,                                       \
      static_cast<const T *>(self)->name());                                  \
    }));                                                                      \
    T::mDeserializeVistor.insert(                                             \
    std::make_pair(#name, [](ProtoBase<T> *self) {                            \
        self->serializer()->get(#name,                                        \
        &(static_cast<T *>(self)->mutable_##name()));                         \
    }));                                                                      \
  }                                                                           \
  bool _init_##name = [this]() {                                              \
    _regist_field_##name(this);                                               \
    return true;                                                              \
  }();


#define CS_DECLARE_PROTO_FIELD(name)                                          \
 private:                                                                     \
  template <typename T>                                                       \
  inline static void _regist_field_##name(T *) {                              \
    static bool _init_##name = false;                                         \
    if (_init_##name) { return; }                                             \
    _init_##name = true;                                                      \
    T::mSerializeVistor.insert(std::make_pair(#name, [](ProtoBase<T> *self) { \
      self->serializer()->insert(#name, static_cast<const T *>(self)->name);  \
    }));                                                                      \
    T::mDeserializeVistor.insert(                                             \
    std::make_pair(#name, [](ProtoBase<T> *self) {                            \
        self->serializer()->get(#name, &(static_cast<T *>(self)->name));      \
    }));                                                                      \
  }                                                                           \
  bool _init_##name = [this]() {                                              \
    _regist_field_##name(this);                                               \
    return true;                                                              \
  }();

#ifdef __GNU__
#define CS_DECLARE_PROTO_FIELDS(...) FOR_EACH(CS_DECLARE_PROTO_FIELD, __VA_ARGS__)
#endif

class ProtoFactory {
 public:
  ProtoFactory(int major = 0, int minor = 0, int patch = 1);
  ~ProtoFactory();
  void init();

#if __cplusplus >= 202100L
  template <Proto T>
#else
  template <typename T>
#endif
  void regist(const char *name) {
    kProtoNameMap.insert(std::make_pair(_proto_name<T>(), proto_type<T>()));
    kProtoMap.insert(std::make_pair(proto_type<T>(), creater<T>));
  }

#if __cplusplus >= 202100L
  template <Proto T>
#else
  template <typename T>
#endif
  inline constexpr static int proto_type() {
    return _proto_type<T>();
  }

#if __cplusplus >= 202100L
  template <Proto T>
#else
  template <typename T>
#endif
  inline constexpr static const char *proto_name() {
    return _proto_name<T>();
  }

  inline IProto *create(int type) {
    LOG("create by type: %d", type);
    auto item = kProtoMap.find(type);
    if (kProtoMap.end() != item) {
      return (item->second)();
    }
    return nullptr;
  }

  inline IProto *create(const char *name) {
    LOG("create by name: %s", name);
    auto item = kProtoNameMap.find(name);
    if (kProtoNameMap.end() != item) {
      LOG("find type(%d) by name: %s", item->second, name);
      return kProtoMap[item->second]();
    }
    return nullptr;
  }

#if __cplusplus >= 202100L
  template <Proto T>
#else
  template <typename T>
#endif
  inline T *create() {
    return new T();
  }

  uint32_t version();

 private:
  std::map<int, std::function<IProto *()>> kProtoMap;
  std::map<const char *, int> kProtoNameMap;
  uint32_t kCounter = 0;
  uint32_t kVersion = 0;

#if __cplusplus >= 202100L
  template <Proto T>
#else
  template <typename T>
#endif
  inline static IProto *creater() {
    return new T();
  }
  void setVersion(int major, int minor, int patch);
};

std::vector<std::function<void(ProtoFactory *)>> static_init_funcs(
    std::function<void(ProtoFactory *)>);

CS_PROTO_END_NAMESPACE

#define CS_PROTO_SET_TYPE_START(start)                                         \
namespace {                                                                    \
    constexpr const static int _proto_type_start = start - __COUNTER__ - 1;    \
}                                                                              \

#define CS_DECLARE_PROTO(type, name)                                           \
  CS_PROTO_BEGIN_NAMESPACE                                                     \
  template <>                                                                  \
  constexpr int _proto_type<type>() {                                          \
    return _proto_type_start + __COUNTER__;                                    \
  }                                                                            \
  template <>                                                                  \
  constexpr const char *_proto_name<type>() {                                  \
    return name;                                                               \
  }                                                                            \
  namespace {                                                                  \
  struct _regist_##type {                                                      \
    _regist_##type() {                                                         \
      static_init_funcs([](ProtoFactory *self) { self->regist<type>(name); }); \
    }                                                                          \
  } _regist_##type##__tmp;                                                     \
  }                                                                            \
  CS_PROTO_END_NAMESPACE
