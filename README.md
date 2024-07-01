# A PROTO FOR CPP 

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

If you only need to use the serialization and deserialization support of the library, you will only need headers :
```C++
#include "core/serializer_base.hpp"
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
    JsonSerializer serializer; // you can implement your own serializer, of course this repository provides a json serializer in header json_serializer.hpp and more details while introducing later.
    std::vector<char> data;
    serializer.startSerialize(&data)
    sa.serialize(serializer);
    serializer.endSerialize();
    std::string str(data.begin(), data.size());
    std::cout << str << std::endl;
    return 0;
}
```

If you want to use the protocol factory to generate the message type of proto, you will need to add the following headers :
```C++
#include "core/proto_base.hpp"
#include "core/serializer_base.hpp" // it is needed to use the protocol factory
#include "core/json_serializer.hpp" // it is default serializer for proto message, if you want to use your own serializer, you need provided template parameters, can remove this header if not use default serializer.
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
| std::u8string | yes | STRING | json_serializer.hpp |
| std::vector\<T\> | yes | ARRAY | json_serializer.hpp |
| class public PorotoBase<T, JsonSerializer> | yes | OBJECT | json_serializer.hpp |
| std::array\<T, N\> | yes | ARRAY | json_serializer_contrain.hpp |
| std::set\<T\> | yes | ARRAY | json_serializer_contrain.hpp |
| std::list\<T\> | yes | ARRAY | json_serializer_contrain.hpp |
| std::map\<std::string, T\> | yes | OBJECT | json_serializer_contrain.hpp |
| std::tuple\<T...\> | yes | ARRAY | json_serializer_contrain.hpp |
| custom struct type | yes | ARRAY | json_serializer_struct.hpp |
| enum | yes | STRING [ INT ] | json_serializer_enum.hpp |

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
| float | no | - | - |
| double | no | - | - |
| std::string | yes | 4 + len | binary_serializer.hpp |
| std::u8string | yes | 4 + len | binary_serializer.hpp |

##### 3.1.3. custom serializer

If you want to use your own serializer, cant pulic `ProtoBase<T, CustomSerializer>` class. you need implement interface as:

```C++
class CustomSerializer {
public:
    /**
     * @brief start serialize
     *  while calling this function before traversing all fields, 
     * can clear the buffer in this function or initialize the buffer and serializor.
     */
    void startSerialize();
    /**
     * @brief end serialize
     *  while calling this function after traversing all fields.
     *  if success serialize all fields, return true, otherwise return false.
     * 
     * @param[out] data the buf to output
     * @return true 
     * @return false 
     */
    bool endSerialize(std::vector<char>* data);
    /**
     * @brief insert value
     * for all fields, this function will be called by original class.
     * if success serialize this field, return true, otherwise return false.
     * @tparam T the type of value
     * @param[in] name the field name
     * @param[in] value the field value
     * @return true 
     * @return false 
     */
    template <typename T>
    bool insert(const char* name, const size_t len, const T& value);
    /**
     * @brief start deserialize
     *  while calling this function before traversing all fields, 
     * can clear the last time deserialize states in this function and initialize the deserializor.
     * @param[in] data the buf to input
     * @return true 
     * @return false 
     */
    bool startDeserialize(const std::vector<char>& data);
    /**
     * @brief end deserialize
     *  while calling this function after traversing all fields.
     *  if success deserialize all fields, return true, otherwise return false.
     * @return true 
     * @return false
     */
    inline bool endDeserialize();
    /**
     * @brief get value
     * for all fields, this function will be called by original class.
     * if success deserialize this field, return true, otherwise return false.
     * @tparam T the type of value
     * @param[in] name the field name
     * @param[out] value the field value
     * @return true 
     * @return false 
     */
    template <typename T>
    bool get(const char* name, const size_t len, T* value);
};
```
make sure this class can structure by default.

#### 3.2. protocol message manager

protoFactory support create & version. is a manager of proto message. by you can make proto as a dynamic library to dynamic replace proto. the interface is:

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
At the same time, the simpler this thing is done, the better.

### 5. the future

- [x] support visit fields by name
- [ ] support udp protocol in communication channel
- [ ] support more protocol in communication channel
