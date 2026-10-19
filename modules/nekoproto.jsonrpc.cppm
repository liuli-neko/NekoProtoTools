module;
#include "nekoproto/modules/detail/io_module_prelude.hpp"

export module nekoproto.jsonrpc;
export import nekoproto.rpc;

#define NEKO_BUILDING_MODULE NEKO_MODULE_JSONRPC
#include "nekoproto/modules/detail/module_exports.hpp"
#include "nekoproto/reflection/tag_macros.hpp"
#include "nekoproto/serialization/macros.hpp"

extern "C++" {
#include "nekoproto/jsonrpc/jsonrpc.hpp"
}
