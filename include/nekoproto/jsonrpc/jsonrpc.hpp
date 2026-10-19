/**
 * @file jsonrpc.hpp
 */
#pragma once
#include "nekoproto/global/macros.hpp"

#if !defined(NEKO_BUILDING_MODULE) || NEKO_BUILDING_MODULE == NEKO_MODULE_JSONRPC

#include "nekoproto/jsonrpc/backend.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"
#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include "nekoproto/jsonrpc/protocol.hpp"
#endif // current component: jsonrpc
