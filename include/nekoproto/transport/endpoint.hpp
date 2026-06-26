#pragma once

// Ilias-based stream and message endpoint helpers shared by optional transport modules.

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <ilias/io.hpp>
#include <ilias/io/error.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/result.hpp>
#include <ilias/task.hpp>

#include "nekoproto/global/global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename T>
concept CommunicationStream = ilias::Stream<T>;

namespace detail {

template <typename T>
concept MessageEndpointRecvSize = requires(T endpoint, std::vector<std::byte>& out) {
    { endpoint.recv(out) } -> std::same_as<ilias::IoTask<std::size_t>>;
};

template <typename T>
concept MessageEndpointRecvVoid = requires(T endpoint, std::vector<std::byte>& out) {
    { endpoint.recv(out) } -> std::same_as<ilias::IoTask<void>>;
};

template <typename T>
concept MessageEndpointSendSize = requires(T endpoint, std::span<const std::byte> in) {
    { endpoint.send(in) } -> std::same_as<ilias::IoTask<std::size_t>>;
};

template <typename T>
concept MessageEndpointSendVoid = requires(T endpoint, std::span<const std::byte> in) {
    { endpoint.send(in) } -> std::same_as<ilias::IoTask<void>>;
};

} // namespace detail

template <typename T>
concept MessageEndpoint =
    requires(T endpoint, std::vector<std::byte>& out, std::span<const std::byte> in) {
        { endpoint.close() } -> std::same_as<void>;
        { endpoint.shutdown() } -> std::same_as<ilias::IoTask<void>>;
        { endpoint.flush() } -> std::same_as<ilias::IoTask<void>>;
    } && (detail::MessageEndpointRecvSize<T> || detail::MessageEndpointRecvVoid<T>) &&
    (detail::MessageEndpointSendSize<T> || detail::MessageEndpointSendVoid<T>);

namespace detail {

template <typename StreamT>
auto close_stream(StreamT& stream) noexcept -> void {
    if constexpr (requires(StreamT& value) { value.close(); }) {
        stream.close();
    }
}

template <typename StreamT>
auto flush_stream(StreamT& stream) -> ilias::IoTask<void> {
    if constexpr (requires(StreamT& value) {
                      { value.flush() } -> std::same_as<ilias::IoTask<void>>;
                  }) {
        co_return co_await stream.flush();
    } else {
        co_return {};
    }
}

template <typename StreamT>
auto shutdown_stream(StreamT& stream) -> ilias::IoTask<void> {
    if constexpr (requires(StreamT& value) {
                      { value.shutdown() } -> std::same_as<ilias::IoTask<void>>;
                  }) {
        if (auto ret = co_await flush_stream(stream); !ret) {
            co_return ilias::Err(ret.error());
        }
        co_return co_await stream.shutdown();
    } else {
        close_stream(stream);
        co_return {};
    }
}

template <typename T>
auto recv_message_endpoint(T& endpoint, std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> {
    if constexpr (MessageEndpointRecvSize<T>) {
        auto ret = co_await endpoint.recv(buffer);
        if (!ret) {
            co_return ilias::Err(ret.error());
        }
        co_return ret.value();
    } else {
        auto ret = co_await endpoint.recv(buffer);
        if (!ret) {
            co_return ilias::Err(ret.error());
        }
        co_return buffer.size();
    }
}

template <typename T>
auto send_message_endpoint(T& endpoint, std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> {
    if constexpr (MessageEndpointSendSize<T>) {
        auto ret = co_await endpoint.send(buffer);
        if (!ret) {
            co_return ilias::Err(ret.error());
        }
        co_return ret.value();
    } else {
        auto ret = co_await endpoint.send(buffer);
        if (!ret) {
            co_return ilias::Err(ret.error());
        }
        co_return buffer.size();
    }
}

class IMessageEndpoint {
public:
    IMessageEndpoint()          = default;
    virtual ~IMessageEndpoint() = default;

    virtual auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t>    = 0;
    virtual auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> = 0;
    virtual auto close() -> void                                                       = 0;
    virtual auto shutdown() -> ilias::IoTask<void>                                     = 0;
    virtual auto flush() -> ilias::IoTask<void>                                        = 0;
};

template <typename T, typename = void>
class MessageEndpointWrapper;

template <MessageEndpoint T>
class MessageEndpointWrapper<T, void> : public IMessageEndpoint {
public:
    explicit MessageEndpointWrapper(T&& endpoint) : mEndpoint(std::move(endpoint)) {}

    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> override {
        co_return co_await recv_message_endpoint(mEndpoint, buffer);
    }
    auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> override {
        co_return co_await send_message_endpoint(mEndpoint, buffer);
    }
    auto close() -> void override { return mEndpoint.close(); }
    auto shutdown() -> ilias::IoTask<void> override { return mEndpoint.shutdown(); }
    auto flush() -> ilias::IoTask<void> override { return mEndpoint.flush(); }
    auto wrappedEndpoint() noexcept -> T& { return mEndpoint; }
    auto wrappedEndpoint() const noexcept -> const T& { return mEndpoint; }

private:
    T mEndpoint;
};

template <typename T, class enable = void>
struct is_message_endpoint : std::false_type {};

template <MessageEndpoint T>
struct is_message_endpoint<T, std::enable_if_t<std::is_base_of_v<IMessageEndpoint, T>>> : std::true_type {};

template <CommunicationStream StreamT>
class LengthPrefixedStreamMessageEndpoint {
public:
    LengthPrefixedStreamMessageEndpoint()
        requires std::default_initializable<StreamT>
    = default;
    explicit LengthPrefixedStreamMessageEndpoint(StreamT&& stream,
                                                 std::error_code messageTooLarge = ilias::IoError::MessageTooLarge)
        : mStream(std::move(stream)), mMessageTooLarge(messageTooLarge) {}

    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> {
        ILIAS_CO_TRY(auto size, co_await ilias::io::readU32Le(mStream));
        buffer.resize(size);
        if (size == 0U) {
            co_return buffer.size();
        }

        ILIAS_CO_TRY(auto bodySize,
                     co_await (ilias::io::readAll(mStream, std::span<std::byte>{buffer.data(), buffer.size()}) |
                               ilias::unstoppable()));
        if (bodySize != buffer.size()) {
            co_return ilias::Err(ilias::IoError::UnexpectedEOF);
        }
        co_return buffer.size();
    }

    auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> {
        if (buffer.size() > std::numeric_limits<std::uint32_t>::max()) {
            co_return ilias::Err(mMessageTooLarge);
        }
        ILIAS_CO_TRYV(co_await ilias::io::writeU32Le(mStream, static_cast<std::uint32_t>(buffer.size())));
        ILIAS_CO_TRY(auto writtenSize, (co_await ilias::io::writeAll(mStream, buffer) | ilias::unstoppable));
        if (writtenSize != buffer.size()) {
            co_return ilias::Err(ilias::IoError::WriteZero);
        }
        if (auto flushRet = co_await flush(); !flushRet) {
            co_return ilias::Err(flushRet.error());
        }
        co_return buffer.size();
    }

    auto close() -> void { close_stream(mStream); }
    auto shutdown() -> ilias::IoTask<void> { co_return co_await shutdown_stream(mStream); }
    auto flush() -> ilias::IoTask<void> { co_return co_await flush_stream(mStream); }

protected:
    auto _stream() noexcept -> StreamT& { return mStream; }
    auto _stream() const noexcept -> const StreamT& { return mStream; }

private:
    StreamT mStream;
    std::error_code mMessageTooLarge = ilias::IoError::MessageTooLarge;
};

template <typename DatagramT, typename EndpointT>
concept DatagramPeerEndpoint =
    requires(DatagramT datagram, std::span<std::byte> out, std::span<const std::byte> in, EndpointT endpoint) {
        { datagram.recvfrom(out) } -> std::same_as<ilias::IoTask<std::pair<std::size_t, EndpointT>>>;
        { datagram.sendto(in, endpoint) } -> std::same_as<ilias::IoTask<std::size_t>>;
        { datagram.close() } -> std::same_as<void>;
    };

template <typename DatagramT, typename EndpointT, std::size_t ChunkSize = 1500>
    requires DatagramPeerEndpoint<DatagramT, EndpointT>
class ChunkedDatagramMessageEndpoint {
public:
    ChunkedDatagramMessageEndpoint(DatagramT&& datagram, EndpointT endpoint,
                                   std::error_code messageTooLarge = ilias::IoError::MessageTooLarge)
        : mDatagram(std::move(datagram)), mEndpoint(std::move(endpoint)), mMessageTooLarge(messageTooLarge) {}

    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> {
        auto current = buffer.size();
        buffer.resize(current + ChunkSize);
        while (true) {
            auto ret = co_await mDatagram.recvfrom({buffer.data() + current, buffer.size() - current});
            if (!ret) {
                co_return ilias::Err(ret.error());
            }

            const auto readSize = ret.value().first;
            mEndpoint           = std::move(ret.value().second);
            if (readSize + current == buffer.size()) {
                current = buffer.size();
                buffer.resize(current + ChunkSize);
                continue;
            }
            buffer.resize(readSize + current);
            co_return readSize + current;
        }
    }

    auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> {
        if (buffer.size() >= ChunkSize) {
            co_return ilias::Err(mMessageTooLarge);
        }
        auto ret  = co_await mDatagram.sendto(buffer, mEndpoint);
        auto sent = ret.value_or(0);
        while (ret && sent < buffer.size()) {
            ret = co_await mDatagram.sendto({buffer.data() + sent, buffer.size() - sent}, mEndpoint);
            if (ret && ret.value() > 0) {
                sent += ret.value();
            } else {
                break;
            }
        }
        if (!ret) {
            co_return ilias::Err(ret.error());
        }
        co_return sent;
    }

    auto close() -> void { mDatagram.close(); }
    auto shutdown() -> ilias::IoTask<void> { co_return {}; }
    auto flush() -> ilias::IoTask<void> { co_return {}; }

protected:
    auto _datagram() noexcept -> DatagramT& { return mDatagram; }
    auto _endpoint() noexcept -> EndpointT& { return mEndpoint; }

private:
    DatagramT mDatagram;
    EndpointT mEndpoint;
    std::error_code mMessageTooLarge = ilias::IoError::MessageTooLarge;
};

} // namespace detail

NEKO_END_NAMESPACE
