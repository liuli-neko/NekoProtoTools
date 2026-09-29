#include "workload.hpp"

#include "nekoproto/serialization/binary_serializer.hpp"

void compile_binary_write(const neko_compile_bench::BenchmarkStruct& value, std::vector<char>& buffer) {
    nekoproto::BinarySerializer::OutputSerializer output(buffer);
    (void)output(value);
    (void)output.end();
}
