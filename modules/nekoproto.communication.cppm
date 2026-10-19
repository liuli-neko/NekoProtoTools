module;
#include "nekoproto/modules/detail/io_module_prelude.hpp"

export module nekoproto.communication;
export import nekoproto.proto;
export import nekoproto.transport;

#define NEKO_BUILDING_MODULE NEKO_MODULE_COMMUNICATION
#include "nekoproto/modules/detail/module_exports.hpp"
#include "nekoproto/reflection/tag_macros.hpp"
#include "nekoproto/serialization/macros.hpp"
#include "nekoproto/proto/macros.hpp"

extern "C++" {
#include "nekoproto/communication/communication_base.hpp"
}
