#include <nekoproto/reflect.hpp>
struct Invalid { [[=nekoproto::JsonTag{.raw_string=true}]] int value; };
static_assert(nekoproto::Reflect<Invalid>::value_count == 1);
int main() {}
