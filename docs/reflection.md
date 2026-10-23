# 反射与 metadata

公共入口为 `<nekoproto/reflect.hpp>`；业务类型可以通过 `nekoproto::Meta<T>` 非侵入地声明 `Object(...)`。`Reflect<T>::forEach` 可按字段遍历，`Reflect<T>::names()` 和 `value_count` 可读取字段信息。字段 tags 用于格式、CLI 等不同调用上下文。

`Reflectable` / `is_reflectable_v`、`NamedReflectable` / `has_named_reflection_v`、`TupleLike` / `is_tuple_v` 和 `OptionalLike` / `is_optional_v` 均可用于泛型约束，统一忽略顶层 cv/ref。`void`、基本类型、指针、函数、数组和未完成类型可以安全参与检测；不满足相应条件时返回 `false`。枚举满足 `Reflectable`，不满足 `NamedReflectable`。`is_optional_v` 还识别含 `std::monostate` 的 variant；`OptionalLike` 检测的是 optional 操作接口。

反射能力检测只选择 metadata / 后端，不展开字段。错误的自定义 metadata 检测为 `false`；实际使用 `Reflect<T>` 时仍报告声明错误。旧式自动展开的复杂聚合仍需满足结构化绑定的要求，字段与 tags 的完整校验发生在实际反射时。`Reflect<T>::hasMember<P>()` 对非成员对象指针或不属于该类型的成员返回 `false`。

可编译的最小示例：[examples/reflection.cpp](../examples/reflection.cpp)；复用同一份 metadata 的示例：[examples/all_in_one.cpp](../examples/all_in_one.cpp)。

C++26 接入约定：默认假设每个类型只采用一种反射声明方式。若多种来源并存，优先级固定为：

1. 自定义 metadata（`Meta<T>` 或类型内的 `T::Neko`）。
2. 原生反射及语法注解（`[[=...]]`）。
3. 自动推导。

每个类型整体选择一个来源，字段集合、顺序、名称与 tags 均由该来源决定；不同来源之间不合并、不补全。选中来源的信息缺省或声明错误按其自身规则处理，不从低优先级来源借用信息。不同类型可以分别采用不同方式。

能力判断优先使用标准 feature-test 宏，其次使用 CMake / xmake 原生编译探测；仍不明确的部分保留 TODO。保持上层接口，优先完成 C++26 原生反射与注解能力，性能测量用于识别明显回退和后续优化。

原生对象、注解、枚举及 `nekoproto.reflect` 模块已接入，首次发布默认 `legacy`。使用方式、范围和回退配置见 [C++26 使用文档](cpp26.md)。
