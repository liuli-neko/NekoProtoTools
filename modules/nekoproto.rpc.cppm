module;
#include "nekoproto/modules/detail/io_module_prelude.hpp"

export module nekoproto.rpc;
export import nekoproto.transport;

#define NEKO_BUILDING_MODULE NEKO_MODULE_RPC
#include "nekoproto/modules/detail/module_exports.hpp"
#include "nekoproto/reflection/tag_macros.hpp"
#include "nekoproto/serialization/macros.hpp"

extern "C++" {
#include "nekoproto/rpc/rpc.hpp"
}
