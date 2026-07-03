#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <ilias/io.hpp>
#include <ilias/io/error.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/task.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/rpc/endpoint.hpp"

NEKO_BEGIN_NAMESPACE
namespace rpc {

template <ilias::Stream StreamT, typename Codec, std::uint8_t CodecId>
class NekoRpcStreamEndpoint {
public:
    using Message = typename Codec::MessageType;

    explicit NekoRpcStreamEndpoint(StreamT stream) : mStream(std::move(stream)) {}

    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> {
        ILIAS_CO_TRYV(co_await _readFrame(buffer));
        co_return buffer.size();
    }

    auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> {
        ILIAS_CO_TRYV(co_await _writeFrame(buffer));
        co_return buffer.size();
    }

    auto close() -> void { detail::close_stream(mStream); }
    auto shutdown() -> ilias::IoTask<void> { co_return co_await detail::shutdown_stream(mStream); }
    auto flush() -> ilias::IoTask<void> { co_return co_await detail::flush_stream(mStream); }

private:
    auto _readFrame(Message& buffer) -> ilias::IoTask<void> {
        const auto header_size = Codec::headerSize();
        std::vector<std::byte> header_bytes(header_size);
        ILIAS_CO_TRY(auto header_ret, co_await ilias::io::readAll(
                                          mStream, std::span<std::byte>{header_bytes.data(), header_bytes.size()}));
        if (header_ret != header_size) {
            co_return ilias::Err(ilias::IoError::UnexpectedEOF);
        }

        const auto header = std::span<const std::byte>{header_bytes.data(), header_bytes.size()};
        ILIAS_CO_TRY(auto body_size, Codec::headerBodySize(header, CodecId));

        buffer.clear();
        buffer.reserve(header_size + body_size);
        buffer.insert(buffer.end(), header_bytes.begin(), header_bytes.end());

        if (body_size == 0U) {
            co_return {};
        }

        buffer.resize(header_size + body_size);
        auto body_span = std::span<std::byte>{buffer.data() + header_size, body_size};
        ILIAS_CO_TRY(auto body_ret, co_await (ilias::io::readAll(mStream, body_span) | ilias::unstoppable()));
        if (body_ret != body_size) {
            co_return ilias::Err(ilias::IoError::UnexpectedEOF);
        }

        co_return {};
    }

    auto _writeFrame(std::span<const std::byte> frame) -> ilias::IoTask<void> {
        auto guard = co_await mWriteMutex->lock();
        ILIAS_CO_TRY(auto ret, co_await ilias::io::writeAll(mStream, frame));
        if (ret != frame.size()) {
            co_return ilias::Err(ilias::IoError::WriteZero);
        }
        ILIAS_CO_TRYV(co_await detail::flush_stream(mStream));
        co_return {};
    }

    StreamT mStream;
    std::unique_ptr<ilias::Mutex> mWriteMutex = std::make_unique<ilias::Mutex>();
};

} // namespace rpc
NEKO_END_NAMESPACE
