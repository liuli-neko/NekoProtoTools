# A PROTO FOR CPP 

### language

[中文](./README_zh.md) 

[English](./README.md)

### 1. Introduction

This repository is a pure C++ proto auxiliary library. The library provides abstract template class with common functions of proto Message. It also provides very convenient and easy-to-extend serialization and deserialization support by macros, which are used to generate large and repetitive proto. code.
Through the macros provided by this library, you only need to add a small number of macros to the original data class. A base class can register it as the message type of proto, implement serialization and deserialization, and automatically use the protocol factory. generate.

#### 1.2 CI status

|name|status|
|:---:|:---:|
|Linux|[![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml)|
|Windows|[![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml)|
|Codecov|[![codecov](https://codecov.io/gh/liuli-neko/NekoProtoTools/graph/badge.svg?token=F5OR647TV7)](https://codecov.io/gh/liuli-neko/NekoProtoTools)|

### 2. Usage
> for serializer, it depends the Popular libraries, Since it is not necessarily required, it is not included directly in this library. you can select by yourself. 
> for json serializer can use simdjson or rapidjson.

If you only need to use the serialization and deserialization support of the library, you will only need headers :
```C++
#include "proto/serializer_base.hpp"
```
and use macro `NEKO_SERIALIZER` pack your class members you want to serialize and deserialize.
```C++
class SerializerAble {
    int a;
    std::string b;
    NEKO_SERIALIZER(a, b);
}

int main() {
    SerializerAble sa;
    sa.a = 1;
    sa.b = "hello";
    std::vector<char> data;
    JsonSerializer::OutputSerializer out(data); // you can implement your own serializer, of course this repository provides a json serializer in header json_serializer.hpp and more details while introducing later.
    out(sa);
    out.end(); // or make out destory by RAII
    std::string str(data.begin(), data.size());
    std::cout << str << std::endl;
    return 0;
}
```

If you want to use the protocol factory to generate the message type of proto, you will need to add the following headers :
```C++
#include "proto/proto_base.hpp"
#include "proto/serializer_base.hpp" // it is needed to use the protocol factory
#include "proto/json_serializer.hpp" // it is default serializer for proto message, if you want to use your own serializer, you need provided template parameters, can remove this header if not use default serializer.
```
and a cpp file `src/proto_base.cpp`

and use macro `NEKO_DECLARE_PROTOCOL` to declare your class you want to register as the message type of proto.
```C++
// SelfT is the type of your class, SerializerT is the type of serializer you want to use. default is JsonSerializer.
class SerializerAble {
    int a;
    std::string b;
    NEKO_SERIALIZER(a, b);
    NEKO_DECLARE_PROTOCOL(SerializerAble, JsonSerializer)
}

int main() {
    auto sa = makeProtocol(SerializerAble{});
    (*sa)->a = 1;
    (*sa)->b = "hello";
    auto data = sa->toData(); // for proto message, you can serialize it by toData() and deserialize it by fromData(data)

    ProtoFactory factory(1, 0, 0); // you can generate the factory. and proto message while auto regist to this factory.
    auto proto = factory.create("SerializerAble"); // you can create the proto message by the name or type value.
    proto->fromData(data);
    return 0;
}
```


### 3. support

#### 3.1. serializer

##### 3.1.1. json serializer

This repository provides a default json serializer. support most of commonly type in standard c++[^1].

| type | support | json type| header |
| ---- | -------- | ---- | ---- |
| bool | yes | BOOL | json_serializer.hpp |
| int8_t | yes | INT | json_serializer.hpp |
| int32_t | yes | INT | json_serializer.hpp |
| int64_t | yes | INT | json_serializer.hpp |
| uint8_t | yes | INT | json_serializer.hpp |
| uint32_t | yes | INT | json_serializer.hpp |
| uint64_t | yes | INT | json_serializer.hpp |
| float | yes | FLOAT | json_serializer.hpp |
| double | yes | FLOAT | json_serializer.hpp |
| std::string | yes | STRING | json_serializer.hpp |
| std::u8string | yes | STRING | types/u8string.hpp |
| std::vector\<T\> | yes | ARRAY | types/vector.hpp |
| class public PorotoBase<T, JsonSerializer> | yes | OBJECT | types/binary_data.hpp |
| std::array\<T, N\> | yes | ARRAY | types/array.hpp |
| std::set\<T\> | yes | ARRAY | types/set.hpp |
| std::list\<T\> | yes | ARRAY | types/list.hpp |
| std::map\<std::string, T\> std::map\<T, T\> | yes | OBJECT | types/map.hpp |
| std::tuple\<T...\> | yes | ARRAY | types/tuple.hpp |
| custom struct type | yes | ARRAY | types/struct_unwrap.hpp |
| enum | yes | STRING [ INT ] | types/enum.hpp |
| std::optional\<T\> | yes | OBJECT | json_serializer.hpp |
| std::variant\<T...\> | yes | OBJECT | types/variant.hpp |
| std::pair\<T, T\> | yes | OBJECT | types/pair.hpp |
| std::bitset\<N\> | yes | STRING | types/bitset.hpp |
| std::shared_ptr\<T\> | yes | Depends on T | types/shared_ptr.hpp |
| std::unique_ptr\<T\> | yes | Depends on T | types/unique_ptr.hpp |
| std::atomic\<T\> | yes | Depends on T | types/atomic.hpp |
| std::unordered_set\<T\>| yes | OBJECT | types/unordered_set.hpp |
| std::unordered_map\<std::string, T\> std::unordered_map\<T, T\> | yes | OBJECT ARRAY | types/unordered_map.hpp |
| std::multiset\<T\> | yse | ARRAY | types/multiset.hpp |
| std::multimap\<T, T\> | yse | ARRAY | types/multimap.hpp |
| std::unordered_multiset\<T\>| yse | ARRAY | types/unordered_multiset.hpp |
| std::unordered_multimap\<T, T\> | yse | ARRAY | types/unordered_multimap.hpp |
| std::deque\<T\> | yse | ARRAY | types/deque.hpp |
| std::any | no | - | - |

[^1]: https://en.cppreference.com/w/cpp/language/types

[*T]: T refers to all supported types

##### 3.1.2. binary serializer

This repository provides a default binary serializer.

| type | support | binary len | header |
| ---- | -------- | ---- | ---- |
| bool | yes | 1 | binary_serializer.hpp |
| int8_t | yes | 1 | binary_serializer.hpp |
| int32_t | yes | 4 | binary_serializer.hpp |
| int64_t | yes | 8 | binary_serializer.hpp |
| uint8_t | yes | 1 | binary_serializer.hpp |
| uint32_t | yes | 4 | binary_serializer.hpp |
| uint64_t | yes | 8 | binary_serializer.hpp |
| float | yes | 4 | binary_serializer.hpp |
| double | yes | 8 | binary_serializer.hpp |
| std::string | yes | 4 + len | binary_serializer.hpp |
| NamePairValue\<std::string, T\> | yes | name len + value len | binary_serializer.hpp |

**Note:**
other types are supported like json, more info can see in json support which in folder "types", the binary len is 4 + value len * container size.

##### 3.1.3. custom serializer

If you want to use your own serializer. you need implement interface as:

```C++
class CustomOutputSerializer : public detail::OutputSerializer<CustomOutputSerializer> {
public:
    using BufferType =;
public:
    CustomOutputSerializer(BufferType& out);
    template <typename T>
    inline bool saveValue(SizeTag<T> const& size);
    inline bool saveValue(const int8_t value);
    inline bool saveValue(const uint8_t value);
    inline bool saveValue(const int16_t value);
    inline bool saveValue(const uint16_t value);
    inline bool saveValue(const int32_t value);
    inline bool saveValue(const uint32_t value);
    inline bool saveValue(const int64_t value);
    inline bool saveValue(const uint64_t value);
    inline bool saveValue(const float value);
    inline bool saveValue(const double value);
    inline bool saveValue(const bool value);
    inline bool saveValue(const std::string& value);
    inline bool saveValue(const char* value);
    inline bool saveValue(const std::string_view value); // if c++17 or later
    inline bool saveValue(const NameValuePair<T>& value); // T while is std::option and has no value, if you want to support ,you maust resolve it in here
    inline bool startArray(const std::size_t size); // it is for json, maybe you need more block, or less, you can do it by other states, or make your self class to tag it.
    inline bool endArray();
    inline bool startObject(const std::size_t size); // size whill not always corrent, maybe -1.
    inline bool endObject();
    inline bool end(); // construction start, destructuring stop, it is a good ideal, but some times I want end() by self. because a block {} in function is not beautiful.

private:
    CustomOutputSerializer& operator=(const CustomOutputSerializer&) = delete;
    CustomOutputSerializer& operator=(CustomOutputSerializer&&)      = delete;
};

class CustomInputSerializer : public detail::InputSerializer<CustomInputSerializer> {
public:
    using BufferType;

public:
    inline CustomInputSerializer(const BufferType& buf);
    inline operator bool() const;
    inline bool loadValue(std::string& value);
    inline bool loadValue(int8_t& value);
    inline bool loadValue(int16_t& value);
    inline bool loadValue(int32_t& value);
    inline bool loadValue(int64_t& value);
    inline bool loadValue(uint8_t& value);
    inline bool loadValue(uint16_t& value);
    inline bool loadValue(uint32_t& value);
    inline bool loadValue(uint64_t& value);
    inline bool loadValue(float& value);
    inline bool loadValue(double& value);
    inline bool loadValue(bool& value);
    template <typename T>
    inline bool loadValue(const SizeTag<T>& value);
    template <typename T>
    inline bool loadValue(const NameValuePair<T>& value);

private:
    CustomInputSerializer& operator=(const CustomInputSerializer&) = delete;
    CustomInputSerializer& operator=(CustomInputSerializer&&)      = delete;
};

```
make sure this class can structure by default.

#### 3.2. protocol message manager

protoFactory support create & version. is a manager of proto message. the interface is:

```C++
class NEKO_PROTO_API ProtoFactory {
public:
    template <typename T>
    static int proto_type();
    template <typename T>
    static const char* proto_name();
    /**
     * @brief create a proto object by type
     *  this object is a pointer, you need to delete it by yourself
     * @param type
     * @return std::unique_ptr<IProto>
     */
    std::unique_ptr<IProto> create(int type) const;
    /**
     * @brief create a proto object by name
     *  this object is a pointer, you need to delete it by yourself
     * @param name
     * @return std::unique_ptr<IProto>
     */
    inline std::unique_ptr<IProto> create(const char* name) const;
    uint32_t version() const;
};
```

you can define a proto message by inheritance from ProtoBase, it while atuo register to protoFactory when you create protoFactory instance.

```C++
struct ProtoMessage {
    int a;
    std::string b;

    NEKO_SERIALIZE(a, b)
    NEKO_DECLARE_PROTOCOL(ProtoMessage, JsonSerializer)
}

int main() {
    ProtoFactory factory(1, 0, 0);
    auto msg = factory.create("ProtoMessage");
    auto raw = msg->cast<ProtoMessage>();
    raw->a = 1;
    raw->b = "hello";
    std::vector<char> data;
    data = msg->toData();
    // do something
    return 0;
}
```

### 4. the end

why I want to do this?

I think I should not spend too much time designing and maintaining protocol libraries, especially for a large number of protocols, we need to try our best to avoid increasing maintenance costs caused by duplication of code.

### 5. todo list:

**serializer**
- [x] support visit fields by name
- [x] using simdjson for json serialization
    - [x] support simdjson input serializer in simdjson::dom (this is old API?)
    - [ ] support serializer interface in simdjson::ondemand (What are the differences between the DNS APIs in the dom and ondemand namespaces?)
    - [x] output serializer (support by self)
- [x] support more cpp stl types
- [ ] support protoFactory dynamic layer interface

**communication**
- [ ] support udp protocol in communication channel
- [ ] support more protocol in communication channel

### 6. the future
#### Latest
- Make almost all call is serializer(variable)
    - NameValuePair, SizeTag, etc while are special struct unable to auto enter the object. because they are not a real object. Other class whitout minimal_serializable trait will be auto enter 1 level before process it.
    - support simdjson backend (now noly under simdjson::dom namespace), it only support input serializer, Slightly faster than rapidjson backend.
- support almost all stl types

#### v0.2.0 - alpha
- Modify serializer interface
    - Make serialize function to operator()
    - Make JsonConvert struct to load function and save function
    - Split standard library types into more granular support
- Initial optimizations have been made to performance
- ProtoBase need less space, but all proto class while created static object for every thread
- support tcp protocol in communication channel

#### v0.1.0
- Make protoBase as a helper class, not a base class
- Add support for reflection fields
- Add support for optional, variant

