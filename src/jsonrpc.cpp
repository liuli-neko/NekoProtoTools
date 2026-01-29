#include "nekoproto/jsonrpc/jsonrpc.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include <string>

namespace NEKO_NAMESPACE::detail {

NEKO_PROTO_API std::string to_string(JsonRpcIdType id) {
    switch (id.index()) {
    case 0:
        return "null";
    case 1:
        return std::to_string(std::get<1>(id));
    case 2:
        return std::get<2>(id);
    default:
        return "unknown";
    }
}

auto JsonRpcServerImp::_makeBatch(JsonSerializer::InputSerializer& in, JsonSerializer::OutputSerializer& out,
                                  int batchSize) noexcept -> void {
    for (int ix = 0; ix < batchSize; ++ix) {
        JsonRpcRequestMethod method;
        if (!in(method)) {
            break;
        }
        if (method.jsonrpc.has_value() && method.jsonrpc.value() != "2.0") {
            NEKO_LOG_ERROR("jsonrpc", "unsupport jsonrpc version! {}", method.jsonrpc.value());
            break;
        }
        auto id = method.id;
        mCurrentIds.emplace_back(id);
        in.rollbackItem();
        if (auto it = mHandlers.find(method.method); it != mHandlers.end()) {
            NEKO_LOG_INFO("jsonrpc", "spawn taskhandle for method {} with id {}", method.method, to_string(id));
            mTaskScope.spawn((*it->second)(in, out, std::move(method)));
        } else {
            NEKO_LOG_WARN("jsonrpc", "method {} not found!", method.method);
            _handleError<RpcMethodErrorHelper>(
                out, std::move(method), (int)JsonRpcError::MethodNotFound,
                JsonRpcErrorCategory::instance().message((int)JsonRpcError::MethodNotFound));
        }
    }
}

auto JsonRpcServerImp::methodDatas() noexcept -> std::vector<MethodData> {
    std::vector<MethodData> metas;
    for (auto& [name, handler] : mHandlers) {
        metas.push_back({.name = handler->name(), .description = handler->description(), .isBind = handler->isBind()});
    }
    return metas;
}

auto JsonRpcServerImp::methodDatas(std::string_view name) noexcept -> MethodData {
    if (auto item = mHandlers.find(name); item != mHandlers.end()) {
        return {
            .name = item->second->name(), .description = item->second->description(), .isBind = item->second->isBind()};
    }
    return {};
}

auto JsonRpcServerImp::processRequest(const char* data, std::size_t size, detail::IMessageStream* client) noexcept
    -> ILIAS_NAMESPACE::Task<std::vector<char>> {
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer out(buffer);
    JsonSerializer::InputSerializer in(data, size);
    while ((*data == '\n' || *data == ' ') && size > 0) {
        data++;
        size--;
    }
    bool batchRequest     = false;
    std::size_t batchSize = 1;
    if (data[0] == '[') { // batch request
        batchRequest = true;
        in.startNode();
        in(make_size_tag(batchSize));
        out.startArray(batchSize);
    }
    _makeBatch(in, out, (int)batchSize);
    co_await mTaskScope.waitAll();
    NEKO_LOG_INFO("jsonrpc", "Finished processing batch request, batch size {}", batchSize);
    mCurrentIds.clear();
    mCancelHandles.clear();
    if (batchRequest) {
        in.finishNode();
        out.endArray();
    }
    if (batchSize > 0) {
        out.end();
    }
    if (client != nullptr && buffer.size() > 3) {
        NEKO_LOG_INFO("jsonrpc", "sending {} bytes : {}", buffer.size(),
                      std::string_view{buffer.data(), buffer.size()});
        auto ret = co_await client->send({reinterpret_cast<std::byte*>(buffer.data()), buffer.size()});
        if (ret.error_or(ILIAS_NAMESPACE::IoError::Unknown) == JsonRpcError::MessageToolLarge) {
            NEKO_LOG_ERROR("jsonrpc", "message too large!");
        }
    }
    co_return buffer;
}

void JsonRpcServerImp::cancel(JsonRpcIdType id) noexcept {
    NEKO_LOG_INFO("jsonrpc", "cancelling id {}", to_string(id));
    if (auto it = mCancelHandles.find(id); it != mCancelHandles.end()) {
        it->second.stop();
    } else {
        NEKO_LOG_WARN("jsonrpc", "id not found");
    }
}

void JsonRpcServerImp::cancelAll() {
    NEKO_LOG_INFO("jsonrpc", "cancelling all {}", mCancelHandles.size());
    for (auto& [id, handle] : mCancelHandles) {
        NEKO_LOG_INFO("jsonrpc", "cancelling id {}", to_string(id));
        handle.stop();
    }
    mCancelHandles.clear();
}

auto JsonRpcServerImp::receiveLoop(detail::IMessageStream* client) noexcept -> ILIAS_NAMESPACE::Task<void> {
    while (client != nullptr) {
        std::vector<std::byte> buffer;
        if (auto ret = co_await client->recv(buffer); ret && buffer.size() > 0) {
            co_await (processRequest(reinterpret_cast<const char*>(buffer.data()), buffer.size(), client) |
                      ILIAS_NAMESPACE::unstoppable());
        } else {
            break;
        }
    }
}

auto JsonRpcServerImp::getCurrentIds() const noexcept -> const std::vector<JsonRpcIdType>& { return mCurrentIds; }

auto MessageStream<UdpStream, void>::recv(std::vector<std::byte>& buffer) -> IoTask<void> {
    if (!mClient) {
        co_return Unexpected(JsonRpcError::ClientNotInit);
    }
    int current = buffer.size();
    buffer.resize(current + 1500);
    while (true) {
        auto ret = co_await (mClient.recvfrom({buffer.data() + current, buffer.size() - current}) | unstoppable());
        if (ret && ret.value().first + current == buffer.size()) { // UDP单个包理论上不会超过1500
            mEndpoint = ret.value().second;
            current   = buffer.size();
            buffer.resize(current + 1500);
        } else if (ret && ret.value().first + current < buffer.size()) {
            mEndpoint = ret.value().second;
            buffer.resize(ret.value().first + current);
            co_return {};
        } else {
            co_return Unexpected<Error>(ret.error());
        }
    }
}

auto MessageStream<UdpStream, void>::send(std::span<const std::byte> data) -> IoTask<void> {
    if (!mClient) {
        co_return Unexpected(JsonRpcError::ClientNotInit);
    }
    if (data.size() >= 1500) {
        co_return Unexpected(JsonRpcError::MessageToolLarge);
    }
    auto ret  = co_await (mClient.sendto(data, mEndpoint) | unstoppable());
    auto send = ret.value_or(0);
    while (ret && send < data.size()) {
        ret = co_await (mClient.sendto({data.data() + send, data.size() - send}, mEndpoint) | unstoppable());
        if (ret && ret.value() > 0) {
            send += ret.value();
        } else {
            break;
        }
    }
    if (!ret) {
        co_return Unexpected<Error>(ret.error());
    }
    co_return {};
}

auto MessageStream<UdpStream, void>::close() -> void {
    if (mClient) {
        mClient.close();
    }
}

auto MessageStream<UdpStream, void>::cancel() -> void {
    if (mClient) {
        mClient.cancel();
    }
}

auto MessageStream<TcpStream, void>::recv(std::vector<std::byte>& buffer) -> IoTask<void> {
    if (!mClient) {
        co_return Unexpected(JsonRpcError::ClientNotInit);
    }
    uint32_t size = 0;
    if (auto ret = co_await (mClient.readAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) | unstoppable());
        !ret) {
        co_return Unexpected(ret.error());
    } else if (ret.value() == sizeof(size)) {
        size = networkToHost(size);
    } else {
        mClient.close();
        co_return Unexpected(ILIAS_NAMESPACE::IoError::Unknown);
    }
    if (buffer.size() < size) {
        buffer.resize(size);
    }
    auto ret = co_await (mClient.readAll({buffer.data(), size}) | unstoppable());
    if (ret && ret.value() == size) {
        co_return {};
    } else {
        mClient.close();
        co_return Unexpected(ret.error_or(ILIAS_NAMESPACE::IoError::Unknown));
    }
}

auto MessageStream<TcpStream, void>::send(std::span<const std::byte> data) -> IoTask<void> {
    if (!mClient) {
        co_return Unexpected(JsonRpcError::ClientNotInit);
    }
    auto size = hostToNetwork((uint32_t)data.size());
    if (auto ret = co_await (mClient.writeAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) | unstoppable());
        !ret) {
        mClient.close();
        co_return Unexpected(ret.error());
    } else if (ret.value() != sizeof(size)) {
        mClient.close();
        co_return Unexpected(ILIAS_NAMESPACE::IoError::Unknown);
    }
    auto ret = co_await (mClient.writeAll(data) | unstoppable());
    if (!ret || ret.value() != data.size()) {
        mClient.close();
        co_return Unexpected(ret.error_or(ILIAS_NAMESPACE::IoError::Unknown));
    }
    co_return {};
}
auto MessageStream<TcpStream, void>::close() -> void {
    if (mClient) {
        mClient.close();
    }
}

auto MessageStream<TcpStream, void>::cancel() -> void {
    if (mClient) {
        mClient.cancel();
    }
}

auto MessageStream<TcpListener, void>::close() -> void {
    if (mListener) {
        mListener.close();
    }
}

auto MessageStream<TcpListener, void>::cancel() -> void {
    if (mListener) {
        mListener.cancel();
    }
}

auto MessageStream<TcpListener, void>::accept() -> IoTask<MessageStream<TcpStream>> {
    if (!mListener) {
        co_return Unexpected(JsonRpcError::ClientNotInit);
    }
    if (auto ret = co_await mListener.accept(); ret) {
        co_return MessageStream<TcpStream>(std::move(ret.value().first));
    } else {
        co_return Unexpected(ret.error());
    }
}
NEKO_PROTO_API
auto make_tcp_stream_client(IPEndpoint ipendpoint) -> IoTask<MessageStream<TcpStream>> {
    if (auto ret1 = co_await IliasTcpClient::connect(ipendpoint); ret1) {
        co_return MessageStream<TcpStream>(std::move(ret1.value()));
    } else {
        co_return Unexpected(ret1.error());
    }
}

// tcp://127.0.0.1:8080
// 127.0.0.1:8080
NEKO_PROTO_API
auto make_tcp_stream_client(std::string_view url) -> IoTask<MessageStream<TcpStream>> {
    std::string_view ipstr;
    if (url.substr(0, 6) == "tcp://") {
        ipstr = url.substr(6);
    } else {
        ipstr = url;
    }
    auto ipendpoint = IPEndpoint::fromString(ipstr);
    if (!ipendpoint) {
        co_return Unexpected(ILIAS_NAMESPACE::IoError::InvalidArgument);
    }
    co_return co_await make_tcp_stream_client(ipendpoint.value());
}
NEKO_PROTO_API
auto make_tcp_stream_client(const char* url) -> IoTask<MessageStream<TcpStream>> {
    return make_tcp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_tcp_stream_client(const std::string& url) -> IoTask<MessageStream<TcpStream>> {
    return make_tcp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_udp_stream_client(IPEndpoint bindIpendpoint, IPEndpoint remoteIpendpoint)
    -> IoTask<MessageStream<UdpStream>> {
    if (auto ret = ILIAS_NAMESPACE::Socket::make(bindIpendpoint.family(), SOCK_DGRAM, 0); ret) {
        auto socket = std::move(ret.value());
        socket.setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
        if (auto ret1 = socket.bind(bindIpendpoint); !ret1) {
            socket.close();
            co_return Unexpected(ret1.error());
        }
        auto udpclient = IliasUdpClient::from(std::move(socket));
        co_return MessageStream<UdpStream>(std::move(udpclient.value()), remoteIpendpoint);
    } else {
        co_return Unexpected(ret.error());
    }
}

// like udp://127.0.0.1:12345-127.0.0.1:12346
// 127.0.0.1:12345-127.0.0.1:12346
NEKO_PROTO_API
auto make_udp_stream_client(std::string_view url) -> IoTask<MessageStream<UdpStream>> {
    std::string_view bindRemoteIp;
    if (url.substr(0, 6) == "udp://") {
        bindRemoteIp = url.substr(6);
    } else {
        bindRemoteIp = url;
    }
    auto pos = bindRemoteIp.find('-');
    if (pos == std::string_view::npos) {
        co_return Unexpected(ILIAS_NAMESPACE::IoError::InvalidArgument);
    }
    auto bindIpendpoint   = IPEndpoint::fromString(bindRemoteIp.substr(0, pos));
    auto remoteIpendpoint = IPEndpoint::fromString(bindRemoteIp.substr(pos + 1));
    if (!bindIpendpoint || !remoteIpendpoint) {
        co_return Unexpected(ILIAS_NAMESPACE::IoError::InvalidArgument);
    }
    co_return co_await make_udp_stream_client(bindIpendpoint.value(), remoteIpendpoint.value());
}
NEKO_PROTO_API
auto make_udp_stream_client(const char* url) -> IoTask<MessageStream<UdpStream>> {
    return make_udp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_udp_stream_client(const std::string& url) -> IoTask<MessageStream<UdpStream>> {
    co_return co_await make_udp_stream_client(std::string_view(url));
}
NEKO_PROTO_API
auto make_tcp_stream_server(IPEndpoint ipendpoint) -> IoTask<MessageStream<TcpListener>> {
    if (auto ret = ILIAS_NAMESPACE::Socket::make(ipendpoint.family(), SOCK_STREAM, IPPROTO_TCP); ret) {
        ret.value().setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
        if (auto ret1 = ret.value().bind(ipendpoint); ret1) {
            auto ret2 = ret.value().listen();
            if (!ret2) {
                co_return Unexpected(ret2.error());
            }
            co_return MessageStream<TcpListener>(IliasTcpListener::from(std::move(ret.value())).value());
        } else {
            co_return Unexpected(ret1.error());
        }
    } else {
        co_return Unexpected(ret.error());
    }
}
NEKO_PROTO_API
auto make_tcp_stream_server(std::string_view url) -> IoTask<MessageStream<TcpListener>> {
    std::string_view ipstr;
    if (url.substr(0, 6) == "tcp://") {
        ipstr = url.substr(6);
    } else {
        ipstr = url;
    }
    auto ipendpoint = IPEndpoint::fromString(ipstr);
    if (!ipendpoint) {
        co_return Unexpected(ILIAS_NAMESPACE::IoError::InvalidArgument);
    }
    co_return co_await make_tcp_stream_server(ipendpoint.value());
}
NEKO_PROTO_API
auto make_tcp_stream_server(const char* url) -> IoTask<MessageStream<TcpListener>> {
    return make_tcp_stream_server(std::string_view(url));
}
NEKO_PROTO_API
auto make_tcp_stream_server(const std::string& url) -> IoTask<MessageStream<TcpListener>> {
    return make_tcp_stream_server(std::string_view(url));
}

} // namespace NEKO_NAMESPACE::detail

namespace NEKO_NAMESPACE {

JsonRpcErrorCategory& JsonRpcErrorCategory::instance() {
    static JsonRpcErrorCategory kInstance;
    return kInstance;
}

auto JsonRpcErrorCategory::message(int value) const noexcept -> std::string {
    switch ((JsonRpcError)value) {
    case JsonRpcError::Ok:
        return "Ok";
    case JsonRpcError::MethodNotBind:
        return "Method not Bind";
    case JsonRpcError::ParseError:
        return "Parse Eror";
    case JsonRpcError::InvalidRequest:
        return "InvalidRequest";
    case JsonRpcError::MethodNotFound:
        return "Method ot Found";
    case JsonRpcError::InvalidParams:
        return "InvalidParams";
    case JsonRpcError::InternalError:
        return "Internal Error";
    case JsonRpcError::ClientNotInit:
        return "Client Not Init";
    case JsonRpcError::ResponseIdNotMatch:
        return "Response Id Not Match";
    default:
        return "Unknow Error";
    }
}

auto make_error_code(JsonRpcError value) noexcept -> std::error_code {
    return std::error_code(static_cast<int>(value), JsonRpcErrorCategory::instance());
}

} // namespace NEKO_NAMESPACE