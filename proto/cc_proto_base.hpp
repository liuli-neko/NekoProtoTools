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

class CS_PROTO_API IProto {
 public:
  IProto() = default;
  virtual ~IProto() {}
  virtual std::vector<char> serialize() const = 0;
  virtual bool deserialize(const std::vector<char> &) = 0;
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
  using ProtoBaseType = ProtoBase;

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
      if (!(item.second)(const_cast<ProtoBase *>(this))) {
        CS_LOG_WARN("field {} serialize error", item.first);
      }
    }
    std::vector<char> ret;
    if (!mSerializer.endSerialize(&ret)) {
      CS_LOG_ERROR("Proto({}) serialize error", _proto_name<T>());
    }
    return std::move(ret);
  }

  inline bool deserialize(const std::vector<char> &data) override {
    if (!mSerializer.startDeserialize(data)) {
      return false;
    }
    bool ret = true;
    for (auto item : mDeserializeVistor) {
      if (!(item.second)(const_cast<ProtoBase *>(this))) {
        ret = false;
        CS_LOG_WARN("field {} deserialize error", item.first);
      }
    }
    if (!mSerializer.endDeserialize() || !ret) {
      return false;
    }
    return true;
  }

  template <typename U>
  bool deserialize(const U &data) {
    if (!mSerializer.startDeserialize(data)) {
      return false;
    }
    bool ret = true;
    for (auto item : mDeserializeVistor) {
      if (!(item.second)(const_cast<ProtoBase *>(this))) {
        ret = false;
        CS_LOG_WARN("field {} deserialize error", item.first);
      }
    }
    if (!mSerializer.endDeserialize() || !ret) {
      return false;
    }
    return true;
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
  static std::map<const char *, std::function<bool(ProtoBase *)>, less>
      mSerializeVistor;

  static std::map<const char *, std::function<bool(ProtoBase *)>, less>
      mDeserializeVistor;
};

template <typename T, typename SerializerT>
std::map<const char *, std::function<bool(ProtoBase<T, SerializerT> *)>, typename ProtoBase<T, SerializerT>::less>
    ProtoBase<T, SerializerT>::mSerializeVistor;
template <typename T, typename SerializerT>
std::map<const char *, std::function<bool(ProtoBase<T, SerializerT> *)>, typename ProtoBase<T, SerializerT>::less>
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
    T::mSerializeVistor.insert(std::make_pair(#name,                          \
      [](typename T::ProtoBaseType *self) {                                   \
      return self->serializer()->insert(#name,                                \
      static_cast<const T *>(self)->name());                                  \
    }));                                                                      \
    T::mDeserializeVistor.insert(                                             \
    std::make_pair(#name, [](typename T::ProtoBaseType *self) {               \
        return self->serializer()->get(#name,                                 \
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
    T::mSerializeVistor.insert(std::make_pair(#name, [](                      \
        typename T::ProtoBaseType *self) {                                    \
      return self->serializer()->insert(#name,                                \
        static_cast<const T *>(self)->name);                                  \
    }));                                                                      \
    T::mDeserializeVistor.insert(                                             \
    std::make_pair(#name, [](typename T::ProtoBaseType *self) {               \
        return self->serializer()->get(#name,                                 \
          &(static_cast<T *>(self)->name));                                   \
    }));                                                                      \
  }                                                                           \
  bool _init_##name = [this]() {                                              \
    _regist_field_##name(this);                                               \
    return true;                                                              \
  }();

#ifdef __GNU__
#define CS_DECLARE_PROTO_FIELDS(...) FOR_EACH(CS_DECLARE_PROTO_FIELD, __VA_ARGS__)
#endif

class CS_PROTO_API ProtoFactory {
 public:
  ProtoFactory(int major = 0, int minor = 0, int patch = 1);
  ProtoFactory(const ProtoFactory &);
  ProtoFactory(ProtoFactory &&);
  ProtoFactory &operator=(const ProtoFactory &);
  ProtoFactory &operator=(ProtoFactory &&);
  ~ProtoFactory();
  void init();

#if __cplusplus >= 202100L
  template <Proto T>
#else
  template <typename T>
#endif
  void regist(const char *name) {
    auto itemType = mProtoMap.find(proto_type<T>());
    auto itemName = mProtoNameMap.find(name);
    if (itemType != mProtoMap.end()) {
      auto rname = "";
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
    mProtoNameMap.insert(std::make_pair(_proto_name<T>(), proto_type<T>()));
    mProtoMap.insert(std::make_pair(proto_type<T>(), creater<T>));
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
/**
 * @brief create a proto object by type
 *  this object is a pointer, you need to delete it by yourself
 * @param type 
 * @return IProto* 
 */
  inline IProto *create(int type) const {
    auto item = mProtoMap.find(type);
    if (mProtoMap.end() != item) {
      return (item->second)();
    }
    return nullptr;
  }
/**
 * @brief create a proto object by name
 *  this object is a pointer, you need to delete it by yourself
 * @param name 
 * @return IProto* 
 */
  inline IProto *create(const char *name) const {
    auto item = mProtoNameMap.find(name);
    if (mProtoNameMap.end() == item) {
      return nullptr;
    }
    auto itemType = mProtoMap.find(item->second);
    if (mProtoMap.end() != itemType) {
      return (itemType->second)();
    }
    return nullptr;
  }

#if __cplusplus >= 202100L
  template <Proto T>
#else
  template <typename T>
#endif
/**
 * @brief create a object
 * this object is a pointer, you need to delete it by yourself
 * @return T* 
 */
  inline T *create() const {
    return new T();
  }

  uint32_t version() const;

 private:
  std::map<int, std::function<IProto *()>> mProtoMap;
  std::map<const char *, int> mProtoNameMap;
  uint32_t mVersion = 0;

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

std::map<const char *, std::function<void(ProtoFactory *)>> static_init_funcs(const char *,
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
    return #name;                                                              \
  }                                                                            \
  namespace {                                                                  \
  struct _regist_##name {                                                      \
    _regist_##name() {                                                         \
      static_init_funcs(#name, [](ProtoFactory *self)                          \
        { self->regist<type>(#name); });                                       \
    }                                                                          \
  } _regist_##name##__tmp;                                                     \
  }                                                                            \
  CS_PROTO_END_NAMESPACE
