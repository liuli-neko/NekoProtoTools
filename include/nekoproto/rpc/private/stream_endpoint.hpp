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
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/rpc/private/backend_base.hpp"

namespace nekoproto {
namespace rpc {

template <ilias::Stream StreamT, typename Codec, std::uint8_t CodecId>
class NekoRpcStreamEndpoint {
public:
    using Message = typename Codec::MessageType;

    explicit NekoRpcStreamEndpoint(StreamT stream, NekoRpcFrameLimits limits = {})
        : mStream(std::move(stream)), mLimits(limits) {}

    bool isStreamClosed() const noexcept {
        if (mClosed) {
            return true;
        }
        if constexpr (requires(const StreamT& s) { static_cast<bool>(s); }) {
            return !static_cast<bool>(mStream);
        }
        return false;
    }

    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<std::size_t> {
        if (!mReadMutex) {
            mReadMutex = std::make_shared<ilias::Mutex>();
        }
        auto guard = co_await mReadMutex->lock();
        if (isStreamClosed()) {
            co_return ilias::Err(ilias::IoError::UnexpectedEOF);
        }
        ILIAS_CO_TRYV(co_await readFrame(buffer));
        co_return buffer.size();
    }

    auto send(std::span<const std::byte> buffer) -> ilias::IoTask<std::size_t> {
        if (!mWriteMutex) {
            mWriteMutex = std::make_shared<ilias::Mutex>();
        }
        auto guard = co_await mWriteMutex->lock();
        if (isStreamClosed()) {
            co_return ilias::Err(ilias::IoError::UnexpectedEOF);
        }
        ILIAS_CO_TRYV(co_await writeFrame(buffer));
        co_return buffer.size();
    }

    void close() {
        mClosed = true;
        detail::closeStream(mStream);
    }
    auto shutdown() -> ilias::IoTask<void> { co_return co_await detail::shutdownStream(mStream); }
    auto flush() -> ilias::IoTask<void> { co_return co_await detail::flushStream(mStream); }

private:
    auto readFrame(Message& buffer) -> ilias::IoTask<void> {
        const auto header_size = Codec::headerSize();
        std::vector<std::byte> header_bytes(header_size);
        ILIAS_CO_TRY(auto header_ret, co_await ilias::io::readAll(
                                          mStream, std::span<std::byte>{header_bytes.data(), header_bytes.size()}));
        if (header_ret != header_size) {
            co_return ilias::Err(ilias::IoError::UnexpectedEOF);
        }

        const auto header = std::span<const std::byte>{header_bytes.data(), header_bytes.size()};
        ILIAS_CO_TRY(auto body_size, Codec::headerBodySize(header, CodecId, mLimits));

        buffer.clear();
        buffer.reserve(header_size + body_size);
        buffer.insert(buffer.end(), header_bytes.begin(), header_bytes.end());

        if (body_size == 0U) {
            co_return {};
        }

        buffer.resize(header_size + body_size);
        auto body_span = std::span<std::byte>{buffer.data() + header_size, body_size};
        // A partial peer frame must not make endpoint shutdown unbounded. If
        // this receive is cancelled, the stream is no longer frame-aligned and
        // the owning client/server closes it rather than reusing it.
        ILIAS_CO_TRY(auto body_ret, co_await ilias::io::readAll(mStream, body_span));
        if (body_ret != body_size) {
            co_return ilias::Err(ilias::IoError::UnexpectedEOF);
        }

        co_return {};
    }

    auto writeFrame(std::span<const std::byte> frame) -> ilias::IoTask<void> {
        const auto header_size = Codec::headerSize();
        if (frame.size() < header_size || frame.size() > mLimits.max_frame_bytes) {
            co_return ilias::Err(ilias::IoError::MessageTooLarge);
        }
        ILIAS_CO_TRY(auto body_size, Codec::headerBodySize(frame.first(header_size), CodecId, mLimits));
        if (body_size != frame.size() - header_size) {
            co_return ilias::Err(RpcError::InvalidRequest);
        }

        ILIAS_CO_TRY(auto ret, co_await ilias::io::writeAll(mStream, frame));
        if (ret != frame.size()) {
            co_return ilias::Err(ilias::IoError::WriteZero);
        }
        ILIAS_CO_TRYV(co_await detail::flushStream(mStream));
        co_return {};
    }

    StreamT mStream;
    NekoRpcFrameLimits mLimits;
    bool mClosed = false;
    std::shared_ptr<ilias::Mutex> mReadMutex  = std::make_shared<ilias::Mutex>();
    std::shared_ptr<ilias::Mutex> mWriteMutex = std::make_shared<ilias::Mutex>();
};

} // namespace rpc
} // namespace nekoproto
