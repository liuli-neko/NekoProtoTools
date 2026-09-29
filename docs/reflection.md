# 反射与 metadata

公共入口为 `<nekoproto/reflect.hpp>`；业务类型可以通过 `nekoproto::Meta<T>` 非侵入地声明 `Object(...)`。`Reflect<T>::forEach` 可按字段遍历，`Reflect<T>::names()` 和 `value_count` 可读取字段信息。字段 tags 用于格式、CLI 等不同调用上下文。

可编译的最小示例：[examples/reflection.cpp](../examples/reflection.cpp)；复用同一份 metadata 的示例：[examples/all_in_one.cpp](../examples/all_in_one.cpp)。
