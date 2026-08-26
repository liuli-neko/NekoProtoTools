#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "nekoproto/global/config.h"
#include "nekoproto/rpc/tracing.hpp"

namespace nekoproto {

#if defined(NEKO_PROTO_RPC_TRACE)

class RpcTracingWebUi {
public:
    /**
     * @brief Construct a new Rpc Tracing Web Ui object
     * @note environment variable `NEKO_RPC_TRACE_WEBUI_BIND` can be used to override the bind endpoint
     * 
     * @param bind The bind endpoint of the webui (default: 127.0.0.1:8067)
     */
    explicit RpcTracingWebUi(std::string_view bind = "127.0.0.1:8067");
    ~RpcTracingWebUi();

    RpcTracingWebUi(const RpcTracingWebUi&) = delete;
    auto operator=(const RpcTracingWebUi&) -> RpcTracingWebUi& = delete;
    RpcTracingWebUi(RpcTracingWebUi&&) noexcept;
    auto operator=(RpcTracingWebUi&&) noexcept -> RpcTracingWebUi&;

    /**
     * @brief Install the webui server to the current coroutine executor/runtime
     * 
     * @param registry The trace registry to monitor (defaults to global instance)
     * @return true if successfully installed and started
     * @return false otherwise
     */
    auto install(RpcTraceRegistry& registry = RpcTraceRegistry::instance()) -> bool;

    /**
     * @brief Get the bind endpoint of the webui
     * 
     * @return std::string_view 
     */
    auto endpoint() const noexcept -> std::string_view;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

#else // !defined(NEKO_PROTO_RPC_TRACE) - Zero-overhead no-op stub

class RpcTracingWebUi {
public:
    explicit RpcTracingWebUi(std::string_view = "127.0.0.1:8067") noexcept {}
    ~RpcTracingWebUi() = default;

    RpcTracingWebUi(const RpcTracingWebUi&) = delete;
    auto operator=(const RpcTracingWebUi&) -> RpcTracingWebUi& = delete;
    RpcTracingWebUi(RpcTracingWebUi&&) noexcept = default;
    auto operator=(RpcTracingWebUi&&) noexcept -> RpcTracingWebUi& = default;

    auto install(RpcTraceRegistry& = RpcTraceRegistry::instance()) noexcept -> bool { return false; }
    auto endpoint() const noexcept -> std::string_view { return {}; }
};

#endif // defined(NEKO_PROTO_RPC_TRACE)

} // namespace nekoproto

