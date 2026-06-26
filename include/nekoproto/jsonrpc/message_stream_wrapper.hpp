/**
 * @file datagram_wapper.hpp
 * @author llhsdmd(llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ilias/io/dyn_traits.hpp>
#include <ilias/io/error.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/net/sockopt.hpp>
#include <ilias/net/system.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/net/udp.hpp>
#include <ilias/sync/event.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/task.hpp>
#include <ilias/task/scope.hpp>
#include <system_error>

#include "ilias/defines.hpp"
#include "ilias/task/utils.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/rpc/endpoint.hpp"

NEKO_BEGIN_NAMESPACE

namespace detail {
using ilias::IoTask;
using ilias::IPEndpoint;
using ilias::Task;
using IliasDynStream   = ilias::DynStream;
using IliasUdpSocket   = ilias::UdpSocket;
using ilias::Err;

using IliasLengthPrefixedMessageEndpoint = LengthPrefixedStreamMessageEndpoint<IliasDynStream>;
using IliasChunkedDatagramMessageEndpoint = ChunkedDatagramMessageEndpoint<IliasUdpSocket, IPEndpoint>;

NEKO_PROTO_API
auto make_tcp_stream_client(IPEndpoint ipendpoint) -> IoTask<IliasLengthPrefixedMessageEndpoint>;
// tcp://127.0.0.1:8080
// 127.0.0.1:8080
NEKO_PROTO_API
auto make_tcp_stream_client(std::string_view url) -> IoTask<IliasLengthPrefixedMessageEndpoint>;
NEKO_PROTO_API
auto make_tcp_stream_client(const char* url) -> IoTask<IliasLengthPrefixedMessageEndpoint>;
NEKO_PROTO_API
auto make_tcp_stream_client(const std::string& url) -> IoTask<IliasLengthPrefixedMessageEndpoint>;
NEKO_PROTO_API
auto make_udp_stream_client(IPEndpoint bindIpendpoint, IPEndpoint remoteIpendpoint)
    -> IoTask<IliasChunkedDatagramMessageEndpoint>;
// like udp://127.0.0.1:12345-127.0.0.1:12346
// 127.0.0.1:12345-127.0.0.1:12346
NEKO_PROTO_API
auto make_udp_stream_client(std::string_view url) -> IoTask<IliasChunkedDatagramMessageEndpoint>;
NEKO_PROTO_API
auto make_udp_stream_client(const char* url) -> IoTask<IliasChunkedDatagramMessageEndpoint>;
NEKO_PROTO_API
auto make_udp_stream_client(const std::string& url) -> IoTask<IliasChunkedDatagramMessageEndpoint>;
} // namespace detail

NEKO_END_NAMESPACE
