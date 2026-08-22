#include "workload.hpp"

#include "nekoproto/serialization/binary_serializer.hpp"

void compile_binary_read(const char* data, std::size_t size, neko_compile_bench::BenchmarkStruct& value) {
    NEKO_NAMESPACE::BinarySerializer::InputSerializer input(data, size);
    (void)input(value);
}
