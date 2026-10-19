#include <nekoproto/reflect.hpp>
struct Invalid { int value; };
template <> struct nekoproto::Meta<Invalid> { static constexpr int value = 0; };
static_assert(nekoproto::Reflect<Invalid>::value_count == 1);
int main() {}
