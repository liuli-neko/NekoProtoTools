module;
#include "nekoproto/modules/detail/module_prelude.hpp"

export module nekoproto.reflect;

// Tuple protocol lookup happens in the importing TU, including for language
// structured bindings. Re-export its existing primary declarations as well as
// our specializations below; no prerequisite <tuple> include is required.
export namespace std {
using ::std::tuple_size;
using ::std::tuple_element;
#if NEKO_USE_NATIVE_REFLECTION
// GCC reflection builtins also look up their result container on instantiation.
using ::std::vector;
#endif
}

#define NEKO_BUILDING_MODULE NEKO_MODULE_REFLECTION
#include "nekoproto/modules/detail/module_exports.hpp"

// Preserve global ownership/type identity for header and import consumers.
extern "C++" {
#include "nekoproto/reflect.hpp"
}
