module;
#include "nekoproto/modules/detail/serializer_module_prelude.hpp"

export module nekoproto.serializer;
export import nekoproto.reflect;

#define NEKO_BUILDING_MODULE NEKO_MODULE_SERIALIZER
#include "nekoproto/modules/detail/module_exports.hpp"
#include "nekoproto/reflection/tag_macros.hpp"
#include "nekoproto/serialization/macros.hpp"

extern "C++" {
#include "nekoproto/serialization.hpp"
}
