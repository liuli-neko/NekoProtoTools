module;
#include "nekoproto/modules/detail/module_prelude.hpp"

export module nekoproto.proto;
export import nekoproto.serializer;

#define NEKO_BUILDING_MODULE NEKO_MODULE_PROTOCOL
#include "nekoproto/modules/detail/module_exports.hpp"
#include "nekoproto/reflection/tag_macros.hpp"
#include "nekoproto/serialization/macros.hpp"

extern "C++" {
#include "nekoproto/proto/proto_base.hpp"
}
