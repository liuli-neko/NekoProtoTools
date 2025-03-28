# 一个c++的协议库

### 语言

[English](./README.md)

[中文](./README_zh.md)

### 1. 简介
这是一个纯C++的proto辅助库。该库提供抽象模板类，通过模板为proto Message通过了必要的函数，尽可能的简化了proto Message的定义和实现。
通过使用该库提供的模板，你只需要定义好消息的字段，并声明需要序列化/反序列化的字段以及需要使用的序列化器即可将任意的自定义c++类型作为消息使用，
本库为消息提供了序列化，反序列化，以及简单的反射功能。

#### 1.2 CI 状态

|name|status|
|:---:|:---:|
|Linux|[![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml)|
|Windows|[![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml)|
|Codecov|[![codecov](https://codecov.io/gh/liuli-neko/NekoProtoTools/graph/badge.svg?token=F5OR647TV7)](https://codecov.io/gh/liuli-neko/NekoProtoTools)|

### 2. 使用
> 对于序列化是依赖当下流行的，快速的库来实现的，这部分依赖本库并没有直接包含，因为可能并不需要，为了尽可能减少体积。
> 对于 json 序列化提供了两个选择，rapidjson和simdjson，

如果你只需要序列化与反序列化的支持，需要如下头文件:
```C++
#include "nekoproto/proto/serializer_base.hpp"
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

如果你需要提供反射和多态的协议，需要如下头文件 :
```C++
#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/proto/serializer_base.hpp" // it is needed to use the protocol factory
#include "nekoproto/proto/json_serializer.hpp" // it is default serializer for proto message, if you want to use your own serializer, you need provided template parameters, can remove this header if not use default serializer.
```
and a cpp file `nekoproto/proto/proto_base.cpp`

and use macro `NEKO_DECLARE_PROTOCOL` to declare your class you want to register as the message type of proto.
```C++
// SelfT is the type of your class, SerializerT is the type of serializer you want to use. default is JsonSerializer.
struct SerializerAble {
    int a;
    std::string b;
    NEKO_SERIALIZER(a, b);
    NEKO_DECLARE_PROTOCOL(SerializerAble, JsonSerializer)
}

int main() {
    auto sa = SerializerAble::emplaceProto();
    (*sa)->a = 1;
    (*sa)->b = "hello";
    auto data = sa->toData(); // for proto message, you can serialize it by toData() and deserialize it by fromData(data)

    ProtoFactory factory(1, 0, 0); // you can generate the factory. and proto message while auto regist to this factory.
    auto proto = factory.create("SerializerAble"); // you can create the proto message by the name or type value.
    proto.fromData(data);
    return 0;
}
```


### 3. 类型支持

#### 3.1. 序列化

##### 3.1.1. json 序列化器

本库提供了大部分c++标准库类型[^1]的序列化支持。

| c++ 类型 | 支持 | json 类型 | 所在头文件 |
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
| std::map\<std::string, T\> std::map\<T, T\> | yes | OBJECT / ARRAY | types/map.hpp |
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

[*T]: T表示任意已支持类型

##### 3.1.2. 二进制序列化器

| c++ 类型 | 支持 | 占用长度 | 头文件 |
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
其他c++容器库支持同json序列化器，占用长度为 4 + value len * container size.
##### 3.1.3. xml序列化器
类型支持同二进制序列化器，所有对象都会被转换成字符串存储。

##### 3.1.4. 自定义序列化器

如果你需要使用自己的序列化器，你需要实现接口如下：

```C++
class CustomOutputSerializer : public detail::OutputSerializer<CustomOutputSerializer> {
public:
    CustomOutputSerializer(std::vector<char>& out);
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
    inline CustomInputSerializer(const std::vector<char>& buf);
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

#### 3.2. 协议消息管理

protoFactory 提供了协议的协议的创建和版本管理。

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
     * @return IProto
     */
    IProto create(int type) const;
    /**
     * @brief create a proto object by name
     *  this object is a pointer, you need to delete it by yourself
     * @param name
     * @return IProto
     */
    inline IProto create(const char* name) const;
    uint32_t version() const;
};
```

定义一个message你只需要简单的定义好你想要的对象，然后通过NEKO_SERIALIZE声明需要序列化/反序列化的字段，通过NEKO_DECLARE_PROTOCOL注册类型并说明你希望使用的序列化器

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
    auto* raw = msg.cast<ProtoMessage>();
    raw->a = 1;
    raw->b = "hello";
    std::vector<char> data;
    data = msg.toData();
    // do something
    return 0;
}
```

#### 3.3 通信
示例代码:

> 完整代码:
> tests\unit\communication\test_communication.cpp

```C++
#include <sstream>

#include "nekoproto/communication/communication_base.hpp"
#include "nekoproto/proto/json_serializer.hpp"
#include "nekoproto/proto/types/vector.hpp"

#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <ilias/task/when_all.hpp>
NEKO_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;

class Message {
public:
    uint64_t timestamp;
    std::string msg;
    std::vector<int> numbers;

    NEKO_SERIALIZER(timestamp, msg, numbers);
    NEKO_DECLARE_PROTOCOL(Message, JsonSerializer)
};
int main() {
    PlatformContext ioContext;
    ProtoFactory protoFactory;
    TcpListener listener(ioContext, AF_INET);
    listener.bind(IPEndpoint("127.0.0.1", 12345));
    ilias_go listener.accept();
    TcpClient tcpClient(ioContext, AF_INET);
    auto ret = tcpClient.connect(IPEndpoint("127.0.0.1", 12345)).wait();
    if (!ret) {
        return -1;
    }
    ProtoStreamClient<TcpClient> client(protoFactory, ioContext, std::move(tcpClient));
    Message msg;
    msg.msg       = "this is a message from client";
    msg.timestamp = time(NULL);
    msg.numbers   = (std::vector<int>{1, 2, 3, 5});
    auto ret1 = ilias_go client.send(msg.makeProto(),
      ProtoStreamClient<TcpClient>::SerializerInThread);
    if (!ret1) {
        return -1;
    }
    auto ret = client.recv().wait();
    if (!ret) {
        return -1;
    }
    auto msg1   = ret->cast<Message>();
    client.close().wait();
    return 0;
}
```

### 4. 最后

为什么要做这个库？

我认为作为程序开发人员，我们需要将更多的尽力放在程序本身，而非协议的构造和维护上，协议更多的是枯燥且重复的数据处理，这种重复如果管理不善总是容易滋生bug,而一个可测试的稳定的协议库可以很好的降低这种复杂度。

### 5. todo list:

**serializer**
- [x] 支持通过字符串名称访问协议字段
- [x] 使用simdjson的json序列化器
    - [x] 支持了 simdjson::dom 下的输入序列化器
    - [ ] 支持 simdjson::ondemand 命名空间下的接口 (simdjson::ondemand 和 simdjson::dom 命名空间下的序列化接口区别是什么?)
    - [x] 输出序列化器（手动实现了一份，效率上可能不会很好。）
- [x] 支持更多的c++stl容器

**communication**
- [x] 支持udp的通信通道
- [ ] 支持更多协议的通信通道
- [ ] 协议库取消需要ilias提供原子操作支持，对于发送和接收，一旦数据开始处理，该帧数据就必须完整否则会导致数据流状态混乱，为此发送端也需要改成小帧发送，确保在取消时能及时介入控制信息。

### 6. the future
#### Latest
- 统一大部分序列化调用为括号表达式serializer(variable)
    - NameValuePair, SizeTag, 等特殊的结构体不会触发节点的展开（类似json对象的嵌套对象，展开意味着从父对象的遍历深入到了子对象），其他没有minimal_serializable属性的对象将会触发一次节点的展开.
    - 支持simdjson作为json序列化后端(目前仅支持了simdjson::dom命名空间下的), 它只支持从json数据输入，目前效率比rapidjson略高.
- 几乎支持所有stl容器
- 增加了通用的std::optional的支持
- 增加了simdjson为后端的序列化器的序列化支持
- 修改通信接口为传输底层的壳，通信协议可自由替换且不在关心连接。
- 新增通信过程的序列化和反序列化支持推到其他线程执行。
- 新增了选项来控制功能启用和日志输出
- 新增Udp通信支持
- 新增同步自身协议表的支持（协议仍旧得保证同名协议的兼容性）
- 预留了1-64号的协议类型，用于支持控制信息的传输
- 完善了二进制协议，修复了部分类型的序列化问题
- 在协议传输中支持了更多的flag
    - 支持发送时先分享协议表，接收端在收到协议表后，根据协议表进行反序列化
    - 支持接收端在收到自身无法处理的协议时，将数据作为二进制数据包继续接收
    - 流传输支持切片传输，并在中途中途中断取消
- 支持了xml的序列化

#### v0.2.0 - alpha
- 修改序列化器的接口
    - 将序列化函数统一替换为重载的括号表达式
    - 将辅助的Convert类替换为save，load函数
    - 对支持的类型进行了更细致的拆分
- 初步优化反序列化性能
- 减少协议基类的空间开销，将原型作为线程独立的静态成员。
- 支持tcp通信通道

#### v0.1.0
- 改协议基类为组合
- 增加字段反射的支持
- 增加 optional, variant 的支持

